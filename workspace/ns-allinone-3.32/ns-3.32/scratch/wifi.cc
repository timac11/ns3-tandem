#include "ns3/command-line.h"
#include "ns3/config.h"
#include "ns3/uinteger.h"
#include "ns3/boolean.h"
#include "ns3/config.h"
#include "ns3/integer.h"
#include "ns3/string.h"
#include "ns3/yans-wifi-helper.h"
#include "ns3/internet-stack-helper.h"
#include "ns3/ipv4-address-helper.h"
#include "ns3/udp-echo-helper.h"
#include "ns3/yans-wifi-channel.h"
#include "ns3/constant-position-mobility-model.h"
#include "ns3/propagation-loss-model.h"
#include "ns3/propagation-delay-model.h"
#include "ns3/on-off-helper.h"
#include "ns3/flow-monitor-helper.h"
#include "ns3/ipv4-flow-classifier.h"
#include "ns3/aodv-module.h"
#include "ns3/poisson-generator.h"
#include "ns3/mobility-helper.h"
#include "ns3/random-variable-stream.h"
#include "ns3/rng-seed-manager.h"
#include "ns3/wifi-module.h"
#include "ns3/packet-socket-helper.h"
#include "ns3/packet-socket-address.h"
#include <ctime>
#include <chrono>

/**
 * Topology: [node 0] <-- -50 dB --> [node 1]<-- -50db - --> .... <-- -50 dB --> [node stationsNumber - 1]
 */

using namespace ns3;

int placements[5] = {10, 40, 100, 140, 300};
int p_tr[5] = {-67, -67, -67, -67, -69};
int g_tr[5] = {5, 5, 5, 5, 5};

// STATION STATISTIC IMPLEMENTATION ///////////////////////////////////

class WifiStationStatistic
{
private:
    Ptr<NetDevice> m_device;
    Ptr<const Packet> currentPacket;
    uint32_t count;
    uint32_t ackCount;
    uint64_t currentId;
    bool requested;
    Time requestedTime;
    void requestAccessCallback(const WifiMacQueueItem & item);
    void recieveMessage(Ptr<const Packet> item);
    void recieveMessageHeader(const WifiMacHeader & item);
    std::vector<Time> intervals;
    std::string fileName;
public:
    void writeIntervals();
    WifiStationStatistic(Ptr<NetDevice> m_device, std::string fileName);
};

WifiStationStatistic::WifiStationStatistic (Ptr<NetDevice> m_device, std::string fileName)
{
    this->m_device = m_device;
    this->requested = false;
    this->fileName = fileName;
    this->currentPacket = NULL;
    this->count = 0;
    this->ackCount = 0;

    Ptr<WifiNetDevice> wifi_dev = DynamicCast<WifiNetDevice> (m_device);
    Ptr<RegularWifiMac> rmac = DynamicCast<RegularWifiMac> (wifi_dev->GetMac());
    
    rmac->TraceConnectWithoutContext("TxOkHeader", MakeCallback (& WifiStationStatistic::recieveMessageHeader, this));
    rmac->TraceConnectWithoutContext("TxOkCallback", MakeCallback (& WifiStationStatistic::recieveMessage, this));
    rmac->TraceConnectWithoutContext("RequestChannelPacket", MakeCallback(& WifiStationStatistic::requestAccessCallback, this));
} 

void
WifiStationStatistic::requestAccessCallback(const WifiMacQueueItem & item) 
{
    if (!requested && item.GetPacket()->GetSize() == 1500) {
        currentId = item.GetPacket()->GetUid();
        this->count ++;
        requestedTime = Simulator::Now();
        requested = true;
    }
}

void
WifiStationStatistic::recieveMessage(Ptr<const Packet> item) 
{ 
    
}

void
WifiStationStatistic::recieveMessageHeader(const WifiMacHeader & item) 
{ 
    if (requested) {
        this->ackCount ++;
        requested = false;
        intervals.push_back(Simulator::Now() - requestedTime);
    }
}

void 
WifiStationStatistic::writeIntervals() 
{
    std::ofstream file;
	file.open(fileName);
    file << "[";
	for(long unsigned int i = 0; i < intervals.size(); ++i) {
		file << intervals[i].GetSeconds();
        if (i != intervals.size() - 1) {
            file << ",";
        }
	}
    file << "]";
	file.close();
    std::cout << fileName << " : " << this->ackCount << std::endl;
    std::cout << fileName << " : " << this->count << std::endl;
}
////////////////////////////////////////////////////////////////////////////////////

