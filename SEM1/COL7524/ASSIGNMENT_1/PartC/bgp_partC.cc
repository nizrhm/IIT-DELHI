// bgp_partC.cc
#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/applications-module.h"
#include "ns3/flow-monitor-module.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("bgp-partC");

int main(int argc, char *argv[])
{
    Time::SetResolution(Time::NS);
    CommandLine cmd;
    cmd.Parse(argc, argv);

    LogComponentEnable("bgp-partC", LOG_LEVEL_INFO);
    LogComponentEnable("UdpEchoClientApplication", LOG_LEVEL_INFO);
    LogComponentEnable("UdpEchoServerApplication", LOG_LEVEL_INFO);

    NodeContainer nodes;
    nodes.Create(3); // R0=AS1, R1=AS2, R2=AS3

    InternetStackHelper stack;
    stack.Install(nodes);

    // Normal AS1-AS2 and AS2-AS3 links
    PointToPointHelper p2p;
    p2p.SetDeviceAttribute("DataRate", StringValue("10Mbps"));
    p2p.SetChannelAttribute("Delay", StringValue("2ms"));
    NetDeviceContainer d12 = p2p.Install(nodes.Get(0), nodes.Get(1));
    NetDeviceContainer d23 = p2p.Install(nodes.Get(1), nodes.Get(2));

    // Direct AS1-AS3 link (fast)
    PointToPointHelper fastLink;
    fastLink.SetDeviceAttribute("DataRate", StringValue("100Mbps"));
    fastLink.SetChannelAttribute("Delay", StringValue("1ms"));
    NetDeviceContainer d13 = fastLink.Install(nodes.Get(0), nodes.Get(2));

    // IP assignment
    Ipv4AddressHelper ipv4;
    ipv4.SetBase("10.0.1.0", "255.255.255.0");
    Ipv4InterfaceContainer i12 = ipv4.Assign(d12);
    ipv4.SetBase("10.0.2.0", "255.255.255.0");
    Ipv4InterfaceContainer i23 = ipv4.Assign(d23);
    ipv4.SetBase("10.0.3.0", "255.255.255.0");
    ipv4.Assign(d13);

    Ipv4GlobalRoutingHelper::PopulateRoutingTables();

    // Add static route at AS1 to force path via AS2
    Ipv4StaticRoutingHelper srHelper;
    Ptr<Ipv4> ipv4Node0 = nodes.Get(0)->GetObject<Ipv4>();
    Ptr<Ipv4StaticRouting> staticRouting = srHelper.GetStaticRouting(ipv4Node0);

    Ipv4Address serverAddr = i23.GetAddress(1); // Server (AS3) IP
    Ipv4Address nextHop = i12.GetAddress(1);    // Next-hop is AS2

    uint32_t ifIndex = ipv4Node0->GetInterfaceForDevice(d12.Get(0));
    staticRouting->AddHostRouteTo(serverAddr, nextHop, ifIndex);

    // Apps
    uint16_t port = 9;
    UdpEchoServerHelper server(port);
    ApplicationContainer apps = server.Install(nodes.Get(2));
    apps.Start(Seconds(1.0));
    apps.Stop(Seconds(15.0));

    UdpEchoClientHelper client(serverAddr, port);
    client.SetAttribute("MaxPackets", UintegerValue(10));
    client.SetAttribute("Interval", TimeValue(Seconds(1.0)));
    client.SetAttribute("PacketSize", UintegerValue(512));
    apps = client.Install(nodes.Get(0));
    apps.Start(Seconds(2.0));
    apps.Stop(Seconds(15.0));

    FlowMonitorHelper fm;
    Ptr<FlowMonitor> monitor = fm.InstallAll();

    Simulator::Stop(Seconds(16.0));
    Simulator::Run();

    monitor->SerializeToXmlFile("bgp-partC-results.xml", true, true);

    Simulator::Destroy();
    return 0;
}
