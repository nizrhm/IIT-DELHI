// bgp_partE_dualpath.cc
#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/applications-module.h"
#include "ns3/flow-monitor-module.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("DualPathRerouting");

int main(int argc, char *argv[])
{
    // Create 4 routers
    NodeContainer routers;
    routers.Create(4); // R1=0, R2=1, R3=2, R4=3

    InternetStackHelper internet;
    internet.Install(routers);

    // Setup point-to-point links
    PointToPointHelper link;
    link.SetDeviceAttribute("DataRate", StringValue("10Mbps"));
    link.SetChannelAttribute("Delay", StringValue("5ms"));

    // Path A: R1 → R2 → R3
    NetDeviceContainer devR1R2 = link.Install(routers.Get(0), routers.Get(1));
    NetDeviceContainer devR2R3 = link.Install(routers.Get(1), routers.Get(2));

    // Path B: R1 → R4 → R3
    NetDeviceContainer devR1R4 = link.Install(routers.Get(0), routers.Get(3));
    NetDeviceContainer devR4R3 = link.Install(routers.Get(3), routers.Get(2));

    // Assign IP addresses
    Ipv4AddressHelper ipv4;
    ipv4.SetBase("10.1.1.0", "255.255.255.0"); auto ifaceR1R2 = ipv4.Assign(devR1R2);
    ipv4.SetBase("10.1.2.0", "255.255.255.0"); auto ifaceR2R3 = ipv4.Assign(devR2R3);
    ipv4.SetBase("10.1.3.0", "255.255.255.0"); auto ifaceR1R4 = ipv4.Assign(devR1R4);
    ipv4.SetBase("10.1.4.0", "255.255.255.0"); auto ifaceR4R3 = ipv4.Assign(devR4R3);

    // Loopback address on R3 as destination
    Ipv4AddressHelper loopback;
    loopback.SetBase("192.168.1.0", "255.255.255.255");
    Ipv4InterfaceContainer r3LoopIface = loopback.Assign(routers.Get(2)->GetDevice(0));
    Ipv4Address destAddr = r3LoopIface.GetAddress(0);
    std::cout << "Destination (R3 loopback): " << destAddr << std::endl;

    // Static routing setup
    Ipv4StaticRoutingHelper staticRouting;
    Ptr<Ipv4StaticRouting> r1Route = staticRouting.GetStaticRouting(routers.Get(0)->GetObject<Ipv4>());
    Ptr<Ipv4StaticRouting> r2Route = staticRouting.GetStaticRouting(routers.Get(1)->GetObject<Ipv4>());
    Ptr<Ipv4StaticRouting> r4Route = staticRouting.GetStaticRouting(routers.Get(3)->GetObject<Ipv4>());

    // Add primary and backup routes on R1
    r1Route->AddHostRouteTo(destAddr, ifaceR1R2.GetAddress(1), 1, 1); // via R2
    r1Route->AddHostRouteTo(destAddr, ifaceR1R4.GetAddress(1), 2, 2); // via R4

    // Routing for intermediate routers
    r2Route->AddHostRouteTo(destAddr, ifaceR2R3.GetAddress(1), 2, 1);
    r4Route->AddHostRouteTo(destAddr, ifaceR4R3.GetAddress(1), 2, 1);

    // UDP sink on R3
    uint16_t port = 9090;
    PacketSinkHelper sink("ns3::UdpSocketFactory", InetSocketAddress(Ipv4Address::GetAny(), port));
    ApplicationContainer sinkApp = sink.Install(routers.Get(2));
    sinkApp.Start(Seconds(1.0));
    sinkApp.Stop(Seconds(12.0));

    // UDP source on R1
    OnOffHelper src("ns3::UdpSocketFactory", InetSocketAddress(destAddr, port));
    src.SetAttribute("OnTime", StringValue("ns3::ConstantRandomVariable[Constant=1]"));
    src.SetAttribute("OffTime", StringValue("ns3::ConstantRandomVariable[Constant=0]"));
    src.SetAttribute("DataRate", StringValue("1Mbps"));
    src.SetAttribute("PacketSize", UintegerValue(1024));
    ApplicationContainer srcApp = src.Install(routers.Get(0));
    srcApp.Start(Seconds(2.0));
    srcApp.Stop(Seconds(12.0));

    // Simulate failure and recovery of Path A
    Simulator::Schedule(Seconds(5.0), &Ipv4::SetDown, routers.Get(1)->GetObject<Ipv4>(), 2);
    Simulator::Schedule(Seconds(5.0), &Ipv4::SetDown, routers.Get(2)->GetObject<Ipv4>(), 1);

    Simulator::Schedule(Seconds(7.0), &Ipv4::SetUp, routers.Get(1)->GetObject<Ipv4>(), 2);
    Simulator::Schedule(Seconds(7.0), &Ipv4::SetUp, routers.Get(2)->GetObject<Ipv4>(), 1);

    // Monitor flows
    FlowMonitorHelper fm;
    Ptr<FlowMonitor> monitor = fm.InstallAll();

    Simulator::Stop(Seconds(13.0));
    Simulator::Run();

    // Print flow stats
    monitor->CheckForLostPackets();
    Ptr<Ipv4FlowClassifier> classifier = DynamicCast<Ipv4FlowClassifier>(fm.GetClassifier());
    auto stats = monitor->GetFlowStats();

    std::cout << "\n--- FlowMonitor Statistics ---\n";
    for (auto const& x : stats)
    {
        auto fiveTuple = classifier->FindFlow(x.first);
        std::cout << "Flow " << x.first << " (" 
                  << fiveTuple.sourceAddress << " → " << fiveTuple.destinationAddress << ")\n";
        std::cout << "  Tx: " << x.second.txPackets
                  << " Rx: " << x.second.rxPackets
                  << " Lost: " << x.second.lostPackets << "\n";
        if (x.second.rxPackets > 0)
        {
            double avgDelay = x.second.delaySum.GetSeconds() / x.second.rxPackets;
            std::cout << "  Avg Delay: " << avgDelay * 1000 << " ms\n";
        }
    }

    Simulator::Destroy();
    return 0;
}
