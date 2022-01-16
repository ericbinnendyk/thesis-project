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

#include "ndn-qos-queue.hpp"

NS_LOG_COMPONENT_DEFINE( "ndn.QosQueue" );

namespace nfd {
namespace fw {

QosQueue::QosQueue()
  : m_maxQueueSize( QUEUE_SIZE ),
  m_weight( 0.0 ),
  m_lastVirtualFinishTime( 0.0 )
{
}

void
QosQueue::SetMaxQueueSize( uint32_t size )
{
  m_maxQueueSize = size;
}

uint32_t
QosQueue::GetMaxQueueSize() const
{
  return m_maxQueueSize;
}

uint32_t
QosQueue::GetCurrQueueSize() const
{
  return m_queue.size();
}

void
QosQueue::SetWeight( float weight )
{
  m_weight = weight;
}

float
QosQueue::GetWeight()
{
  return m_weight;
}

void
QosQueue::SetLastVirtualFinishTime( uint64_t lvft )
{
  m_lastVirtualFinishTime = lvft;
}

uint64_t
QosQueue::GetLastVirtualFinishTime()
{
  return m_lastVirtualFinishTime;
}

bool
QosQueue::Enqueue( QueueItem item )
{
  if( m_queue.size() < GetMaxQueueSize() ) {
    //std::cout << "Enqueing Item: "<< endl;
    //std::cout << "\tWireWncode: " << item.wireEncode;
    //std::cout << "\tpacketType: " << item.packetType;
    //std::cout << "\tpitEntry: " << item.pitEntry;
    //std::cout << "\tinterface: " << item.interface->getId()  << endl;

    m_queue.push_back( item );

  } else {
    return false;
    //std::cout << "Enqueue failed. Queue full!!!!" << endl;
  }

  return true;
}

QueueItem
QosQueue::Dequeue()
{
  struct QueueItem m_item;

  if( !m_queue.empty() ) {

    m_item = m_queue.front();
    m_queue.pop_front();

  } else {
    //cout << "Dequeue failed. Queue is empty!!!" << endl;
  }

  return m_item;
}

void 
QosQueue::DisplayQueue()
{
  std::cout << "QosQueue::DisplayQueue()" << std::endl;

  if( !m_queue.empty() ) {

    std::cout <<"Queue Items: "<< std::endl;

    for( auto it = m_queue.cbegin(); it != m_queue.cend(); ++it ) {
      std::cout << "\tpacketType: " << it->packetType;
      std::cout << "\tinterface: " << it->inface->getId() << std::endl;
    }

  } else {
    std::cout << "Queue is empty!!!" << std::endl;
  }
}

bool
QosQueue::IsEmpty() const
{
  return m_queue.empty();
}

QueueItem
QosQueue::GetFirstElement()
{
  struct QueueItem m_item;

  if( !m_queue.empty() ) {
    m_item = m_queue.front();
  } else {
    //std::cout << "Queue is Empty !!!!" << endl;
  }

  return m_item;
}


} // namespace ndn
} // namespace ns3

