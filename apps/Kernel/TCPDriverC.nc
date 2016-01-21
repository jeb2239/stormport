#include <bsdtcp.h>

configuration TCPDriverC {
    provides interface Driver;
} implementation {
    components TCPDriverP, BsdTcpC;
    components RealMainP;
    
    TCPDriverP.BSDTCPActiveSocket[0] -> BsdTcpC.BSDTCPActiveSocket[unique(UQ_BSDTCP_ACTIVE)];
    TCPDriverP.BSDTCPActiveSocket[1] -> BsdTcpC.BSDTCPActiveSocket[unique(UQ_BSDTCP_ACTIVE)];
    TCPDriverP.BSDTCPActiveSocket[2] -> BsdTcpC.BSDTCPActiveSocket[unique(UQ_BSDTCP_ACTIVE)];
    TCPDriverP.BSDTCPPassiveSocket[0] -> BsdTcpC.BSDTCPPassiveSocket[unique(UQ_BSDTCP_PASSIVE)];
    TCPDriverP.BSDTCPPassiveSocket[1] -> BsdTcpC.BSDTCPPassiveSocket[unique(UQ_BSDTCP_PASSIVE)];
    TCPDriverP.BSDTCPPassiveSocket[2] -> BsdTcpC.BSDTCPPassiveSocket[unique(UQ_BSDTCP_PASSIVE)];
    
    TCPDriverP.Init <- RealMainP.SoftwareInit;
    Driver = TCPDriverP;
}
