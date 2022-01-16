/* -*- Mode: C++; c-file-style: "gnu"; indent-tabs-mode:nil -*- */
/*
 * Copyright ( C ) 2020 New Mexico State University- Board of Regents
 *
 * George Torres, Anju Kunnumpurathu James
 * See AUTHORS.md for complete list of authors and contributors.
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * ( at your option ) any later version.

 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.

 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 *
 */

// Token bucket strategy edited to only use two queues
// We continue to have three queues, we just don't use the third.
// All interests have priority 1 (first queue) or 21 (second queue)

#include "qos-strategy.hpp"
#include "algorithm.hpp"
#include "core/logger.hpp"
#include "../../../apps/TBucketRef.cpp"
#include "TBucketDebug.hpp"
#include "ns3/simulator.h"
#include "../../../helper/ndn-scenario-helper.hpp"
#include "ns3/point-to-point-net-device.h"

#include <zmq.hpp>

#include <cstdlib> // for itoa

#include "ns3/queue.h"
#include "ns3/callback.h"

namespace nfd {
namespace fw {

NFD_REGISTER_STRATEGY( QosStrategy );

NFD_LOG_INIT( QosStrategy );

const time::milliseconds QosStrategy::RETX_SUPPRESSION_INITIAL( 10 );
const time::milliseconds QosStrategy::RETX_SUPPRESSION_MAX( 250 );

/* std::string determine_port()
{
    ns3::Ptr<ns3::Node> node= ns3::NodeContainer::GetGlobal().Get( ns3::Simulator::GetContext() );
    if (node->GetId() == 4) {
        return "tcp://localhost:5556"
    }
} */

std::string determine_port()
{
    ns3::Ptr<ns3::Node> node = ns3::NodeContainer::GetGlobal().Get(ns3::Simulator::GetContext());
    char a[256];
    sprintf(a, "%d", 5550 + node->GetId());
    std::string str = std::string("tcp://localhost:") + std::string(a);
    return str;
}

QosStrategy::QosStrategy( Forwarder& forwarder, const Name& name )
  : Strategy( forwarder )
  , ProcessNackTraits( this )
  , m_retxSuppression( RETX_SUPPRESSION_INITIAL,
                      RetxSuppressionExponential::DEFAULT_MULTIPLIER,
                      RETX_SUPPRESSION_MAX )
{
  CT.m_tokens = 0;
  ParsedInstanceName parsed = parseInstanceName( name );

  if( !parsed.parameters.empty() ) {
    BOOST_THROW_EXCEPTION( std::invalid_argument( "QosStrategy does not accept parameters" ) );
  }

  if( parsed.version && *parsed.version != getStrategyName()[-1].toVersion() ) {
    BOOST_THROW_EXCEPTION( std::invalid_argument( 
          "QosStrategy does not support version " + to_string( *parsed.version ) ) );
  }

  this->setInstanceName( makeInstanceName( name, getStrategyName() ) );

  ns3::Ptr<ns3::Node> node = ns3::NodeContainer::GetGlobal().Get( ns3::Simulator::GetContext() );

  CT.sender1[node->GetId()] = &m_sender1;
  CT.sender2[node->GetId()] = &m_sender2;
  CT.sender3[node->GetId()] = &m_sender3;
  CT.hasSender[node->GetId()] = true;
  CT.sender1[node->GetId()]->send.connect( [this]() {
      this->prioritySend();
      } );
  CT.sender2[node->GetId()]->send.connect( [this]() {
      this->prioritySend();
      } );
  CT.sender3[node->GetId()]->send.connect( [this]() {
      this->prioritySend();
      } );

  high_priority_packets = 0;
  med_priority_packets = 0;
  high_priority_loss = 0;
  med_priority_loss = 0;
  base_ns3_loss = 0;
  ns3::Simulator::Schedule(ns3::Seconds(1.0), &QosStrategy::everyNSeconds, this);
  // We are going to have multiple ports for multiple addresses
  // Testing multiple connections now.
  std::string port_address = determine_port();
  socket.connect(port_address);
  //socket.connect("tcp://localhost:5555");

  // test callback when dropped
  for (uint32_t devId = 0; devId < node->GetNDevices(); devId++) {
    ns3::Ptr<ns3::PointToPointNetDevice> p2pnd = ns3::DynamicCast<ns3::PointToPointNetDevice>(node->GetDevice(devId));
    p2pnd->GetQueue()->TraceConnectWithoutContext("Drop", ns3::MakeCallback(&QosStrategy::callback, this));
  }

  // counts the number of rounds of writing/reading statistics
  round_num = 0;
}

const Name&
QosStrategy::getStrategyName()
{
  static Name strategyName( "/localhost/nfd/strategy/qos/%FD%03" );
  return strategyName;
}

static bool
isNextHopEligible( const Face& inFace, const Interest& interest,
    const fib::NextHop& nexthop,
    const shared_ptr<pit::Entry>& pitEntry,
    bool wantUnused = false,
    time::steady_clock::TimePoint now = time::steady_clock::TimePoint::min() )
{
  const Face& outFace = nexthop.getFace();

  // Do not forward back to the same face, unless it is ad hoc.
  if( outFace.getId() == inFace.getId() && outFace.getLinkType() != ndn::nfd::LINK_TYPE_AD_HOC )
    return false;

  // Forwarding would violate scope.
  if( wouldViolateScope( inFace, interest, outFace ) )
    return false;

  if( wantUnused ) {
    // Nexthop must not have unexpired out-record
    auto outRecord = pitEntry->getOutRecord( outFace );
    if( outRecord != pitEntry->out_end() && outRecord->getExpiry() > now ) {
      return false;
    }
  }

  return true;
}

/** \brief Pick an eligible NextHop with earliest out-record.
 *  \note It is assumed that every nexthop has an out-record.
 */
static fib::NextHopList::const_iterator
findEligibleNextHopWithEarliestOutRecord( const Face& inFace, const Interest& interest,
    const fib::NextHopList& nexthops,
    const shared_ptr<pit::Entry>& pitEntry )
{
  auto found = nexthops.end();
  auto earliestRenewed = time::steady_clock::TimePoint::max();

  for( auto it = nexthops.begin(); it != nexthops.end(); ++it ) {
    if( !isNextHopEligible( inFace, interest, *it, pitEntry ) )
      continue;

    auto outRecord = pitEntry->getOutRecord( it->getFace() );
    BOOST_ASSERT( outRecord != pitEntry->out_end() );

    if( outRecord->getLastRenewed() < earliestRenewed ) {
      found = it;
      earliestRenewed = outRecord->getLastRenewed();
    }
  }

  return found;
}

void
QosStrategy::afterReceiveInterest( const Face& inFace, const Interest& interest,
                                        const shared_ptr<pit::Entry>& pitEntry )
{
  ns3::Ptr<ns3::Node> node = ns3::NodeContainer::GetGlobal().Get(ns3::Simulator::GetContext());
  struct QueueItem item( &pitEntry );
  std::string s = interest.getName().getSubName( 2,1 ).toUri();
  uint32_t pr_level;

  if( interest.getName().getSubName( 1,1 ).toUri() == "/typeI"  ) {
    pr_level = 1;
  } else if( interest.getName().getSubName( 1,1 ).toUri() == "/typeII"  ) {
    pr_level = 21;
  /*} else if( interest.getName().getSubName( 1,1 ).toUri() == "/typeIII"  ) {
    pr_level = 60;*/
  /*} else {
    pr_level = 60;*/
  } else {
    pr_level = 21;
  }

  if( interest.getName().getSubName( 1,1 ).toUri() == "/typeI"  ) {
    high_priority_packets += 1;
  } else {
    med_priority_packets += 1;
  }

  item.wireEncode = interest.wireEncode();
  item.packetType = INTEREST;
  item.inface = &inFace;

  if( pr_level != 60  ) {
    const fib::Entry& fibEntry = this->lookupFib( *pitEntry );
    const fib::NextHopList& nexthops = fibEntry.getNextHops();
    int nEligibleNextHops = 0;
    bool isSuppressed = false;

    for( const auto& nexthop : nexthops ) {
      Face& outFace = nexthop.getFace();
      RetxSuppressionResult suppressResult = m_retxSuppression.decidePerUpstream( *pitEntry, outFace );

      if( suppressResult == RetxSuppressionResult::SUPPRESS ) {
        NFD_LOG_DEBUG( interest << " from=" << inFace.getId()
            << "to=" << outFace.getId() << " suppressed" );
        isSuppressed = true;
        continue;
      }

      if( ( outFace.getId() == inFace.getId() && outFace.getLinkType() != ndn::nfd::LINK_TYPE_AD_HOC ) ||
          wouldViolateScope( inFace, interest, outFace ) ) {
        continue;
      }

      uint32_t f = outFace.getId();
      item.outface = &outFace;

      bool queue_result = m_tx_queue[f].DoEnqueue( item, pr_level );
      if (!queue_result) {
        if (1 <= pr_level && pr_level <= 20)
          high_priority_loss += 1;
        if (21 <= pr_level && pr_level <= 40)
          med_priority_loss += 1;
      }

      NFD_LOG_DEBUG( interest << " from=" << inFace.getId()
          << " pitEntry-to=" << outFace.getId() );

      if( suppressResult == RetxSuppressionResult::FORWARD ) {
        m_retxSuppression.incrementIntervalForOutRecord( *pitEntry->getOutRecord( outFace ) );
      }

      ++nEligibleNextHops;
    }

    if( nEligibleNextHops == 0 && !isSuppressed ) {
      NFD_LOG_DEBUG( interest << " from=" << inFace.getId() << " noNextHop" );

      lp::NackHeader nackHeader;

      nackHeader.setReason( lp::NackReason::NO_ROUTE );

      this->sendNack( pitEntry, inFace, nackHeader );
      this->rejectPendingInterest( pitEntry );
    }

    RetxSuppressionResult suppression = m_retxSuppression.decidePerPitEntry( *pitEntry );

    if( suppression == RetxSuppressionResult::SUPPRESS ) {
      NFD_LOG_DEBUG( interest << " from=" << inFace.getId()
          << " suppressed" );
      return;
    }
  } else {
    RetxSuppressionResult suppression = m_retxSuppression.decidePerPitEntry( *pitEntry );

    if( suppression == RetxSuppressionResult::SUPPRESS ) {
      NFD_LOG_DEBUG( interest << " from=" << inFace.getId()
          << " suppressed" );
      return;
    }

    const fib::Entry& fibEntry = this->lookupFib( *pitEntry );
    const fib::NextHopList& nexthops = fibEntry.getNextHops();
    auto it = nexthops.end();

    if( suppression == RetxSuppressionResult::NEW ) {
      // Forward to nexthop with lowest cost except downstream.
      it = std::find_if( nexthops.begin(), nexthops.end(), [&] ( const auto& nexthop ) {
          return isNextHopEligible( inFace, interest, nexthop, pitEntry );
          } );

      if( it == nexthops.end() ) {
        NFD_LOG_DEBUG( interest << " from=" << inFace.getId() << " noNextHop" );

        lp::NackHeader nackHeader;
        nackHeader.setReason( lp::NackReason::NO_ROUTE );
        this->sendNack( pitEntry, inFace, nackHeader );

        this->rejectPendingInterest( pitEntry );
        return;
      }

      Face& outFace = it->getFace();
      uint32_t f = outFace.getId();
      item.outface = &outFace;
      bool queue_result = m_tx_queue[f].DoEnqueue( item, pr_level );
      if (!queue_result) {
        if (1 <= pr_level && pr_level <= 20)
          high_priority_loss += 1;
        if (21 <= pr_level && pr_level <= 40)
          med_priority_loss += 1;
      }

      NFD_LOG_DEBUG( interest << " from=" << inFace.getId()
          << " newPitEntry-to=" << outFace.getId() );
      prioritySend();
      return;
    }

    // Find an unused upstream with lowest cost except downstream.
    it = std::find_if( nexthops.begin(), nexthops.end(), [&] ( const auto& nexthop ) {
        return isNextHopEligible( inFace, interest, nexthop, pitEntry, true, time::steady_clock::now() );
        } );

    if( it != nexthops.end() ) {
      Face& outFace = it->getFace();
      uint32_t f = outFace.getId();
      item.outface = &outFace;
      bool queue_result = m_tx_queue[f].DoEnqueue( item, pr_level );
      if (!queue_result) {
        if (1 <= pr_level && pr_level <= 20)
          high_priority_loss += 1;
        if (21 <= pr_level && pr_level <= 40)
          med_priority_loss += 1;
      }

      NFD_LOG_DEBUG( interest << " from=" << inFace.getId()
          << " retransmit-unused-to=" << outFace.getId() );

      prioritySend();
      return;
    }

    // Find an eligible upstream that is used earliest.
    it = findEligibleNextHopWithEarliestOutRecord( inFace, interest, nexthops, pitEntry );
    if( it == nexthops.end() ) {
      NFD_LOG_DEBUG( interest << " from=" << inFace.getId() << " retransmitNoNextHop" );
    } else {
      Face& outFace = it->getFace();
      uint32_t f = outFace.getId();
      item.outface = &outFace;
      bool queue_result = m_tx_queue[f].DoEnqueue( item, pr_level );
      if (!queue_result) {
        if (1 <= pr_level && pr_level <= 20)
          high_priority_loss += 1;
        if (21 <= pr_level && pr_level <= 40)
          med_priority_loss += 1;
      }

      NFD_LOG_DEBUG( interest << " from=" << inFace.getId()
          << " retransmit-retry-to=" << outFace.getId() );
      prioritySend();
      return;
    }
  }

  prioritySend();
}

void
QosStrategy::afterReceiveNack( const Face& inFace, const lp::Nack& nack,
    const shared_ptr<pit::Entry>& pitEntry )
{
  return;
  struct QueueItem item( &pitEntry );

  //std::cout << "***Nack name: " << nack.getInterest().getName() << " Reason: "<<nack.getReason()<< std::endl;

  std::string s = nack.getInterest().getName().getSubName( 2,1 ).toUri();
  uint32_t pr_level;

  if( nack.getInterest().getName().getSubName( 1,1 ).toUri() == "/typeI"  ) {
    pr_level = 1;
  } else if( nack.getInterest().getName().getSubName( 1,1 ).toUri() == "/typeII"  ) {
    pr_level = 21;
  /*} else if( interest.getName().getSubName( 1,1 ).toUri() == "/typeIII"  ) {
    pr_level = 60;*/
  /*} else {
    pr_level = 60;*/
  } else {
    pr_level = 21;
  }

  if( nack.getInterest().getName().getSubName( 1,1 ).toUri() == "/typeI"  ) {
    high_priority_packets += 1;
  } else {
    med_priority_packets += 1;
  }

  item.wireEncode = nack.getInterest().wireEncode();
  item.packetType = NACK;
  item.inface = &inFace;
  uint32_t f = inFace.getId();
  this->processNack( inFace, nack, pitEntry );
}

void
QosStrategy::afterReceiveData( const shared_ptr<pit::Entry>& pitEntry,
                           const Face& inFace, const Data& data )
{
  struct QueueItem item( &pitEntry );

  NFD_LOG_DEBUG( "afterReceiveData pitEntry=" << pitEntry->getName() <<
      " inFace=" << inFace.getId() << " data=" << data.getName() );
  //std::cout << "***Data name: " << data.getName() << std::endl;

  this->beforeSatisfyInterest( pitEntry, inFace, data );

  std::string s = data.getName().getSubName( 2,1 ).toUri();
  uint32_t pr_level;

  if( data.getName().getSubName( 1,1 ).toUri() == "/typeI"  ) {
    pr_level = 1;
  } else if( data.getName().getSubName( 1,1 ).toUri() == "/typeII"  ) {
    pr_level = 21;
  /*} else if( interest.getName().getSubName( 1,1 ).toUri() == "/typeIII"  ) {
    pr_level = 60;*/
  /*} else {
    pr_level = 60;*/
  } else {
    pr_level = 21;
  }

  if( data.getName().getSubName( 1,1 ).toUri() == "/typeI"  ) {
    high_priority_packets += 1;
  } else {
    med_priority_packets += 1;
  }

  item.wireEncode = data.wireEncode();
  item.packetType = DATA;
  item.inface = &inFace;
  std::set<Face*> pendingDownstreams;
  auto now = time::steady_clock::now();

  // Remember pending downstreams
  for( const pit::InRecord& inRecord : pitEntry->getInRecords() ) {
    if( inRecord.getExpiry() > now ) {
      if( inRecord.getFace().getId() == inFace.getId() &&
          inRecord.getFace().getLinkType() != ndn::nfd::LINK_TYPE_AD_HOC ) {
        continue;
      }
      pendingDownstreams.insert( &inRecord.getFace() );
    }
  }

  for( const Face* pendingDownstream : pendingDownstreams ) {
    uint32_t f = ( *pendingDownstream ).getId();
    item.outface = pendingDownstream;
    bool queue_result = m_tx_queue[f].DoEnqueue( item, pr_level );
    if (!queue_result) {
      if (1 <= pr_level && pr_level <= 20)
        high_priority_loss += 1;
      if (21 <= pr_level && pr_level <= 40)
        med_priority_loss += 1;
    }
  }

  prioritySend();
}

void
QosStrategy::prioritySend()
{
  ns3::Ptr<ns3::Node> node= ns3::NodeContainer::GetGlobal().Get( ns3::Simulator::GetContext() );
  Interest interest;
  Data data;
  lp::Nack nack;
  double TOKEN_REQUIRED = 1;
  bool tokenwait = false;

  std::unordered_map<uint32_t,NdnPriorityTxQueue >::iterator itt = m_tx_queue.begin();

  while( itt != m_tx_queue.end() ) {

    while( !m_tx_queue[itt->first].IsEmpty() && !tokenwait ) {

      double token1 = CT.sender1[node->GetId()]->m_capacity;
      double token2 = CT.sender2[node->GetId()]->m_capacity;
      double token3 = CT.sender3[node->GetId()]->m_capacity;

      if( CT.sender1[node->GetId()]->m_tokens.find( itt->first ) != CT.sender1[node->GetId()]->m_tokens.end() )
        token1 = CT.sender1[node->GetId()]->m_tokens[itt->first];
      if( CT.sender2[node->GetId()]->m_tokens.find( itt->first ) != CT.sender2[node->GetId()]->m_tokens.end() )
        token2 = CT.sender2[node->GetId()]->m_tokens[itt->first];
      if( CT.sender3[node->GetId()]->m_tokens.find( itt->first ) != CT.sender3[node->GetId()]->m_tokens.end() )
        token3 = CT.sender3[node->GetId()]->m_tokens[itt->first];

      int choice = m_tx_queue[itt->first].SelectQueueToSend( token1, token2, token3 );

      if( choice != -1 ) {
        TokenBucket *sender;

        if( choice == 0 ) {
          sender = CT.sender1[node->GetId()];
        } else if( choice == 1 ){ sender = CT.sender2[node->GetId()];
        } else {
          sender = CT.sender3[node->GetId()];
        }

        sender->hasFaces = true;
        sender->consumeToken( TOKEN_REQUIRED, itt->first );
        sender->m_need[itt->first] = 0;

        // Dequeue the packet
        struct QueueItem item = m_tx_queue[itt->first].DoDequeue( choice );
        const shared_ptr<pit::Entry>* PE = &( item.pitEntry );

        switch( item.packetType ) {

          case INTEREST:
            interest.wireDecode(  item.wireEncode  );
            prioritySendInterest( *( PE ), *( item.inface ), interest, ( *item.outface ) );
            break;

          case DATA:
            //std::cout<<"prioritySend( DATA )\n";
            data.wireDecode(  item.wireEncode  );
            prioritySendData( *( PE ), *( item.inface ), data, ( *item.outface ) );
            break;

          case NACK:
            //std::cout<<"prioritySend( NACK )\n";
            interest.wireDecode(  item.wireEncode  );
            nack = lp::Nack( interest );
            prioritySendNack( *( PE ), *( item.inface ), nack );
            break;

          default:
            //std::cout<<"prioritySend( Invalid Type )\n";
            break;
        }
      } else {

        CT.sender1[node->GetId()]->m_need[itt->first] =  m_tx_queue[itt->first].tokenReqHig();
        CT.sender2[node->GetId()]->m_need[itt->first] =  m_tx_queue[itt->first].tokenReqMid();
        CT.sender3[node->GetId()]->m_need[itt->first] =  m_tx_queue[itt->first].tokenReqLow();

        tokenwait = true;
      }
    }

    itt++;
  }
}

void
QosStrategy::onDroppedInterest(const Face& outFace, const Interest& interest)
{
  if( interest.getName().getSubName( 1,1 ).toUri() == "/typeI"  ) {
    high_priority_loss += 1;
  } else {
    med_priority_loss += 1;
  }
  Strategy::onDroppedInterest(outFace, interest);
}

void
QosStrategy::prioritySendData( const shared_ptr<pit::Entry>& pitEntry,
                            const Face& inFace, const Data& data, const Face& outFace )
{
  this->sendData( pitEntry, data, outFace );
}

void
QosStrategy::prioritySendNack( const shared_ptr<pit::Entry>& pitEntry,
                            const Face& inFace, const lp::Nack& nack )
{
  this->processNack( inFace, nack, pitEntry );
}

void
QosStrategy::prioritySendInterest( const shared_ptr<pit::Entry>& pitEntry,
                            const Face& inFace, const Interest& interest, const Face& outFace )
{
  /*
  const fib::Entry& fibEntry = this->lookupFib( *pitEntry );
  const fib::NextHopList& nexthops = fibEntry.getNextHops();
  int nEligibleNextHops = 0;
  bool isSuppressed = false;

  for( const auto& nexthop : nexthops ) {
    Face& outFace = nexthop.getFace();
    RetxSuppressionResult suppressResult = m_retxSuppression.decidePerUpstream( *pitEntry, outFace );

    if( suppressResult == RetxSuppressionResult::SUPPRESS ) {
      NFD_LOG_DEBUG( interest << " from=" << inFace.getId()
          << "to=" << outFace.getId() << " suppressed" );
      isSuppressed = true;
      continue;
    }

    if( ( outFace.getId() == inFace.getId() && outFace.getLinkType() != ndn::nfd::LINK_TYPE_AD_HOC ) ||
        wouldViolateScope( inFace, interest, outFace ) ) {
      continue;
    }
    */

    //Face& outface = outFace;

    this->sendInterest( pitEntry, *const_pointer_cast<Face>( outFace.shared_from_this() ), interest );

    /*
    NFD_LOG_DEBUG( interest << " from=" << inFace.getId()
        << " pitEntry-to=" << outFace.getId() );
    if( suppressResult == RetxSuppressionResult::FORWARD ) {
      m_retxSuppression.incrementIntervalForOutRecord( *pitEntry->getOutRecord( outFace ) );
    }

    ++nEligibleNextHops;
  }

  if( nEligibleNextHops == 0 && !isSuppressed ) {
    NFD_LOG_DEBUG( interest << " from=" << inFace.getId() << " noNextHop" );
    lp::NackHeader nackHeader;
    nackHeader.setReason( lp::NackReason::NO_ROUTE );
    //this->sendNack( pitEntry, inFace, nackHeader );
    this->rejectPendingInterest( pitEntry );
  }
  */
}

// Copy a number of bytes from a memory buffer into a zeroMQ socket. Wait to get reply back and discard it.
void QosStrategy::send_string_and_receive_reply(char *memory, uint64_t numbytes)
{
    std::cout << "Node " << ns3::NodeContainer::GetGlobal().Get( ns3::Simulator::GetContext() )->GetId() << " sending " << memory << std::endl;
    // Building the buffer directly from a string doesn't seem to include the \0 at the end of the string. So I am using memcpy instead.
    zmq::message_t buf(256);
    memcpy (buf.data (), memory, numbytes);
    socket.send(buf, zmq::send_flags::none);
    // wait for reply from server
    zmq::message_t reply{};
    socket.recv(reply, zmq::recv_flags::none);
    // don't do anything with the reply. I only had the reply be received because we apparently need that.
}

// Send message to python agent asking for action, and get reply which is the ID of an action.
int QosStrategy::receive_action() {
    zmq::message_t buf(256);
    memcpy (buf.data(), "Action", 7);
    socket.send(buf, zmq::send_flags::none);
    // wait for reply from server
    zmq::message_t reply{};
    socket.recv(reply, zmq::recv_flags::none);
    std::cout << "Node " << ns3::NodeContainer::GetGlobal().Get( ns3::Simulator::GetContext() )->GetId() << " received action " << reply.to_string() << std::endl;
    if ('0' <= reply.to_string()[0] && reply.to_string()[0] <= '8') {
        return (int) (reply.to_string()[0] - '0');
    }
    std::cout << "Error: Cannot read reply to request for action." << std::endl;
    return 0;
}

// xyzzy. How much should we change the token rate in each action?
void QosStrategy::perform_action(int action) {
  if (learning_mode) {
    ns3::Ptr<ns3::Node> node= ns3::NodeContainer::GetGlobal().Get( ns3::Simulator::GetContext() );
    int new_fill_rate_1 = CT.sender1[node->GetId()]->m_fillRate;
    int new_fill_rate_2 = CT.sender2[node->GetId()]->m_fillRate;
    /* switch (action) {
    case 0:
      if (new_fill_rate_1 + 5 + new_fill_rate_2 <= 120) {
        new_fill_rate_1 += 5;
      }
      break;
    case 1:
      if (new_fill_rate_1 + 10 + new_fill_rate_2 <= 120) {
        new_fill_rate_1 += 10;
      }
      break;
    case 2:
      if (new_fill_rate_1 > 5) {
        new_fill_rate_1 -= 5;
      }
      break;
    case 3:
      if (new_fill_rate_1 > 10) {
        new_fill_rate_1 -= 10;
      }
      break;
    case 4:
      if (new_fill_rate_1 + new_fill_rate_2 + 5 <= 120) {
        new_fill_rate_2 += 5;
      }
      break;
    case 5:
      if (new_fill_rate_1 + new_fill_rate_2 + 10 <= 120) {
        new_fill_rate_2 += 10;
      }
      break;
    case 6:
      if (new_fill_rate_2 > 5) {
        new_fill_rate_2 -= 5;
      }
      break;
    case 7:
      if (new_fill_rate_2 > 10) {
        new_fill_rate_2 -= 10;
      }
      break;
    } */
    switch (action) {
    case 0:
      new_fill_rate_1 += 10;
      break;
    case 1:
      if (new_fill_rate_1 > 10) {
        new_fill_rate_1 -= 10;
      }
      break;
    case 2:
      if (new_fill_rate_1 > 10) {
        new_fill_rate_1 -= 10;
        new_fill_rate_2 += 10;
      }
      break;
    case 3:
      new_fill_rate_2 += 10;
      break;
    case 4:
      if (new_fill_rate_2 > 10) {
        new_fill_rate_2 -= 10;
      }
      break;
    case 5:
      if (new_fill_rate_2 > 10) {
        new_fill_rate_1 += 10;
        new_fill_rate_2 -= 10;
      }
      break;
    }
    CT.sender1[node->GetId()]->m_fillRate = new_fill_rate_1;
    CT.sender2[node->GetId()]->m_fillRate = new_fill_rate_2;
    printf("Node %d, action %d; Fill rates changed to (%d, %d)\n", node->GetId(), action, new_fill_rate_1, new_fill_rate_2);
  }
}

void QosStrategy::callback(ns3::Ptr<const ns3::Packet> packet) {
    base_ns3_loss += 1;
}

// write the performance data down in the input file
void
QosStrategy::everyNSeconds()
{
    ns3::Ptr<ns3::Node> node= ns3::NodeContainer::GetGlobal().Get( ns3::Simulator::GetContext() );

    // We have to start out with a buffer that we copy our messages into.
    char msg[256];
    FILE *f = fopen("nn-input.txt", "a");

    // Send back reward: based on the droppage of high priority, low priority, and base ns-3 packets averaged over all interfaces
    double high_reward = 1 - (((double) high_priority_loss) / high_priority_packets) / m_tx_queue.size();
    double low_reward = 1 - (((double) med_priority_loss) / med_priority_packets) / m_tx_queue.size();
    double base_reward = 1 - (((double) base_ns3_loss) / (high_priority_packets + med_priority_packets)) / m_tx_queue.size();
    double reward = 0.4*high_reward + 0.2*low_reward + 0.4*base_reward;
    sprintf(msg, "%f", reward);
    send_string_and_receive_reply(msg, 256);

    // I'll get back to this later. I don't think this is the right way to implement this. I should probably separate the channels for each node first.
    // // Send back reward
    // if (round_num > 0) {
    //     zmq::message_t note{};
    //     socket.recv(note, zmq::recv_flags::none);
    //     if (!strcmp((char *) note.data(), "Reward")) {
    //         sprintf(msg, "Node: %d\n%d", node->GetId(), high_priority_packets);
    //         socket.send(buf, zmq::send_flags::none);
    //     }
    // }

    // Print identifier of node
    fprintf(f, "Node: %d", node->GetId());
    sprintf(msg, "Node: %d", node->GetId());
    send_string_and_receive_reply(msg, 256);

    std::unordered_map<uint32_t,NdnPriorityTxQueue >::iterator itt = m_tx_queue.begin();
    // for each of the two priority queues, report the average number of enqueued packets for all faces
    int total_packets_high = 0;
    int total_packets_low = 0;
    while( itt != m_tx_queue.end() ) {
        total_packets_high += m_tx_queue[itt->first].GetCurrQueueSize2(0);
        total_packets_low += m_tx_queue[itt->first].GetCurrQueueSize2(1);
        itt++;
    }
    fprintf(f, "%f %f ", total_packets_high / (double) m_tx_queue.size(), total_packets_low / (double) m_tx_queue.size());
    sprintf(msg, "Queue sizes: %f %f ", total_packets_high / (double) m_tx_queue.size(), total_packets_low / (double) m_tx_queue.size());
    send_string_and_receive_reply(msg, strlen(msg) + 1);
    // print number of incoming packets of each priority
    fprintf(f, "%d %d ", high_priority_packets, med_priority_packets);
    sprintf(msg, "Incoming packets: %d %d ", high_priority_packets, med_priority_packets);
    send_string_and_receive_reply(msg, strlen(msg) + 1);
    fprintf(f, "\n");
    fclose(f);

    // trace the droppage from the priority queue, the droppage from the base ns-3 queue is already traced
    f = fopen("priority-queue-drop-trace.txt", "a");
    fprintf(f, "%d\t%d\t%d\t%d\n", round_num, node->GetId(), high_priority_loss, med_priority_loss);
    fclose(f);
    // clear interest and data counts
    //interest_counts = std::map<std::string, int>();
    //data_counts = std::map<std::string, int>();
    high_priority_packets = 0;
    med_priority_packets = 0;
    high_priority_loss = 0;
    med_priority_loss = 0;
    base_ns3_loss = 0;

    // send data on token bucket rates
    sprintf(msg, "Rates: %d %d ", nfd::fw::CT.sender1[node->GetId()]->m_fillRate, nfd::fw::CT.sender2[node->GetId()]->m_fillRate);
    send_string_and_receive_reply(msg, strlen(msg) + 1);

    int action = receive_action();
    perform_action(action);

    round_num += 1;
    ns3::Simulator::Schedule(ns3::Seconds(1.0), &QosStrategy::everyNSeconds, this);
}

} // namespace fw
} // namespace nfd
