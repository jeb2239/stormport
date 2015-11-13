/*-
 * Copyright (c) 1982, 1986, 1988, 1990, 1993, 1995
 *	The Regents of the University of California.  All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 4. Neither the name of the University nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE REGENTS AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE REGENTS OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 *	@(#)tcp_subr.c	8.2 (Berkeley) 5/24/95
 */
 
#include "tcp.h"
#include "tcp_fsm.h"
#include "tcp_var.h"
#include "tcp_seq.h"
#include "tcp_timer.h"
#include "cbuf.c"

/* EXTERN DECLARATIONS FROM TCP_TIMER.H */ 
int tcp_keepinit;		/* time to establish connection */
int tcp_keepidle;		/* time before keepalive probes begin */
int tcp_keepintvl;		/* time between keepalive probes */
//int tcp_keepcnt;			/* number of keepalives */
int tcp_delacktime;		/* time before sending a delayed ACK */
int tcp_maxpersistidle;
int tcp_rexmit_min;
int tcp_rexmit_slop;
int tcp_msl;
//int tcp_ttl;			/* time to live for TCP segs */
int tcp_finwait2_timeout;
 
/* This is based on tcp_init in tcp_subr.c. */
void tcp_init(void) {
#if 0 // I'M NOT USING A HASH TABLE TO STORE TCBS. I SUPPORT SUFFICIENTLY FEW THAT A LIST IS BETTER.
	const char *tcbhash_tuneable;
	int hashsize;

	tcbhash_tuneable = "net.inet.tcp.tcbhashsize";

	if (hhook_head_register(HHOOK_TYPE_TCP, HHOOK_TCP_EST_IN,
	    &V_tcp_hhh[HHOOK_TCP_EST_IN], HHOOK_NOWAIT|HHOOK_HEADISINVNET) != 0)
		printf("%s: WARNING: unable to register helper hook\n", __func__);
	if (hhook_head_register(HHOOK_TYPE_TCP, HHOOK_TCP_EST_OUT,
	    &V_tcp_hhh[HHOOK_TCP_EST_OUT], HHOOK_NOWAIT|HHOOK_HEADISINVNET) != 0)
		printf("%s: WARNING: unable to register helper hook\n", __func__);

	hashsize = TCBHASHSIZE;
	TUNABLE_INT_FETCH(tcbhash_tuneable, &hashsize);
	if (hashsize == 0) {
		/*
		 * Auto tune the hash size based on maxsockets.
		 * A perfect hash would have a 1:1 mapping
		 * (hashsize = maxsockets) however it's been
		 * suggested that O(2) average is better.
		 */
		hashsize = maketcp_hashsize(maxsockets / 4);
		/*
		 * Our historical default is 512,
		 * do not autotune lower than this.
		 */
		if (hashsize < 512)
			hashsize = 512;
		if (bootverbose)
			printf("%s: %s auto tuned to %d\n", __func__,
			    tcbhash_tuneable, hashsize);
	}
	/*
	 * We require a hashsize to be a power of two.
	 * Previously if it was not a power of two we would just reset it
	 * back to 512, which could be a nasty surprise if you did not notice
	 * the error message.
	 * Instead what we do is clip it to the closest power of two lower
	 * than the specified hash value.
	 */
	if (!powerof2(hashsize)) {
		int oldhashsize = hashsize;

		hashsize = maketcp_hashsize(hashsize);
		/* prevent absurdly low value */
		if (hashsize < 16)
			hashsize = 16;
		printf("%s: WARNING: TCB hash size not a power of 2, "
		    "clipped from %d to %d.\n", __func__, oldhashsize,
		    hashsize);
	}
	in_pcbinfo_init(&V_tcbinfo, "tcp", &V_tcb, hashsize, hashsize,
	    "tcp_inpcb", tcp_inpcb_init, NULL, UMA_ZONE_NOFREE,
	    IPI_HASHFIELDS_4TUPLE);

	/*
	 * These have to be type stable for the benefit of the timers.
	 */
	V_tcpcb_zone = uma_zcreate("tcpcb", sizeof(struct tcpcb_mem),
	    NULL, NULL, NULL, NULL, UMA_ALIGN_PTR, UMA_ZONE_NOFREE);
	uma_zone_set_max(V_tcpcb_zone, maxsockets);
	uma_zone_set_warning(V_tcpcb_zone, "kern.ipc.maxsockets limit reached");

	tcp_tw_init();
	syncache_init();
	tcp_hc_init();

	TUNABLE_INT_FETCH("net.inet.tcp.sack.enable", &V_tcp_do_sack);
	V_sack_hole_zone = uma_zcreate("sackhole", sizeof(struct sackhole),
	    NULL, NULL, NULL, NULL, UMA_ALIGN_PTR, UMA_ZONE_NOFREE);

	/* Skip initialization of globals for non-default instances. */
	if (!IS_DEFAULT_VNET(curvnet))
		return;

	tcp_reass_global_init();
#endif
	/* XXX virtualize those bellow? */
	tcp_delacktime = TCPTV_DELACK;
	tcp_keepinit = TCPTV_KEEP_INIT;
	tcp_keepidle = TCPTV_KEEP_IDLE;
	tcp_keepintvl = TCPTV_KEEPINTVL;
	tcp_maxpersistidle = TCPTV_KEEP_IDLE;
	tcp_msl = TCPTV_MSL;
	tcp_rexmit_min = TCPTV_MIN;
	if (tcp_rexmit_min < 1)
		tcp_rexmit_min = 1;
	tcp_rexmit_slop = TCPTV_CPU_VAR;
	tcp_finwait2_timeout = TCPTV_FINWAIT2_TIMEOUT;
	//tcp_tcbhashsize = hashsize;

#if 0 // Ignoring this for now (may bring it back later if necessary)
	if (tcp_soreceive_stream) {
#ifdef INET
		tcp_usrreqs.pru_soreceive = soreceive_stream;
#endif
#ifdef INET6
		tcp6_usrreqs.pru_soreceive = soreceive_stream;
#endif /* INET6 */
	}

#ifdef INET6
#define TCP_MINPROTOHDR (sizeof(struct ip6_hdr) + sizeof(struct tcphdr))
#else /* INET6 */
#define TCP_MINPROTOHDR (sizeof(struct tcpiphdr))
#endif /* INET6 */
	if (max_protohdr < TCP_MINPROTOHDR)
		max_protohdr = TCP_MINPROTOHDR;
	if (max_linkhdr + TCP_MINPROTOHDR > MHLEN)
		panic("tcp_init");
#undef TCP_MINPROTOHDR

	ISN_LOCK_INIT();
	EVENTHANDLER_REGISTER(shutdown_pre_sync, tcp_fini, NULL,
		SHUTDOWN_PRI_DEFAULT);
	EVENTHANDLER_REGISTER(maxsockets_change, tcp_zone_change, NULL,
		EVENTHANDLER_PRI_ANY);
#ifdef TCPPCAP
	tcp_pcap_init();
#endif
#endif
}

 /* This is based on tcp_newtcb in tcp_subr.c, and tcp_usr_attach in tcp_usrreq.c. */
