#include <bsdtcp.h>

configuration BSDTCPDriverC {
    provides interface Driver;
} implementation {
    components BSDTCPDriverP, BsdTcpC;
    
    BSDTCPDriverP.BSDTCPActiveSocket -> BsdTcpC.BSDTCPActiveSocket[unique(UQ_BSDTCP_ACTIVE)];
    BSDTCPDriverP.BSDTCPPassiveSocket -> BsdTcpC.BSDTCPPassiveSocket[unique(UQ_BSDTCP_PASSIVE)];
    
    Driver = BSDTCPDriverP;
}
