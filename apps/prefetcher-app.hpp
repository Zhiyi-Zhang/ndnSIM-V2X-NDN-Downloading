#ifndef PREFETCHER_APP_HPP_
#define PREFETCHER_APP_HPP_

#include "ns3/ndnSIM-module.h"
#include "ns3/integer.h"
#include "ns3/string.h"
#include "prefetcher-node.hpp"
#include "ns3/uinteger.h"
#include "ns3/boolean.h"

namespace ns3 {

class PrefetcherApp : public Application
{
public:
  static TypeId
  GetTypeId()
  {
    static TypeId tid = TypeId("PrefetcherApp")
      .SetParent<Application>()
      .AddConstructor<PrefetcherApp>()
      .AddAttribute("Prefix", "Prefix for prefetcher", StringValue("/"),
                    ndn::MakeNameAccessor(&PrefetcherApp::prefix_), ndn::MakeNameChecker())
      .AddAttribute("NodeID", "NodeID for prefetcher", UintegerValue(0),
                    MakeUintegerAccessor(&PrefetcherApp::nid_), MakeUintegerChecker<uint64_t>());


    return tid;
  }

Address
GetCurrentAP() {
  Ptr<WifiNetDevice> wifiDev = GetNode()->GetDevice(0)->GetObject<WifiNetDevice>();
  assert(wifiDev != nullptr);
  Ptr<StaWifiMac> staMac = wifiDev->GetMac()->GetObject<StaWifiMac>();
  assert(staMac != nullptr);
  Address ap = staMac->GetBssid();
  //std::cout << "App " << m_appId << " on Node " << GetNode()->GetId() << " connected to " << dest << std::endl;
  return ap;
}

protected:
  // inherited from Application base class.
  virtual void
  StartApplication()
  {
    m_instance.reset(new ::ndn::PrefetcherNode(prefix_, nid_, std::bind(&PrefetcherApp::GetCurrentAP, this)));
    m_instance->Start();
  }

  virtual void
  StopApplication()
  {
    m_instance.reset();
  }

private:
  std::unique_ptr<::ndn::PrefetcherNode> m_instance;
  ndn::Name prefix_;
  uint64_t nid_;
};

}

#endif