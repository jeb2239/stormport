interface BSDTCPActiveSocket {
    command int getID();
    command int getState();

    command error_t bind(uint16_t port);
    
    command error_t connect(struct sockaddr_in6* addr);
    event void connectDone(struct sockaddr_in6* addr);
    
    event void sendReady();
    command error_t send(uint8_t* data, uint8_t length, int moretocome, size_t* bytessent);
    
    event void receiveReady();
    command error_t receive(uint8_t* buffer, uint8_t length, size_t* bytessent);
    
    event void connectionLost(uint8_t how);
    
    command error_t shutdown(bool reads, bool writes);
    command error_t abort();
}
