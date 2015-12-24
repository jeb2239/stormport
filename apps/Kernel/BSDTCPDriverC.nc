#include <bsdtcp.h>

configuration BSDTCPDriverC {
    provides interface Driver;
} implementation {
    components BSDTCPDriverP, BsdTcpC;
    components RealMainP;
    
    BSDTCPDriverP.BSDTCPActiveSocket[0] -> BsdTcpC.BSDTCPActiveSocket[unique(UQ_BSDTCP_ACTIVE)];
    BSDTCPDriverP.BSDTCPActiveSocket[1] -> BsdTcpC.BSDTCPActiveSocket[unique(UQ_BSDTCP_ACTIVE)];
    BSDTCPDriverP.BSDTCPActiveSocket[2] -> BsdTcpC.BSDTCPActiveSocket[unique(UQ_BSDTCP_ACTIVE)];
    BSDTCPDriverP.BSDTCPPassiveSocket[0] -> BsdTcpC.BSDTCPPassiveSocket[unique(UQ_BSDTCP_PASSIVE)];
    BSDTCPDriverP.BSDTCPPassiveSocket[1] -> BsdTcpC.BSDTCPPassiveSocket[unique(UQ_BSDTCP_PASSIVE)];
    BSDTCPDriverP.BSDTCPPassiveSocket[2] -> BsdTcpC.BSDTCPPassiveSocket[unique(UQ_BSDTCP_PASSIVE)];
    
    BSDTCPDriverP.Init <- RealMainP.SoftwareInit;
    Driver = BSDTCPDriverP;
}
