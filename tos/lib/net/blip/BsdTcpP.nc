#include <bsdtcp/cbuf.h>
#include <bsdtcp/tcp_var.h>
#include <bsdtcp/tcp_subr.c>
#include <bsdtcp/tcp_output.c>
module BsdTcpP {

    provides {
        interface BSDTCP[uint8_t sockid];
    } uses {
        interface Boot;
        interface IP;
        interface IPAddress;
        interface Timer<TMilli>[uint8_t timerid];
    }
    
} implementation {
    struct tcpcb tcbs[1];
    
    event void Boot.booted() {
        tcp_init();
        initialize_tcb(&tcbs[0]);
        tcbs[0].index = 0;
    }
    
    event void Timer.fired[uint8_t timer_id]() {
        printf("Timer %d fired!\n", timer_id);
        // TODO dispatch to the correct handler.
    }
    
    event void IP.recv(struct ip6_hdr* iph, void* packet, size_t len,
                       struct ip6_metadata* meta) {
    }
    
    event void IPAddress.changed(bool valid) {
    }
    
    command error_t BSDTCP.bind[uint8_t sockid](uint16_t port) {
        return SUCCESS;
    }
    
    command void BSDTCP.listen[uint8_t sockid]() {
    }
    
    command error_t BSDTCP.accept[uint8_t sockid](struct sockaddr_in6* addr) {
        return SUCCESS;
    }
    
    command error_t BSDTCP.connect[uint8_t sockid](struct sockaddr_in6* addr) {
        return SUCCESS;
    }
    
    command error_t BSDTCP.send[uint8_t sockid](uint8_t* data, uint8_t length) {
        return SUCCESS;
    }
    
    command uint8_t BSDTCP.receive[uint8_t sockid](uint8_t* buffer, uint8_t len) {
        return 0;
    }
    
    command error_t BSDTCP.close[uint8_t sockid]() {
    	tcp_output(&tcbs[0]);
    	return SUCCESS;
    }
    
    command error_t BSDTCP.abort[uint8_t sockid]() {
        return SUCCESS;
    }

    /* Wrapper for underlying C code. */
    void send_message(struct ip6_packet* msg) {
        call IP.send(msg);
    }
    
    uint32_t get_time() {
        return call Timer.getNow[0]();
    }
    
    void set_timer(struct tcpcb* tcb, uint8_t timer_id, uint32_t delay) {
        uint8_t tcb_index = (uint8_t) tcb->index;
        uint8_t timer_index = (tcb_index << 2) & timer_id;
        if (timer_index > 0x3) {
            printf("WARNING: setting out of bounds timer!\n");
        }
        call Timer.startOneShot[timer_index](delay);
    }
}
