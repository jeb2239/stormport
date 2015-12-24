#include <bsdtcp/syserrno.h>

#define NUMACTIVE 3
#define NUMPASSIVE 3

module BSDTCPDriverP {
    provides interface Driver;
    provides interface Init;
    uses interface BSDTCPActiveSocket[uint8_t aclient];
    uses interface BSDTCPPassiveSocket[uint8_t pclient];
} implementation {
    #include <bsdtcp/socket.h>
    #include <bsdtcp/tcp.h>
    #include <bsdtcp/tcp_fsm.h>
    
    /* Bitmasks that record which sockets are currently allocated. */
    uint32_t activemask;
    uint32_t passivemask;
    
    /* For each passive socket, stores which active socket it is accepting into. */
    int acceptinginto[NUMPASSIVE];
    
    command error_t Init.init() {
        int i;
        activemask = 0;
        passivemask = 0;
        for (i = 0; i < NUMPASSIVE; i++) {
            acceptinginto[i] = -1;
        }
    }

    command driver_callback_t Driver.peek_callback() {
        return NULL;
    }
    
    command void Driver.pop_callback() {
    }
    
    event void BSDTCPPassiveSocket.acceptDone[uint8_t pi](struct sockaddr_in6* addr, int asockid) {
        acceptinginto[pi] = -1;
        printf("Accepted connection!\n");
    }
    
    event void BSDTCPActiveSocket.connectDone[uint8_t ai](struct sockaddr_in6* addr) {
        printf("Connection done!\n");
    }
    
    event void BSDTCPActiveSocket.receiveReady[uint8_t ai]() {
        printf("Receive ready!\n");
    }
    
    event void BSDTCPActiveSocket.sendReady[uint8_t ai]() {
        printf("Send ready!\n");
    }
    
    event void BSDTCPActiveSocket.connectionLost[uint8_t ai](uint8_t how) {
        printf("Connection lost!\n");
    }
    
    default command error_t BSDTCPPassiveSocket.bind[uint8_t pundef](uint16_t port) {
        return EBADF;
    }
    
    default command error_t BSDTCPPassiveSocket.listenaccept[uint8_t pundef] (int activesockid) {
        return EBADF;
    }
    
    default command error_t BSDTCPPassiveSocket.close[uint8_t pundef] () {
        return EBADF;
    }
    
    default command error_t BSDTCPActiveSocket.bind[uint8_t aundef](uint16_t port) {
        return EBADF;
    }
    
    default command int BSDTCPPassiveSocket.getID[uint8_t pundef]() {
        return -1; // BE CAREFUL
    }
    
    default command int BSDTCPActiveSocket.getID[uint8_t aundef]() {
        return -1; // BE CAREFUL
    }
    
    default command int BSDTCPActiveSocket.getState[uint8_t aundef]() {
        return 0; // BE CAREFUL
    }
    
    default command error_t BSDTCPActiveSocket.connect[uint8_t aundef](struct sockaddr_in6* addr) {
        return EBADF;
    }
    
    default command error_t BSDTCPActiveSocket.send[uint8_t aundef](uint8_t* data, uint8_t length, int moretocome, size_t* bytessent) {
        return EBADF;
    }
    
    default command error_t BSDTCPActiveSocket.receive[uint8_t aundef](uint8_t* buffer, uint8_t length, size_t* bytessent) {
        return EBADF;
    }
    
    default command error_t BSDTCPActiveSocket.shutdown[uint8_t aundef](bool reads, bool writes) {
        return EBADF;
    }
    
    default command error_t BSDTCPActiveSocket.abort[uint8_t aundef]() {
        return EBADF;
    }
    
    int alloc_fd(uint32_t* mask, int maxalloc, bool (*isvalid)(int) ) {
        int i;
        for (i = 0; i < maxalloc; i++) {
            if (!(*mask & (1 << i)) && isvalid(i)) {
                *mask |= (1 << i);
                return i;
            }
        }
        return -1;
    }
    
    bool always_true(int pi) {
        return TRUE;
    }
    
    int alloc_pfd() {
        return alloc_fd(&passivemask, NUMPASSIVE, always_true);
    }
    
    bool active_isclosed(int ai) {
        return TCPS_CLOSED == call BSDTCPActiveSocket.getState[ai]();
    }
    
    bool active_istimewait(int ai) {
        return TCPS_TIME_WAIT == call BSDTCPActiveSocket.getState[ai]();
    }
    
    int alloc_afd() {
        int afd;
        // First, try to get a socket that's closed.
        afd = alloc_fd(&activemask, NUMACTIVE, active_isclosed);
        if (afd == -1) {
            // If that failed, try to get a socket in TIME-WAIT, and end the TIME-WAIT early.
            afd = alloc_fd(&activemask, NUMACTIVE, active_istimewait);
            call BSDTCPActiveSocket.abort[afd]();
        }
        return afd;
    }
    
    void dealloc_fd(uint32_t fd, uint32_t* mask) {
        *mask &= ~(1 << fd);
    }
    
    inline bool check_fd(uint32_t fd, uint32_t* mask) {
        return (*mask & (1 << fd));
    }
    
    int decode_fd(uint32_t rawfd, bool* passive) {
        int index;
        if (rawfd >= NUMACTIVE) {
            *passive = TRUE;
            rawfd -= NUMACTIVE;
            if (!check_fd(rawfd, &passivemask)) {
                return -1;
            }
        } else {
            *passive = FALSE;
            if (!check_fd(rawfd, &activemask)) {
                return -1;
            }
        }
        return index;
    }
    
    async command syscall_rv_t Driver.syscall_ex(uint32_t number, uint32_t arg0, uint32_t arg1, uint32_t arg2, uint32_t* argx) {
        struct sockaddr_in6 addr;
        bool passive;
        uint8_t* buffer;
        int fd;
        int afd;
        size_t length;
        size_t xbytes = 0;
        syscall_rv_t rv = (syscall_rv_t) EBADF;
        fd = decode_fd(arg0, &passive); // most syscalls need this info
        switch (number & 0xFF) {
            case 0x00: // passivesocket
                rv = (syscall_rv_t) alloc_pfd();
                break;
            case 0x01: // activesocket
                rv = (syscall_rv_t) alloc_afd();
                break;
            case 0x02: // bind
                if (fd < 0) {
                    break;
                }
                if (passive) {
                    rv = (syscall_rv_t) call BSDTCPPassiveSocket.bind[fd]((uint16_t) arg1);
                    printf("Bound passive socket to port %d\n", arg1);
                } else {
                    rv = (syscall_rv_t) call BSDTCPActiveSocket.bind[fd]((uint16_t) arg1);
                    printf("Bound active socket to port %d\n", arg1);
                }
                break;
            case 0x03: // connect
                if (fd < 0 || passive) {
                    break;
                }
                if (arg2 > 0xFFFF) {
                    rv = (syscall_rv_t) EINVAL;
                    break;
                }
                inet_pton6((char*) arg1, &addr.sin6_addr);
                addr.sin6_port = htons((uint16_t) arg2);
                rv = (syscall_rv_t) call BSDTCPActiveSocket.connect[fd](&addr);
                break;
            case 0x04: // listenaccept
                if (fd < 0 || !passive) {
                    break;
                }
                afd = alloc_afd();
                if (afd == -1) {
                    rv = (syscall_rv_t) ENFILE;
                    break;
                }
                acceptinginto[fd] = afd;
                call BSDTCPPassiveSocket.listenaccept[fd](call BSDTCPActiveSocket.getID[afd]());
                printf("Accepting into socket %d\n", afd);
                break;
            case 0x05: // send
                if (fd < 0 || passive) {
                    break;
                }
                buffer = (uint8_t*) arg1;
                length = (size_t) arg2;
                rv = (syscall_rv_t) call BSDTCPActiveSocket.send[fd](buffer, length, 0, (size_t*) argx[0]);
                break;
            case 0x06: // receive
                if (fd < 0 || passive) {
                    break;
                }
                buffer = (uint8_t*) arg1;
                length = (size_t) arg2;
                rv = (syscall_rv_t) call BSDTCPActiveSocket.receive[fd](buffer, length, (size_t*) argx[0]);
                break;
            case 0x07: // shutdown
                if (fd < 0 || passive) {
                    break;
                }
                rv = (syscall_rv_t) call BSDTCPActiveSocket.shutdown[fd]((arg1 == SHUT_RD) || (arg1 == SHUT_RDWR),
                                                                         (arg1 == SHUT_WR) || (arg1 == SHUT_RDWR));
                break;
            case 0x08: // close
                if (fd < 0) {
                    break;
                }
                if (passive) {
                    rv = (syscall_rv_t) call BSDTCPPassiveSocket.close[fd]();
                    dealloc_fd((uint32_t) fd, &passivemask);
                    if (acceptinginto[fd] != -1) {
                        dealloc_fd((uint32_t) acceptinginto[fd], &activemask);
                        acceptinginto[fd] = -1;
                    }
                } else {
                    rv = (syscall_rv_t) call BSDTCPActiveSocket.shutdown[fd](TRUE, TRUE);
                    dealloc_fd((uint32_t) fd, &activemask);
                }
                break;
            case 0x09: // abort
                if (fd < 0 || passive) {
                    break;
                }
                rv = (syscall_rv_t) call BSDTCPActiveSocket.abort[fd]();
                break;
            default:
                printf("Doing nothing\n");
                break;
        }
        return rv;
    }
}
