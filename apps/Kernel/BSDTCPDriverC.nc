configuration BSDTCPDriverC {
    provides interface Driver;
} implementation {
    components BSDTCPDriverP, BsdTcpC;
    
    BSDTCPDriverP.BSDTCP -> BsdTcpC.BSDTCP[0];
    
    Driver = BSDTCPDriverP;
}
