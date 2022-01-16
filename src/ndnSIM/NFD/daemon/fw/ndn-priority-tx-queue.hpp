/* -*- Mode: C++; c-file-style: "gnu"; indent-tabs-mode:nil -*- */
/*
 * Copyright (  C  ) 2020 New Mexico State University- Board of Regents 
 *
 * George Torres, Anju Kunnumpurathu James
 * See AUTHORS.md for complete list of authors and contributors.
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (  at your option  ) any later version.

 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.

 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 *
 */

#ifndef NDN_PRIORITY_TX_QUEUE_H
#define NDN_PRIORITY_TX_QUEUE_H

#include <list>
#include <limits>
#include "ns3/log.h"
#include "ndn-qos-queue.hpp"

#include <ctime> 
#include <chrono>

#define PACKET_SIZE 1024
#define TOTAL_QUEUES 3

using namespace std;

namespace nfd {
namespace fw {


/**
 * @ingroup ndnQoS
 * \brief Class defines priority queue.
 *
 * Consists of three QoS queues each with different priority levels. 
 * Makes use of Weighted Fair Queuing algrothrim to process queues. 
 */

class NdnPriorityTxQueue : private QosQueue
{

public:

  //constructor
  NdnPriorityTxQueue();

  /** \brief Find flow rate of the given queue.
   *  \param queue The queue form which we will obtain the flow rate.
   */
  float
  GetFlowRate( QosQueue *queue );

  /** \brief Update the time of the given packet within the queue.
   *  \param packet The packet we are updating.
   *  \param queue The queue which has the packet we want to update.
   */
  void
  UpdateTime( ndn::Block packet, QosQueue *queue );

  /** \brief Use WFQ algorithm to select the next queue to send from taking the token count into account.
   *  \param highTokens The number of tokens the high priorty queue has.
   *  \param midTokens The number of tokens the mid priorty queue has.
   *  \param lowTokens The number of tokens the low priorty queue has.
   */
  int
  SelectQueueToSend( double highTokens, double midTokens, double lowTokens );

  /** \brief Push the given packet and corresponding meta info onto a queue.
   *  \param item The packet and its metainfo, incoming face, pit entry, etc.
   *  \param pr_level The value which determins packet priorty.
   */
  bool
  DoEnqueue( QueueItem item, uint32_t pr_level );

  /** \brief Dequeue a packet from the indicated queue.
   *  \param choice An int value repersenting one of the three queues.
   */
  QueueItem
  DoDequeue( int choice );

  /** \brief Check if all queues are empty.
   */
  bool
  IsEmpty();

  /** \brief Tokens required for high priority queue. 
   */
  int
  tokenReqHig();

  /** \brief Tokens required for medium priority queue.
   */
  int
  tokenReqMid();

  /** \brief Tokens required for low priority queue.
   */  
  int
  tokenReqLow();

  /** \brief Get the queue size. A function to do this was also in the parent class, but it was private, and I needed to call it from an instance of this class.
   */
  uint32_t
  GetMaxQueueSize2( int choice );

  /** \brief Get the current number of entries in the queue.
   */
  uint32_t
  GetCurrQueueSize2( int choice );

public:

  QosQueue m_highPriorityQueue; //< @brief High priority Queue. 
  QosQueue m_mediumPriorityQueue; //< @brief Medium priority Queue.
  QosQueue m_lowPriorityQueue;//< @brief Low priority Queue.
};

}// namespace fw
}// namespace nfd
 
#endif // NDN_PRIORITY_TX_QUEUE_H
