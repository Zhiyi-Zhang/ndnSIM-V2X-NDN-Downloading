/* -*- Mode:C++; c-file-style:"gnu"; indent-tabs-mode:nil; -*- */
/**
 * Copyright (c) 2011-2015  Regents of the University of California.
 *
 * This file is part of ndnSIM. See AUTHORS for complete list of ndnSIM authors and
 * contributors.
 *
 * ndnSIM is free software: you can redistribute it and/or modify it under the terms
 * of the GNU General Public License as published by the Free Software Foundation,
 * either version 3 of the License, or (at your option) any later version.
 *
 * ndnSIM is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY;
 * without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR
 * PURPOSE.  See the GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License along with
 * ndnSIM, e.g., in COPYING.md file.  If not, see <http://www.gnu.org/licenses/>.
 **/

#include "ndn-consumer-cbr.hpp"
#include "ns3/ptr.h"
#include "ns3/log.h"
#include "ns3/simulator.h"
#include "ns3/packet.h"
#include "ns3/callback.h"
#include "ns3/string.h"
#include "ns3/boolean.h"
#include "ns3/uinteger.h"
#include "ns3/integer.h"
#include "ns3/double.h"

#include "ns3/wifi-net-device.h"
#include "ns3/sta-wifi-mac.h"

NS_LOG_COMPONENT_DEFINE("ndn.ConsumerCbr");

namespace ns3 {
namespace ndn {

NS_OBJECT_ENSURE_REGISTERED(ConsumerCbr);
static const double kDisplayRate = 1.0;

TypeId
ConsumerCbr::GetTypeId(void)
{
  static TypeId tid =
    TypeId("ns3::ndn::ConsumerCbr")
      .SetGroupName("Ndn")
      .SetParent<Consumer>()
      .AddConstructor<ConsumerCbr>()

      .AddAttribute("Frequency", "Frequency of interest packets", StringValue("1.0"),
                    MakeDoubleAccessor(&ConsumerCbr::m_frequency), MakeDoubleChecker<double>())

      .AddAttribute("Randomize",
                    "Type of send time randomization: none (default), uniform, exponential",
                    StringValue("none"),
                    MakeStringAccessor(&ConsumerCbr::SetRandomize, &ConsumerCbr::GetRandomize),
                    MakeStringChecker())

      .AddAttribute("MaxSeq", "Maximum sequence number to request",
                    IntegerValue(std::numeric_limits<uint32_t>::max()),
                    MakeIntegerAccessor(&ConsumerCbr::m_seqMax), MakeIntegerChecker<uint32_t>())

    ;

  return tid;
}

ConsumerCbr::ConsumerCbr()
  : m_frequency(1.0)
  , m_firstTime(true)
{
  NS_LOG_FUNCTION_NOARGS();
  m_seqMax = std::numeric_limits<uint32_t>::max();
  exp_seq = 0;
}

ConsumerCbr::~ConsumerCbr()
{
}

void
ConsumerCbr::ScheduleNextPacket()
{
  // double mean = 8.0 * m_payloadSize / m_desiredRate.GetBitRate ();
  // std::cout << "next: " << Simulator::Now().ToDouble(Time::S) + mean << "s\n";

  if (m_firstTime) {
    m_sendEvent = Simulator::Schedule(Seconds(0.0), &ConsumerCbr::sendPackett, this);
    m_firstTime = false;
    // start timer for dispalying video data
    Simulator::Schedule(Seconds(1.0 / kDisplayRate), &ConsumerCbr::DisplayData, this);
  }
  else if (!m_sendEvent.IsRunning())
    m_sendEvent = Simulator::Schedule((m_random == 0) ? Seconds(1.0 / m_frequency)
                                                      : Seconds(m_random->GetValue()),
                                      &ConsumerCbr::sendPackett, this);
}

void
ConsumerCbr::sendPackett()
{
  SendPacket(m_frequency);
}

void
ConsumerCbr::DisplayData()
{
  bool is_fetch = Consumer::GetSeqFromCache(exp_seq);
  if (is_fetch == true) {
    NS_LOG_INFO("Displaying Seq: " << exp_seq);
    exp_seq++;
  }
  else {
    NS_LOG_INFO("Get Stuck for Seq: " << exp_seq);
  }
  Simulator::Schedule(Seconds(1.0 / kDisplayRate), &ConsumerCbr::DisplayData, this);
}

void
ConsumerCbr::SetRandomize(const std::string& value)
{
  if (value == "uniform") {
    m_random = CreateObject<UniformRandomVariable>();
    m_random->SetAttribute("Min", DoubleValue(0.0));
    m_random->SetAttribute("Max", DoubleValue(2 * 1.0 / m_frequency));
  }
  else if (value == "exponential") {
    m_random = CreateObject<ExponentialRandomVariable>();
    m_random->SetAttribute("Mean", DoubleValue(1.0 / m_frequency));
    m_random->SetAttribute("Bound", DoubleValue(50 * 1.0 / m_frequency));
  }
  else
    m_random = 0;

  m_randomType = value;
}

std::string
ConsumerCbr::GetRandomize() const
{
  return m_randomType;
}

void
ConsumerCbr::ConnectedToNewAp(int deviceId)
{
  Ptr<ns3::WifiNetDevice> wifiDev = GetNode()->GetDevice(deviceId)->GetObject<ns3::WifiNetDevice>();
  if (wifiDev != nullptr) {
    Ptr<ns3::StaWifiMac> staMac = wifiDev->GetMac()->GetObject<ns3::StaWifiMac>();
    if (staMac != nullptr) {
      // Address dest = staMac->GetBssid();
      Ssid ssid = staMac->GetSsid();
      NS_LOG_INFO("App " << m_appId << " on Node " << GetNode()->GetId() << " connected to " << ssid);
    }
  }
}

} // namespace ndn
} // namespace ns3
