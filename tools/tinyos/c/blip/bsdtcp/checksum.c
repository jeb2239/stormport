/* This isn't from the BSD code.
 * This is taken from my implementation of TCP in PC userland.
 */
#include "tcp.h"

uint16_t get_checksum(struct in6_addr* src, struct in6_addr* dest,
                      struct tcphdr* tcpseg, uint32_t len) {
    uint32_t total;
    uint16_t* current;
    struct {
        struct in6_addr srcaddr;
        struct in6_addr destaddr;
        uint32_t tcplen;
        uint8_t reserved[3];
        uint8_t protocol;
    } __attribute__((packed)) pseudoheader;
    pseudoheader.srcaddr = *src;
    pseudoheader.destaddr = *dest;
    pseudoheader.reserved[0] = 0;
    pseudoheader.reserved[1] = 0;
    pseudoheader.reserved[2] = 0;
    pseudoheader.protocol = 6; // TCP
    pseudoheader.tcplen = (uint32_t) htonl(len);

    total = 0;
    for (current = (uint16_t*) &pseudoheader;
         current < (uint16_t*) (&pseudoheader + 1); current++) {
        total += (uint32_t) *current;
    }
    for (current = (uint16_t*) tcpseg;
         current < (uint16_t*) (((uint8_t*) tcpseg) + len); current++) {
        total += (uint32_t) *current;
    }
    
    while (total >> 16) {
        total = (total & 0xFFFF) + (total >> 16);
    }
    
    return ~((uint16_t) total);
}
