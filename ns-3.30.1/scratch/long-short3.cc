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

#include <fstream>
#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/applications-module.h"
#include "ns3/netanim-module.h"

#include <iostream>
#include <fstream>

using namespace std;

using namespace ns3;

NS_LOG_COMPONENT_DEFINE ("LongFlowShortFlow");

// ===========================================================================
//
// Network topology
//
//       node0 ---+      +--- node2
//                |      |
//                r0 -- r1
//                |      |
//       node1 ---+      +--- node3
//
// - All links are P2P
// - TCP long flow form node0 to node2
// - TCP short flow from node1 to node3
//
//
// We want to look at changes in the ns-3 TCP congestion window.  We need
// to crank up a flow and hook the CongestionWindow attribute on the socket
// of the sender.  Normally one would use an on-off application to generate a
// flow, but this has a couple of problems.  First, the socket of the on-off 
// application is not created until Application Start time, so we wouldn't be 
// able to hook the socket (now) at configuration time.  Second, even if we 
// could arrange a call after start time, the socket is not public so we 
// couldn't get at it.
//
// So, we can cook up a simple version of the on-off application that does what
// we want.  On the plus side we don't need all of the complexity of the on-off
// application.  On the minus side, we don't have a helper, so we have to get
// a little more involved in the details, but this is trivial.
//
// So first, we create a socket and do the trace connect on it; then we pass 
// this socket into the constructor of our simple application which we then 
// install in the source node.
// ===========================================================================
//
class MyApp : public Application 
{
public:

  MyApp ();
  virtual ~MyApp();

