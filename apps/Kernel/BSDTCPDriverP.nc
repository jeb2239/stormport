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
    }
    
    event void BSDTCPActiveSocket.receiveReady(uint8_t numbytes) {
    }
    
    event void BSDTCPActiveSocket.closed(uint8_t how) {
    }
    
    async command syscall_rv_t Driver.syscall_ex(uint32_t number, uint32_t arg0, uint32_t arg1, uint32_t arg2, uint32_t* argx) {
        struct sockaddr_in6 addr;
        switch (number & 0xFF) {
            case 0x00: // connect
                addr.sin6_port = htons(32067);
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
            default:
                printf("Doing nothing\n");
                break;
        }
        return 0;
    }
}
