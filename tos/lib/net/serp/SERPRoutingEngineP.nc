#include <lib6lowpan/ip_malloc.h>
#include <lib6lowpan/in_cksum.h>
#include <lib6lowpan/ip.h>
#include "neighbor_discovery.h"

module SERPRoutingEngineP {
    provides {
        interface StdControl;
        interface RootControl;
    }
    uses {
        interface IP as IP_RA;
        interface IP as IP_RS;
        interface Random;
        interface IPAddress;
        interface Ieee154Address;
        interface NeighborDiscovery;
    }
} implementation {

#define ADD_SECTION(SRC, LEN) ip_memcpy(cur, (uint8_t *)(SRC), LEN);\
  cur += (LEN); length += (LEN);

  /**** Global Vars ***/
  // whether or not the routing protocol is running
  bool running = FALSE;

  // if this node is root
  bool I_AM_ROOT = FALSE;

  // this is where we store the LL address of the node
  // who sent us a broadcast RS. We need to unicast an RA
  // back to them.
  struct in6_addr unicast_ra_destination;

  // initial period becomes 2 << (10 - 1) == 512 milliseconds
  uint32_t tx_interval_min = 10;

  task void init() {
    if (I_AM_ROOT) {
        // we don't actually advertise the mesh. It is driven entirely
        // by clients that want to join
        //post startMeshAdvertising();
    } else {
    }
  }

  // Returns the length of the added option
  uint8_t add_sllao (uint8_t* data) {
    struct nd_option_slla_t sllao;

    sllao.type = ND6_OPT_SLLAO;
    sllao.option_length = 2; // length in multiples of 8 octets, need 10 bytes
                             // so we must round up to 16.
    sllao.ll_addr = call Ieee154Address.getExtAddr();

    ip_memcpy(data, (uint8_t*) &sllao, sizeof(struct nd_option_slla_t));
    memset(data+sizeof(struct nd_option_slla_t), 0,
      16-sizeof(struct nd_option_slla_t));
    return 16;
  }

  /***** Handle Incoming RA *****/
  event void IP_RA.recv(struct ip6_hdr *hdr,
                        void *packet,
                        size_t len,
                        struct ip6_metadata *meta) {
  // TODO: implement this
    printf("received an RA in SERP\n");
  }

  /***** Send Router Advertisement *****/
  // This is the RA that we send to a new mote that
  // probably hasn't joined the mesh yet. This is sent as a
  // unicast in response to a received RS
  // Most of this implementation is taken from IPNeighborDiscoveryP
  task void send_RA_to_new() {
    struct nd_router_advertisement_t ra;

    struct ip6_packet pkt;
    struct ip_iovec   v[1];


    uint8_t sllao_len;
    uint8_t data[120];
    uint8_t* cur = data;
    uint16_t length = 0;

    // if we don't have prefix, we aren't part of mesh and
    // shouldn't respond to this
    if (!call NeighborDiscovery.havePrefix()) {
        return;
    }

    ra.icmpv6.type = ICMP_TYPE_ROUTER_ADV;
    ra.icmpv6.code = ICMPV6_CODE_RA;
    ra.icmpv6.checksum = 0;
    ra.hop_limit = 16;
    ra.flags_reserved = 0;
    ra.flags_reserved |= RA_FLAG_MANAGED_ADDR_CONF << ND6_ADV_M_SHIFT;
    ra.flags_reserved |= RA_FLAG_OTHER_CONF << ND6_ADV_O_SHIFT;
    ra.router_lifetime = RTR_LIFETIME;
    ra.reachable_time = 0; // unspecified at this point...
    ra.retransmit_time = 0; // unspecified at this point...
    ADD_SECTION(&ra, sizeof(struct nd_router_advertisement_t));

    sllao_len = add_sllao(cur);
    cur += sllao_len;
    length += sllao_len;

    if (call NeighborDiscovery.havePrefix()) {
        struct nd_option_serp_mesh_info_t option;
        // add prefix length
        option.prefix_length = call NeighborDiscovery.getPrefixLength();
        // add prefix
        memcpy(&option.prefix, call NeighborDiscovery.getPrefix(), sizeof(struct in6_addr));
        // treat all nodes as powered for now
        // TODO: fix this!
        option.powered = SERP_MAINS_POWERED;
        ADD_SECTION(&option, sizeof(struct nd_option_serp_mesh_info_t));
    }

    v[0].iov_base = data;
    v[0].iov_len = length;
    v[0].iov_next = NULL;

    pkt.ip6_hdr.ip6_nxt = IANA_ICMP;
    pkt.ip6_hdr.ip6_plen = htons(length);

    pkt.ip6_data = &v[0];

    // Send unicast RA to the link local address
    memcpy(&pkt.ip6_hdr.ip6_dst, &unicast_ra_destination, 16);

    // set the src address to our link layer address
    call IPAddress.getLLAddr(&pkt.ip6_hdr.ip6_src);
    call IP_RA.send(&pkt);
  }

  /***** Handle Incoming RS *****/
  // When we receive an RS
  event void IP_RS.recv(struct ip6_hdr *hdr,
                        void *packet,
                        size_t len,
                        struct ip6_metadata *meta) {
    printf("received an RS in SERP from ");
    printf_in6addr(&hdr->ip6_dst);
    printf("\n");
    memcpy(&unicast_ra_destination, &(hdr->ip6_src), sizeof(struct in6_addr));

    // send our unicast reply
    post send_RA_to_new();
  }

  /***** StdControl *****/
  command error_t StdControl.start() {
    if (!running) {
        post init();
        running = TRUE;
    }
    return SUCCESS;
  }

  command error_t StdControl.stop() {
      running = FALSE;
      return SUCCESS;
  }

  /***** RootControl *****/
  command error_t RootControl.setRoot() {
    I_AM_ROOT = TRUE;
    //call RPLRankInfo.declareRoot();
    return SUCCESS;
  }

  command error_t RootControl.unsetRoot() {
    I_AM_ROOT = FALSE;
    //call RPLRankInfo.cancelRoot();
    return SUCCESS;
  }

  command bool RootControl.isRoot() {
    return I_AM_ROOT;
  }

  event void Ieee154Address.changed() {}
  event void IPAddress.changed(bool global_valid) {}
}
