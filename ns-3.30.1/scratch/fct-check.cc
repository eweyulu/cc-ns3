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

// Network topology
//
//       n0-----r0----r1-----n1
//
// - Point-to-point link with indicated one-way BW/delay
// - 1 TCP flow from n0 to n1
// - Bottleneck between r0 --r1
//

#include <iostream>
#include <fstream>
#include <string>
#include <cassert>

#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/applications-module.h"
#include "ns3/flow-monitor-helper.h"
#include "ns3/ipv4-global-routing-helper.h"
#include "ns3/flow-monitor-module.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE ("TCP_FCTScript");

int
main (int argc, char *argv[])
{
    std::string delay = "10ms";
    std::string rate = "12Mbps";
    bool tracing = true;
    //bool sack = true;
    uint32_t PacketSize = 1440;
    uint32_t numPkts = 2;
    uint32_t queueSize = 20;
    uint32_t initcwnd = 10;
    float simDuration = 12.0;
    std::string file_name = "100.newreno-data";

    CommandLine cmd;
    cmd.AddValue ("numPkts", "Number of packets to transmit", numPkts);
    cmd.AddValue ("queueSize", "Queue size in packets", queueSize);
    cmd.AddValue ("delay", "One-way delay in ms", delay);
    cmd.AddValue ("rate", "Link bandwidth in Mbps", rate);
    cmd.Parse (argc, argv);
  
  Time::SetResolution (Time::NS);
 // LogComponentEnable ("TCP_UDPScript", LOG_LEVEL_INFO);
 // LogComponentEnable ("TCP_UDPScript", LOG_LEVEL_INFO);

  // Set a few attributes
  //Config::SetDefault ("ns3::QueueBase::MaxSize", StringValue ("1000p"));
  Config::SetDefault ("ns3::QueueBase::MaxSize", QueueSizeValue (QueueSize (QueueSizeUnit::PACKETS, queueSize)));
  Config::SetDefault ("ns3::TcpSocket::SegmentSize", UintegerValue (PacketSize));

  //Change congestion control variant
   //Config::SetDefault ("ns3::TcpL4Protocol::SocketType", TypeIdValue (TcpCubic::GetTypeId()));
   Config::SetDefault ("ns3::TcpL4Protocol::SocketType", TypeIdValue (TcpNewReno::GetTypeId()));


  // Explicitly create the nodes and routers.
  NodeContainer n0r0;
    n0r0.Create (2);

  /*NodeContainer r0r1;
    r0r1.Add (n0r0.Get(1));
    r0r1.Create(1);
  NodeContainer r1n1;
    r1n1.Add (r0r1.Get(0));
    r1n1.Create(1);*/

  InternetStackHelper stack;
    stack.InstallAll();

  // Create chanel between n0 and n1
  PointToPointHelper p2p;
  p2p.SetDeviceAttribute ("DataRate", StringValue (rate));
  p2p.SetChannelAttribute ("Delay", StringValue (delay));

  NetDeviceContainer dev0 = p2p.Install (n0r0);
  /*NetDeviceContainer dev1 = p2p.Install (r0r1);
  NetDeviceContainer dev2 = p2p.Install (r1n1);*/

  // Add IP addresses.
  Ipv4AddressHelper ipv4;
  ipv4.SetBase ("10.1.1.0", "255.255.255.0");
  Ipv4InterfaceContainer ipAddr = ipv4.Assign (dev0);

  /*ipv4.SetBase ("10.1.2.0", "255.255.255.0");
  Ipv4InterfaceContainer ipAddr = ipv4.Assign (dev1);
  ipv4.SetBase ("10.1.3.0", "255.255.255.0");
  Ipv4InterfaceContainer ipAddr2 = ipv4.Assign (dev2);*/

  NS_LOG_INFO ("Enable static global routing.");
  //
  // Turn on global static routing so we can actually be routed across the network.
  //
  Ipv4GlobalRoutingHelper::PopulateRoutingTables ();

  //================================================

  NS_LOG_INFO ("Create Applications.");

  ApplicationContainer sourceApps;
  ApplicationContainer sinkApps;
  uint16_t port = 10000;
  BulkSendHelper source  ("ns3::TcpSocketFactory",
                          InetSocketAddress (ipAddr.GetAddress (1), port));
  // Set the amount of data to send in bytes.  Zero is unlimited.
  //uint32_t maxBytes = PacketSize * numPkts;
  //source.SetAttribute ("MaxBytes", UintegerValue (10000));
  //source.SetAttribute("SendSize", UintegerValue(PacketSize));
  sourceApps.Add (source.Install (n0r0.Get (0)));

  Ptr<Socket> ns3TcpSocket = Socket::CreateSocket (n0r0.Get (0), TcpSocketFactory::GetTypeId ());
  ns3TcpSocket->SetAttribute("InitialCwnd", UintegerValue (initcwnd));

  PacketSinkHelper sink ("ns3::TcpSocketFactory",
                         InetSocketAddress (Ipv4Address::GetAny (), port));
  sinkApps.Add (sink.Install (n0r0.Get (1)));

  sinkApps.Start (Seconds (0.0));
  sinkApps.Stop (Seconds (simDuration));
  sourceApps.Start (Seconds (0.0));
  sourceApps.Stop (Seconds (simDuration));

  if (tracing)
    {
      AsciiTraceHelper ascii;
      p2p.EnableAsciiAll (ascii.CreateFileStream (file_name + "-fct.tr"));
      p2p.EnablePcapAll (file_name + "-fct", false);
    }

  FlowMonitorHelper flowmon;
  Ptr<FlowMonitor> monitor = flowmon.InstallAll ();

  NS_LOG_INFO ("Run Simulation.");
  Simulator::Stop (Seconds (simDuration));
  Simulator::Run ();

  /*monitor->CheckForLostPackets ();
  Ptr<Ipv4FlowClassifier> classifier = DynamicCast<Ipv4FlowClassifier> (flowmon.GetClassifier ());
  FlowMonitor::FlowStatsContainer stats = monitor->GetFlowStats ();
  for (std::map<FlowId, FlowMonitor::FlowStats>::const_iterator i = stats.begin (); i != stats.end (); ++i)
    {
      Ipv4FlowClassifier::FiveTuple t = classifier->FindFlow (i->first);
      if (t.sourceAddress == "10.1.1.2")
        {
          continue;
        }
      std::cout << "Flow " << i->first  << " (" << t.sourceAddress << " -> " << t.destinationAddress << ")\n";
      std::cout << "  Tx Packets: " << i->second.txPackets << "\n";
      std::cout << "  Tx Bytes:   " << i->second.txBytes << "\n";
      std::cout << "  TxOffered:  " << i->second.txBytes * 8.0 / 9.0 / 1000 / 1000  << " Mbps\n";
      std::cout << "  Rx Packets: " << i->second.rxPackets << "\n";
      std::cout << "  Rx Bytes:   " << i->second.rxBytes << "\n";
      std::cout << "  Throughput: " << i->second.rxBytes * 8.0 / 9.0 / 1000 / 1000  << " Mbps\n";
    }*/

  Simulator::Destroy ();
  NS_LOG_INFO ("Done.");
}
