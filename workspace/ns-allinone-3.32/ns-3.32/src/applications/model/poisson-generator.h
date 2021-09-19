#include "ns3/command-line.h"
#include "ns3/log.h"
#include "ns3/simulator.h"
#include "ns3/string.h"
#include "ns3/pointer.h"
#include "ns3/application.h"
#include "ns3/random-variable-stream.h"
#include "ns3/socket.h"
#include "ns3/simulator.h"
#include "ns3/uinteger.h"
#include "ns3/double.h"
#include "ns3/application-container.h"
#include "ns3/node-container.h"
#include "ns3/tag.h"
#include "ns3/packet.h"
#include "ns3/uinteger.h"
#include <iostream>

namespace ns3 {

class PoissonGenerator : public Application
{
public:
  static TypeId GetTypeId (void);

  PoissonGenerator ();
  virtual ~PoissonGenerator();
  void SetDelay (double delay);
  void SetSize (uint32_t size);
  void SetRemote (TypeId socketType, Address remote);


private:
  virtual void StartApplication (void);
  virtual void StopApplication (void);
  void DoGenerate (void);
  void CancelEvents ();

  double m_delay;
  uint32_t m_size;
  TypeId m_protocol;   
  Address m_remote;
  Address  m_local;        //!< Local address to bind to
  Ptr<Socket> m_socket;
  EventId m_sendEvent;
  bool m_connected;

     /**
   * \brief Handle a Connection Succeed event
   * \param socket the connected socket
   */
  void ConnectionSucceeded (Ptr<Socket> socket);
  /**
   * \brief Handle a Connection Failed event
   * \param socket the not connected socket
   */
  void ConnectionFailed (Ptr<Socket> socket);
};  

class PoissonAppHelper
{
public:
  PoissonAppHelper (std::string protocol, Address remote, double delay, uint32_t packageSize);
  void SetAttribute (std::string name, const AttributeValue &value);
  ApplicationContainer Install (NodeContainer c);
private:
  std::string m_protocol;
  std::string m_socketType;
  Address m_remote;
  ObjectFactory m_factory;
};

class PacketIdTag:public Tag
{
public:
  static TypeId GetTypeId (void);
  virtual TypeId GetInstanceTypeId (void) const;
  virtual uint32_t GetSerializedSize (void) const;
  virtual void Serialize (TagBuffer i) const;
  virtual void Deserialize (TagBuffer i);
  virtual void Print (std::ostream &os) const;
 
  // these are our accessors to our tag structure
  void SetValue (uint8_t value);
  uint8_t GetValue (void) const;
private:
  uint8_t m_Value;  
};

}