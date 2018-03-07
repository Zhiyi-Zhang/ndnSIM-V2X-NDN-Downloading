/* -*- Mode:C++; c-file-style:"gnu"; indent-tabs-mode:nil; -*- */
/**
 * Authors:  Zhiyi Zhang: UCLA
 *           Xin Xu: UCLA
 *
 **/

#ifndef NDNSIM_EXAMPLES_PRECACHE_STRATEGY_SEND_MORE_INTEREST_HPP
#define NDNSIM_EXAMPLES_PRECACHE_STRATEGY_SEND_MORE_INTEREST_HPP

#include "../ndn-consumer.hpp"
#include <vector>

namespace ns3 {

std::vector<uint32_t>
moreInterestsToSend(uint32_t seqJustSent, ns3::ndn::Consumer::TrafficInfo trafficInfo, int& apCounter);

std::vector<uint32_t>
oneHopV2VPrefetch(uint32_t seqJustSent, ns3::ndn::Consumer::TrafficInfo trafficInfo);

std::vector<uint32_t>
MultiHopV2VPrefetch(uint32_t seqJustSent, ns3::ndn::Consumer::TrafficInfo trafficInfo);

}
#endif // NDNSIM_EXAMPLES_PRECACHE_STRATEGY_SEND_MORE_INTEREST_HPP
