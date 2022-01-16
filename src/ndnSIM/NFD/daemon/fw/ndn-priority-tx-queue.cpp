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

#include "ndn-priority-tx-queue.hpp"

NS_LOG_COMPONENT_DEFINE( "ndn.NdnPriorityTxQueue" );

namespace nfd {
namespace fw {

NdnPriorityTxQueue::NdnPriorityTxQueue()
{
  m_highPriorityQueue.SetWeight( 3 );
  m_mediumPriorityQueue.SetWeight( 2 );
  m_lowPriorityQueue.SetWeight( 1 );
}

float 
NdnPriorityTxQueue::GetFlowRate( QosQueue *queue )
{
  float total_weight = m_highPriorityQueue.GetWeight() + m_lowPriorityQueue.GetWeight() + m_mediumPriorityQueue.GetWeight();
  float flowRate = queue->GetWeight() / total_weight;

  return flowRate;
}

void
NdnPriorityTxQueue::UpdateTime( ndn::Block packet, QosQueue *queue )
{
  uint64_t current_time = ns3::Simulator::Now().GetMilliSeconds();
  uint64_t virtualStartTime = std::max( current_time, queue->GetLastVirtualFinishTime() );
  uint64_t virtualFinishTime = virtualStartTime + ( packet.size() * ( 1 - GetFlowRate( queue ) ) );

  queue->SetLastVirtualFinishTime( virtualFinishTime );
}

int
NdnPriorityTxQueue::SelectQueueToSend( double highTokens, double midTokens, double lowTokens )
{
  int squeue = -1;
  QosQueue* selected_queue = &m_highPriorityQueue;
  float minVirtualFinishTime = std::numeric_limits<float>::max();

  if( m_highPriorityQueue.IsEmpty() == false && highTokens > 1 ) {
    minVirtualFinishTime = m_highPriorityQueue.GetLastVirtualFinishTime();
    selected_queue = &m_highPriorityQueue;
    squeue = 0;
  }

  if( m_mediumPriorityQueue.IsEmpty() == false && midTokens > 1 ) {

    if( minVirtualFinishTime > m_mediumPriorityQueue.GetLastVirtualFinishTime() ) {
      minVirtualFinishTime = m_mediumPriorityQueue.GetLastVirtualFinishTime();
      selected_queue = &m_mediumPriorityQueue;
      squeue = 1;

    }
  }

  if( m_lowPriorityQueue.IsEmpty() == false && lowTokens > 1 ) {

    if( minVirtualFinishTime > m_lowPriorityQueue.GetLastVirtualFinishTime() ) {
      minVirtualFinishTime = m_lowPriorityQueue.GetLastVirtualFinishTime();
      selected_queue = &m_lowPriorityQueue;
      squeue = 2;
    }
  }

  return squeue;
}

bool
NdnPriorityTxQueue::DoEnqueue( QueueItem item, uint32_t pr_level )
{
  QosQueue *queue;

  if( pr_level >= 1 && pr_level <= 20 ) {
    queue = &m_highPriorityQueue;
  } else if( pr_level >= 21 && pr_level <= 40 ) {
    queue = &m_mediumPriorityQueue;
  }else if( pr_level >= 41 && pr_level <= 64 ) {
    queue = &m_lowPriorityQueue;
  } else {
    return false;
  }

  if( queue->Enqueue( item ) ) {
    UpdateTime( item.wireEncode, queue );
    return true;
  } else {
    return false;
  }
}

QueueItem
NdnPriorityTxQueue::DoDequeue( int choice )
{
  QueueItem item;

  if( ( !m_highPriorityQueue.IsEmpty() || ( !m_mediumPriorityQueue.IsEmpty() ) || ( !m_lowPriorityQueue.IsEmpty() ) ) ) {

    if( choice == 0 ) {
      item = m_highPriorityQueue.Dequeue();
    } else if( choice == 1 ) {
      item = m_mediumPriorityQueue.Dequeue();
    } else { item = m_lowPriorityQueue.Dequeue();
    }
  } else {
    //std::cout << "DoDequeue failed. All queues are empty !!!!" << std::endl;
  }

  return item;
}

bool
NdnPriorityTxQueue::IsEmpty()
{
  return ( m_highPriorityQueue.IsEmpty() && m_mediumPriorityQueue.IsEmpty() && m_lowPriorityQueue.IsEmpty() );
}

int
NdnPriorityTxQueue::tokenReqHig()
{
  if( m_highPriorityQueue.IsEmpty() == false ) {
    return 1;
  }

  return 0;
}

int
NdnPriorityTxQueue::tokenReqMid()
{
  if( m_mediumPriorityQueue.IsEmpty() == false ) {
    return 1;
  }

  return 0;
}

int
NdnPriorityTxQueue::tokenReqLow()
{
  if( m_lowPriorityQueue.IsEmpty() == false ) {
    return 1;
  }

  return 0;
}

uint32_t
NdnPriorityTxQueue::GetMaxQueueSize2( int choice )
{
  switch (choice) {
  case 0:
    return m_highPriorityQueue.GetMaxQueueSize();
  case 1:
    return m_mediumPriorityQueue.GetMaxQueueSize();
  case 2:
    return m_lowPriorityQueue.GetMaxQueueSize();
  }
}

uint32_t
NdnPriorityTxQueue::GetCurrQueueSize2( int choice )
{
  switch (choice) {
  case 0:
    return m_highPriorityQueue.GetCurrQueueSize();
  case 1:
    return m_mediumPriorityQueue.GetCurrQueueSize();
  case 2:
    return m_lowPriorityQueue.GetCurrQueueSize();
  }
}

} // namespace ndn
} // namespace ns3

