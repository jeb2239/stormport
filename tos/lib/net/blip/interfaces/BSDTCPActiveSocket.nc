interface BSDTCPActiveSocket {
    command error_t bind(uint16_t port);
    
    command int getID();
    
    command error_t connect(struct sockaddr_in6* addr);
    event void connectDone(struct sockaddr_in6* addr);
    
    command error_t send(uint8_t* data, uint8_t length);
    
    event void receiveReady(uint8_t numbytes);
    command uint8_t receive(uint8_t* buffer, uint8_t length);
    
    event void closed(uint8_t how);
    
    command error_t close();
    
    command error_t abort();
}
