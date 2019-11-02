/* -*- Mode:C++; c-file-style:"gnu"; indent-tabs-mode:nil; -*- */
/**
 * Authors:  Zhiyi Zhang: UCLA
 *           Xin Xu: UCLA
 *           Your name: your affiliation
 *
 **/

#ifndef NDNSIM_EXAMPLES_PRECACHE_STRATEGY_SEND_MORE_INTEREST_HPP
#define NDNSIM_EXAMPLES_PRECACHE_STRATEGY_SEND_MORE_INTEREST_HPP

#include "../ndn-consumer.hpp"
#include <vector>

namespace ns3 {

std::tuple<std::vector<uint32_t>, bool, bool>
moreInterestsToSend(uint32_t seqJustSent, ns3::ndn::Consumer::TrafficInfo trafficInfo, int frequency);

std::tuple<std::vector<uint32_t>, bool>
oneHopV2VPrefetch(uint32_t seqJustSent, ns3::ndn::Consumer::TrafficInfo trafficInfo, int& apCounter);

// std::vector<uint32_t>
// MultiHopV2VPrefetch(uint32_t seqJustSent, ns3::ndn::Consumer::TrafficInfo trafficInfo);

}
#endif // NDNSIM_EXAMPLES_PRECACHE_STRATEGY_SEND_MORE_INTEREST_HPP
