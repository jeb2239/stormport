#include "ethernetshield.h"

configuration EthernetClientAppC
{
}
implementation
{
  components MainC, EthernetClientC, HplSam4lIOC;
  components SocketP, SocketSpiP;
  components new Sam4lUSART0C();
  components new Timer32khzC() as SocketSpiTimer;
  SocketSpiP.SpiPacket -> Sam4lUSART0C.SpiPacket;
  SocketSpiP.SpiHPL -> Sam4lUSART0C;
  SocketSpiP.EthernetSS -> HplSam4lIOC.PB11;
  SocketSpiP.Timer -> SocketPTimer;

  // arbiter
  components new FcfsArbiterC(ETHERNETRESOURCE_ID) as arbiter;
  components EthernetClientResourceConfigureP;
  arbiter.ResourceConfigure -> EthernetClientResourceConfigureP.ResourceConfigure;

  // is this the correct way to do this?
  MainC.SoftwareInit -> SocketSpiP;

  components EthernetShieldConfigC;
  components new Timer32khzC() as EthernetShieldTimer;
  EthernetShieldConfigC.Timer -> EthernetShieldTimer;
  EthernetShieldConfigC.SocketSpi -> SocketSpiP.SocketSpi;
  EthernetShieldConfigC.SpiResource -> arbiter.Resource[unique(ETHERNETRESOURCE_ID)];

  EthernetClientC.Boot -> MainC;
  components SerialPrintfC;
  components new Timer32khzC();
  EthernetClientC.Timer -> Timer32khzC;

  components new Timer32khzC() as SocketPTimer;
  SocketP.SocketSpi -> SocketSpiP.SocketSpi;
  SocketP.Timer -> SocketPTimer;
  SocketP.InitResource -> arbiter.Resource[unique(ETHERNETRESOURCE_ID)];
  SocketP.SendResource -> arbiter.Resource[unique(ETHERNETRESOURCE_ID)];
  SocketP.RecvResource -> arbiter.Resource[unique(ETHERNETRESOURCE_ID)];
  SocketP.ArbiterInfo -> arbiter.ArbiterInfo;
  SocketP.GpioInterrupt -> HplSam4lIOC.PB12IRQ;
  SocketP.IRQPin -> HplSam4lIOC.PB12;

  EthernetClientC.UDPSocket -> SocketP.UDPSocket;
  EthernetClientC.ArbiterInfo -> arbiter.ArbiterInfo;
  EthernetClientC.EthernetShieldConfig -> EthernetShieldConfigC;
}