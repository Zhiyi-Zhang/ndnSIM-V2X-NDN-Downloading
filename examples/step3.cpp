/* -*- Mode:C++; c-file-style:"gnu"; indent-tabs-mode:nil; -*- */
/**
 * Authors:  Zhiyi Zhang: UCLA
 *           Your name: your affiliation
 *
 **/

#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/mobility-module.h"
#include "ns3/config-store-module.h"
#include "ns3/wifi-module.h"
#include "ns3/internet-module.h"
#include "ns3/ndnSIM-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/netanim-module.h"
#include "ns3/constant-velocity-mobility-model.h"
#include <iostream>
#include <fstream>
#include <vector>
#include <string>

NS_LOG_COMPONENT_DEFINE ("step01");

using namespace std;

namespace ns3 {

/**
 * DESCRIPTION:
 * The scenario simulates a tree topology (using topology reader module)
 *
 *                                   /----------\
 *                                   | Producer |
 *                                   \----------/
 *                                         |
 *                                     Internet       10Mbps 100ms
 *                                         |
 *                                    /--------\
 *                           +------->|  root  |<--------+
 *                           |        \--------/         |    10Mbps 20ms
 *                           |                           |
 *                           v                           v
 *                      /-------\                    /-------\
 *              +------>| rtr-4 |<-------+   +------>| rtr-5 |<--------+
 *              |       \-------/        |   |       \-------/         |
 *              |                        |   |                         |   10Mbps 10ms
 *              v                        v   v                         v
 *         /-------\                   /-------\                    /-------\
 *      +->| rtr-1 |<-+             +->| rtr-2 |<-+              +->| rtr-3 |<-+
 *      |  \-------/  |             |  \-------/  |              |  \-------/  |
 *      |             |             |             |              |             | 10Mbps 2ms
 *      v             v             v             v              v             v
 *   /------\      /------\      /------\      /------\      /------\      /------\
 *   |wifi-1|      |wifi-2|      |wifi-3|      |wifi-4|      |wifi-5|      |wifi-6|
 *   \------/      \------/      \------/      \------/      \------/      \------/
 *
 *
 * |v1|-->      |v2|-->
 *
 *
 * To run scenario and see what is happening, use the following command:
 *
 *     ./waf --run=step2
 *
 * With LOGGING: e.g.
 *
 *     NS_LOG=ndn.Consumer:ndn.Producer:ndn.Prefetcher ./waf --run=step2-vfetch 2>&1 | tee log2.txt
 */

int main (int argc, char *argv[])
{
  std::string phyMode ("DsssRate1Mbps");
  uint32_t wifiSta = 2;

  int bottomrow = 6;            // number of AP nodes
  int spacing = 200;            // between bottom-row nodes
  int range = 90;               // AP ranges
  double endtime = 40.0;
  double speed = (double)(bottomrow*spacing)/endtime; //setting speed to span full sim time

  string animFile = "ap-mobility-animation.xml";

  CommandLine cmd;
  cmd.AddValue ("animFile", "File Name for Animation Output", animFile);
  cmd.Parse (argc, argv);

  ////// Reading file for topology setup
  AnnotatedTopologyReader topologyReader("", 1);
  topologyReader.SetFileName("src/ndnSIM/examples/topologies/step01.txt");
  topologyReader.Read();

  ////// Getting containers for the producer/wifi-ap
  Ptr<Node> producer = Names::Find<Node>("root");
  Ptr<Node> wifiApNodes[6] = {Names::Find<Node>("ap1"),
                              Names::Find<Node>("ap2"),
                              Names::Find<Node>("ap3"),
                              Names::Find<Node>("ap4"),
                              Names::Find<Node>("ap5"),
                              Names::Find<Node>("ap6")};

  Ptr<Node> routers[6] = {Names::Find<Node>("r1"),
                          Names::Find<Node>("r2"),
                          Names::Find<Node>("r3"),
                          Names::Find<Node>("r4"),
                          Names::Find<Node>("r5"),
                          Names::Find<Node>("r6"),};

  NodeContainer consumers;
  consumers.Create(wifiSta);

  ////// disable fragmentation, RTS/CTS for frames below 2200 bytes and fix non-unicast data rate
  Config::SetDefault("ns3::WifiRemoteStationManager::FragmentationThreshold", StringValue("2200"));
  Config::SetDefault("ns3::WifiRemoteStationManager::RtsCtsThreshold", StringValue("2200"));
  Config::SetDefault("ns3::WifiRemoteStationManager::NonUnicastMode", StringValue(phyMode));

  ////// The below set of helpers will help us to put together the wifi NICs we want
  WifiHelper wifi;

  wifi.SetStandard (WIFI_PHY_STANDARD_80211b);

  YansWifiPhyHelper wifiPhy = YansWifiPhyHelper::Default ();
  wifiPhy.SetPcapDataLinkType (YansWifiPhyHelper::DLT_IEEE802_11_RADIO);
  YansWifiChannelHelper wifiChannel;
  wifiChannel.SetPropagationDelay("ns3::ConstantSpeedPropagationDelayModel");
  wifiChannel.AddPropagationLoss ("ns3::RangePropagationLossModel",
                                  "MaxRange", DoubleValue(range));
  wifiPhy.SetChannel (wifiChannel.Create ());
  wifi.SetRemoteStationManager ("ns3::ConstantRateWifiManager",
                                "DataMode", StringValue (phyMode),
                                "ControlMode", StringValue (phyMode));

  ////// Setting SSID, optional. Modified net-device to get Bssid, mandatory for AP unicast
  Ssid ssid = Ssid ("wifi-default");

  ////// Add a non-QoS upper mac of STAs, and disable rate control
  ////// Active associsation of STA to AP via probing.
  NqosWifiMacHelper wifiMacHelper = NqosWifiMacHelper::Default();
  wifiMacHelper.SetType("ns3::StaWifiMac", "Ssid", SsidValue(ssid),
                        "ActiveProbing", BooleanValue(true),
                        "ProbeRequestTimeout", TimeValue(Seconds(0.25)));
  NetDeviceContainer staDevice = wifi.Install(wifiPhy, wifiMacHelper, consumers);
  NetDeviceContainer devices = staDevice;

  ////// Setup AP.
  NqosWifiMacHelper wifiMac = NqosWifiMacHelper::Default();
  wifiMac.SetType("ns3::ApWifiMac", "Ssid", SsidValue(ssid),
                  "BeaconGeneration", BooleanValue(false));
  for (int i = 0; i < bottomrow; i++) {
    NetDeviceContainer apDevice = wifi.Install(wifiPhy, wifiMac, wifiApNodes[i]);
    devices.Add(apDevice);
  }

  ////// Add AdHoc Mac for Consumers
  WifiHelper wifi2 = WifiHelper::Default ();
  // wifi.SetRemoteStationManager ("ns3::AarfWifiManager");
  wifi2.SetStandard (WIFI_PHY_STANDARD_80211a);
  wifi2.SetRemoteStationManager ("ns3::ConstantRateWifiManager",
                                "DataMode", StringValue ("OfdmRate24Mbps"));
  YansWifiChannelHelper wifiChannel2;// = YansWifiChannelHelper::Default ();
  wifiChannel2.SetPropagationDelay ("ns3::ConstantSpeedPropagationDelayModel");
  wifiChannel2.AddPropagationLoss ("ns3::RangePropagationLossModel",
                                  "MaxRange", DoubleValue(range));
  YansWifiPhyHelper wifiPhy2 = YansWifiPhyHelper::Default ();
  wifiPhy2.SetChannel (wifiChannel.Create ());
  NqosWifiMacHelper wifiMac2 = NqosWifiMacHelper::Default ();
  wifiMac2.SetType("ns3::AdhocWifiMac");
  wifi.Install(wifiPhy2, wifiMac2, consumers);

  ////// set positions for APs
  MobilityHelper sessile;
  Ptr<ListPositionAllocator> positionAlloc = CreateObject<ListPositionAllocator>();
  int Xpos = 0;
  for (int i = 0; i < bottomrow; i++) {
    positionAlloc->Add(Vector(100+Xpos, 0.0, 0.0));
    Xpos += spacing;
  }
  sessile.SetPositionAllocator(positionAlloc);
  for (int i = 0; i < bottomrow; i++) {
    sessile.SetMobilityModel ("ns3::ConstantPositionMobilityModel");
    sessile.Install (wifiApNodes[i]);
  }

  ////// Setting mobility model and movement parameters for mobile nodes
  ////// ConstantVelocityMobilityModel is a subclass of MobilityModel
  MobilityHelper mobile;
  mobile.SetMobilityModel("ns3::ConstantVelocityMobilityModel");
  mobile.Install(consumers);

  ////// Setting each mobile consumer 100m apart from each other
  int nxt = 0;
  for (uint32_t i=0; i<wifiSta ; i++) {
    Ptr<ConstantVelocityMobilityModel> cvmm =
      consumers.Get(i)->GetObject<ConstantVelocityMobilityModel>();
    Vector pos(nxt, 0, 0);
    Vector vel(speed, 0, 0);
    cvmm->SetPosition(pos);
    cvmm->SetVelocity(vel);
    nxt += 80;
  }

  // std::cout << "position: " << cvmm->GetPosition() << " velocity: " << cvmm->GetVelocity() << std::endl;
  // std::cout << "mover mobility model: " << mobile.GetMobilityModelType() << std::endl; // just for confirmation

  // 3. Install NDN stack on all nodes
  NS_LOG_INFO("Installing NDN stack");
  ndn::StackHelper ndnHelper;
  //ndnHelper.InstallAll();
  ndnHelper.SetOldContentStore("ns3::ndn::cs::Lru", "MaxSize", "1000");
  //ndnHelper.SetDefaultRoutes(true);
  //ndnHelper.SetOldContentStore("ns3::ndn::cs::Nocache");
  // ndnHelper.InstallAll();
  ndnHelper.Install(Names::Find<Node>("root"));
  ndnHelper.Install(Names::Find<Node>("ap1"));
  ndnHelper.Install(Names::Find<Node>("ap2"));
  ndnHelper.Install(Names::Find<Node>("ap3"));
  ndnHelper.Install(Names::Find<Node>("ap4"));
  ndnHelper.Install(Names::Find<Node>("ap5"));
  ndnHelper.Install(Names::Find<Node>("ap6"));
  ndnHelper.Install(Names::Find<Node>("r1"));
  ndnHelper.Install(Names::Find<Node>("r2"));
  ndnHelper.Install(Names::Find<Node>("r3"));
  ndnHelper.Install(Names::Find<Node>("r4"));
  ndnHelper.Install(Names::Find<Node>("r5"));
  ndnHelper.Install(Names::Find<Node>("r6"));
  ndnHelper.Install(consumers);

  // Choosing forwarding strategy
  ndn::StrategyChoiceHelper::InstallAll("/prefix", "/localhost/nfd/strategy/multicast");
  //ndn::StrategyChoiceHelper::InstallAll("/", "/localhost/nfd/strategy/best-route");

  // Installing global routing interface on all nodes
  ndn::GlobalRoutingHelper ndnGlobalRoutingHelper;
  ndnGlobalRoutingHelper.InstallAll();

  // 4. Set up applications
  NS_LOG_INFO("Installing Applications");
  // Producer Helpers
  ndn::AppHelper producerHelper("ns3::ndn::Producer");
  producerHelper.SetAttribute("PayloadSize", StringValue("1024"));
  // Register /root prefix with global routing controller and
  // install producer that will satisfy Interests in /youtube namespace
  ndnGlobalRoutingHelper.AddOrigins("/youtube", producer);
  producerHelper.SetPrefix("/youtube/video001");
  producerHelper.Install(producer);

  // Calculate and install FIBs
  ndn::GlobalRoutingHelper::CalculateRoutes();

  // Consumer Helpers
  ndn::AppHelper consumerHelper("ns3::ndn::ConsumerCbr");
  consumerHelper.SetPrefix("/youtube/video001");
  // consumerHelper.SetPrefix("/youtube/prefix");
  consumerHelper.SetAttribute("Frequency", DoubleValue(10.0));
  consumerHelper.SetAttribute("Step3", BooleanValue(true));
  // consumerHelper.SetAttribute("RetxTimer", );
  consumerHelper.Install(consumers.Get(0)).Start(Seconds(0.1));
  // consumerHelper.Install(consumers.Get(1)).Start(Seconds(0.0));

  for (auto consumer: consumers) {
    ndn::FibHelper::AddRouteForDevice(consumer, "/youtube/video001", std::numeric_limits<int32_t>::max(), 0);
    ndn::FibHelper::AddRouteForDevice(consumer, "/prefetch", std::numeric_limits<int32_t>::max(), 1);
  }

  // Tracing
  wifiPhy.EnablePcap("step01", devices);

  Simulator::Stop(Seconds(endtime));
  AnimationInterface anim(animFile);

  for (uint32_t i = 0; i < wifiSta; i++) {
    string str = "step01-" + std::to_string(i+1) + ".txt";
    ndn::L3RateTracer::Install(consumers.Get(i), str, Seconds(endtime-0.5));
  }

  //ndn::CsTracer::Install(routers[0],"simple-wifi-mobility-trace.txt", Seconds(1.0));

  Simulator::Run ();
  Simulator::Destroy ();

  return 0;
}
}

int main(int argc, char* argv[])
{
  return ns3::main(argc, argv);
}
