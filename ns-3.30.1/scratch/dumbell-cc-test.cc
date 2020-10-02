/* -*- Mode:C++; c-file-style:"gnu"; indent-tabs-mode:nil; -*- */
/*
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation;
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 */



/*
	LAB Assignment #2
    1. Create a simple dumbbell topology, two client Node1 and Node2 on
    the left side of the dumbbell and server nodes Node3 and Node4 on the
    right side of the dumbbell. Let Node5 and Node6 form the bridge of the
    dumbbell. Use point to point links.

    2. Install a TCP socket instance on Node1 that will connect to Node3.

    3. Install a UDP socket instance on Node2 that will connect to Node4.

    4. Start the TCP application at time 1s.

    5. Start the UDP application at time 20s at rate Rate1 such that it clogs
    half the dumbbell bridge's link capacity.

    6. Increase the UDP application's rate at time 30s to rate Rate2
    such that it clogs the whole of the dumbbell bridge's capacity.

    7. Use the ns-3 tracing mechanism to record changes in congestion window
    size of the TCP instance over time. Use gnuplot/matplotlib to visualise plots of cwnd vs time.

    8. Mark points of fast recovery and slow start in the graphs.

    9. Perform the above experiment for TCP variants Tahoe, Reno and New Reno,
    all of which are available with ns-3.

	Solution by: Konstantinos Katsaros (K.Katsaros@surrey.ac.uk)
	based on fifth.cc
*/

// Network topology
//
//       node0 ---+      +--- node2
//                |      |
//                r0 -- r1
//                |      |
//       node1 ---+      +--- node3
//
// - All links are P2P with 500kb/s and 2ms
// - TCP flow form node0 to node2
// - UDP flow from node1 to node3

#include <fstream>
#include <string>
#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/applications-module.h"
#include "ns3/internet-module.h"
#include "ns3/flow-monitor-module.h"
#include "ns3/ipv4-global-routing-helper.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE ("Lab2");

class MyApp : public Application
{
public:

  MyApp ();
  virtual ~MyApp();

  void Setup (Ptr<Socket> socket, Address address, uint32_t packetSize, uint32_t nPackets, DataRate dataRate);
  void ChangeRate(DataRate newrate);

private:
  virtual void StartApplication (void);
  virtual void StopApplication (void);

  void ScheduleTx (void);
  void SendPacket (void);

  Ptr<Socket>     m_socket;
  Address         m_peer;
  uint32_t        m_packetSize;
  uint32_t        m_nPackets;
  DataRate        m_dataRate;
  EventId         m_sendEvent;
  bool            m_running;
  uint32_t        m_packetsSent;
};

MyApp::MyApp ()
  : m_socket (0),
    m_peer (),
    m_packetSize (0),
    m_nPackets (0),
    m_dataRate (0),
    m_sendEvent (),
    m_running (false),
    m_packetsSent (0)
{
}

MyApp::~MyApp()
{
  m_socket = 0;
}

void
MyApp::Setup (Ptr<Socket> socket, Address address, uint32_t packetSize, uint32_t nPackets, DataRate dataRate)
{
  m_socket = socket;
  m_peer = address;
  m_packetSize = packetSize;
  m_nPackets = nPackets;
  m_dataRate = dataRate;
}

void
MyApp::StartApplication (void)
{
  m_running = true;
  m_packetsSent = 0;
  m_socket->Bind ();
  m_socket->Connect (m_peer);
  SendPacket ();
}

void
MyApp::StopApplication (void)
{
  m_running = false;

  if (m_sendEvent.IsRunning ())
    {
      Simulator::Cancel (m_sendEvent);
    }

  if (m_socket)
    {
      m_socket->Close ();
    }
}

void
MyApp::SendPacket (void)
{
  Ptr<Packet> packet = Create<Packet> (m_packetSize);
  m_socket->Send (packet);

  if (++m_packetsSent < m_nPackets)
    {
      ScheduleTx ();
    }
}

