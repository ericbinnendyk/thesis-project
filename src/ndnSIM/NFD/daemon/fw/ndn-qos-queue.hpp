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

#ifndef QOS_QUEUE_H
#define QOS_QUEUE_H

#include <list>
#include <ndn-cxx/encoding/block.hpp>
#include <NFD/daemon/table/pit-entry.hpp>
#include <NFD/daemon/face/face.hpp>
#include "ns3/log.h"

#define QUEUE_SIZE 10

using namespace std;

namespace nfd {
namespace fw {


/**
 * \brief Enumerator used to describe packet type.
 */
	
enum QosPacketType
{
  INVALID = -1,
  INTEREST = 0,
  DATA,
  NACK
};



/**
 * \brief Defines queue item for use qos queues.
 *
 * Saves packet details, such as block representation, incoming interface, type, etc.
 */

struct QueueItem
{
  ndn::Block wireEncode;
  QosPacketType packetType;
  shared_ptr<pit::Entry> pitEntry;
  const Face* inface;
  const Face* outface;

  QueueItem() : wireEncode( 0 ),
  packetType( INVALID ),
  pitEntry( NULL ),
  inface( NULL ),
  outface( NULL ) { }
  QueueItem( const shared_ptr<pit::Entry>* pe ) : wireEncode( 0 ),
  packetType( INVALID ),
  inface( NULL ),
  outface( NULL ) {
    shared_ptr<pit::Entry> temp( *pe );
    pitEntry = temp;
  }
};


/**
 * @ingroup ndnQoS
 * \brief Class that implements QoS queue for NDN packets.
 *
 */

class QosQueue
{

public:

  /** \brief Constructor.
   */
  QosQueue();

  /** \brief Set the max queue size.
   *  \param size The maximum allowable size of the queue.
   */
  void
  SetMaxQueueSize( uint32_t size );

  /** \brief Get the queue size.
   */
  uint32_t
  GetMaxQueueSize() const;

  /** \brief Get the current number of elements in the queue.
   */
  uint32_t
  GetCurrQueueSize() const;

  /** \brief Set the wieght of the queue for WFQ.
   *  \param weight The weight of the queue.
   */
  void 
  SetWeight( float weight );

  /** \brief Get the current weight of the queue. 
   */
  float
  GetWeight();

  /** \brief Update the last virtual finish time.
   *  \param lvft The last virtual finish time.
   */
  void 
  SetLastVirtualFinishTime( uint64_t lvft );

  /** \brief Get the last virtual finish time.
   */
  uint64_t
  GetLastVirtualFinishTime();

  /** \brief Push the given packet and corresponding metainfo onto the queue.
   *  \param Item The packet and its metainfo, incoming face, pit entry, etc.
   */
  bool
  Enqueue( QueueItem item );

  /** \brief Dequeue the packet currently at the top of the queue.
   */
  QueueItem
  Dequeue();

  /** \brief Print the contents of the queue.
   */
  void
  DisplayQueue();

  /** \brief Check whether queue is empty.
   */
  bool
  IsEmpty() const;

  /** \brief Get the packet at the top of the queue.
   */
  QueueItem
  GetFirstElement();

public:

  typedef std::list<QueueItem> Queue; ///< @brief Defines basic queue structure.

private:

  uint32_t m_maxQueueSize; ///< @brief Maximum size for the queue.
  float m_weight; ///< @brief Queue weight for use in WFQ.
  uint64_t m_lastVirtualFinishTime; ///< @brief The estimated time needed to dequeue all packets. 
  Queue m_queue; //< @brief Queue object
};

}// namespace fw
}// namespace nfd
 
#endif // QOS_QUEUE_H
