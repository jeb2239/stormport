module BsdTcpP {

    provides {
        interface BSDTCP[uint8_t sockid];
    } uses {
        interface Boot;
        interface IP;
        interface IPAddress;
        interface Timer<TMilli>[uint8_t timerid];
    }
    
} implementation {

    event void Boot.booted() {
    }
    
    event void Timer.fired[uint8_t timer_id]() {
    }
    
    event void IP.recv(struct ip6_hdr* iph, void* packet, size_t len,
                       struct ip6_metadata* meta) {
    }
    
    event void IPAddress.changed(bool valid) {
    }
    
    command error_t BSDTCP.bind[uint8_t sockid](uint16_t port) {
    }
    
    command void BSDTCP.listen[uint8_t sockid]() {
    }
    
    command error_t BSDTCP.accept[uint8_t sockid](struct sockaddr_in6* addr) {
    }
    
    command error_t BSDTCP.connect[uint8_t sockid](struct sockaddr_in6* addr) {
    }
    
    command error_t BSDTCP.send[uint8_t sockid](uint8_t* data, uint8_t length) {
    }
    
    command uint8_t BSDTCP.receive[uint8_t sockid](uint8_t* buffer, uint8_t len) {
    }
    
    command error_t BSDTCP.close[uint8_t sockid]() {
    }
    
    command error_t BSDTCP.abort[uint8_t sockid]() {
    }
    
    /* Wrapper for underlying C code. */
    void send_message(struct ip6_packet* msg) {
        call IP.send(msg);
    }
}
