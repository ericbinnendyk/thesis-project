#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/ndnSIM-module.h"

namespace ns3 {

    /**
     * This scenario simulates a very simple network topology:
     *
     *
     *      +----------+     1Mbps      +--------+     1Mbps      +----------+
     *      | consumer | <------------> | router | <------------> | producer |
     *      +----------+         10ms   +--------+          10ms  +----------+
     *
     *
     * Consumer requests data from producer with frequency 10 interests per second
     * (interests contain constantly increasing sequence number).
     *
     * For every received interest, producer replies with a data packet, containing
     * 1024 bytes of virtual payload.
     *
     * To run scenario and see what is happening, use the following command:
     *
     *     NS_LOG=ndn.Consumer:ndn.Producer ./waf --run=ndn-simple
     */

    int
    main(int argc, char* argv[])
    {
       // setting default parameters for PointToPoint links and channels
       Config::SetDefault("ns3::PointToPointNetDevice::DataRate", StringValue("1Mbps"));
       Config::SetDefault("ns3::PointToPointChannel::Delay", StringValue("10ms"));
       Config::SetDefault("ns3::QueueBase::MaxSize", StringValue("20p"));

       // Read optional command-line parameters (e.g., enable visualizer with ./waf --run=<> --visualize
       CommandLine cmd;
       cmd.Parse(argc, argv);

       // Creating nodes from topology
       AnnotatedTopologyReader topologyReader("", 25);
       topologyReader.SetFileName("src/ndnSIM/examples/topologies/topology.txt");
       topologyReader.Read();
       /* NodeContainer nodes;
       nodes.Create(5);

       // Connecting nodes using two links
       PointToPointHelper p2p;
       p2p.Install(nodes.Get(0), nodes.Get(3));
       p2p.Install(nodes.Get(1), nodes.Get(3));
       p2p.Install(nodes.Get(2), nodes.Get(3));
       p2p.Install(nodes.Get(3), nodes.Get(4)); */

       // Install NDN stack on all nodes
       ndn::StackHelper ndnHelper;
       ndnHelper.SetDefaultRoutes(true);
       ndnHelper.InstallAll();

       // Setup dynamic routing
       ndn::GlobalRoutingHelper ndnGlobalRoutingHelper;
       ndnGlobalRoutingHelper.InstallAll();

       // Choosing forwarding strategy
       // We're doing multicast strategy for all nodes except the two routers, which get qos
       ndn::StrategyChoiceHelper::InstallAll("/prefix", "/localhost/nfd/strategy/multicast");
       ndn::StrategyChoiceHelper::Install(Names::Find<Node>("Rtr1"), "/prefix", "/localhost/nfd/strategy/qos");
       ndn::StrategyChoiceHelper::Install(Names::Find<Node>("Rtr2"), "/prefix", "/localhost/nfd/strategy/qos");

       // Installing applications

       // Consumer
       // There will be two types of interests: high priority (typeI) and low priority (typeII)
       ndn::AppHelper consumerHelper("ns3::ndn::ConsumerCbr");
       // High priority Consumer 
       consumerHelper.SetPrefix("/prefix/typeI");
       consumerHelper.SetAttribute("Frequency", StringValue("50")); // 25 interests a second
       auto apps = consumerHelper.Install(Names::Find<Node>("Src1"));
       apps = consumerHelper.Install(Names::Find<Node>("Src3"));

       // Low priority Consumer 
       consumerHelper.SetPrefix("/prefix/typeII");
       consumerHelper.SetAttribute("Frequency", StringValue("100")); // 50 interests a second
       apps = consumerHelper.Install(Names::Find<Node>("Src2"));
       apps = consumerHelper.Install(Names::Find<Node>("Src4"));

       //Set token bucket values
       ndn::AppHelper tokenHelper("ns3::ndn::TokenBucketDriver");
       tokenHelper.SetAttribute("FillRate1", StringValue("80")); // Calculated max of 120 packets per second on main route times 2/3
       tokenHelper.SetAttribute("Capacity1", StringValue("1000"));
       tokenHelper.SetAttribute("FillRate2", StringValue("40")); // Calculated max of 120 packets per second on main route times 1/3
       tokenHelper.SetAttribute("Capacity2", StringValue("1000"));
       tokenHelper.SetAttribute("FillRate3", StringValue("10"));
       tokenHelper.SetAttribute("Capacity3", StringValue("10"));
       //consumerHelper.Install(nodes.Get(0));                        // first node
       apps = tokenHelper.Install(Names::Find<Node>("Rtr1"));
       apps = tokenHelper.Install(Names::Find<Node>("Rtr2"));

       // Producer
       // There will be four producers all for serving /prefix requests
       ndn::AppHelper producerHelper("ns3::ndn::Producer");
       producerHelper.SetPrefix("/prefix/");
       producerHelper.SetAttribute("PayloadSize", StringValue("1024"));
       producerHelper.Install(Names::Find<Node>("Dst1"));
       producerHelper.Install(Names::Find<Node>("Dst2"));
       producerHelper.Install(Names::Find<Node>("Dst3"));
       producerHelper.Install(Names::Find<Node>("Dst4"));

       ndnGlobalRoutingHelper.AddOrigin("/prefix", Names::Find<Node>("Dst1"));
       ndnGlobalRoutingHelper.AddOrigin("/prefix", Names::Find<Node>("Dst2"));
       ndnGlobalRoutingHelper.AddOrigin("/prefix", Names::Find<Node>("Dst3"));
       ndnGlobalRoutingHelper.AddOrigin("/prefix", Names::Find<Node>("Dst4"));
       ndn::GlobalRoutingHelper::CalculateAllPossibleRoutes();

       // priority queue drop trace
       FILE *f = fopen("priority-queue-drop-trace.txt", "w");
       fprintf(f, "Time\tNode\tDrop1\tDrop2\n");
       fclose(f);

       Simulator::Stop(Seconds(50.0));
       // Record the round-trip delay between requests sent and responses received
       ndn::AppDelayTracer::InstallAll("app-delays-trace.txt");
       // Record the number of packets dropped every half second
       L2RateTracer::InstallAll("drop-trace.txt", Seconds(1.0));
       // Record the number of interests and data being produced
       ndn::L3RateTracer::InstallAll("rate-trace.txt", Seconds(1.0));
       Simulator::Run();
       Simulator::Destroy();

       return 0;
   }

} // namespace ns3

    int
main(int argc, char* argv[])
{
    return ns3::main(argc, argv);
}
