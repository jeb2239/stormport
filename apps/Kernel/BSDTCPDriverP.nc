module BSDTCPDriverP {
    provides interface Driver;
    uses interface BSDTCPActiveSocket;
    uses interface BSDTCPPassiveSocket;
} implementation {
    command driver_callback_t Driver.peek_callback() {
        return NULL;
    }
    
    command void Driver.pop_callback() {
    }
    
    event void BSDTCPPassiveSocket.acceptDone(struct sockaddr_in6* addr, int asockid) {
        printf("Accepted connection!\n");
    }
    
    event void BSDTCPActiveSocket.connectDone(struct sockaddr_in6* addr) {
        printf("Connection done!\n");
    }
    
    event void BSDTCPActiveSocket.receiveReady() {
        printf("Receive ready!\n");
    }
    
    event void BSDTCPActiveSocket.sendReady() {
        printf("Send ready!\n");
    }
    
    event void BSDTCPActiveSocket.connectionLost(uint8_t how) {
        printf("Connection lost!\n");
    }
    
    async command syscall_rv_t Driver.syscall_ex(uint32_t number, uint32_t arg0, uint32_t arg1, uint32_t arg2, uint32_t* argx) {
        struct sockaddr_in6 addr;
        uint8_t* buffer;
        size_t length;
        size_t xbytes = 0;
        switch (number & 0xFF) {
            case 0x00: // connect
                addr.sin6_port = htons((uint16_t) arg0);
                inet_pton6(/*"2001:0470:83ae:0002:*/"fe80::0212:6d02:0000:4021", &addr.sin6_addr);
                call BSDTCPActiveSocket.connect(&addr);
                break;
            case 0x01: // bind
                if (arg0 & 1) {
                    call BSDTCPPassiveSocket.bind((uint16_t) arg0);
                    printf("Bound passive socket to port %d\n", arg0);
                } else {
                    call BSDTCPActiveSocket.bind((uint16_t) arg0);
                    printf("Bound active socket to port %d\n", arg0);
                }
                break;
            case 0x02: // listenaccept
                call BSDTCPPassiveSocket.listenaccept(0);
                printf("Accepting into socket 0\n");
                break;
            case 0x03: // send
                buffer = (uint8_t*) arg0;
                length = (size_t) arg1;
                printf("Send rv = %d\n", call BSDTCPActiveSocket.send(buffer, length, 0, &xbytes));
                break;
            case 0x04: // receive
                buffer = (uint8_t*) arg0;
                length = (size_t) arg1;
                printf("Receive rv = %d\n", call BSDTCPActiveSocket.receive(buffer, length, &xbytes));
                break;
            case 0x05: // shutdown
                call BSDTCPActiveSocket.shutdown(FALSE, TRUE);
                break;
            case 0x06: // close
                call BSDTCPActiveSocket.shutdown(TRUE, TRUE);
                break;
            case 0x07: // abort
                call BSDTCPActiveSocket.abort();
                break;
            default:
                printf("Doing nothing\n");
                break;
        }
        return (syscall_rv_t) xbytes;
    }
}
