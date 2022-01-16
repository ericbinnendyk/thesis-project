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

// Eric's idea:
/* Let's add an attribute for token rate to the TokenBucket object itself, and
  then have this value initialized by TokenBucketDriver (which is the object
  that receives the token rate parameters from the outside). The actual attributes
  in the app are discarded after use, and the token rates in the TokenBucket objects
  are referenced when deciding when to schedule the next token. */

#include "tokenBucketDriver.hpp"
#include "ns3/log.h"
#include "ns3/string.h"
#include "ns3/uinteger.h"
#include "ns3/packet.h"
#include "ns3/simulator.h"
#include "../NFD/daemon/fw/ndn-token-bucket.hpp"
#include "TBucketRef.hpp"
#include "ns3/double.h"
#include "ns3/simulator.h"
#include "../helper/ndn-scenario-helper.hpp"


#include "model/ndn-l3-protocol.hpp"
#include "helper/ndn-fib-helper.hpp"

#include <memory>

NS_LOG_COMPONENT_DEFINE( "ndn.TBDriver" );

namespace ns3 {
namespace ndn {

NS_OBJECT_ENSURE_REGISTERED( TBDriver );

TypeId
TBDriver::GetTypeId( void )
{
  static TypeId tid =
    TypeId( "ns3::ndn::TokenBucketDriver" )
      .SetGroupName( "Ndn" )
      .SetParent<App>()
      .AddConstructor<TBDriver>()
      .AddAttribute( "FillRate1", "Fill rate of token bucket", StringValue( "1.0" ),
                    MakeDoubleAccessor( &TBDriver::m_fillRate1 ), MakeDoubleChecker<double>() )
      .AddAttribute( "Capacity1", "Capacity of token bucket", StringValue( "80" ),
                    MakeDoubleAccessor( &TBDriver::m_capacity1 ), MakeDoubleChecker<double>() )
      .AddAttribute( "FillRate2", "Fill rate of token bucket", StringValue( "1.0" ),
                    MakeDoubleAccessor( &TBDriver::m_fillRate2 ), MakeDoubleChecker<double>() )
      .AddAttribute( "Capacity2", "Capacity of token bucket", StringValue( "80" ),
                    MakeDoubleAccessor( &TBDriver::m_capacity2 ), MakeDoubleChecker<double>() )
      .AddAttribute( "FillRate3", "Fill rate of token bucket", StringValue( "1.0" ),
                    MakeDoubleAccessor( &TBDriver::m_fillRate3 ), MakeDoubleChecker<double>() )
      .AddAttribute( "Capacity3", "Capacity of token bucket", StringValue( "80" ),
                    MakeDoubleAccessor( &TBDriver::m_capacity3 ), MakeDoubleChecker<double>() );

  return tid;
}

TBDriver::TBDriver()
  :m_first1( true ),
   m_first2( true ),
   m_first3( true ),
   m_connected( false )
{
    NS_LOG_FUNCTION_NOARGS();
}

// Inherited from Application base class.
void
TBDriver::StartApplication()
{
  NS_LOG_FUNCTION_NOARGS();
  App::StartApplication();

  // set fill rates of token buckets (which are now stored in the buckets themselves) to fillRate parameters
  ns3::Ptr<ns3::Node> node= ns3::NodeContainer::GetGlobal().Get( ns3::Simulator::GetContext() );
  nfd::fw::CT.sender1[node->GetId()]->m_fillRate = m_fillRate1;
  nfd::fw::CT.sender2[node->GetId()]->m_fillRate = m_fillRate2;
  nfd::fw::CT.sender3[node->GetId()]->m_fillRate = m_fillRate3;

  ScheduleNextToken( 0 );
  ScheduleNextToken( 1 );
  ScheduleNextToken( 2 );
}

void
TBDriver::StopApplication()
{
  NS_LOG_FUNCTION_NOARGS();
  App::StopApplication();
}

void
TBDriver::ScheduleNextToken( int bucket )
{
  ns3::Ptr<ns3::Node> node= ns3::NodeContainer::GetGlobal().Get( ns3::Simulator::GetContext() );

  bool first;
  double fillRate;

  if ( bucket == 0 ) {
    first = m_first1;
    fillRate = nfd::fw::CT.sender1[node->GetId()]->m_fillRate;
  } else if ( bucket == 1 ) {
    first = m_first2;
    fillRate = nfd::fw::CT.sender2[node->GetId()]->m_fillRate;
  } else {
    first = m_first3;
    fillRate = nfd::fw::CT.sender3[node->GetId()]->m_fillRate;
  }

  if ( first ) {
    m_sendEvent = Simulator::Schedule( Seconds( 0.0 ), &TBDriver::UpdateBucket, this, bucket );
  } else {
    m_sendEvent = Simulator::Schedule( Seconds( 1.0 /fillRate ),
        &TBDriver::UpdateBucket, this, bucket );
  }
}

void
TBDriver::UpdateBucket( int bucket )
{ 
  bool first;
  double capacity;
  nfd::fw::TokenBucket* sender;

  int node= ns3::NodeContainer::GetGlobal().Get( ns3::Simulator::GetContext() )->GetId();

  if ( m_connected == false && nfd::fw::CT.hasSender[node] == true ) {
    m_connected = true;
  }

  if ( bucket == 0 ) { 
    first = m_first1; 
    m_first1 = false;
    capacity = m_capacity1;
    sender = nfd::fw::CT.sender1[node];
  } else if ( bucket == 1 ) { 
    first = m_first2; 
    m_first2 = false;
    capacity = m_capacity2;
    sender = nfd::fw::CT.sender2[node];
  } else {
    first = m_first3; 
    m_first3 = false;
    capacity = m_capacity3;
    sender = nfd::fw::CT.sender3[node];
  }

  // Check to make sure tokens are not generated beyong specified capacity
  if ( m_connected == true ) {
    sender->m_capacity = capacity; // I don't know why we need this line. Why do we have to set the token bucket's capacity to the same value when it should never be changed in the first place?
    sender->addToken();
  }

  ScheduleNextToken( bucket );
}

double
TBDriver::GetFillRate( int bucket )
{
  ns3::Ptr<ns3::Node> node= ns3::NodeContainer::GetGlobal().Get( ns3::Simulator::GetContext() );
  switch (bucket) {
  case 0:
    return nfd::fw::CT.sender1[node->GetId()]->m_fillRate;
  case 1:
    return nfd::fw::CT.sender2[node->GetId()]->m_fillRate;
  case 2:
    return nfd::fw::CT.sender3[node->GetId()]->m_fillRate;
  default:
    return 0;
  }
}

void
TBDriver::SetFillRate( int bucket, double value )
{
  ns3::Ptr<ns3::Node> node= ns3::NodeContainer::GetGlobal().Get( ns3::Simulator::GetContext() );
  switch (bucket) {
  case 0:
    nfd::fw::CT.sender1[node->GetId()]->m_fillRate = value;
    break;
  case 1:
    nfd::fw::CT.sender2[node->GetId()]->m_fillRate = value;
    break;
  case 2:
    nfd::fw::CT.sender3[node->GetId()]->m_fillRate = value;
    break;
  }
}

/*// Copy a number of bytes from a memory buffer into a zeroMQ socket (to the forwarder). Wait to get reply back and discard it.
void TBDriver::update_token_bucket_rates(char *memory, uint64_t numbytes)
{
    // Building the buffer directly from a string doesn't seem to include the \0 at the end of the string. So I am using memcpy instead.
    zmq::message_t reply{};
    socket_app.recv(reply, zmq::recv_flags::none);
    std::cout << "Received action " << reply.to_string() << std::endl;
    if (reply.to_string()[0] == '0') {
        if (m_fillRate1 < 3995) {
            m_fillRate1 += 5;
        }
    }
    if (reply.to_string()[0] == '1') {
        if (m_fillRate1 > 5) {
            m_fillRate1 -= 5;
        }
    }
    // wait for reply from server
    zmq::message_t reply{};
    socket_app.send(reply, zmq::send_flags::none);
    // don't do anything with the reply. I only had the reply be received because we apparently need that.
}*/

/*// write the performance data down in the input file
void
TBDriver::everyNSeconds()
{
    // We have to start out with a buffer that we copy our messages into.
    char msg[256];
    ns3::Ptr<ns3::Node> node= ns3::NodeContainer::GetGlobal().Get( ns3::Simulator::GetContext() );
    if (node->GetId() == 5) {
        FILE *f = fopen("nn-input.txt", "a");
        fprintf(f, "Node: %d", node->GetId());
        sprintf(msg, "Node: %d", node->GetId());
        update_token_bucket_rates();

        //std::unordered_map<uint32_t,NdnPriorityTxQueue >::iterator itt = m_tx_queue.begin();

        // write parameters to NN input file
        fprintf(f, "Rates: %d %d ", (int) m_fillRate1, (int) m_fillRate2);
        sprintf(msg, "Rates: %d %d ", (int) m_fillRate1, (int) m_fillRate2);
        update_token_bucket_rates();
        //fprintf(f, "%d", 10); // write down the setting of ROUTE_RENEW_LIFETIME -- no, this stays constant throughout the simulation
        fprintf(f, "\n");
        fclose(f);

        // Updating actual token rate

    }
    // clear interest and data counts
    //interest_counts = std::map<std::string, int>();
    //data_counts = std::map<std::string, int>();
    ns3::Simulator::Schedule(ns3::Seconds(1.0), &TBDriver::everyNSeconds, this);
}*/

} // namespace ndn
} // namespace ns3
