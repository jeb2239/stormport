#define NUMBSDTCPACTIVESOCKETS 2
#define NUMBSDTCPPASSIVESOCKETS 2

#define hz 10 // number of ticks per second
#define MILLIS_PER_TICK 100 // number of milliseconds per tick

module BsdTcpP {

    provides {
        interface BSDTCPActiveSocket[uint8_t asockid];
        interface BSDTCPPassiveSocket[uint8_t psockid];
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
    void send_message(struct tcpcb* tp, struct ip6_packet* msg, struct tcphdr* th, uint32_t tlen);
    void set_timer(struct tcpcb* tcb, uint8_t timer_id, uint32_t delay);
    void stop_timer(struct tcpcb* tcb, uint8_t timer_id);
    
#include <bsdtcp/tcp_subr.c>
#include <bsdtcp/tcp_output.c>
#include <bsdtcp/tcp_input.c>
#include <bsdtcp/tcp_timer.c>
#include <bsdtcp/tcp_timewait.c>
#include <bsdtcp/tcp_usrreq.c>
#include <bsdtcp/checksum.c>
    
    struct tcpcb tcbs[NUMBSDTCPACTIVESOCKETS];
    struct tcpcb_listen tcbls[NUMBSDTCPPASSIVESOCKETS];
    uint32_t ticks = 0;
    
    event void Boot.booted() {
        tcp_init();
        initialize_tcb(&tcbs[0]);
        tcbs[0].index = 0;
        call TickTimer.startPeriodic(MILLIS_PER_TICK);
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
            printf("Retransmit\n");
		    tcp_timer_rexmt(tp);
		    break;
	    case TOS_PERSIST:
	        printf("Persist\n");
	        tcp_timer_persist(tp);
		    break;
	    case TOS_KEEP:
	        printf("Keep\n");
	        tcp_timer_keep(tp);
		    break;
	    case TOS_2MSL:
	        printf("2MSL\n");
	        tcp_timer_2msl(tp);
		    break;
        }
    }
    
    event void IP.recv(struct ip6_hdr* iph, void* packet, size_t len,
                       struct ip6_metadata* meta) {
        // This is only being called if the IP address matches mine.
        // Match this to a TCP socket
        volatile int j;
        int i;
        struct tcphdr* th;
        uint16_t sport, dport;
        struct tcpcb* tcb;
        struct tcpcb_listen* tcbl;
        th = (struct tcphdr*) packet;
        sport = th->th_sport; // network byte order
        dport = th->th_dport; // network byte order
        printf("Got a packet!\n");
        if (get_checksum(&iph->ip6_src, &iph->ip6_dst, th, len)) {
            printf("Dropping packet: bad checksum\n");
            return;
        }
        for (i = 0; i < NUMBSDTCPACTIVESOCKETS; i++) {
            tcb = &tcbs[i];
            if (tcb->t_state != TCP6S_CLOSED && dport == tcb->lport && sport == tcb->fport && !memcmp(&iph->ip6_src, &tcb->faddr, sizeof(iph->ip6_src))) {
                // Matches this active socket
                printf("Matches active socket %d\n", i); 
                tcp_input(iph, (struct tcphdr*) packet, &tcbs[i], NULL);
                return;
            }
        }
        for (i = 0; i < NUMBSDTCPPASSIVESOCKETS; i++) {
            tcbl = &tcbls[i];
            if (tcbl->t_state == TCP6S_LISTEN && dport == tcbl->lport) {
                // Matches this passive socket
                printf("Matches passive socket %d\n", i);
                tcp_input(iph, (struct tcphdr*) packet, NULL, &tcbls[i]);
                return;
            }
        }
        printf("Does not match any socket\n");
    }
    
    event void IPAddress.changed(bool valid) {
    }
    
    command int BSDTCPActiveSocket.getID[uint8_t asockid]() {
        return tcbs[asockid].index;
    }
    
    command error_t BSDTCPActiveSocket.bind[uint8_t asockid](uint16_t port) {
        tcbs[asockid].lport = htons(port);
        return SUCCESS;
    }
    
    command error_t BSDTCPPassiveSocket.bind[uint8_t psockid](uint16_t port) {
        tcbls[psockid].lport = htons(port);
        return SUCCESS;
    }
    
    command error_t BSDTCPPassiveSocket.listenaccept[uint8_t psockid](int asockid) {
        tcbls[psockid].t_state = TCP6S_LISTEN;
        if (tcbs[asockid].t_state != TCP6S_CLOSED) {
            printf("Cannot accept connection into active socket that isn't closed\n");
            return -1;
        }
        tcbls[psockid].acceptinto = &tcbs[asockid];
        return SUCCESS;
    }
    
    command error_t BSDTCPActiveSocket.connect[uint8_t asockid](struct sockaddr_in6* addr) {
        struct tcpcb* tp = &tcbs[asockid];
        return tcp6_usr_connect(tp, addr);
    }
    
    command error_t BSDTCPActiveSocket.send[uint8_t asockid](uint8_t* data, uint8_t length) {
        return SUCCESS;
    }
    
    command uint8_t BSDTCPActiveSocket.receive[uint8_t asockid](uint8_t* buffer, uint8_t len) {
        return 0;
    }
    
    command error_t BSDTCPActiveSocket.close[uint8_t asockid]() {
    	return SUCCESS;
    }
    
    command error_t BSDTCPPassiveSocket.close[uint8_t psockid]() {
        return SUCCESS;
    }
    
    command error_t BSDTCPActiveSocket.abort[uint8_t asockid]() {
        return SUCCESS;
    }

    /* Wrapper for underlying C code. */
    void send_message(struct tcpcb* tp, struct ip6_packet* msg, struct tcphdr* th, uint32_t tlen) {
        char destaddr[100];
        call IPAddress.setSource(&msg->ip6_hdr);
        th->th_sum = 0; // should be zero already, but just in case
        th->th_sum = get_checksum(&msg->ip6_hdr.ip6_src, &msg->ip6_hdr.ip6_dst, th, tlen);
        inet_ntop6(&msg->ip6_hdr.ip6_dst, destaddr, 100);
        printf("Sending message to %s\n", destaddr);
        call IP.send(msg);
    }
    
    uint32_t get_ticks() {
        return ticks;
    }
    
    void set_timer(struct tcpcb* tcb, uint8_t timer_id, uint32_t delay) {
        uint8_t tcb_index = (uint8_t) tcb->index;
        uint8_t timer_index = (tcb_index << 2) | timer_id;
        if (timer_id > 0x3) {
            printf("WARNING: setting out of bounds timer!\n");
        }
        printf("Setting timer %d, delay is %d\n", timer_index, delay * MILLIS_PER_TICK);
        call Timer.startOneShot[timer_index](delay * MILLIS_PER_TICK + 10);
    }
    
    void stop_timer(struct tcpcb* tcb, uint8_t timer_id) {
        uint8_t tcb_index = (uint8_t) tcb->index;
        uint8_t timer_index = (tcb_index << 2) & timer_id;
        if (timer_index > 0x3) {
            printf("WARNING: stopping out of bounds timer!\n");
        }
        printf("Stopping timer %d\n", timer_index);
        call Timer.stop[timer_index]();
    }
}
