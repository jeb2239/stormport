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
        printf("Got syscall!\n");
    }
}
