configuration BsdTcpC {
    provides interface BSDTCPActiveSocket[uint8_t asockid];
    provides interface BSDTCPPassiveSocket[uint8_t psockid];
} implementation {
    components MainC, BsdTcpP, IPStackC, IPAddressC, LocalTimeMilliC;
    components new TimerMilliC();
    components new VirtualizeTimerC(TMilli, uniqueCount(UQ_BSDTCP_ACTIVE) << 2);
    
    VirtualizeTimerC.TimerFrom -> TimerMilliC;
    
    BsdTcpP.Boot -> MainC.Boot;
    BsdTcpP.IP -> IPStackC.IP[IANA_TCP];
    BsdTcpP.IPAddress -> IPAddressC.IPAddress;
    BsdTcpP.Timer -> VirtualizeTimerC.Timer;
    BsdTcpP.LocalTime -> LocalTimeMilliC;
    
    BSDTCPActiveSocket = BsdTcpP.BSDTCPActiveSocket;
    BSDTCPPassiveSocket = BsdTcpP.BSDTCPPassiveSocket;
}
