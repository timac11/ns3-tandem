#include "poisson-generator.h"
#include "ns3/udp-socket-factory.h"
#include "ns3/socket-factory.h"
#include "ns3/packet-socket-address.h"
#include <random>

namespace ns3 {

NS_LOG_COMPONENT_DEFINE ("PoissonGenerator");
NS_OBJECT_ENSURE_REGISTERED (PoissonGenerator);

 
 TypeId 
 PacketIdTag::GetTypeId (void)
 {
   static TypeId tid = TypeId ("ns3::PacketIdTag")
     .SetParent<Tag> ()
     .AddConstructor<PacketIdTag> ()
     .AddAttribute ("Value",
                    "Integer value",
                    EmptyAttributeValue (),
                    MakeUintegerAccessor (&PacketIdTag::GetValue),
                    MakeUintegerChecker<uint8_t> ())             
   ;
   return tid;
 }
 TypeId 
 PacketIdTag::GetInstanceTypeId (void) const
 {
   return GetTypeId ();
 }
 uint32_t 
 PacketIdTag::GetSerializedSize (void) const
 {
   return 1;
 }
 void 
 PacketIdTag::Serialize (TagBuffer i) const
 {
   i.WriteU8 (m_Value);
 }
 void 
 PacketIdTag::Deserialize (TagBuffer i)
 {
   m_Value = i.ReadU8 ();
 }
 void 
 PacketIdTag::Print (std::ostream &os) const
 {
   os << "v=" << (uint8_t)m_Value;
 }
 void 
 PacketIdTag::SetValue (uint8_t value)
 {
   m_Value = value;
 }
 uint8_t 
 PacketIdTag::GetValue (void) const
 {
   return m_Value;
 }

PoissonGenerator::PoissonGenerator() :
    m_delay (1.0),
    m_size (1400)
{
  NS_LOG_FUNCTION (this);
}

PoissonGenerator::~PoissonGenerator()
{
  NS_LOG_FUNCTION (this);
}

void 
PoissonGenerator::SetRemote (TypeId socketType, 
                            Address remote)
{
    std::cout << "Try connect" << std::endl; 
    m_socket = Socket::CreateSocket (GetNode (), socketType);
    int ret = -1;

    if (! m_local.IsInvalid())
      {
        ret = m_socket->Bind (m_local);
      }
    else
      {
        if (Inet6SocketAddress::IsMatchingType (remote))
          {
            ret = m_socket->Bind6 ();
          }
        else if (InetSocketAddress::IsMatchingType (remote) ||
                  PacketSocketAddress::IsMatchingType (remote))
          {
            ret = m_socket->Bind ();
          }
        }

      if (ret == -1)
        {
          NS_FATAL_ERROR ("Failed to bind socket");
        }
    std::cout << ret << std::endl;
    m_socket->SetConnectCallback (
      MakeCallback (&PoissonGenerator::ConnectionSucceeded, this),
      MakeCallback (&PoissonGenerator::ConnectionFailed, this));
    m_socket->Connect (remote);
    m_socket->SetAllowBroadcast (true);
    m_socket->ShutdownRecv ();
  //TypeId tid = socketType;
  //m_socket = Socket::CreateSocket (GetNode (), tid);
  //m_socket->Bind (m_local);
  //m_socket->SetConnectCallback (
  //      MakeCallback (&PoissonGenerator::ConnectionSucceeded, this),
  //      MakeCallback (&PoissonGenerator::ConnectionFailed, this));
  //m_socket->SetAllowBroadcast (true);
  //m_socket->Connect (remote);
  //m_socket->ShutdownRecv ();
  //std::cout << "try connect" << std::endl;
}


void
PoissonGenerator::CancelEvents (void) {
    Simulator::Cancel (m_sendEvent);
}

void
PoissonGenerator::DoGenerate (void)
{
    Ptr<ExponentialRandomVariable> y = CreateObject<ExponentialRandomVariable> ();
    y->SetAttribute("Mean", DoubleValue(m_delay));
    //std::cout << y->GetValue() << std::endl;
    m_sendEvent = Simulator::Schedule (Seconds (y->GetValue()), &PoissonGenerator::DoGenerate, this);
    Ptr<Packet> p = Create<Packet> (reinterpret_cast<const uint8_t*> ("hello") ,m_size);
    m_socket->Send(p);
    std::cout << p->ToString();
}
void PoissonGenerator::ConnectionSucceeded (Ptr<Socket> socket)
{
  //NS_LOG_FUNCTION (this << socket);
  std::cout << "Connected!" << std::endl;
  m_connected = true;
}

void PoissonGenerator::ConnectionFailed (Ptr<Socket> socket)
{
  //NS_LOG_FUNCTION (this << socket);
  //NS_FATAL_ERROR ("Can't connect");
  std::cout << "Can't connect" << std::endl;
}

TypeId
PoissonGenerator::GetTypeId (void)
{
  static TypeId tid = TypeId ("ns3::PoissonGenerator")
    .SetParent<Application> ()
    .SetGroupName("Applications")
    .AddConstructor<PoissonGenerator> ()
    .AddAttribute ("Delay", "The delay between two packets (s)",
					DoubleValue (0.1),
					MakeDoubleAccessor (&PoissonGenerator::m_delay),
					MakeDoubleChecker<double> ())
    .AddAttribute ("Remote", "The address of the destination",
                   AddressValue (),
                   MakeAddressAccessor (&PoissonGenerator::m_remote),
                   MakeAddressChecker ())     
    .AddAttribute ("Protocol", "The type of protocol to use. This should be "
                   "a subclass of ns3::SocketFactory",
                   TypeIdValue (UdpSocketFactory::GetTypeId ()),
                   MakeTypeIdAccessor (&PoissonGenerator::m_protocol),
                   // This should check for SocketFactory as a parent
                   MakeTypeIdChecker ())                 
    .AddAttribute ("PacketSize", "The size of each packet (bytes)",
                    UintegerValue (1400),
                    MakeUintegerAccessor (&PoissonGenerator::m_size),
                    MakeUintegerChecker<uint32_t> ())
     .AddAttribute ("Local",
                   "The Address on which to bind the socket. If not set, it is generated automatically.",
                   AddressValue (),
                   MakeAddressAccessor (&PoissonGenerator::m_local),
                   MakeAddressChecker ())                  
    ;
  return tid;
} 

void
PoissonGenerator::StartApplication ()
{
    SetRemote(m_protocol, m_remote);
    DoGenerate();
}

void
PoissonGenerator::StopApplication ()
{
  NS_LOG_FUNCTION (this);

  if(m_socket != 0)
    {
      m_socket->Close ();
    }
  else
    {
      NS_LOG_WARN ("OnOffApplication found null socket to close in StopApplication");
    }
}

PoissonAppHelper::PoissonAppHelper (std::string protocol, Address address, double delay, uint32_t packageSize)
{
  m_factory.SetTypeId ("ns3::PoissonGenerator");
  m_factory.Set ("Protocol", StringValue (protocol));
  m_factory.Set ("Remote", AddressValue (address));
  m_factory.Set ("Delay", DoubleValue(delay));
  m_factory.Set ("PacketSize", UintegerValue (packageSize));
  //m_factory.Set ("Local", AddressValue(local));
}

void 
PoissonAppHelper::SetAttribute (std::string name, const AttributeValue &value)
{
  m_factory.Set (name, value);
}

ApplicationContainer 
PoissonAppHelper::Install (NodeContainer nodes)
{
  ApplicationContainer applications;
  for (NodeContainer::Iterator i = nodes.Begin (); i != nodes.End (); ++i)
    {
      Ptr<PoissonGenerator> app = m_factory.Create<PoissonGenerator> ();
      (*i)->AddApplication (app);
      applications.Add (app);
    }
  return applications;
}
}