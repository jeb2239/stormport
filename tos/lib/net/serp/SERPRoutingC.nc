#include <lib6lowpan/ip.h>
#include <serp_messages.h>

configuration SERPRoutingC {
  provides {
    interface RootControl;
  }
} implementation {
    components SERPRoutingEngineC;
    components IPNeighborDiscoveryC;
    RootControl = SERPRoutingEngineC.RootControl;
    SERPRoutingEngineC.NeighborDiscovery -> IPNeighborDiscoveryC;
}
