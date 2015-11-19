configuration BsdTcpC {
    provides interface BSDTCP[uint8_t sockid];
} implementation {
    components MainC, BsdTcpP, IPStackC, IPAddressC;
    components new TimerMilliC();
    components new TimerMilliC() as TickTimerMilliC;
    components new VirtualizeTimerC(TMilli, 16);
    
    VirtualizeTimerC.TimerFrom -> TimerMilliC;
    
    BsdTcpP.Boot -> MainC.Boot;
    BsdTcpP.IP -> IPStackC.IP[IANA_TCP];
    BsdTcpP.IPAddress -> IPAddressC.IPAddress;
    BsdTcpP.Timer -> VirtualizeTimerC.Timer;
    BsdTcpP.TickTimer -> TickTimerMilliC;
    
    BSDTCP = BsdTcpP.BSDTCP;
}