void
experiment(uint32_t stationsNumber, uint32_t simulationTime, std::string filePrefix,  double delay) {
    RngSeedManager::SetSeed(time(0));

    // Nodes creation
    NodeContainer nodes;
    nodes.Create(stationsNumber);

    // Stations placement
    MobilityHelper mobility;
    Ptr <ListPositionAllocator> positionAlloc = CreateObject<ListPositionAllocator>();
    mobility.SetMobilityModel("ns3::ConstantPositionMobilityModel");

    for (uint32_t i = 0; i < stationsNumber; i++) {
        positionAlloc->Add(Vector(placements[i], 0.0, 0.0));
    }

    mobility.SetPositionAllocator(positionAlloc);
    mobility.Install(nodes);

    // Channels matrix

    Ptr <MatrixPropagationLossModel> lossModel = CreateObject<MatrixPropagationLossModel>();
    //lossModel->SetDefaultLoss(50000000);

    for (uint32_t i = 0; i < stationsNumber - 1; i++) {
      lossModel->SetLoss(nodes.Get(i)->GetObject<MobilityModel>(), nodes.Get(i + 1)->GetObject<MobilityModel>(), 1, true);
    }

    Ptr <YansWifiChannel> wifiChannel = CreateObject<YansWifiChannel>();
    wifiChannel->SetPropagationLossModel(lossModel);
    wifiChannel->SetPropagationDelayModel(CreateObject<ConstantSpeedPropagationDelayModel>());

    // Wi-Fi configuration
    WifiHelper wifi;
    wifi.SetRemoteStationManager("ns3::IdealWifiManager");
    wifi.SetStandard(WIFI_STANDARD_80211n_5GHZ);
    YansWifiPhyHelper wifiPhy = YansWifiPhyHelper::Default();

    wifiPhy.SetChannel(wifiChannel);
    WifiMacHelper wifiMac;
    Ssid ssid = Ssid ("network-A");
    wifiMac.SetType ("ns3::AdhocWifiMac",
                    "QosSupported", BooleanValue (false),
                    "Ssid", SsidValue (ssid));

    NetDeviceContainer devices = wifi.Install(wifiPhy, wifiMac, nodes);

    // TCP-IP
    AodvHelper aodv;
    InternetStackHelper internet;
    internet.SetRoutingHelper(aodv);
    
    internet.Install(nodes);
    Ipv4AddressHelper ipv4;
    ipv4.SetBase("10.0.0.0", "255.0.0.0");
    Ipv4InterfaceContainer wifiInterfaces;
    wifiInterfaces = ipv4.Assign(devices);
 
    Ptr<OutputStreamWrapper> routingStream = Create<OutputStreamWrapper> ("aodv.routes", std::ios::out);
    aodv.PrintRoutingTableAllAt (Seconds (8), routingStream);

    std::vector<WifiStationStatistic*> statistics;

    for (uint32_t i = 0; i < stationsNumber - 1; ++i) {
        statistics.push_back(new WifiStationStatistic(devices.Get (i), filePrefix + std::to_string(i) + ".json"));
    }

    // PacketSocketAddress socket;
    // socket.SetSingleDevice (devices.Get (0)->GetIfIndex ());
    // socket.SetPhysicalAddress (devices.Get (1)->GetAddress ());
    // socket.SetProtocol (1);

    // OnOffHelper onoff ("ns3::PacketSocketFactory", Address (socket));
    // onoff.SetConstantRate (DataRate ("500kb/s"));

    // ApplicationContainer apps = onoff.Install (nodes.Get (0));
    // apps.Start (Seconds (2.0));
    // apps.Stop (Seconds (43.0));

    uint32_t payloadSize = 964 + 500; //bytes
    uint16_t port = 5000;
    
    InetSocketAddress destA(wifiInterfaces.GetAddress(stationsNumber - 1), port);
    PoissonAppHelper app("ns3::UdpSocketFactory", destA, delay, payloadSize);
    
    // cross traffic
    // for (uint32_t i = 0; i < stationsNumber - 1; ++i) {
    //     ApplicationContainer clientAppA = app.Install(nodes.Get(i));
    //     clientAppA.Start(Seconds(1.0));
    //     clientAppA.Stop(Seconds(simulationTime));
    // }

    ApplicationContainer clientAppA = app.Install(nodes.Get(0));
    clientAppA.Start(Seconds(1.0));
    clientAppA.Stop(Seconds(simulationTime));

    // FlowMonitor
    FlowMonitorHelper flowmon;
    Ptr <FlowMonitor> monitor = flowmon.InstallAll();
    internet.EnablePcapIpv4All("wifi-aodv.tr");

    Simulator::Stop(Seconds(simulationTime));
    auto start = std::chrono::system_clock::now();
    Simulator::Run();

    // // 10. Print per flow statistics
    monitor->CheckForLostPackets();
    Ptr <Ipv4FlowClassifier> classifier = DynamicCast<Ipv4FlowClassifier>(flowmon.GetClassifier());
    FlowMonitor::FlowStatsContainer stats = monitor->GetFlowStats();
    
    std::cout << "---------------------------------------------" << std::endl;
    std::cout << stats.size() << std::endl;

    for (std::map<FlowId, FlowMonitor::FlowStats>::const_iterator i = stats.begin(); i != stats.end(); ++i) {
        Ipv4FlowClassifier::FiveTuple t = classifier->FindFlow(i->first);
        if (t.sourceAddress == "10.0.0.1" && t.destinationAddress == "10.0.0.4") {
            std::cout << "Flow " << i->first << " (" << t.sourceAddress << " -> " << t.destinationAddress << ")\n";
            std::cout << "Tx Packets: " << i->second.txPackets << std::endl;
            std::cout << "Tx Bytes:   " << i->second.txBytes << std::endl;
            std::cout << "Rx Packets: " << i->second.rxPackets << std::endl;
            std::cout << "Delay: " << i->second.delaySum.GetSeconds() / i->second.rxPackets << std::endl;
            std::cout << "Rx Bytes:   " << i->second.rxBytes << std::endl;
            std::cout << "Throughput: " << i->second.rxBytes * 8.0 / simulationTime / 1000 / 1000 << " Mbps" << std::endl;;
            std::cout << "Lost Packets: " << i->second.lostPackets << std::endl;
            std::cout << "Lost Probability: " << i->second.lostPackets * 1.0 / i->second.rxPackets << std::endl;
            std::cout << "---------------------------------------------" << std::endl;
        }
    }

    auto end = std::chrono::system_clock::now();
    std::chrono::duration<double> elapsed_seconds = end - start;

    std::cout << "elapsed time: " << elapsed_seconds.count() << "s\n";
    std::cout << "---------------------------------------------" << std::endl;

    for(long unsigned int i = 0; i < statistics.size(); ++i) {
		statistics[i]->writeIntervals();
	}

    Simulator::Destroy();
}

