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
#include <ndn-cxx/lp/tags.hpp>

#include "ns3/application.h"
#include "ns3/ptr.h"
#include "ns3/simulator.h"
#include "ns3/nstime.h"
#include "ns3/wifi-net-device.h"
#include "ns3/sta-wifi-mac.h"

namespace ndn {

NS_LOG_COMPONENT_DEFINE("ndn.Prefetcher");

static const Name kPrefetchPrefix = Name("/prefetch");
static const time::milliseconds kInterestLifetime = time::milliseconds(280);

class PrefetcherNode {
 public:
  using GetCurrentAP =
      std::function<ns3::Address()>;

  PrefetcherNode(const Name& prefix, uint64_t nid, GetCurrentAP getCurrentAP):
    scheduler_(face_.getIoService()),
    key_chain_(ns3::ndn::StackHelper::getKeyChain()),
    prefix_(prefix),
    nid_(nid),
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
    auto seq = data.getName().get(-1).toSequenceNumber();
    NS_LOG_INFO( "node(" << nid_ << ") receives the data: " << seq );
    scheduler_.cancelEvent(retx_timer[seq]);
    retx_timer.erase(seq);
  }

  void OnPrefetchInterest(const Interest& interest) {
    auto n = interest.getName();
    prefetchInterestName = n;
    NS_LOG_INFO( "node(" << nid_ << ") receives the pretch interest: " << n.toUri() );
    // assert(retx_timer.empty());

    // one-hop V2V communication
    // interest: /prefetch/prefix_/seq/[last-ap], prefix_ = /youtube/video00
    // interest: /prefetch/prefix/seq1/seq2/ap
    std::string last_ap = decode(n.get(-1).toUri());
    auto seq1 = n.get(-2).toNumber();
    auto seq2 = n.get(-3).toNumber();
    NS_LOG_INFO( "node(" << nid_ << ") will help to fetch seq " << seq1 << " to " << seq2);

    ns3::Address cur_ap_addr = getCurrentAP_();
    std::ostringstream os;
    os << cur_ap_addr;
    std::string cur_ap = os.str().c_str();
    std::cout << "node(" << nid_ << "), last ap = " << last_ap << ", current ap = " << cur_ap << std::endl;

    for (int seq = seq2; seq < seq1 + 1; seq++) {
      SendInterest(seq);
    }
  }

  void SendInterest(uint32_t seq) {
    auto real_interest_name = Name(prefix_).append(std::to_string(seq)).appendSequenceNumber(seq);
    Interest preInterest(real_interest_name, kInterestLifetime);
    face_.expressInterest(preInterest, std::bind(&PrefetcherNode::OnRemoteData, this, _2),
                          [](const Interest&, const lp::Nack&) {},
                          [](const Interest&) {});
    retx_timer[seq] = scheduler_.scheduleEvent(kInterestLifetime, [this, seq] {
      NS_LOG_INFO( "node(" << nid_ << ") Retx Timeout for " << seq );
      // SendInterest(seq);
      // do nothing
    });
    // preInterest.refreshNonce();
    // preInterest.setTag<lp::NextHopFaceIdTag>(make_shared<lp::NextHopFaceIdTag>(257));
    // face_.expressInterest(preInterest, std::bind(&PrefetcherNode::OnRemoteData, this, _2),
    //                       [](const Interest&, const lp::Nack&) {},
    //                       [](const Interest&) {});
    // retx_timer[seq] = scheduler_.scheduleEvent(kInterestLifetime, [this, seq] {
    //   NS_LOG_INFO( "node(" << nid_ << ") Retx Timeout for " << seq );
    //   // SendInterest(seq);
    //   // do nothing
    // });
    // NS_LOG_INFO( "node(" << nid_ << ") send out Interest for " << seq );
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
  GetCurrentAP getCurrentAP_;

  unordered_map<uint32_t, EventId> retx_timer;
  Name prefetchInterestName;
};

}

#endif
