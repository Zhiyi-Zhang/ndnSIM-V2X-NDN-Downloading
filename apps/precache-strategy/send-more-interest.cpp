/* -*- Mode:C++; c-file-style:"gnu"; indent-tabs-mode:nil; -*- */
/**
 * Authors:  Zhiyi Zhang: UCLA
 *           Your name: your affiliation
 *           Xin Xu: UCLA
 *
 **/

#include "send-more-interest.hpp"
#include <iostream>

namespace ns3 {

const std::vector<int64_t> handoff_tp = {3166666666*2/*3.166s*2, 190m, AP1*/,
                                         6500000000*2/*6.5s*2, 390m, AP2*/,
                                         9833333333*2/*9.833s*2, 590m, AP3*/,
                                         13166666666*2/*9.833s*2, 790m, AP4*/,
                                         16500000000*2/*16.5s*2, 990m, AP5*/,
                                         19833333333*2/*19.833s*2, 1190m, AP6*/};

// the time interval between two APs when there is no wifi connection
const int64_t wifilessInterval = 333333333*2; /*0.333s*2*/

/**
 * @brief Use line regression to capture the trend of RTT change, thus knowing whether a station
 * is getting close to or far from the Access Point or Base Station
 *
 * @param The vector length does not need to be a fixed value
 * @return The seq(s) of packet to be sent
 */
std::vector<uint32_t>

moreInterestsToSend(uint32_t seqJustSent, ns3::ndn::Consumer::TrafficInfo trafficInfo, int& apCounter)
{
  if (trafficInfo.real_rtt.size() < 2)
    return std::vector<uint32_t>(0);

  // ordinary least squares of line regression
  double sumX = 0;
  double sumY = 0;
  double sumXY = 0;
  double sumXX = 0;

  int index = 1;
  auto rttVec = trafficInfo.real_rtt;
  auto it = rttVec.begin();
  while (it != rttVec.end()) {
    sumX += index;
    sumY += *it;;
    sumXY += index * (*it);
    sumXX += index * index;
    it++;
    index++;
  }

  double aveX = sumX / rttVec.size();
  double aveY = sumY / rttVec.size();
  double rate = sumXY - rttVec.size() * aveX * aveY;
  rate = rate / (sumXX - rttVec.size() * aveX * aveX);

  // new algorithm under new assumption
  uint64_t currentTp = ns3::Simulator::Now().GetNanoSeconds();
  double threshold = aveY;
  for (int i = 0; i < handoff_tp.size(); i++) {
    if (handoff_tp[i] > currentTp && handoff_tp[i] - currentTp <= threshold) {
      if (apCounter >= i + 1) {
        // already prefetch for this AP
        break;
      }

      std::vector<uint32_t> result;
      int insideNumber = static_cast<int>(threshold / 100000000);
      int outsideNumber = static_cast<int>(wifilessInterval / 100000000);
      for (int j = 0; j < outsideNumber + 1; j++) {
        result.push_back(seqJustSent + insideNumber + j);
      }
      apCounter++;
      return result;
    }
  }
  return std::vector<uint32_t>(0);

  // // if the station is getting far from Access Point/Base Station
  // // pre-send an Interest with seq = seqJustSent + 5
  // std::vector<uint32_t> result;
  // if (rate > 0) {
  //   result.push_back(seqJustSent + 5);
  // }
  // return result;
}

std::vector<uint32_t>
oneHopV2VPrefetch(uint32_t seqJustSent, ns3::ndn::Consumer::TrafficInfo trafficInfo)
{
  std::vector<uint32_t> ret;
  ret.push_back(seqJustSent + 10);
  // return std::vector<uint32_t>(0);
  return ret;
}

std::vector<uint32_t>
MultiHopV2VPrefetch(uint32_t seqJustSent, ns3::ndn::Consumer::TrafficInfo trafficInfo)
{
  return std::vector<uint32_t>(0);
}

} // namespace ns3