int
main(int argc, char **argv) {
    std::string wifiManager("Aarf");
    CommandLine cmd;
    cmd.AddValue(
            "wifiManager",
            "Set wifi rate manager (Aarf, Aarfcd, Amrr, Arf, Cara, Ideal, Minstrel, Onoe, Rraa)",
            wifiManager);
    cmd.Parse(argc, argv);

    // experiment(4, 25, "hui", 0.01);

    // std::cout << "lambda = 100" << std::endl;
    
    // for (int i = 0; i < 10; i ++) {
    //     std::cout << "experment # " + std::to_string(i) << std::endl;
    //     experiment(4, 1000, std::to_string(i) + "_lambda=100_250000_packets_count" + "_experiment_802_11_N_", 0.01);
    // }

    // std::cout << "lambda = 50" << std::endl;


    // for (int i = 0; i < 10; i ++) {
    //     std::cout << "experment # " + std::to_string(i) << std::endl;
    //     experiment(4, 1000, std::to_string(i) + "_lambda=50_250000_packets_count" + "_experiment_802_11_N_", 0.02);
    // }

    std::cout << "lambda = 200" << std::endl;


    for (int i = 3; i < 10; i ++) {
        std::cout << "experment # " + std::to_string(i) << std::endl;
        experiment(4, 1000, std::to_string(i) + "_lambda=200_250000_packets_count" + "_experiment_802_11_N_", 0.005);
    }

    std::cout << "lambda = 500" << std::endl;


    for (int i = 0; i < 10; i ++) {
        std::cout << "experment # " + std::to_string(i) << std::endl;
        experiment(4, 1000, std::to_string(i) + "_lambda=500_250000_packets_count" + "_experiment_802_11_N_", 0.002);
    }

    return 0;
}
