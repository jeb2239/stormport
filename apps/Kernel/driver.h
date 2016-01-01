#ifndef __DRIVER_H__
#define __DRIVER_H__

#include <lib6lowpan/ip.h>

//TODO

typedef void* driver_callback_t;


typedef struct
{
    uint32_t addr;
    void *r;
} simple_callback_t;

typedef struct
{
    uint32_t addr;
    void *r;
    uint8_t* buffer;
    uint32_t buflen;
    uint8_t src_address [16];
    uint32_t port;
    uint8_t lqi;
    uint8_t rssi;
} __attribute__((__packed__)) udp_callback_t;

typedef struct
{
    uint32_t addr;
    void *r;
    uint32_t status;
} i2c_callback_t;

typedef struct
{
    uint32_t addr;
    void *r;
    uint32_t arg0;
    uint32_t arg1;
} ble_callback_t;

#define NUM_CB_TYPES 5
#define CONNECT_DONE 0x01
#define SEND_READY 0x02
#define RECV_READY 0x04
#define CONNECTION_LOST 0x08
#define ACCEPT_DONE 0x10
#define IS_PASSIVE_CB(cbt) ((cbt) == ACCEPT_DONE)

typedef struct
{
    uint32_t addr;
    void* r;
    uint8_t type;
    uint8_t arg0;
    // SEND_READY, RECV_READY, and CONNECT_DONE don't require additional arguments
    // CONNECTION_LOST requires a one-byte error code
} __attribute__((packed)) tcp_lite_callback_t;

typedef struct
{
    uint32_t addr;
    void* r;
    uint8_t type;
    uint8_t arg0;
    uint16_t src_port;
    struct in6_addr src_address;
    // ACCEPT_DONE requires a one-byte fd and an IP Address and a two byte port.
} __attribute__((packed)) tcp_full_callback_t;

typedef uint32_t syscall_rv_t;
/*
typedef callback_t* pcallback_t @combine("cbcombine");

pcallback_t cbcombine(pcallback_t a, pcallback_t b)
{
    if (a != NULL) return a; return b;
}

typedef uint32_t syscall_rv_t @combine("rvcombine");
syscall_rv_t rvcombine(syscall_rv_t a, syscall_rv_t b)
{
    return a | b;
}
*/

#endif
