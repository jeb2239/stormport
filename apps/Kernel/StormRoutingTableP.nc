#include "driver.h"
#include <stdint.h>
#include <lib6lowpan/ip.h>
#include <iprouting.h>

module StormRoutingTableP
{
    provides interface Driver;
    uses interface ForwardingTable;
    uses interface NeighborDiscovery;
    uses interface NeighborBlacklist;
}
implementation
{
    command driver_callback_t Driver.peek_callback()
    {
        return NULL;
    }

    command void Driver.pop_callback() {}

    /* Used for packing a route_entry struct into a uint8_t* for
     * returning in a syscall. Uses @arg as the start address of a
     * uint8_t*. Starts packing in the 35 bytes necessary to store a
     * route_entry struct at the indicated index @start
     */
    void pack_routing_table(uint32_t arg, uint32_t start, struct route_entry *entry)
    {
        int i;
        // add key
        ((uint8_t *)arg)[start+0] = (uint8_t) entry->key;
        // add prefix
        for (i=0;i<16;i++)
        {
            ((uint8_t *)arg)[start+1+i] = entry->prefix.s6_addr[i];
        }
        // add prefix len
        ((uint8_t *)arg)[start+17] = entry->prefixlen;
        // add next_hop
        for (i=0;i<16;i++)
        {
            ((uint8_t *)arg)[start+18+i] = entry->next_hop.s6_addr[i];
        }
        // add ifindex
        ((uint8_t *)arg)[start+34] = entry->ifindex;
    }

    void print_entry(struct route_entry *entry)
    {
        printf("valid? %d\n", entry->valid);
        printf("route key %d\nprefix ", entry->key);
        printf_in6addr(&entry->prefix);
        printf("\nprefix len %d\nnext_hop ", entry->prefixlen);
        printf_in6addr(&entry->next_hop);
        printf("\nifindex %d\n", entry->ifindex);
    }

    async command syscall_rv_t Driver.syscall_ex(
        uint32_t number, uint32_t arg0,
        uint32_t arg1, uint32_t arg2,
        uint32_t *argx)
    {
        switch(number & 0xFF)
        {
                   //              arg0            arg1        arg2      argx[0]
        case 0x01: //add_route(ip address prefix, prefix len, nexthop, interface) -> error or route index
        {
            struct in6_addr _next_hop;
            struct in6_addr *next_hop = &_next_hop;
            struct in6_addr _prefix;
            struct in6_addr *prefix = &_prefix;
            uint8_t ifindex;

            int prefix_len_bits = (int)arg1; // prefix len

            // check next hop for NULL. If not null, parse it
            if ((char*)arg2 == NULL)
            {
                next_hop = NULL;
            }
            else
            {
                inet_pton6((char*)arg2, next_hop); // prefix
            }

            // do the same for the prefix
            if ((char*)arg0 == NULL)
            {
                prefix = NULL;
            }
            else
            {
                inet_pton6((char*)arg0, prefix); // prefix
            }

            // check for valid interface
            if (argx[0] > 3) // invalid iface
            {
                return -1; // error
            }
            else
            {
                ifindex = (uint8_t) argx[0]; // iface index
            }
            // returns the route_key_t, an int
            if (prefix == NULL)
            {
                return call ForwardingTable.addRoute(NULL, prefix_len_bits, next_hop, ifindex);
            }
            else
            {
                return call ForwardingTable.addRoute(prefix->s6_addr, prefix_len_bits, next_hop, ifindex);
            }
        }
                   //           arg0
        case 0x02: //del_route(route key (int)) -> error
        {
            route_key_t index = (route_key_t) arg0;
            return call ForwardingTable.delRoute(index);
        }

                   //             arg0             arg1
        case 0x03: //get_route(route key (int), return buffer uint8_t [16] (just prefix for now) )
        {
            route_key_t index = (route_key_t) arg0;
            struct route_entry *entry = call ForwardingTable.lookupRouteKey(index);
            pack_routing_table(arg1, 0, entry);
            return 0;
        }

        case 0x04: //lookup_route(prefix, prefix_len_bits, return buffer uint8_t)
        {
            int i;
            struct route_entry *entry;
            struct in6_addr _prefix;
            struct in6_addr *prefix = &_prefix;
            int prefix_len_bits = (int)arg1; // prefix len
            // check prefix
            if ((char*)arg0 == NULL)
            {
                prefix = NULL;
            }
            else
            {
                inet_pton6((char*)arg0, prefix); // prefix
            }

            if (prefix == NULL)
            {
                entry = call ForwardingTable.lookupRoute(NULL, prefix_len_bits);
            }
            else
            {
                entry = call ForwardingTable.lookupRoute(prefix->s6_addr, prefix_len_bits);
            }
            pack_routing_table(arg2, 0, entry);
            return 0;
        }

        case 0x05: //get_table(return size, return buffer)
        {
            struct route_entry *table;
            int size;
            int i;
            table = call ForwardingTable.getTable(&size);
            // save the results of the table in a buffer
            for (i=0;i<size;i++)
            {
                // only store entries who have valid != 0 this should remove
                //"empty" entries, as the routing table doesn't know how big it is
                if ((table+i)->valid != 0)
                {
                    (*((int *)arg0))++;
                    pack_routing_table(arg1, i*35, table+i);
                }
            }
        }

        case 0x06: //ignore_neighbor(address)
        {
            error_t err = call NeighborBlacklist.ignoreAddrFromString((char*)arg0);
            if (err == SUCCESS) {
                return 0;
            } else {
                return -1;
            }
        }

        }
    }
}
