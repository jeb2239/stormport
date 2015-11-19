interface BSDTCPPassiveSocket {
    command error_t bind(uint16_t port);
    
    command void listen();
    
    command error_t accept(struct sockaddr_in6* addr, int activesockid);
    event void acceptDone(struct sockaddr_in6* addr, int activesockid);
    
    command error_t close();
}
