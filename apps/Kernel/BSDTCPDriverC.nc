configuration BSDTCPDriverC {
    provides interface Driver;
} implementation {
    components BSDTCPDriverP, BsdTcpC;
    
    BSDTCPDriverP.BSDTCPActiveSocket -> BsdTcpC.BSDTCPActiveSocket[0];
    BSDTCPDriverP.BSDTCPPassiveSocket -> BsdTcpC.BSDTCPPassiveSocket[0];
    
    Driver = BSDTCPDriverP;
}
