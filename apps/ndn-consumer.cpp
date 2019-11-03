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

#include "ndn-consumer.hpp"
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

#include "utils/ndn-ns3-packet-tag.hpp"
#include "utils/ndn-rtt-mean-deviation.hpp"

#include <ndn-cxx/lp/tags.hpp>

#include <boost/lexical_cast.hpp>
#include <boost/ref.hpp>
#include <boost/random/uniform_int_distribution.hpp>

#include "precache-strategy/send-more-interest.hpp"

NS_LOG_COMPONENT_DEFINE("ndn.Consumer");

namespace ns3 {
namespace ndn {

static bool startNormalSending = false;

NS_OBJECT_ENSURE_REGISTERED(Consumer);

static const int kRttVectorMaxSize = 5;
static const int kRetxVectorMaxSize = 100;

TypeId
Consumer::GetTypeId(void)
{
  static TypeId tid =
    TypeId("ns3::ndn::Consumer")
    .SetGroupName("Ndn")
    .SetParent<App>()
    .AddAttribute("StartSeq", "Initial sequence number", IntegerValue(0),
                  MakeIntegerAccessor(&Consumer::m_seq), MakeIntegerChecker<int32_t>())

    .AddAttribute("Prefix", "Name of the Interest", StringValue("/"),
                  MakeNameAccessor(&Consumer::m_interestName), MakeNameChecker())

    .AddAttribute("LifeTime", "LifeTime for interest packet", StringValue("4s"),
                  MakeTimeAccessor(&Consumer::m_interestLifeTime), MakeTimeChecker())

    .AddAttribute("Step2", "Application support to step2", BooleanValue(false),
                  MakeBooleanAccessor(&Consumer::m_step2), MakeBooleanChecker())

    .AddAttribute("Step3", "Application support to step3", BooleanValue(false),
                  MakeBooleanAccessor(&Consumer::m_step3), MakeBooleanChecker())

    .AddAttribute("HitChance", "Probability to prefetch each pkt", UintegerValue(100),
                  MakeUintegerAccessor(&Consumer::m_chance), MakeUintegerChecker<uint64_t>())

    .AddAttribute("RetxTimer",
                  "Timeout defining how frequent retransmission timeouts should be checked",
                  StringValue("200ms"),
                  MakeTimeAccessor(&Consumer::GetRetxTimer, &Consumer::SetRetxTimer),
                  MakeTimeChecker())

    .AddTraceSource("LastRetransmittedInterestDataDelay",
                    "Delay between last retransmitted Interest and received Data",
                    MakeTraceSourceAccessor(&Consumer::m_lastRetransmittedInterestDataDelay),
                    "ns3::ndn::Consumer::LastRetransmittedInterestDataDelayCallback")

    .AddTraceSource("FirstInterestDataDelay",
                    "Delay between first transmitted Interest and received Data",
                    MakeTraceSourceAccessor(&Consumer::m_firstInterestDataDelay),
                    "ns3::ndn::Consumer::FirstInterestDataDelayCallback");

  return tid;
}

Consumer::Consumer()
  : m_rand(CreateObject<UniformRandomVariable>())
  , m_seq(0)
  , m_seqMax(0) // don't request anything
  , rengine_(rdevice_())
{
  NS_LOG_FUNCTION_NOARGS();

  m_rtt = CreateObject<RttMeanDeviation>();
  avoidSeqStart = avoidSeqEnd = 0;
}

Address
Consumer::GetCurrentAP()
{
  Ptr<ns3::WifiNetDevice> wifiDev = GetNode()->GetDevice(0)->GetObject<ns3::WifiNetDevice>();
  assert(wifiDev != nullptr);
  Ptr<ns3::StaWifiMac> staMac = wifiDev->GetMac()->GetObject<ns3::StaWifiMac>();
  assert(staMac != nullptr);
  Address ap = staMac->GetBssid();
  //std::cout << "App " << m_appId << " on Node " << GetNode()->GetId() << " connected to " << dest << std::endl;
  return ap;
}

void
Consumer::SetRetxTimer(Time retxTimer)
{
  m_retxTimer = retxTimer;
  if (m_retxEvent.IsRunning()) {
    // m_retxEvent.Cancel (); // cancel any scheduled cleanup events
    Simulator::Remove(m_retxEvent); // slower, but better for memory
  }

  // schedule even with new timeout
  m_retxEvent = Simulator::Schedule(m_retxTimer, &Consumer::CheckRetxTimeout, this);
}

Time
Consumer::GetRetxTimer() const
{
  return m_retxTimer;
}

void
Consumer::CheckRetxTimeout()
{
  Time now = Simulator::Now();

  Time rto = m_rtt->RetransmitTimeout();
  // NS_LOG_DEBUG ("Current RTO: " << rto.ToDouble (Time::S) << "s");

  while (!m_seqTimeouts.empty()) {
    SeqTimeoutsContainer::index<i_timestamp>::type::iterator entry =
      m_seqTimeouts.get<i_timestamp>().begin();
    if (entry->time + rto <= now) // timeout expired?
      {
        uint32_t seqNo = entry->seq;
        m_seqTimeouts.get<i_timestamp>().erase(entry);
        OnTimeout(seqNo);
      }
    else
      break; // nothing else to do. All later packets need not be retransmitted
  }

  m_retxEvent = Simulator::Schedule(m_retxTimer, &Consumer::CheckRetxTimeout, this);
}

// Application Methods
void
Consumer::StartApplication() // Called at time specified by Start
{
  NS_LOG_FUNCTION_NOARGS();

  // do base stuff
  App::StartApplication();

  ScheduleNextPacket();
}

void
Consumer::StopApplication() // Called at time specified by Stop
{
  NS_LOG_FUNCTION_NOARGS();

  // cancel periodic packet generation
  Simulator::Cancel(m_sendEvent);

  // cleanup base stuff
  App::StopApplication();
}

void
Consumer::SendGeneralInterestToFace257(uint32_t seq)
{
  shared_ptr<Name> nameWithSequence = make_shared<Name>(m_interestName);
  nameWithSequence->append(std::to_string(seq));
  nameWithSequence->appendSequenceNumber(seq);

  shared_ptr<Interest> interest = make_shared<Interest>();
  interest->setNonce(m_rand->GetValue(0, std::numeric_limits<uint32_t>::max()));
  interest->setName(*nameWithSequence);
  interest->setTag<lp::NextHopFaceIdTag>(make_shared<lp::NextHopFaceIdTag>(257));
  time::milliseconds interestLifeTime(m_interestLifeTime.GetMilliSeconds());
  interest->setInterestLifetime(interestLifeTime);

  WillSendOutInterest(seq);

  m_transmittedInterests(interest, this, m_face);
  m_appLink->onReceiveInterest(*interest);
}

void
Consumer::SendGeneralInterest(uint32_t seq)
{
  shared_ptr<Name> nameWithSequence = make_shared<Name>(m_interestName);
  nameWithSequence->append(std::to_string(seq));
  nameWithSequence->appendSequenceNumber(seq);

  shared_ptr<Interest> interest = make_shared<Interest>();
  interest->setNonce(m_rand->GetValue(0, std::numeric_limits<uint32_t>::max()));
  interest->setName(*nameWithSequence);
  time::milliseconds interestLifeTime(m_interestLifeTime.GetMilliSeconds());
  interest->setInterestLifetime(interestLifeTime);

  WillSendOutInterest(seq);

  m_transmittedInterests(interest, this, m_face);
  m_appLink->onReceiveInterest(*interest);
}

void
Consumer::SendPrefetchInterest(uint32_t seq)
{
  shared_ptr<Name> nameWithSequence = make_shared<Name>(m_interestName);
  nameWithSequence->append(std::to_string(seq));
  nameWithSequence->appendSequenceNumber(seq);

  shared_ptr<Interest> interest = make_shared<Interest>();
  interest->setNonce(m_rand->GetValue(0, std::numeric_limits<uint32_t>::max()));
  interest->setName(*nameWithSequence);
  time::milliseconds interestLifeTime(4000);
  interest->setInterestLifetime(interestLifeTime);

  m_transmittedInterests(interest, this, m_face);
  m_appLink->onReceiveInterest(*interest);
}

void
Consumer::SendBundledInterest(int seq1, int seq2) {
  shared_ptr<Name> nameWithSequence = make_shared<Name>(Name("/prefetch"));
  nameWithSequence->append(m_interestName);
  nameWithSequence->appendNumber(seq1);
  nameWithSequence->appendNumber(seq2);
  Address ap = GetCurrentAP();
  std::ostringstream os;
  os << ap;
  nameWithSequence->append(os.str().c_str());

  shared_ptr<Interest> interest = make_shared<Interest>();
  interest->setNonce(m_rand->GetValue(0, std::numeric_limits<uint32_t>::max()));
  interest->setName(*nameWithSequence);
  time::milliseconds interestLifeTime(m_interestLifeTime.GetMilliSeconds());
  interest->setInterestLifetime(interestLifeTime);

  m_transmittedInterests(interest, this, m_face);
  m_appLink->onReceiveInterest(*interest);
}

void
Consumer::SendPacket(int frequency)
{
  if (!m_active)
    return;

  NS_LOG_FUNCTION_NOARGS();

  if (m_step2) {
    std::vector<uint32_t> pre_fetch_seq;
    bool dumpRtxQueue = false;
    bool hasCoverage = false;
    std::tie(pre_fetch_seq, dumpRtxQueue, hasCoverage) = ns3::moreInterestsToSend(m_seq, traffic_info, frequency);
    if (pre_fetch_seq.size() > 0) {
      SendBundledInterest(m_seq + 1, m_seq + 1 + 70);
    }
  }

  if (m_seq == 0) {
    uint32_t seq = std::numeric_limits<uint32_t>::max(); // invalid
    if (seq == std::numeric_limits<uint32_t>::max()) {
      if (m_seqMax != std::numeric_limits<uint32_t>::max()) {
        if (m_seq >= m_seqMax) {
          return; // we are totally done
        }
      }
      seq = m_seq++;
    }
    if (m_step3) {
      std::vector<uint32_t> pre_fetch_seq;
      bool dumpRtxQueue = false;
      bool hasCoverage = false;
      std::tie(pre_fetch_seq, dumpRtxQueue, hasCoverage) = ns3::moreInterestsToSend(m_seq, traffic_info, 20);
      if (!hasCoverage) {
        SendGeneralInterestToFace257(seq);
        NS_LOG_INFO("> Interest for " << seq << " Through Ad Hoc Face");
      }
      else {
        SendGeneralInterest(seq);
        NS_LOG_INFO("> Interest for " << seq);
      }
    }
    else {
      SendGeneralInterest(seq);
      NS_LOG_INFO("> Interest for " << seq);
    }
  }

  ScheduleNextPacket();

  ///////////////////////////////////////////
  //          Start of Algorithm           //
  ///////////////////////////////////////////
  // bool hasCoverage = true;
  // if (m_step2 == true) {
  //   // prefetch by one-hop V2V communiaction
  //   // log the current real rtt vector
  //   std::string rtt_str = "[";
  //   for (auto entry: traffic_info.real_rtt) {
  //     rtt_str += std::to_string(entry) + ",";
  //   }
  //   rtt_str += "]";
  //   NS_LOG_INFO ("Current Real RTT: " << rtt_str );

  //   std::vector<uint32_t> pre_fetch_seq;
  //   bool dumpRtxQueue = false;
  //   std::tie(pre_fetch_seq, dumpRtxQueue, hasCoverage) = ns3::moreInterestsToSend(m_seq, traffic_info, frequency);
  //   NS_LOG_INFO ("ALGO RESULT: " << pre_fetch_seq.size() << " " << dumpRtxQueue << " "  << hasCoverage);
  //   if (pre_fetch_seq.size() > 0) {
  //     NS_LOG_INFO ("CURRENT FREQUENCY: " << frequency);
  //     avoidSeqStart = pre_fetch_seq.front();
  //     avoidSeqEnd = pre_fetch_seq.back();
  //     NS_LOG_INFO ("SET AVOIDSEQ START: " << avoidSeqStart);
  //     NS_LOG_INFO ("SET AVOIDSEQ END: " << avoidSeqEnd);

  //     // /prefetch/prefix/seq1/seq2/ap
  //     SendBundledInterest(pre_fetch_seq.front(), pre_fetch_seq.back());
  //     NS_LOG_INFO("> Pre-Fetch Interest by One-hop V2V Communication for "
  //                 << pre_fetch_seq.front() << " to " << pre_fetch_seq.back());
  //     startNormalSending = false;
  //   }
  //   if (dumpRtxQueue && avoidSeqStart != avoidSeqEnd) {
  //     NS_LOG_INFO("DumpRtxQueue is true");
  //     if (avoidSeqEnd >= m_seq) {
  //       m_seq = avoidSeqEnd + 1;
  //       seq = m_seq + 1;
  //     }
  //     startNormalSending = true;
  //     for (int j = 0; j < 1; j++) {
  //       SendGeneralInterest(avoidSeqStart + j);
  //       NS_LOG_INFO("> Recovery Interest for " << avoidSeqStart + j);
  //     }
  //   }
  // }
  // if (m_step3 == true) {
  //   std::string rtt_str = "[";
  //   for (auto entry: traffic_info.real_rtt) {
  //     rtt_str += std::to_string(entry) + ",";
  //   }
  //   rtt_str += "]";
  //   NS_LOG_INFO ("Current Real RTT: " << rtt_str );

  //   std::vector<uint32_t> pre_fetch_seq;
  //   bool dumpRtxQueue = false;
  //   std::tie(pre_fetch_seq, dumpRtxQueue, hasCoverage) = ns3::moreInterestsToSend(m_seq, traffic_info, frequency);
  // }

  // ///////////////////////////////////////////
  // //          End of Algorithm             //
  // ///////////////////////////////////////////

  // if (m_step3 == true) {
  //   if (!hasCoverage) {
  //     if (rand() % 100 < 77) {
  //       SendGeneralInterestToFace257(seq);
  //       NS_LOG_INFO("> Interest for " << seq << " Through Ad Hoc Face");
  //     }
  //     else {
  //       NS_LOG_INFO("> Interest for " << seq << "Through Ad Hoc Face suppressed by chance");
  //     }
  //   }
  //   else {
  //     SendGeneralInterest(seq);
  //     NS_LOG_INFO("> Interest for " << seq);
  //   }
  // }
  // else { // basic version
  //   if (seq <= avoidSeqEnd && seq >= avoidSeqStart && avoidSeqStart != 0) {
  //     // don't send it out because it's already sent
  //   }
  //   else {
  //     if (hasCoverage && startNormalSending) {
  //       SendGeneralInterest(seq);
  //       NS_LOG_INFO("> Interest for " << seq);
  //     }
  //   }
  // }
}

///////////////////////////////////////////////////
//          Process incoming packets             //
///////////////////////////////////////////////////

void
Consumer::OnData(shared_ptr<const Data> data)
{
  if (!m_active)
    return;

  App::OnData(data); // tracing inside

  uint32_t seq = data->getName().at(-1).toSequenceNumber();

  NS_LOG_INFO("< DATA for " << seq);

  int hopCount = 0;
  auto hopCountTag = data->getTag<lp::HopCountTag>();
  if (hopCountTag != nullptr) { // e.g., packet came from local node's cache
    hopCount = *hopCountTag;
  }
  NS_LOG_DEBUG("Hop count: " << hopCount);

  SeqTimeoutsContainer::iterator entry = m_seqLastDelay.find(seq);

  // calculate the current real rtt
  assert(entry != m_seqLastDelay.end());
  int64_t cur_real_rtt = (Simulator::Now() - entry->time).GetNanoSeconds();

  if (entry != m_seqLastDelay.end()) {
    m_lastRetransmittedInterestDataDelay(this, seq, Simulator::Now() - entry->time, hopCount);
  }

  entry = m_seqFullDelay.find(seq);
  if (entry != m_seqFullDelay.end()) {
    m_firstInterestDataDelay(this, seq, Simulator::Now() - entry->time, m_seqRetxCounts[seq], hopCount);
  }

  m_seqRetxCounts.erase(seq);
  m_seqFullDelay.erase(seq);
  m_seqLastDelay.erase(seq);

  m_seqTimeouts.erase(seq);
  m_retxSeqs.erase(seq);

  m_rtt->AckSeq(SequenceNumber32(seq));

  // calculate the current estimated rtt
  int64_t cur_est_rtt = m_rtt->GetCurrentEstimate().GetNanoSeconds();
  // add the current traffic info
  if (traffic_info.real_rtt.size() == kRttVectorMaxSize) {
    traffic_info.real_rtt.pop_front();
    traffic_info.est_rtt.pop_front();
  }
  traffic_info.real_rtt.push_back(cur_real_rtt);
  traffic_info.est_rtt.push_back(cur_est_rtt);

  // send next interest
  seq = std::numeric_limits<uint32_t>::max(); // invalid
  if (seq == std::numeric_limits<uint32_t>::max()) {
    if (m_seqMax != std::numeric_limits<uint32_t>::max()) {
      if (m_seq >= m_seqMax) {
        return; // we are totally done
      }
    }
    seq = m_seq++;
  }
  if (m_step3) {
    std::vector<uint32_t> pre_fetch_seq;
    bool dumpRtxQueue = false;
    bool hasCoverage = false;
    std::tie(pre_fetch_seq, dumpRtxQueue, hasCoverage) = ns3::moreInterestsToSend(m_seq, traffic_info, 20);
    if (!hasCoverage) {
      if (rand() % 100 < m_chance) {
        SendGeneralInterestToFace257(seq);
        NS_LOG_INFO("> Interest for " << seq << " Through Ad Hoc Face");
      }
      else {
        // let it timeout
        SendGeneralInterest(seq);
      }
    }
    else {
      SendGeneralInterest(seq);
      NS_LOG_INFO("> Interest for " << seq);
    }
  }
  else {
    SendGeneralInterest(seq);
    NS_LOG_INFO("> Interest for " << seq);
  }
}

void
Consumer::OnNack(shared_ptr<const lp::Nack> nack)
{
  /// tracing inside
  App::OnNack(nack);

  NS_LOG_INFO("NACK received for: " << nack->getInterest().getName()
              << ", reason: " << nack->getReason());
}

void
Consumer::OnTimeout(uint32_t sequenceNumber)
{
  NS_LOG_FUNCTION(sequenceNumber);
  m_rtt->SentSeq(SequenceNumber32(sequenceNumber), 1); // make sure to disable RTT calculation for this sample
  m_retxSeqs.insert(sequenceNumber);

  if (m_step3) {
    std::vector<uint32_t> pre_fetch_seq;
    bool dumpRtxQueue = false;
    bool hasCoverage = false;
    std::tie(pre_fetch_seq, dumpRtxQueue, hasCoverage) = ns3::moreInterestsToSend(m_seq, traffic_info, 20);
    if (!hasCoverage) {
      SendGeneralInterestToFace257(sequenceNumber);
      NS_LOG_INFO("> Interest for " << sequenceNumber << " Through Ad Hoc Face");
    }
    else {
      SendGeneralInterest(sequenceNumber);
      NS_LOG_INFO("Retransmission");
      NS_LOG_INFO("> Interest for " << sequenceNumber);
    }
  }
  else {
    SendGeneralInterest(sequenceNumber);
    NS_LOG_INFO("Retransmission");
    NS_LOG_INFO("> Interest for " << sequenceNumber);
  }
  // if (sequenceNumber == avoidSeqStart) {
  //   avoidSeqStart++;
  //   SendGeneralInterest(avoidSeqStart);
  //   NS_LOG_INFO("> Recovery Interest for " << avoidSeqStart);
  // }
  // if (avoidSeqStart == avoidSeqEnd && avoidSeqStart != 0) {
  //   NS_LOG_INFO("Now avoidSeqStart == avoidSeqEnd");
  //   avoidSeqStart = avoidSeqEnd = 0;
  // }

  // record the retx info
  if (traffic_info.retx_time.size() == kRetxVectorMaxSize) {
    traffic_info.retx_time.pop_front();
  }
  traffic_info.retx_time.push_back(Simulator::Now().GetNanoSeconds());
}

void
Consumer::WillSendOutInterest(uint32_t sequenceNumber)
{
  NS_LOG_DEBUG("Trying to add " << sequenceNumber << " with " << Simulator::Now() << ". already "
               << m_seqTimeouts.size() << " items");

  m_seqTimeouts.insert(SeqTimeout(sequenceNumber, Simulator::Now()));
  m_seqFullDelay.insert(SeqTimeout(sequenceNumber, Simulator::Now()));

  m_seqLastDelay.erase(sequenceNumber);
  m_seqLastDelay.insert(SeqTimeout(sequenceNumber, Simulator::Now()));

  m_seqRetxCounts[sequenceNumber]++;

  m_rtt->SentSeq(SequenceNumber32(sequenceNumber), 1);

  // set up the retx timer for current interest
  // Simulator::Schedule(kRetxTimer, &Consumer::OnTimeout, this, sequenceNumber);
}

bool
Consumer::GetSeqFromCache(uint32_t seq)
{
  if (data_cache.find(seq) == data_cache.end()) {
    return false;
  }
  data_cache.erase(seq);
  return true;
}

} // namespace ndn
} // namespace ns3
