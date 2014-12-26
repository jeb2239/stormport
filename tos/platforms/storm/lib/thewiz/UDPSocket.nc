#include <lib6lowpan/iovec.h>

interface UDPSocket
{
  // use this socket for UDP
  command void initialize(uint16_t localport);

  // signaled when the initialization process is done
  event void initializeDone(error_t error);

  // Destport is in little endian, destip is in big endian (network byte order).
  command void sendPacket(uint16_t destport, uint32_t destip, struct ip_iovec data);

  // called when the packet has finished sending
  event void sendPacketDone(error_t error);

  // called upon recipt of a udp packet over ethernet
  // Srcport is in little endian, srcip is in big endian (network byte order).
  event void packetReceived(uint16_t srcport, uint32_t srcip, uint8_t *buf, uint16_t len);
}