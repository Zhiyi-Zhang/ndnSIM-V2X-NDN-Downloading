/* -*- Mode:C++; c-file-style:"gnu"; indent-tabs-mode:nil; -*- */
/**
 * Authors:  Zhiyi Zhang: UCLA
 *           Xin Xu: UCLA
 *           Your name: your affiliation
 *
 **/

#include "send-more-interest.hpp"
#include <iostream>

namespace ns3 {

const std::vector<int64_t> handoff_tp = {8 * 1000000000 /*160m, AP1*/,
                                         18 * 1000000000 /*360m, AP2*/,
                                         28 * 1000000000 /*9.833s*2, 590m, AP3*/,
                                         38 * 1000000000 /*9.833s*2, 790m, AP4*/,
                                         48 * 1000000000 /*16.5s*2, 990m, AP5*/,
                                         58 * 1000000000 /*19.833s*2, 1190m, AP6*/};

const std::vector<int64_t> entering_tp = {2 * 1000000000 /*40m, AP1*/,
                                          12 * 1000000000 /*240m, AP2*/,
                                          22 * 1000000000 /*440m, AP3*/,
                                          32 * 1000000000 /*640m, AP4*/,
                                          42 * 1000000000 /*840m, AP5*/,
                                          52 * 1000000000/*1040m, AP6*/};

// the time interval between two APs when there is no wifi connection
const int64_t wifiLessInterval = 4 * 1000000000;


const int64_t thresholdAfterEnteringAP = 50000000; /*0.050s*/
const int64_t gracePeriod = 350000000; // 0.35s


/**
 * @brief Use line regression to capture the trend of RTT change, thus knowing whether a station
 * is getting close to or far from the Access Point or Base Station
 *
 * @param The vector length does not need to be a fixed value
 * @return The seq(s) of packet to be sent
 */
std::tuple<std::vector<uint32_t>, bool/*recover?*/, bool/*has coverage?*/>
moreInterestsToSend(uint32_t seqJustSent, ns3::ndn::Consumer::TrafficInfo trafficInfo, int& apCounter, int frequency)
{
  if (trafficInfo.real_rtt.size() < 2)
    return std::make_tuple(std::vector<uint32_t>(0), false, true);

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
  // before leaving
  uint64_t currentTp = ns3::Simulator::Now().GetNanoSeconds();
  double threshold = aveY;
  for (int i = 0; i < handoff_tp.size(); i++) {
    if (handoff_tp[i] > currentTp && handoff_tp[i] - currentTp <= threshold) {
      if (apCounter >= i + 1) {
        // already prefetch for this AP
        break;
      }

      std::vector<uint32_t> result;
      int insideNumber = static_cast<int>(threshold / 1000000000.0 * frequency);
      int outsideNumber = static_cast<int>((wifilessInterval + gracePeriod) / 1000000000.0 * frequency);
      for (int j = 0; j < insideNumber + outsideNumber + 1; j++) {
        result.push_back(seqJustSent + j);
      }
      apCounter++;
      return std::make_tuple(result, false, true); // still have coverage
    }
  }

  // after arriving
  for (int i = 0; i < entering_tp.size(); i++) {
    if (currentTp > entering_tp[i] + gracePeriod && currentTp - entering_tp[i] - gracePeriod <= thresholdAfterEnteringAP) {
      return std::make_tuple(std::vector<uint32_t>(0), true, true); // have coverage
    }
  }

   bool hasCoverage = true;
  for (int i = 0; i < handoff_tp.size(); i++) {
    if (handoff_tp[i] <= currentTp && entering_tp[i] + gracePeriod > currentTp) {
      hasCoverage = false;
      break;
    }
  }

  return std::make_tuple(std::vector<uint32_t>(0), false, hasCoverage);

  // // if the station is getting far from Access Point/Base Station
  // // pre-send an Interest with seq = seqJustSent + 5
  // std::vector<uint32_t> result;
  // if (rate > 0) {
  //   result.push_back(seqJustSent + 5);
  // }
  // return result;
}

std::tuple<std::vector<uint32_t>, bool>
oneHopV2VPrefetch(uint32_t seqJustSent, ns3::ndn::Consumer::TrafficInfo trafficInfo, int& apCounter)
{
  if (trafficInfo.real_rtt.size() < 2)
    return std::make_tuple(std::vector<uint32_t>(0), false);

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
  // before leaving
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
      for (int j = 0; j < insideNumber + outsideNumber + 1; j++) {
        result.push_back(seqJustSent + j);
      }
      apCounter++;
      return std::make_tuple(result, false);
    }
  }

  // after arriving
  for (int i = 0; i < entering_tp.size(); i++) {
    if (currentTp > entering_tp[i] && currentTp - entering_tp[i] <= thresholdAfterEnteringAP) {
      return std::make_tuple(std::vector<uint32_t>(0), true);
    }
  }
  return std::make_tuple(std::vector<uint32_t>(0), false);
}

// std::vector<uint32_t>
// MultiHopV2VPrefetch(uint32_t seqJustSent, ns3::ndn::Consumer::TrafficInfo trafficInfo)
// {
//   return std::vector<uint32_t>(0);
// }

} // namespace ns3
