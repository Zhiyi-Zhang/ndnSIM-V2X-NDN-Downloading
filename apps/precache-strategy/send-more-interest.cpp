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

const std::vector<int64_t> handoff_tp = {8 * (uint64_t)1000000000 /*160m, AP1*/,
                                         18 * (uint64_t)1000000000 /*360m, AP2*/,
                                         28 * (uint64_t)1000000000 /*9.833s*2, 590m, AP3*/,
                                         38 * (uint64_t)1000000000 /*9.833s*2, 790m, AP4*/,
                                         48 * (uint64_t)1000000000 /*16.5s*2, 990m, AP5*/,
                                         58 * (uint64_t)1000000000 /*19.833s*2, 1190m, AP6*/};

const std::vector<int64_t> entering_tp = {2 * (uint64_t)1000000000 /*40m, AP1*/,
                                          12 * (uint64_t)1000000000 /*240m, AP2*/,
                                          22 * (uint64_t)1000000000 /*440m, AP3*/,
                                          32 * (uint64_t)1000000000 /*640m, AP4*/,
                                          42 * (uint64_t)1000000000 /*840m, AP5*/,
                                          52 * (uint64_t)1000000000/*1040m, AP6*/};

// the time interval between two APs when there is no wifi connection
const int64_t wifiLessInterval = 4 * (uint64_t)1000000000;


const int64_t not_real_coverage_period = 0.8 * (uint64_t)1000000000; /*0.08s*/
const int64_t gracePeriod = 350000000; // 0.35s
const int64_t default_rtt = 500000000;
static int seq_not_sent_yet_start = 0;
static int prefetch_ap_counter = 0;
static int recover_ap_counter = 0;
static int stop_sending_ap_counter = 0;


/**
 * @brief Use line regression to capture the trend of RTT change, thus knowing whether a station
 * is getting close to or far from the Access Point or Base Station
 *
 * @param The vector length does not need to be a fixed value
 * @return The seq(s) of packet to be sent
 */
std::tuple<std::vector<uint32_t>, bool/*recover?*/, bool/*has coverage?*/>
moreInterestsToSend(uint32_t seqAboutToSend, ns3::ndn::Consumer::TrafficInfo trafficInfo, int frequency)
{
  uint64_t currentTp = ns3::Simulator::Now().GetNanoSeconds();
  bool hasCoverage = true;
  for (int i = 0; i < handoff_tp.size(); i++) {
    if (currentTp >= handoff_tp[i] && currentTp < entering_tp[i] + not_real_coverage_period) {
      hasCoverage = false;
      break;
    }
  }
  if (currentTp <= entering_tp[0] || currentTp >= handoff_tp[handoff_tp.size() - 1]) {
    hasCoverage = false;
  }

  // ordinary least squares of line regression
  double threshold = 0;
  if (trafficInfo.real_rtt.size() < 2) {
    threshold = default_rtt;
  }
  else {
    double sumX = 0, sumY = 0, sumXY = 0, sumXX = 0;

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
    threshold = aveY;
  }

  // check if about to enter the next RSU, if yes, send pre-fetch Interests to front vehicle
  for (int i = 0; i < entering_tp.size(); i++) {
    if (prefetch_ap_counter >= i + 1) {
      continue;
    }
    if (currentTp < entering_tp[i] && entering_tp[i] - currentTp <= threshold) {
      std::vector<uint32_t> result;
      int seqs_before_next_ap = static_cast<int>((threshold + not_real_coverage_period) / 1000000000.0 * frequency);
      for (int i = seq_not_sent_yet_start; i <= seqAboutToSend + seqs_before_next_ap; i++) {
        result.push_back(seq_not_sent_yet_start + i);
      }
      prefetch_ap_counter++;
      return std::make_tuple(result, false, hasCoverage);
    }
  }

  for (int i = 0; i < handoff_tp.size(); i++) {
    if (stop_sending_ap_counter >= i + 1) {
      continue;
    }
    if (currentTp > handoff_tp[i]) {
      seq_not_sent_yet_start = seqAboutToSend;
      stop_sending_ap_counter++;
    }
  }

  // otherwise, check if already enter the next RSU
  for (int i = 0; i < entering_tp.size(); i++) {
    if (recover_ap_counter >= i + 1) {
      continue;
    }
    if (currentTp > entering_tp[i] + not_real_coverage_period) {
      recover_ap_counter++;
      return std::make_tuple(std::vector<uint32_t>(0), true, hasCoverage);
    }
  }
  return std::make_tuple(std::vector<uint32_t>(0), false, hasCoverage);
}

std::tuple<std::vector<uint32_t>, bool>
oneHopV2VPrefetch(uint32_t seqAboutToSend, ns3::ndn::Consumer::TrafficInfo trafficInfo, int& apCounter)
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
      int outsideNumber = static_cast<int>(wifiLessInterval / 100000000);
      for (int j = 0; j < insideNumber + outsideNumber + 1; j++) {
        result.push_back(seqAboutToSend + j);
      }
      apCounter++;
      return std::make_tuple(result, false);
    }
  }

  // after arriving
  for (int i = 0; i < entering_tp.size(); i++) {
    if (currentTp > entering_tp[i] && currentTp - entering_tp[i] <= not_real_coverage_period) {
      return std::make_tuple(std::vector<uint32_t>(0), true);
    }
  }
  return std::make_tuple(std::vector<uint32_t>(0), false);
}

// std::vector<uint32_t>
// MultiHopV2VPrefetch(uint32_t seqAboutToSend, ns3::ndn::Consumer::TrafficInfo trafficInfo)
// {
//   return std::vector<uint32_t>(0);
// }

} // namespace ns3