void initialize_tcb(struct tcpcb* tp) {
	int rv1, rv2;
	
    memset(tp, 0x00, sizeof(struct tcpcb));
    // Congestion control algorithm. For now, don't include it.
    // CC_ALGO(tp) = CC_DEFAULT();
    
    /*
	 * Init srtt to TCPTV_SRTTBASE (0), so we can tell that we have no
	 * rtt estimate.  Set rttvar so that srtt + 4 * rttvar gives
	 * reasonable initial retransmit time.
	 */
	tp->t_srtt = TCPTV_SRTTBASE;
	tp->t_rttvar = ((TCPTV_RTOBASE - TCPTV_SRTTBASE) << TCP_RTTVAR_SHIFT) / 4;
	tp->t_rttmin = tcp_rexmit_min;
	tp->t_rxtcur = TCPTV_RTOBASE;
	tp->snd_cwnd = TCP_MAXWIN << TCP_MAX_WINSHIFT;
	tp->snd_ssthresh = TCP_MAXWIN << TCP_MAX_WINSHIFT;
	//tp->t_rcvtime = get_time();
	
	/* From tcp_usr_attach in tcp_usrreq.c. */
	tp->t_state = TCP6S_CLOSED;
	
	rv1 = cbuf_init(tp->sendbuf, 100);
	rv2 = cbuf_init(tp->recvbuf, 100);
	if (rv1 != 0 || rv2 != 0) {
		printf("Buffers too small!\n");
	}
}

/*
 * Fill in the IP and TCP headers for an outgoing packet, given the tcpcb.
 * tcp_template used to store this data in mbufs, but we now recopy it out
 * of the tcpcb each time to conserve mbufs.
 */
 // NOTE: HAS A DIFFERENT SIGNATURE FROM THE ORIGINAL FUNCTION IN tcp_subr.c
void
tcpip_fillheaders(struct tcpcb* tp, void *ip_ptr, void *tcp_ptr)
{
	struct tcphdr *th = (struct tcphdr *)tcp_ptr;

//	INP_WLOCK_ASSERT(inp);

/* I fill in the IP header elsewhere. In send_message in BsdTcpP.nc, to be exact. */
#if 0
#ifdef INET6
	if ((inp->inp_vflag & INP_IPV6) != 0) {
		struct ip6_hdr *ip6;

		ip6 = (struct ip6_hdr *)ip_ptr;
		ip6->ip6_flow = (ip6->ip6_flow & ~IPV6_FLOWINFO_MASK) |
			(inp->inp_flow & IPV6_FLOWINFO_MASK);
		ip6->ip6_vfc = (ip6->ip6_vfc & ~IPV6_VERSION_MASK) |
			(IPV6_VERSION & IPV6_VERSION_MASK);
		ip6->ip6_nxt = IPPROTO_TCP;
		ip6->ip6_plen = htons(sizeof(struct tcphdr));
		ip6->ip6_src = inp->in6p_laddr;
		ip6->ip6_dst = inp->in6p_faddr;
	}
#endif /* INET6 */
#if defined(INET6) && defined(INET)
	else
#endif
#ifdef INET
	{
		struct ip *ip;

		ip = (struct ip *)ip_ptr;
		ip->ip_v = IPVERSION;
		ip->ip_hl = 5;
		ip->ip_tos = inp->inp_ip_tos;
		ip->ip_len = 0;
		ip->ip_id = 0;
		ip->ip_off = 0;
		ip->ip_ttl = inp->inp_ip_ttl;
		ip->ip_sum = 0;
		ip->ip_p = IPPROTO_TCP;
		ip->ip_src = inp->inp_laddr;
		ip->ip_dst = inp->inp_faddr;
	}
#endif /* INET */
#endif
	/* Fill in the TCP header */
	//th->th_sport = inp->inp_lport;
	//th->th_dport = inp->inp_fport;
	th->th_sport = tp->lport;
	th->th_dport = tp->fport;
	th->th_seq = 0;
	th->th_ack = 0;
	th->th_x2 = 0;
	th->th_off = 5;
	th->th_flags = 0;
	th->th_win = 0;
	th->th_urp = 0;
	th->th_sum = 0;		/* in_pseudo() is called later for ipv4 */
}
