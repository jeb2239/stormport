interface BSDTCPPassiveSocket {
    command int getID();
    
    command error_t bind(uint16_t port);
    
    command error_t listenaccept(int activesockid, uint8_t* recvbuf, size_t recvbuflen, uint8_t* reassbmp);
    event void acceptDone(struct sockaddr_in6* addr, int activesockid);
    
    command error_t close();
}
