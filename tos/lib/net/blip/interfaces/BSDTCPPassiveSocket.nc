interface BSDTCPPassiveSocket {
    command int getID();
    
    command error_t bind(uint16_t port);
    
    command error_t listenaccept(int activesockid);
    event void acceptDone(struct sockaddr_in6* addr, int activesockid);
    
    command error_t close();
}
