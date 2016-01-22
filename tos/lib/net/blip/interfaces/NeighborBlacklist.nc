#include <iprouting.h>
interface NeighborBlacklist {

  // NOTE: neighbor table entries only use the LOWER 64-bits of the provided
  // address.

  // ignore the neighbor with the given address
  command error_t ignore(struct in6_addr *neighbor);
  command error_t ignoreAddrFromString(char *addr);
  
  command void listIgnored();

  command error_t unignore(struct in6_addr *neighbor);

  command bool ignored(struct in6_addr *address);
}
