/*
 * Data structures for SERP routing
 */

#ifndef SERP_MESSAGES_H
#define SERP_MESSAGES_H

#include <icmp6.h>

typedef enum {
    SERP_BATTERY_POWERED = 0x00,
    SERP_MAINS_POWERED = 0xFF
} serp_power_type;

// option attached to a router advertisement that
// is unicast do a node petititioning to be part of
// the mesh
struct nd_option_serp_mesh_info_t {
    uint8_t prefix_length;
    serp_power_type powered;
    struct in6_addr prefix;
};

#endif