  void Setup (Ptr<Socket> socket, Address address, uint32_t packetSize, uint32_t nPackets, DataRate dataRate);

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
 if (m_packetsSent == m_nPackets)
    {
      m_socket->Close ();
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

bool newCwndFile = true;
std::string file_name = "5.newreno-data";

static void
CwndChange (uint32_t oldCwnd, uint32_t newCwnd)
{
  NS_LOG_UNCOND (Simulator::Now ().GetSeconds () << "\t" << newCwnd);

  // Write to a file
  ofstream myfile;
  if (newCwndFile)
    {
      myfile.open (file_name+ "-cwnd.log");
      newCwndFile = false;
    }
  else
    {
       myfile.open (file_name+ "-cwnd.log", ios::out | ios::app);
    }
     myfile << Simulator::Now ().GetSeconds () << " " << newCwnd << "\n";
     myfile.close();
}

/*static void
RxDrop (Ptr<const Packet> p)
{
  NS_LOG_UNCOND ("RxDrop at " << Simulator::Now ().GetSeconds ());
}*/

int 
main (int argc, char *argv[])
{

  std::string delay = "10ms";
  std::string rate = "12Mbps";
  //std::string access_bandwidth = "100Mbps";
  //std::string access_delay = "4ms";
  bool tracing = true;
  //bool sack = true;
  uint32_t PacketSize = 1440;
  uint32_t numPkts = 10;
  std::string queueSize = "20p";
uint32_t initcwnd =10;
  float simDuration = 5.0;

  CommandLine cmd;
  cmd.AddValue ("numPkts", "Number of packets to transmit", numPkts);
  cmd.AddValue ("queueSize", "Queue size in packets", queueSize);
  cmd.AddValue ("delay", "One-way delay in ms", delay);
  cmd.AddValue ("rate", "Link bandwidth in Mbps", rate);
  cmd.Parse (argc, argv);

  Config::SetDefault ("ns3::TcpL4Protocol::SocketType", TypeIdValue (TypeId::LookupByName ("ns3::TcpNewReno")));
  Config::SetDefault ("ns3::TcpSocket::SegmentSize", UintegerValue (PacketSize));
  Config::SetDefault ("ns3::TcpSocket::InitialCwnd", UintegerValue (initcwnd));
  //Config::SetDefault ("ns3::TcpSocketBase::Sack", BooleanValue (sack));
  //Config::SetDefault("ns3::DropTailQueue::MaxPackets", UintegerValue(queueSize));
  //Config::SetDefault ("ns3::QueueBase::MaxSize", StringValue (queueSize));
  //Config::SetDefault ("ns3::TcpSocket::TcpNoDelay", BooleanValue (false));

  //Create 2 senders and attach to router0
  NodeContainer n0r0;
  n0r0.Create(2);
  NodeContainer n1r0;
  n1r0.Add (n0r0.Get(1));
  n1r0.Create(1);

  //Create 2 receivers and attach to router1
  NodeContainer n2r1;
  n2r1.Create(2);
  NodeContainer n3r1;
  n3r1.Add (n2r1.Get(1));
  n3r1.Create(1);

  //Attach router0 to router1
  NodeContainer r0r1;
  r0r1.Add (n1r0.Get(0));
  r0r1.Add (n2r1.Get(1));

  InternetStackHelper stack;
  stack.InstallAll ();

  // Create channel between n0 and r0
  PointToPointHelper p2p_n0r0;
  p2p_n0r0.SetDeviceAttribute ("DataRate", StringValue (rate));
  p2p_n0r0.SetChannelAttribute ("Delay", StringValue ("2ms"));

  // Create channel between n1 and r0
  PointToPointHelper p2p_n1r0;
  p2p_n1r0.SetDeviceAttribute ("DataRate", StringValue (rate));
  p2p_n1r0.SetChannelAttribute ("Delay", StringValue ("1ms"));

  // Create channel between n2 and r1
  PointToPointHelper p2p_n2r1;
  p2p_n2r1.SetDeviceAttribute ("DataRate", StringValue (rate));
  p2p_n2r1.SetChannelAttribute ("Delay", StringValue ("1ms"));

  // Create channel between n3 and r1
  PointToPointHelper p2p_n3r1;
  p2p_n3r1.SetDeviceAttribute ("DataRate", StringValue (rate));
  p2p_n3r1.SetChannelAttribute ("Delay", StringValue ("2ms"));

  // Create channel between r0 and r1
  PointToPointHelper p2p_r0r1;
  p2p_r0r1.SetDeviceAttribute ("DataRate", StringValue (rate));
  p2p_r0r1.SetChannelAttribute ("Delay", StringValue (delay));
  p2p_r0r1.SetQueue ("ns3::DropTailQueue", "MaxSize", StringValue (queueSize));

  NetDeviceContainer dev_n0r0 = p2p_n0r0.Install (n0r0);
  NetDeviceContainer dev_n1r0 = p2p_n1r0.Install (n1r0);
  NetDeviceContainer dev_n2r1 = p2p_n2r1.Install (n2r1);
  NetDeviceContainer dev_n3r1 = p2p_n3r1.Install (n3r1);
  NetDeviceContainer dev_r0r1 = p2p_r0r1.Install (r0r1);

  // Add IP addresses.
  Ipv4AddressHelper ipv4_n0r0;
  ipv4_n0r0.SetBase ("10.0.0.0", "255.255.255.0");
  Ipv4InterfaceContainer ipAddr_n0r0 = ipv4_n0r0.Assign (dev_n0r0);

  Ipv4AddressHelper ipv4_n1r0;
  ipv4_n1r0.SetBase ("10.1.1.0", "255.255.255.0");
  Ipv4InterfaceContainer ipAddr_n1r0 = ipv4_n1r0.Assign (dev_n1r0);

  Ipv4AddressHelper ipv4_n2r1;
  ipv4_n2r1.SetBase ("10.2.2.0", "255.255.255.0");
  Ipv4InterfaceContainer ipAddr_n2r1 = ipv4_n2r1.Assign (dev_n2r1);

  Ipv4AddressHelper ipv4_n3r1;
  ipv4_n3r1.SetBase ("10.3.3.0", "255.255.255.0");
  Ipv4InterfaceContainer ipAddr_n3r1 = ipv4_n3r1.Assign (dev_n3r1);

  Ipv4AddressHelper ipv4_r0r1;
  ipv4_r0r1.SetBase ("10.100.100.0", "255.255.255.0");
  Ipv4InterfaceContainer ipAddr_r0r1 = ipv4_r0r1.Assign (dev_r0r1);

  NS_LOG_INFO ("Enable static global routing.");
  //
  // Turn on global static routing so we can actually be routed across the network.
  //
  Ipv4GlobalRoutingHelper::PopulateRoutingTables ();

  /* Ptr<RateErrorModel> em = CreateObject<RateErrorModel> ();
   em->SetAttribute ("ErrorRate", DoubleValue (0.00001));
   devices.Get (1)->SetAttribute ("ReceiveErrorModel", PointerValue (em));*/

  //================================================

  NS_LOG_INFO ("Create Applications.");

  /*Create app for long flow at node 0(logical node ID) (logical node ID)
  uint16_t port = 8000;
  BulkSendHelper source("ns3::TcpSocketFactory",
                        InetSocketAddress(ipAddr_n2r1.GetAddress(0), port));

  // Set the amount of data to send in bytes (0 for unlimited).
  source.SetAttribute("MaxBytes", UintegerValue(0));
  source.SetAttribute("SendSize", UintegerValue(PacketSize));
  ApplicationContainer apps = source.Install(n0r0.Get(0));
  apps.Start(Seconds (0.1));
  apps.Stop(Seconds(simDuration));

  // Sink for long flow at node 3 (logical node ID)
  PacketSinkHelper sink("ns3::TcpSocketFactory",
                        InetSocketAddress(Ipv4Address::GetAny(), port));
  apps = sink.Install(n2r1.Get(0));
  apps.Start(Seconds(0.1));
  apps.Stop(Seconds(simDuration));*/

  //================================================================================//

  //Create app for short flow at node 2 and receiver at node 5 (logical node IDs)
  uint16_t sinkPort = 9000;
  Address sinkAddress (InetSocketAddress (ipAddr_n3r1.GetAddress (1), sinkPort));
  PacketSinkHelper packetSinkHelper ("ns3::TcpSocketFactory", InetSocketAddress (Ipv4Address::GetAny (), sinkPort));
  ApplicationContainer sinkApps = packetSinkHelper.Install (n3r1.Get (1));
  sinkApps.Start (Seconds (0.));
  sinkApps.Stop (Seconds (simDuration));

  Ptr<Socket> ns3TcpSocket = Socket::CreateSocket (n1r0.Get (1), TcpSocketFactory::GetTypeId ());
  ns3TcpSocket->TraceConnectWithoutContext ("CongestionWindow", MakeCallback (&CwndChange));
  ns3TcpSocket->SetAttribute("InitialCwnd", UintegerValue (initcwnd));

  Ptr<MyApp> app = CreateObject<MyApp> ();
  app->Setup (ns3TcpSocket, sinkAddress, PacketSize, numPkts, DataRate (rate));
  n1r0.Get (1)->AddApplication (app);
  app->SetStartTime (Seconds (1.));
  app->SetStopTime (Seconds (simDuration));

 //================================================================================//

  if (tracing)
    {
      /*AsciiTraceHelper ascii;
      p2p_n0r0.EnableAsciiAll (ascii.CreateFileStream (file_name + "-n0r0.tr"));
      p2p_n0r0.EnablePcapAll (file_name + "n0r0", false);

      p2p_n1r0.EnableAsciiAll (ascii.CreateFileStream (file_name + "-n1r0.tr"));
      p2p_n1r0.EnablePcapAll (file_name + "n1r0", false);

      p2p_n2r1.EnableAsciiAll (ascii.CreateFileStream (file_name + "-n2r1.tr"));
      p2p_n2r1.EnablePcapAll (file_name + "n2r1", false);

      p2p_n3r1.EnableAsciiAll (ascii.CreateFileStream (file_name + "-n3r1.tr"));
      p2p_n3r1.EnablePcapAll (file_name + "n3r1", false);

      p2p_r0r1.EnableAsciiAll (ascii.CreateFileStream (file_name + "-r0r1.tr"));
      p2p_r0r1.EnablePcapAll (file_name + "r0r1", false);*/

      AsciiTraceHelper ascii;
      p2p_r0r1.EnableAsciiAll (ascii.CreateFileStream (file_name+".tr"));
      p2p_r0r1.EnablePcapAll (file_name, false);
    }

  //dev_r0r1.Get (1)->TraceConnectWithoutContext ("PhyRxDrop", MakeCallback (&RxDrop));

  /*AnimationInterface anim(file_name +"-anim.xml");
  anim.SetConstantPosition(n0r0.Get(0),1.0,5.0); //node 0
  anim.SetConstantPosition(n1r0.Get(1),1.0,1.0); //node 2
  anim.SetConstantPosition(n1r0.Get(0),5.0,3.0); //node 1
  anim.SetConstantPosition(n2r1.Get(1),10.0,3.0); //node 4
  anim.SetConstantPosition(n2r1.Get(0),15.0,5.0); //node 3
  anim.SetConstantPosition(n3r1.Get(1),15.0,1.0); //node 5*/

  Simulator::Stop (Seconds (simDuration));
  Simulator::Run ();
  Simulator::Destroy ();

  return 0;
}