void
MyApp::ScheduleTx (void)
{
  if (m_running)
    {
      Time tNext (Seconds (m_packetSize * 8 / static_cast<double> (m_dataRate.GetBitRate ())));
      m_sendEvent = Simulator::Schedule (tNext, &MyApp::SendPacket, this);
    }
}

void
MyApp::ChangeRate(DataRate newrate)
{
   m_dataRate = newrate;
   return;
}

static void
CwndChange (uint32_t oldCwnd, uint32_t newCwnd)
{
  std::cout << Simulator::Now ().GetSeconds () << "\t" << newCwnd <<"\n";
}

void
IncRate (Ptr<MyApp> app, DataRate rate)
{
	app->ChangeRate(rate);
    return;
}

static void
RxDrop (Ptr<const Packet> p)
{
  NS_LOG_UNCOND ("RxDrop at " << Simulator::Now ().GetSeconds ());
}


int main (int argc, char *argv[])
{
  std::string inet_bw = "100Mbps";
  std::string inet_delay = "5ms";
  std::string dc_bw = "100Mbps";
  std::string dc_delay = "1ms";
  std::string bnck_bw = "10Mbps";
  std::string bnck_delay = "1ms";
  //std::string lat = "2ms";
  //std::string rate = "500kb/s"; // P2P link
  bool enableFlowMonitor = false;
  //bool tracing = true;
  float simDuration = 5.0;

  uint32_t nb_servers = 4;
  uint32_t nb_clients = 4;
  uint32_t flowPacketSize = 1440;


  CommandLine cmd;
  cmd.AddValue ("latency", "bottleneck link Latency in miliseconds", bnck_delay);
  cmd.AddValue ("rate", "bottleneck link data rate in Mbps", bnck_bw);
  cmd.AddValue ("EnableMonitor", "Enable Flow Monitor", enableFlowMonitor);
  cmd.AddValue ("nb_clients", "Number of clients sending requests", nb_clients);
  cmd.AddValue ("nc", "Number of clients sending requests", nb_clients);

  cmd.Parse (argc, argv);

//
// Create the nodes required by the topology (shown above).
//

  NodeContainer routers;
    routers.Create(2);
  NodeContainer left_nodes;
    left_nodes.Create(2 * nb_clients + 1);
  NodeContainer right_nodes;
    right_nodes.Create(2 * nb_servers + 1);


  /*NS_LOG_INFO ("Create nodes.");
  NodeContainer c; // ALL Nodes
  c.Create(6);

  NodeContainer node0r0 = NodeContainer (c.Get (0), c.Get (4));
  NodeContainer node1r0 = NodeContainer (c.Get (1), c.Get (4));
  NodeContainer node2r1 = NodeContainer (c.Get (2), c.Get (5));
  NodeContainer node3r1 = NodeContainer (c.Get (3), c.Get (5));
  NodeContainer r0r1 = NodeContainer (c.Get (4), c.Get (5));

//
// Install Internet Stack
//
  InternetStackHelper internet;
  internet.Install (c);

  // We create the channels first without any IP addressing information
  NS_LOG_INFO ("Create channels.");
  PointToPointHelper p2p;
  p2p.SetDeviceAttribute ("DataRate", StringValue (rate));
  p2p.SetChannelAttribute ("Delay", StringValue (lat));
  NetDeviceContainer d0d4 = p2p.Install (node0r0);
  NetDeviceContainer d1d4 = p2p.Install (node1r0);
  NetDeviceContainer d4d5 = p2p.Install (r0r1);
  NetDeviceContainer d2d5 = p2p.Install (node2r1);
  NetDeviceContainer d3d5 = p2p.Install (node3r1);

    // Later, we add IP addresses.
  NS_LOG_INFO ("Assign IP Addresses.");
  Ipv4AddressHelper ipv4;
  ipv4.SetBase ("10.1.1.0", "255.255.255.0");
  Ipv4InterfaceContainer i0i4 = ipv4.Assign (d0d4);

  ipv4.SetBase ("10.1.2.0", "255.255.255.0");
  Ipv4InterfaceContainer i1i4 = ipv4.Assign (d1d4);

  ipv4.SetBase ("10.1.3.0", "255.255.255.0");
  Ipv4InterfaceContainer i4i5 = ipv4.Assign (d4d5);

  ipv4.SetBase ("10.1.4.0", "255.255.255.0");
  Ipv4InterfaceContainer i2i5 = ipv4.Assign (d2d5);

  ipv4.SetBase ("10.1.5.0", "255.255.255.0");
  Ipv4InterfaceContainer i3i5 = ipv4.Assign (d3d5);*/

    // Create and configure access link and bottleneck link
    PointToPointHelper inetLink;
      inetLink.SetDeviceAttribute ("DataRate", StringValue (inet_bw));
      inetLink.SetChannelAttribute ("Delay", StringValue (inet_delay));

    PointToPointHelper dcLink;
      dcLink.SetDeviceAttribute ("DataRate", StringValue (dc_bw));
      dcLink.SetChannelAttribute ("Delay", StringValue (dc_delay));

    PointToPointHelper bnckLink;
      bnckLink.SetDeviceAttribute ("DataRate", StringValue (bnck_bw));
      bnckLink.SetChannelAttribute ("Delay", StringValue (bnck_delay));

    InternetStackHelper stack;
      stack.Install(routers);
      stack.Install(left_nodes);
      stack.Install(right_nodes);

  NS_LOG_INFO ("Enable static global routing.");
  //
  // Turn on global static routing so we can actually be routed across the network.
  //
  Ipv4GlobalRoutingHelper::PopulateRoutingTables ();

  //========================== Internet Links ==========================

  Config::SetDefault ("ns3::QueueBase::MaxSize", StringValue ("1000p"));

    NetDeviceContainer leftrouterdevices;
    NetDeviceContainer left_nodedevices;
    NetDeviceContainer rightrouterdevices;
    NetDeviceContainer right_nodedevices;

    // add the left side links
    for (uint32_t i = 0; i<(2 * nb_clients + 1); ++i) {
        NetDeviceContainer cleft = inetLink.Install(routers.Get(0), left_nodes.Get(i));
          leftrouterdevices.Add(cleft.Get(0));
          left_nodedevices.Add(cleft.Get(1));
    }

  //========================== Datacenter Links ==========================

    // add the right side links
    for (uint32_t i = 0; i<(nb_servers + 1); ++i) {
        NetDeviceContainer cright = dcLink.Install(routers.Get(2), right_nodes.Get(i));
          rightrouterdevices.Add(cright.Get(0));
          right_nodedevices.Add(cright.Get(1));
    }

  //========================== Bottleneck Link =============================

  //Config::SetDefault ("ns3::QueueBase::MaxSize", StringValue (std::to_string (netdevicesQueueSize) + "p"));
  //Defining two containers of bottlneck devices
  NetDeviceContainer devicesInetLink = inetLink.Install (routers.Get(0), routers.Get(1));
  //tchPfifoFastAccess.Install(devicesInetLink);
  NetDeviceContainer devicesBnckLink = bnckLink.Install (routers.Get(1), routers.Get(2));

  // The devices connected to both bottlenecks
  Ptr<NetDevice> right_router = devicesBnckLink.Get(1);
  Ptr<NetDevice> midright_router = devicesBnckLink.Get(0);
  Ptr<NetDevice> midleft_router = devicesInetLink.Get(1);
  Ptr<NetDevice> left_router = devicesInetLink.Get(0);

  /*Ptr<Queue<Packet> > queueHelp_right = StaticCast<PointToPointNetDevice> (right_router)->GetQueue ();
  queueHelp_right->TraceConnectWithoutContext ("PacketsInQueue", MakeCallback (&DevicePacketsInQueueTrace));
  Ptr<Queue<Packet> > queueHelp_midright = StaticCast<PointToPointNetDevice> (midright_router)->GetQueue ();
  queueHelp_midright->TraceConnectWithoutContext ("PacketsInQueue", MakeCallback (&DevicePacketsInQueueTrace));*/

  devicesBnckLink.Get (0) ->TraceConnectWithoutContext ("PhyRxDrop", MakeCallback (&RxDrop));
  devicesBnckLink.Get (1) ->TraceConnectWithoutContext ("PhyRxDrop", MakeCallback (&RxDrop));

  NS_LOG_INFO ("Create Applications.");

  AsciiTraceHelper ascii;
  /*qdiscs.Get(0)->TraceConnectWithoutContext ("PacketsInQueue", MakeCallback (&PacketsInQueueTrace));
  qdiscs.Get(1)->TraceConnectWithoutContext ("PacketsInQueue", MakeCallback (&PacketsInQueueTrace));
  Ptr<OutputStreamWrapper> streamBytesInQdisc1 = ascii.CreateFileStream (outdir + queueDiscType + "-bytesInQdisc1.txt");
  qdiscs.Get(0)->TraceConnectWithoutContext ("BytesInQueue", MakeBoundCallback (&BytesInQueueTraceFile, streamBytesInQdisc1));
  Ptr<OutputStreamWrapper> streamBytesInQdisc2 = ascii.CreateFileStream (outdir + queueDiscType + "-bytesInQdisc2.txt");
  qdiscs.Get(1)->TraceConnectWithoutContext ("BytesInQueue", MakeBoundCallback (&BytesInQueueTraceFile, streamBytesInQdisc2));*/

  // assign ipv4 addresses
  Ipv4AddressHelper routerips = Ipv4AddressHelper("10.3.0.0", "255.255.255.0");
  Ipv4AddressHelper leftips = Ipv4AddressHelper("10.1.0.0", "255.255.255.0");
  Ipv4AddressHelper rightips = Ipv4AddressHelper("10.2.0.0", "255.255.255.0");


  Ipv4InterfaceContainer routerifs_right;
  Ipv4InterfaceContainer routerifs_left;
  Ipv4InterfaceContainer left_nodeifs;
  Ipv4InterfaceContainer leftrouterifs;
  Ipv4InterfaceContainer right_nodeifs;
  Ipv4InterfaceContainer rightrouterifs;

  // assign addresses to connection connecting routers
  routerifs_right = routerips.Assign(devicesBnckLink);
  routerifs_left = routerips.Assign(devicesInetLink);

  // assign addresses to connection between routers and leaves
  for (uint32_t i = 0; i<(2 * nb_clients + 1); ++i)
  {
      // Assign to left side
      NetDeviceContainer ndcleft;
        ndcleft.Add(left_nodedevices.Get(i));
        ndcleft.Add(leftrouterdevices.Get(i));
      Ipv4InterfaceContainer ifcleft = leftips.Assign(ndcleft);
        left_nodeifs.Add(ifcleft.Get(0));
        leftrouterifs.Add(ifcleft.Get(1));
      leftips.NewNetwork();
   }
  for (uint32_t i = 0; i<(nb_servers + 1); ++i)
  {
      // Assign to right side
      NetDeviceContainer ndcright;
        ndcright.Add(right_nodedevices.Get(i));
        ndcright.Add(rightrouterdevices.Get(i));
      Ipv4InterfaceContainer ifcright = rightips.Assign(ndcright);
        right_nodeifs.Add(ifcright.Get(0));
        rightrouterifs.Add(ifcright.Get(1));
      rightips.NewNetwork();
  }

  Ipv4GlobalRoutingHelper::PopulateRoutingTables ();

  Config::SetDefault ("ns3::TcpL4Protocol::SocketType", StringValue ("ns3::TcpNcf"));
  Config::SetDefault ("ns3::TcpSocket::SegmentSize", UintegerValue (flowPacketSize));
  //  Config::SetDefault ("ns3::TcpSocketBase::Sack", StringValue ("0"));
  //  Config::SetDefault ("ns3::TcpSocketBase::Timestamp", StringValue ("0"));
  Config::SetDefault ("ns3::TcpSocketBase::EcnMode", StringValue ("ClassicEcn"));

  //=================  Flows configuration ================================

                   //------ S E R V E R S --------//

  // Set server's IP addresses
  Ipv4Address serverAddresses[nb_servers];
  for (uint32_t i = 0; i<nb_servers; ++i)
  {
    serverAddresses[i] = right_nodeifs.GetAddress (i+1);
  }


/*
  // TCP connfection from node0 to node2

  uint16_t sinkPort = 8080;
  Address sinkAddress (InetSocketAddress (i2i5.GetAddress (0), sinkPort)); // interface of node2
  PacketSinkHelper packetSinkHelper ("ns3::TcpSocketFactory", InetSocketAddress (Ipv4Address::GetAny (), sinkPort));
  ApplicationContainer sinkApps = packetSinkHelper.Install (c.Get (2)); //node2 as sink
  sinkApps.Start (Seconds (0.));
  sinkApps.Stop (Seconds (100.));*/

  Ptr<Socket> ns3TcpSocket = Socket::CreateSocket (left_nodes.Get (0), TcpSocketFactory::GetTypeId ()); //source at node0

  // Trace Congestion window
  ns3TcpSocket->TraceConnectWithoutContext ("CongestionWindow", MakeCallback (&CwndChange));

  // Create TCP application at node0
  /*Ptr<MyApp> app = CreateObject<MyApp> ();
  app->Setup (ns3TcpSocket, sinkAddress, 1040, 100000, DataRate ("250Kbps"));
  c.Get (0)->AddApplication (app);
  app->SetStartTime (Seconds (1.));
  app->SetStopTime (Seconds (simDuration));*/


  // UDP connection from node1 to node3

  /*uint16_t sinkPort2 = 6;
  Address sinkAddress2 (InetSocketAddress (i3i5.GetAddress (0), sinkPort2)); // interface of node3
  PacketSinkHelper packetSinkHelper2 ("ns3::UdpSocketFactory", InetSocketAddress (Ipv4Address::GetAny (), sinkPort2));
  ApplicationContainer sinkApps2 = packetSinkHelper2.Install (c.Get (3)); //node3 as sink
  sinkApps2.Start (Seconds (0.));
  sinkApps2.Stop (Seconds (simDuration));

  Ptr<Socket> ns3UdpSocket = Socket::CreateSocket (c.Get (1), UdpSocketFactory::GetTypeId ()); //source at node1

  // Create UDP application at node1
  Ptr<MyApp> app2 = CreateObject<MyApp> ();
  app2->Setup (ns3UdpSocket, sinkAddress2, 1040, 100000, DataRate ("250Kbps"));
  c.Get (1)->AddApplication (app2);
  app2->SetStartTime (Seconds (20.));
  app2->SetStopTime (Seconds (simDuration));

// Increase UDP Rate
  Simulator::Schedule (Seconds(30.0), &IncRate, app2, DataRate("500kbps"));*/

  // Flow Monitor
  Ptr<FlowMonitor> flowmon;
  if (enableFlowMonitor)
    {
      FlowMonitorHelper flowmonHelper;
      flowmon = flowmonHelper.InstallAll ();
    }

 /* if (tracing)
    {
      AsciiTraceHelper ascii;
      p2p.EnableAsciiAll (ascii.CreateFileStream ("cc-dumbbell.tr"));
      p2p.EnablePcapAll ("cc-dumbbell", false);
    }*/

//
// Now, do the actual simulation.
//
  NS_LOG_INFO ("Run Simulation.");
  Simulator::Stop (Seconds(simDuration));
  Simulator::Run ();
  if (enableFlowMonitor)
    {
	  flowmon->CheckForLostPackets ();
          flowmon->SerializeToXmlFile("cc-dumbbell.flowmon", true, true);
    }

  Simulator::Destroy ();
  NS_LOG_INFO ("Done.");
}
