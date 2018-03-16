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

#include "ns3/application.h"
#include "ns3/ptr.h"
#include "ns3/simulator.h"
#include "ns3/nstime.h"
#include "ns3/wifi-net-device.h"
#include "ns3/sta-wifi-mac.h"

namespace ndn {

NS_LOG_COMPONENT_DEFINE("ndn.Prefetcher");

static const Name kPrefetchPrefix = Name("/prefetch");

class PrefetcherNode {
 public:
  using GetCurrentAP =
      std::function<ns3::Address()>;

  PrefetcherNode(const Name& prefix, uint64_t nid, bool multiHop, GetCurrentAP getCurrentAP):
    scheduler_(face_.getIoService()),
    key_chain_(ns3::ndn::StackHelper::getKeyChain()),
    prefix_(prefix),
    nid_(nid),
    multiHop_(multiHop),
    getCurrentAP_(std::move(getCurrentAP))
  {
    face_.setInterestFilter(
      kPrefetchPrefix, std::bind(&PrefetcherNode::OnPrefetchInterest, this, _2),
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
    auto n = interest.getName();
    NS_LOG_INFO( "node(" << nid_ << ") receives the pretch interest: " << n.toUri() );

    if (!multiHop_) {
      // one-hop V2V communication
      // interest: /prefetch/prefix_/seq/[last-ap], prefix_ = /youtube/video00
      // interest: /prefetch/prefix/seq1/seq2/ap
      std::string last_ap = decode(n.get(-1).toUri());
      auto seq1 = n.get(-2).toNumber();
      auto seq2 = n.get(-3).toNumber();

      ns3::Address cur_ap_addr = getCurrentAP_();
      std::ostringstream os;
      os << cur_ap_addr;
      std::string cur_ap = os.str().c_str();
      std::cout << "node(" << nid_ << "), last ap = " << last_ap << ", current ap = " << cur_ap << std::endl;

      for (int i = seq1; i < seq2 + 1; i++) {
        auto real_interest_name = Name(prefix_).appendSequenceNumber(i);
        // send the real interest
        Interest preInterest(real_interest_name, time::seconds(2));
        face_.expressInterest(preInterest, std::bind(&PrefetcherNode::OnRemoteData, this, _2),
                              [](const Interest&, const lp::Nack&) {},
                              [](const Interest&) {});
        NS_LOG_INFO( "node(" << nid_ << ") send out Interest: " << real_interest_name.toUri() );
      }
    }
    else {
      // multi-hop V2V communication
      // interest: /prefetch/prefix_/seq/[max_hop]
      // if the car is not on the correct direction, drop this interest
      // correct direction: the ap-address is larger than last-ap

    }
  }

  std::string decode(std::string str) {
    size_t start = 0;
    size_t pos = str.find("%3A", start);
    string ret = "";
    while (pos != std::string::npos) {
      ret += str.substr(start, pos - start) + ":";
      start = pos + 3;
      pos = str.find("%3A", start);
    }
    ret += str.substr(start);
    return ret;
  }

 private:
  Face face_;
  Scheduler scheduler_;
  KeyChain& key_chain_;
  Name prefix_;
  uint64_t nid_;
  bool multiHop_;
  GetCurrentAP getCurrentAP_;
};

}

#endif
