/* -*- Mode:C++; c-file-style:"gnu"; indent-tabs-mode:nil; -*- */
/**
 * Authors:  Zhiyi Zhang: UCLA
 *           Your name: your affiliation
 *
 **/

#include "send-more-interest.hpp"
#include <iostream>

namespace ns3 {

/**
 * @brief Use line regression to capture the trend of RTT change, thus knowing whether a station
 * is getting close to or far from the Access Point or Base Station
 *
 * @param The vector length does not need to be a fixed value
 * @return The seq(s) of packet to be sent
 */
std::vector<uint32_t>
moreInterestsToSend(uint32_t seqJustSent, std::deque<ns3::ndn::Consumer::RttInfo> rttVec)
{
  if (rttVec.size() < 2) return std::vector<uint32_t>(0);
  // ordinary least squares of line regression
  double sumX = 0;
  double sumY = 0;
  double sumXY = 0;
  double sumXX = 0;

  int index = 1;
  auto it = rttVec.begin();
  while (it != rttVec.end()) {
    sumX += index;
    sumY += it->real_rtt;;
    sumXY += index * it->real_rtt;
    sumXX += index * index;
    it++;
    index++;
  }

  double aveX = sumX / rttVec.size();
  double aveY = sumY / rttVec.size();
  double rate = sumXY - rttVec.size() * aveX * aveY;
  rate = rate / (sumXX - rttVec.size() * aveX * aveX);

  // if the station is getting far from Access Point/Base Station
  // pre-send an Interest with seq = seqJustSent + 5
  std::vector<uint32_t> result;
  if (rate > 0) {
    result.push_back(seqJustSent + 5);
  }
  return result;
}

} // namespace ns3
