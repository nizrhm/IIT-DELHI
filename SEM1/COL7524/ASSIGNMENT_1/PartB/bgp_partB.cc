// bgp_partB.cc
#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/applications-module.h"
#include "ns3/flow-monitor-module.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("bgp_partB");

int main(int argc, char *argv[])
{
    Time::SetResolution(Time::NS);
    CommandLine cmd;
    cmd.Parse(argc, argv);

    LogComponentEnable("bgp_partB", LOG_LEVEL_INFO);
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

    // New direct AS1-AS3 link (make it faster + lower delay)
    PointToPointHelper fastLink;
    fastLink.SetDeviceAttribute("DataRate", StringValue("100Mbps"));
    fastLink.SetChannelAttribute("Delay", StringValue("1ms"));
    NetDeviceContainer d13 = fastLink.Install(nodes.Get(0), nodes.Get(2));

    // IP addressing
    Ipv4AddressHelper ipv4;
    ipv4.SetBase("10.0.1.0", "255.255.255.0");
    ipv4.Assign(d12);
    ipv4.SetBase("10.0.2.0", "255.255.255.0");
    Ipv4InterfaceContainer i23 = ipv4.Assign(d23);
    ipv4.SetBase("10.0.3.0", "255.255.255.0");
    ipv4.Assign(d13);

    // Build routing tables
    Ipv4GlobalRoutingHelper::PopulateRoutingTables();

    // Applications
    uint16_t port = 9;
    UdpEchoServerHelper server(port);
    ApplicationContainer apps = server.Install(nodes.Get(2));
    apps.Start(Seconds(1.0));
    apps.Stop(Seconds(10.0));

    UdpEchoClientHelper client(i23.GetAddress(1), port);
    client.SetAttribute("MaxPackets", UintegerValue(5));
    client.SetAttribute("Interval", TimeValue(Seconds(1.0)));
    client.SetAttribute("PacketSize", UintegerValue(1024));
    apps = client.Install(nodes.Get(0));
    apps.Start(Seconds(2.0));
    apps.Stop(Seconds(10.0));

    FlowMonitorHelper fm;
    Ptr<FlowMonitor> monitor = fm.InstallAll();

    Simulator::Stop(Seconds(11.0));
    Simulator::Run();

    monitor->SerializeToXmlFile("bgp-partB-results.xml", true, true);

    Simulator::Destroy();
    return 0;
}


