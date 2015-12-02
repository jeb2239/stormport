/* This isn't from the BSD code.
 * This is taken from my implementation of TCP in PC userland.
 */
#include "tcp.h"

inline uint16_t deref_safe(uint16_t* unaligned) {
    return ((uint16_t) *((uint8_t*) unaligned)) | (((uint16_t) *(((uint8_t*) unaligned) + 1)) << 8);
}

uint16_t get_checksum(struct in6_addr* src, struct in6_addr* dest,
                      struct tcphdr* tcpseg, uint32_t len) {
    uint32_t total;
    uint16_t* current;
    uint16_t* end;
    struct {
        struct in6_addr srcaddr;
        struct in6_addr destaddr;
        uint32_t tcplen;
        uint8_t reserved0;
        uint8_t reserved1;
        uint8_t reserved2;
        uint8_t protocol;
    } __attribute__((packed, aligned)) pseudoheader;
    memcpy(&pseudoheader.srcaddr, src, sizeof(struct in6_addr));
    memcpy(&pseudoheader.destaddr, dest, sizeof(struct in6_addr));
    pseudoheader.reserved0 = 0;
    pseudoheader.reserved1 = 0;
    pseudoheader.reserved2 = 0;
    pseudoheader.protocol = 6; // TCP
    pseudoheader.tcplen = (uint32_t) htonl(len);
    
    total = 0;
    for (current = (uint16_t*) &pseudoheader;
         current < (uint16_t*) (&pseudoheader + 1); current++) {
        total += (uint32_t) *current;
    }
    
    end = (uint16_t*) (((uint8_t*) tcpseg) + len);
    
    if (len & 0x1u) {
        // Edge case for odd-length packet
        end = (uint16_t*) (((uint8_t*) end) - 1);
        total += (*((uint8_t*) end)) << 8;
    }
    
    for (current = (uint16_t*) tcpseg;
        current < end; current++) {
        // read the memory byte by byte, in case tcpseg isn't word-aligned 
        total += deref_safe(current);
    }
    
    while (total >> 16) {
        total = (total & 0xFFFF) + (total >> 16);
    }
    
    return ~((uint16_t) total);
}
