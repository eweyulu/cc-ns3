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


int main (int argc, char *argv[])
{
  std::string lat = "25ms";
  std::string send_lat = "10ms";
  std::string recv_lat = "0ms";
  std::string rate = "12000kb/s"; // P2P link
  bool enableFlowMonitor = false;
  bool tracing = true;
  float simDuration = 5.0;


  CommandLine cmd;
  cmd.AddValue ("latency", "P2P link Latency in miliseconds", lat);
  cmd.AddValue ("senderLatency", "P2P link Latency in miliseconds", send_lat);
  cmd.AddValue ("receiverLatency", "P2P link Latency in miliseconds", recv_lat);
  cmd.AddValue ("rate", "P2P data rate in bps", rate);
  cmd.AddValue ("EnableMonitor", "Enable Flow Monitor", enableFlowMonitor);

  cmd.Parse (argc, argv);

//
// Explicitly create the nodes required by the topology (shown above).
//
  NS_LOG_INFO ("Create nodes.");
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

  PointToPointHelper p2psend;
  p2psend.SetDeviceAttribute ("DataRate", StringValue (rate));
  p2psend.SetChannelAttribute ("Delay", StringValue (send_lat));

  PointToPointHelper p2precv;
  p2precv.SetDeviceAttribute ("DataRate", StringValue (rate));
  p2precv.SetChannelAttribute ("Delay", StringValue (recv_lat));

  NetDeviceContainer d0d4 = p2psend.Install (node0r0);
  NetDeviceContainer d1d4 = p2psend.Install (node1r0);
  NetDeviceContainer d4d5 = p2p.Install (r0r1);
  NetDeviceContainer d2d5 = p2precv.Install (node2r1);
  NetDeviceContainer d3d5 = p2precv.Install (node3r1);

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
  Ipv4InterfaceContainer i3i5 = ipv4.Assign (d3d5);

  NS_LOG_INFO ("Enable static global routing.");
  //
  // Turn on global static routing so we can actually be routed across the network.
  //
  Ipv4GlobalRoutingHelper::PopulateRoutingTables ();


  NS_LOG_INFO ("Create Applications.");

  // TCP connection from node0 to node2

  uint16_t sinkPort = 8080;
  Address sinkAddress (InetSocketAddress (i2i5.GetAddress (0), sinkPort)); // interface of node2
  PacketSinkHelper packetSinkHelper ("ns3::TcpSocketFactory", InetSocketAddress (Ipv4Address::GetAny (), sinkPort));
  ApplicationContainer sinkApps = packetSinkHelper.Install (c.Get (2)); //node2 as sink
  sinkApps.Start (Seconds (0.));
  sinkApps.Stop (Seconds (simDuration));

  Ptr<Socket> ns3TcpSocket = Socket::CreateSocket (c.Get (0), TcpSocketFactory::GetTypeId ()); //source at node0

  // Trace Congestion window
  ns3TcpSocket->TraceConnectWithoutContext ("CongestionWindow", MakeCallback (&CwndChange));

  // Create TCP application at node0
  Ptr<MyApp> app = CreateObject<MyApp> ();
  app->Setup (ns3TcpSocket, sinkAddress, 100, 1, DataRate (rate));
  c.Get (0)->AddApplication (app);
  app->SetStartTime (Seconds (0.));
  app->SetStopTime (Seconds (simDuration));


  // TCP connection from node1 to node3
  uint16_t sinkPort2 = 6;
  Address sinkAddress2 (InetSocketAddress (i3i5.GetAddress (0), sinkPort2)); // interface of node3
  PacketSinkHelper packetSinkHelper2 ("ns3::TcpSocketFactory", InetSocketAddress (Ipv4Address::GetAny (), sinkPort2));
  ApplicationContainer sinkApps2 = packetSinkHelper2.Install (c.Get (3)); //node3 as sink
  sinkApps2.Start (Seconds (0.));
  sinkApps2.Stop (Seconds (simDuration));

  Ptr<Socket> ns3TcpSocket2 = Socket::CreateSocket (c.Get (1), TcpSocketFactory::GetTypeId ()); //source at node1

  // Create 2nd TCP application at node1
  Ptr<MyApp> app2 = CreateObject<MyApp> ();
  app2->Setup (ns3TcpSocket2, sinkAddress2, 100, 1, DataRate (rate));
  c.Get (1)->AddApplication (app2);
  app2->SetStartTime (Seconds (0.));
  app2->SetStopTime (Seconds (simDuration));


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

  if (tracing)
    {
      AsciiTraceHelper ascii;
      p2p.EnableAsciiAll (ascii.CreateFileStream ("cc-dumbbell.tr"));
      //p2p.EnablePcapAll ("cc-dumbbell", false);
      p2psend.EnablePcapAll("cc-nodes",false);
    }

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
