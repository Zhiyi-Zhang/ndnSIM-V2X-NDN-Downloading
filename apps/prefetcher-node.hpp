#ifndef PREFETCHER_NODE_HPP_
#define PREFETCHER_NODE_HPP_

#include <functional>
#include <iostream>
#include <random>
#include <boost/lexical_cast.hpp>
#include <boost/asio.hpp>

#include <ndn-cxx/name.hpp>
#include <ndn-cxx/data.hpp>
#include <ndn-cxx/face.hpp>
#include <ndn-cxx/interest.hpp>
#include <ndn-cxx/security/key-chain.hpp>
#include <ndn-cxx/security/signing-helpers.hpp>
#include <ndn-cxx/security/signing-info.hpp>
#include <ndn-cxx/util/backports.hpp>
#include <ndn-cxx/util/scheduler-scoped-event-id.hpp>
#include <ndn-cxx/util/scheduler.hpp>
#include <ndn-cxx/util/signal.hpp>

#include "ns3/simulator.h"
#include "ns3/nstime.h"

namespace ndn {

NS_LOG_COMPONENT_DEFINE("ndn.Prefetcher");

static const std::string kPrefetchTag = "prefetch";

class PrefetcherNode {
 public:
  PrefetcherNode(const Name& prefix, uint64_t nid):
    scheduler_(face_.getIoService()),
    key_chain_(ns3::ndn::StackHelper::getKeyChain())
  {
    prefix_ = prefix;
    nid_ = nid;
    auto prefetchPrefix = prefix_.append(kPrefetchTag);

    face_.setInterestFilter(
      prefetchPrefix, std::bind(&PrefetcherNode::OnPrefetchInterest, this, _2),
      [this](const Name&, const std::string& reason) {
        NS_LOG_INFO( "Failed to register prefetch interest: " << reason );
      });
  }

  void Start() {
  }

  void OnRemoteData(const Data& data) {
    NS_LOG_INFO( "node(" << nid_ << ") receives the data: " << data.getName().toUri() );
  }

  void OnPrefetchInterest(const Interest& interest) {
    // NS_LOG_INFO( "node(" << nid_ << ") current ssid = " << )
    auto n = interest.getName();
    NS_LOG_INFO( "node(" << nid_ << ") receives the pretch interest: " << n.toUri() );
    
    auto seq = n.get(-1).toNumber();
    auto real_interest_name = Name(prefix_).appendNumber(seq);
    // send the real interest
    Interest i(real_interest_name, time::seconds(2));
    face_.expressInterest(i, std::bind(&PrefetcherNode::OnRemoteData, this, _2),
                          [](const Interest&, const lp::Nack&) {},
                          [](const Interest&) {});
    NS_LOG_INFO( "node(" << nid_ << ") send out Interest: " << real_interest_name.toUri() );
  }

 private:
  Face face_;
  Scheduler scheduler_;
  KeyChain& key_chain_;
  Name prefix_;
  uint64_t nid_;
};

}

#endif