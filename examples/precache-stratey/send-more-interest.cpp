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
moreInterestsToSend(uint32_t seqJustSent, const std::vector<double>& rttVec)
{
  // ordinary least squares of line regression
  double sumX = 0;
  double sumY = 0;
  double sumXY = 0;
  double sumXX = 0;

  for (int i = 0; i < rttVec.size(); i++) {
    sumX += i+1;
    sumY += rttVec[i];
    sumXY += (i+1) * rttVec[i];
    sumXX += (i+1) * (i+1);
  }
  double aveX = sumX / rttVec.size();
  double aveY = sumY / rttVec.size();
  double rate = sumXY - rttVec.size() * aveX * aveY;
  rate = rate / (sumXX - rttVec.size() * aveX * aveX);

  // if the station is getting far from Access Point/Base Station
  // pre-send an Interest with seq = seqJustSent + 5
  vector<uint32_t> result;
  if (rate > 0) {
    result.push_back(seqJustSent + 5);
  }
  return result;
}

} // namespace ns3
