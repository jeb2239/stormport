#define NUMBSDTCPSOCKETS 1

module BsdTcpP {

    provides {
        interface BSDTCP[uint8_t sockid];
    } uses {
        interface Boot;
        interface IP;
        interface IPAddress;
        interface Timer<TMilli>[uint8_t timerid];
        interface Timer<TMilli> as TickTimer;
    }
    
} implementation {
#include <bsdtcp/cbuf.h>
#include <bsdtcp/tcp_var.h>

    uint32_t get_ticks();
    void send_message(struct ip6_packet* msg);
    void set_timer(struct tcpcb* tcb, uint8_t timer_id, uint32_t delay);
    
#include <bsdtcp/tcp_subr.c>
#include <bsdtcp/tcp_output.c>
#include <bsdtcp/tcp_input.c>
#include <bsdtcp/tcp_timer.c>
#include <bsdtcp/tcp_timewait.c>
    
    struct tcpcb tcbs[1];
    uint32_t ticks = 0;
    
    event void Boot.booted() {
        tcp_init();
        initialize_tcb(&tcbs[0]);
        tcbs[0].index = 0;
        call TickTimer.startPeriodic(500);
    }
    
    event void TickTimer.fired() {
        ticks++;
    }
    
    event void Timer.fired[uint8_t timer_id]() {
        struct tcpcb* tp;
        if (call Timer.isRunning[timer_id]()) {
            // In case the timer was rescheduled after this was posted but before this runs
            return;
        }
        printf("Timer %d fired!\n", timer_id);
        
        tp = &tcbs[timer_id >> 2];
        timer_id &= 0x3;
        
        switch(timer_id) {
        case TOS_REXMT:
		    tcp_timer_rexmt(tp);
		    break;
	    case TOS_PERSIST:
	        tcp_timer_persist(tp);
		    break;
	    case TOS_KEEP:
	        tcp_timer_keep(tp);
		    break;
	    case TOS_2MSL:
	        tcp_timer_2msl(tp);
		    break;
        }
    }
    
    event void IP.recv(struct ip6_hdr* iph, void* packet, size_t len,
                       struct ip6_metadata* meta) {
        // This is only being called if the IP address matches mine.
        // Match this to a TCP socket
/*        int i;
        struct tcphdr* th;
        uint16_t port;
        struct tcpcb* tcb;
        th = (struct tcphdr*) (iph + 1);
        port = ntohs(th->th_dport);
        for (i = 0; i < NUMBSDTCPSOCKETS; i++) {
            tcb = &tcbs[i];
            if (tcb->t_state != TCP6S_CLOSED && port == ntohs(tcb->lport)) {
                // Matches this socket
                // TODO check the checksum
                tcp_input(iph, (struct tcphdr*) packet, &tcbs[i]);
            }
        }*/
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
    
    uint32_t get_ticks() {
        return ticks;
    }
    
    void set_timer(struct tcpcb* tcb, uint8_t timer_id, uint32_t delay) {
/*
        uint8_t tcb_index = (uint8_t) tcb->index;
        uint8_t timer_index = (tcb_index << 2) & timer_id;
        if (timer_index > 0x3) {
            printf("WARNING: setting out of bounds timer!\n");
        }
        call Timer.startOneShot[timer_index](delay);
        */
    }
}
