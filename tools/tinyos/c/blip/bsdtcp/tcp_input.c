/*-
 * Copyright (c) 1982, 1986, 1988, 1990, 1993, 1994, 1995
 *	The Regents of the University of California.  All rights reserved.
 * Copyright (c) 2007-2008,2010
 *	Swinburne University of Technology, Melbourne, Australia.
 * Copyright (c) 2009-2010 Lawrence Stewart <lstewart@freebsd.org>
 * Copyright (c) 2010 The FreeBSD Foundation
 * Copyright (c) 2010-2011 Juniper Networks, Inc.
 * All rights reserved.
 *
 * Portions of this software were developed at the Centre for Advanced Internet
 * Architectures, Swinburne University of Technology, by Lawrence Stewart,
 * James Healy and David Hayes, made possible in part by a grant from the Cisco
 * University Research Program Fund at Community Foundation Silicon Valley.
 *
 * Portions of this software were developed at the Centre for Advanced
 * Internet Architectures, Swinburne University of Technology, Melbourne,
 * Australia by David Hayes under sponsorship from the FreeBSD Foundation.
 *
 * Portions of this software were developed by Robert N. M. Watson under
 * contract to Juniper Networks, Inc.
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
 *	@(#)tcp_input.c	8.12 (Berkeley) 5/24/95
 */


/*
 * Determine a reasonable value for maxseg size.
 * If the route is known, check route for mtu.
 * If none, use an mss that can be handled on the outgoing interface
 * without forcing IP to fragment.  If no route is found, route has no mtu,
 * or the destination isn't local, use a default, hopefully conservative
 * size (usually 512 or the default IP max size, but no more than the mtu
 * of the interface), as we can't discover anything about intervening
 * gateways or networks.  We also initialize the congestion/slow start
 * window to be a single segment if the destination isn't local.
 * While looking at the routing entry, we also initialize other path-dependent
 * parameters from pre-set or cached values in the routing entry.
 *
 * Also take into account the space needed for options that we
 * send regularly.  Make maxseg shorter by that amount to assure
 * that we can send maxseg amount of data even when the options
 * are present.  Store the upper limit of the length of options plus
 * data in maxopd.
 *
 * NOTE that this routine is only called when we process an incoming
 * segment, or an ICMP need fragmentation datagram. Outgoing SYN/ACK MSS
 * settings are handled in tcp_mssopt().
 */

#include "icmp_var.h"
#include "ip.h"
#include "ip6.h"

const int tcprexmtthresh = 3;
const int V_drop_synfin = 0;
// Copied from in.h
#define IPPROTO_DONE 267
// My own constant
#define RELOOKUP_REQUIRED -1

// I may turn on some of these flags later
int V_tcp_do_ecn = 0;
int V_tcp_do_rfc3042 = 0;
int tcp_fast_finwait2_recycle = 0;

// Copied from sys/libkern.h
static int imax(int a, int b) { return (a > b ? a : b); }
static int imin(int a, int b) { return (a < b ? a : b); }

static void
tcp_do_segment(struct ip6_hdr* ip6, struct tcphdr *th,
    struct tcpcb *tp, int drop_hdrlen, int tlen, uint8_t iptos);

/*
 * Issue RST and make ACK acceptable to originator of segment.
 * The mbuf must still include the original packet header.
 * tp may be NULL.
 */
/* Original signature was:
   static void tcp_dropwithreset(struct mbuf *m, struct tcphdr *th, struct tcpcb *tp,
    int tlen, int rstreason)
*/
static void
tcp_dropwithreset(struct ip6_hdr* ip6, struct tcphdr *th, struct tcpcb *tp,
    int tlen, int rstreason)
{
#if 0
#ifdef INET
	struct ip *ip;
#endif
#ifdef INET6
	struct ip6_hdr *ip6;
#endif

	if (tp != NULL) {
		INP_WLOCK_ASSERT(tp->t_inpcb);
	}

	/* Don't bother if destination was broadcast/multicast. */
	if ((th->th_flags & TH_RST) || m->m_flags & (M_BCAST|M_MCAST))
		goto drop;
#ifdef INET6
	if (mtod(m, struct ip *)->ip_v == 6) {
		ip6 = mtod(m, struct ip6_hdr *);
		if (IN6_IS_ADDR_MULTICAST(&ip6->ip6_dst) ||
		    IN6_IS_ADDR_MULTICAST(&ip6->ip6_src))
			goto drop;
		/* IPv6 anycast check is done at tcp6_input() */
	}
#endif
#if defined(INET) && defined(INET6)
	else
#endif
#ifdef INET
	{x
		ip = mtod(m, struct ip *);
		if (IN_MULTICAST(ntohl(ip->ip_dst.s_addr)) ||
		    IN_MULTICAST(ntohl(ip->ip_src.s_addr)) ||
		    ip->ip_src.s_addr == htonl(INADDR_BROADCAST) ||
		    in_broadcast(ip->ip_dst, m->m_pkthdr.rcvif))
			goto drop;
	}
#endif

	/* Perform bandwidth limiting. */
	if (badport_bandlim(rstreason) < 0)
		goto drop;
#endif
	/* tcp_respond consumes the mbuf chain. */
	if (th->th_flags & TH_ACK) {
//		tcp_respond(tp, mtod(m, void *), th, m, (tcp_seq)0,
//		    th->th_ack, TH_RST);
		tcp_respond(tp, ip6, th, (tcp_seq) 0, th->th_ack, TH_RST);
	} else {
		if (th->th_flags & TH_SYN)
			tlen++;
//		tcp_respond(tp, mtod(m, void *), th, m, th->th_seq+tlen,
//		    (tcp_seq)0, TH_RST|TH_ACK);
		tcp_respond(tp, ip6, th, th->th_seq + tlen, (tcp_seq) 0, TH_RST | TH_ACK);
	}
	return;
/*
drop:
	m_freem(m);
*/
}

void
tcp_mss_update(struct tcpcb *tp, int offer, int mtuoffer,
    struct hc_metrics_lite *metricptr, struct tcp_ifcap *cap)
{
#if 0
	int mss = 0;
	u_long maxmtu = 0;
	struct inpcb *inp = tp->t_inpcb;
	struct hc_metrics_lite metrics;
	int origoffer;
#ifdef INET6
	int isipv6 = ((inp->inp_vflag & INP_IPV6) != 0) ? 1 : 0;
	size_t min_protoh = isipv6 ?
			    sizeof (struct ip6_hdr) + sizeof (struct tcphdr) :
			    sizeof (struct tcpiphdr);
#else
	const size_t min_protoh = sizeof(struct tcpiphdr);
#endif

	INP_WLOCK_ASSERT(tp->t_inpcb);

	if (mtuoffer != -1) {
		KASSERT(offer == -1, ("%s: conflict", __func__));
		offer = mtuoffer - min_protoh;
	}
	origoffer = offer;

	/* Initialize. */
//#ifdef INET6
	if (isipv6) {
		maxmtu = tcp_maxmtu6(&inp->inp_inc, cap);
		tp->t_maxopd = tp->t_maxseg = V_tcp_v6mssdflt;
	}
//#endif
#if 0 // We're IPv6
#if defined(INET) && defined(INET6)
	else
#endif
#ifdef INET
	{
		maxmtu = tcp_maxmtu(&inp->inp_inc, cap);
		tp->t_maxopd = tp->t_maxseg = V_tcp_mssdflt;
	}
#endif
#endif
	/*
	 * No route to sender, stay with default mss and return.
	 */
	if (maxmtu == 0) {
#endif
		// ALWAYS BEHAVE LIKE A CACHE MISS. I DON'T WANT TO SPEND MEMORY ON THIS.
		/*
		 * In case we return early we need to initialize metrics
		 * to a defined state as tcp_hc_get() would do for us
		 * if there was no cache hit.
		 */
		if (metricptr != NULL)
			bzero(metricptr, sizeof(struct hc_metrics_lite));
		return;
#if 0
	}

	/* What have we got? */
	switch (offer) {
		case 0:
			/*
			 * Offer == 0 means that there was no MSS on the SYN
			 * segment, in this case we use tcp_mssdflt as
			 * already assigned to t_maxopd above.
			 */
			offer = tp->t_maxopd;
			break;

		case -1:
			/*
			 * Offer == -1 means that we didn't receive SYN yet.
			 */
			/* FALLTHROUGH */

		default:
			/*
			 * Prevent DoS attack with too small MSS. Round up
			 * to at least minmss.
			 */
			offer = max(offer, V_tcp_minmss);
	}

	/*
	 * rmx information is now retrieved from tcp_hostcache.
	 */
	tcp_hc_get(&inp->inp_inc, &metrics);
	if (metricptr != NULL)
		bcopy(&metrics, metricptr, sizeof(struct hc_metrics_lite));

	/*
	 * If there's a discovered mtu in tcp hostcache, use it.
	 * Else, use the link mtu.
	 */
	if (metrics.rmx_mtu)
		mss = min(metrics.rmx_mtu, maxmtu) - min_protoh;
	else {
#ifdef INET6
		if (isipv6) {
			mss = maxmtu - min_protoh;
			if (!V_path_mtu_discovery &&
			    !in6_localaddr(&inp->in6p_faddr))
				mss = min(mss, V_tcp_v6mssdflt);
		}
#endif
#if defined(INET) && defined(INET6)
		else
#endif
#ifdef INET
		{
			mss = maxmtu - min_protoh;
			if (!V_path_mtu_discovery &&
			    !in_localaddr(inp->inp_faddr))
				mss = min(mss, V_tcp_mssdflt);
		}
#endif
		/*
		 * XXX - The above conditional (mss = maxmtu - min_protoh)
		 * probably violates the TCP spec.
		 * The problem is that, since we don't know the
		 * other end's MSS, we are supposed to use a conservative
		 * default.  But, if we do that, then MTU discovery will
		 * never actually take place, because the conservative
		 * default is much less than the MTUs typically seen
		 * on the Internet today.  For the moment, we'll sweep
		 * this under the carpet.
		 *
		 * The conservative default might not actually be a problem
		 * if the only case this occurs is when sending an initial
		 * SYN with options and data to a host we've never talked
		 * to before.  Then, they will reply with an MSS value which
		 * will get recorded and the new parameters should get
		 * recomputed.  For Further Study.
		 */
	}
	mss = min(mss, offer);

	/*
	 * Sanity check: make sure that maxopd will be large
	 * enough to allow some data on segments even if the
	 * all the option space is used (40bytes).  Otherwise
	 * funny things may happen in tcp_output.
	 */
	mss = max(mss, 64);

	/*
	 * maxopd stores the maximum length of data AND options
	 * in a segment; maxseg is the amount of data in a normal
	 * segment.  We need to store this value (maxopd) apart
	 * from maxseg, because now every segment carries options
	 * and thus we normally have somewhat less data in segments.
	 */
	tp->t_maxopd = mss;

	/*
	 * origoffer==-1 indicates that no segments were received yet.
	 * In this case we just guess.
	 */
	if ((tp->t_flags & (TF_REQ_TSTMP|TF_NOOPT)) == TF_REQ_TSTMP &&
	    (origoffer == -1 ||
	     (tp->t_flags & TF_RCVD_TSTMP) == TF_RCVD_TSTMP))
		mss -= TCPOLEN_TSTAMP_APPA;

	tp->t_maxseg = mss;
#endif
}

/*
 * TCP input handling is split into multiple parts:
 *   tcp6_input is a thin wrapper around tcp_input for the extended
 *	ip6_protox[] call format in ip6_input
 *   tcp_input handles primary segment validation, inpcb lookup and
 *	SYN processing on listen sockets
 *   tcp_do_segment processes the ACK and text of the segment for
 *	establishing, established and closing connections
 */
/* The signature of this function was originally:
   tcp_input(struct mbuf **mp, int *offp, int proto) */
/* NOTE: tcp_fields_to_host(th) must be called before this function is called. */
int
tcp_input(struct ip6_hdr* ip6, struct tcphdr* th, struct tcpcb* tp, struct tcpcb_listen* tpl)
{
	int tlen = 0, off;
	int thflags;
	uint8_t iptos = 0;
	int drop_hdrlen;
	int rstreason = 0;
	uint32_t ticks = get_ticks();
	KASSERT(tp || tpl, ("One of tp and tpl must be positive"));
#if 0
	struct mbuf *m = *mp;
	struct tcphdr *th = NULL;
	struct ip *ip = NULL;
	struct inpcb *inp = NULL;
	struct tcpcb *tp = NULL;
	struct socket *so = NULL;
	u_char *optp = NULL;
	int off0;
	int optlen = 0;
#ifdef INET
	int len;
#endif
	int tlen = 0, off;
	int drop_hdrlen;
	int thflags;
	int rstreason = 0;	/* For badport_bandlim accounting purposes */
#ifdef TCP_SIGNATURE
	uint8_t sig_checked = 0;
#endif
	uint8_t iptos = 0;
	struct m_tag *fwd_tag = NULL;
#ifdef INET6
	struct ip6_hdr *ip6 = NULL;
	int isipv6;
#else
	const void *ip6 = NULL;
#endif /* INET6 */
	struct tcpopt to;		/* options in this segment */
	char *s = NULL;			/* address and port logging */
	int ti_locked;
#define	TI_UNLOCKED	1
#define	TI_RLOCKED	2
#endif
#if 0 // DON'T NEED THE PREPROCESSING; I'LL DO THAT MYSELF
#ifdef TCPDEBUG
	/*
	 * The size of tcp_saveipgen must be the size of the max ip header,
	 * now IPv6.
	 */
	u_char tcp_saveipgen[IP6_HDR_LEN];
	struct tcphdr tcp_savetcp;
	short ostate = 0;
#endif

#ifdef INET6
	isipv6 = (mtod(m, struct ip *)->ip_v == 6) ? 1 : 0;
#endif

	off0 = *offp;
	m = *mp;
	*mp = NULL;
	to.to_flags = 0;
	TCPSTAT_INC(tcps_rcvtotal);

#ifdef INET6
	if (isipv6) {
		/* IP6_EXTHDR_CHECK() is already done at tcp6_input(). */

		if (m->m_len < (sizeof(*ip6) + sizeof(*th))) {
			m = m_pullup(m, sizeof(*ip6) + sizeof(*th));
			if (m == NULL) {
				TCPSTAT_INC(tcps_rcvshort);
				return (IPPROTO_DONE);
			}
		}

		ip6 = mtod(m, struct ip6_hdr *);
		th = (struct tcphdr *)((caddr_t)ip6 + off0);
		tlen = sizeof(*ip6) + ntohs(ip6->ip6_plen) - off0;
		if (m->m_pkthdr.csum_flags & CSUM_DATA_VALID_IPV6) {
			if (m->m_pkthdr.csum_flags & CSUM_PSEUDO_HDR)
				th->th_sum = m->m_pkthdr.csum_data;
			else
				th->th_sum = in6_cksum_pseudo(ip6, tlen,
				    IPPROTO_TCP, m->m_pkthdr.csum_data);
			th->th_sum ^= 0xffff;
		} else
			th->th_sum = in6_cksum(m, IPPROTO_TCP, off0, tlen);
		if (th->th_sum) {
			TCPSTAT_INC(tcps_rcvbadsum);
			goto drop;
		}

		/*
		 * Be proactive about unspecified IPv6 address in source.
		 * As we use all-zero to indicate unbounded/unconnected pcb,
		 * unspecified IPv6 address can be used to confuse us.
		 *
		 * Note that packets with unspecified IPv6 destination is
		 * already dropped in ip6_input.
		 */
		if (IN6_IS_ADDR_UNSPECIFIED(&ip6->ip6_src)) {
			/* XXX stat */
			goto drop;
		}
	}
#endif
#if defined(INET) && defined(INET6)
	else
#endif
#ifdef INET
	{
		/*
		 * Get IP and TCP header together in first mbuf.
		 * Note: IP leaves IP header in first mbuf.
		 */
		if (off0 > sizeof (struct ip)) {
			ip_stripoptions(m);
			off0 = sizeof(struct ip);
		}
		if (m->m_len < sizeof (struct tcpiphdr)) {
			if ((m = m_pullup(m, sizeof (struct tcpiphdr)))
			    == NULL) {
				TCPSTAT_INC(tcps_rcvshort);
				return (IPPROTO_DONE);
			}
		}
		ip = mtod(m, struct ip *);
		th = (struct tcphdr *)((caddr_t)ip + off0);
		tlen = ntohs(ip->ip_len) - off0;

		if (m->m_pkthdr.csum_flags & CSUM_DATA_VALID) {
			if (m->m_pkthdr.csum_flags & CSUM_PSEUDO_HDR)
				th->th_sum = m->m_pkthdr.csum_data;
			else
				th->th_sum = in_pseudo(ip->ip_src.s_addr,
				    ip->ip_dst.s_addr,
				    htonl(m->m_pkthdr.csum_data + tlen +
				    IPPROTO_TCP));
			th->th_sum ^= 0xffff;
		} else {
			struct ipovly *ipov = (struct ipovly *)ip;

			/*
			 * Checksum extended TCP header and data.
			 */
			len = off0 + tlen;
			bzero(ipov->ih_x1, sizeof(ipov->ih_x1));
			ipov->ih_len = htons(tlen);
			th->th_sum = in_cksum(m, len);
			/* Reset length for SDT probes. */
			ip->ip_len = htons(tlen + off0);
		}

		if (th->th_sum) {
			TCPSTAT_INC(tcps_rcvbadsum);
			goto drop;
		}
		/* Re-initialization for later version check */
		ip->ip_v = IPVERSION;
	}
#endif /* INET */
#endif
	tlen = ntohs(ip6->ip6_plen); // assume *off == sizeof(*ip6)
/*
#ifdef INET6
	if (isipv6)
*/
		iptos = (ntohl(ip6->ip6_flow) >> 20) & 0xff;
/*
#endif
#if defined(INET) && defined(INET6)
	else
#endif
#ifdef INET
		iptos = ip->ip_tos;
#endif
*/
	/*
	 * Check that TCP offset makes sense,
	 * pull out TCP options and adjust length.		XXX
	 */
	off = th->th_off << 2;
	if (off < sizeof (struct tcphdr) || off > tlen) {
//		TCPSTAT_INC(tcps_rcvbadoff);
		goto drop;
	}
	tlen -= off;	/* tlen is used instead of ti->ti_len */
	// It seems that now tlen is the length of the data
	/* FOR NOW, OMIT HANDLING OF EXTRA OPTIONS.
	   I WILL PUT THIS BACK FOR TCP TIMESTAMPS. */
#if 0
	if (off > sizeof (struct tcphdr)) {
#ifdef INET6
		if (isipv6) {
			IP6_EXTHDR_CHECK(m, off0, off, IPPROTO_DONE);
			ip6 = mtod(m, struct ip6_hdr *);
			th = (struct tcphdr *)((caddr_t)ip6 + off0);
		}
#endif
#if defined(INET) && defined(INET6)
		else
#endif
#ifdef INET
		{
			if (m->m_len < sizeof(struct ip) + off) {
				if ((m = m_pullup(m, sizeof (struct ip) + off))
				    == NULL) {
					TCPSTAT_INC(tcps_rcvshort);
					return (IPPROTO_DONE);
				}
				ip = mtod(m, struct ip *);
				th = (struct tcphdr *)((caddr_t)ip + off0);
			}
		}
#endif
		optlen = off - sizeof (struct tcphdr);
		optp = (u_char *)(th + 1);
	}
#endif
	thflags = th->th_flags;

	/*
	 * Convert TCP protocol specific fields to host format.
	 * Sam: I moved this call before this function, in case we return early on a time-wait socket and start over.
	 */
	//tcp_fields_to_host(th);

	/*
	 * Delay dropping TCP, IP headers, IPv6 ext headers, and TCP options.
	 */
	drop_hdrlen = /*off0*/sizeof(struct ip6_hdr) + off; // Assume off0 is sizeof(struct ip6_hdr);

	/*
	 * Locate pcb for segment; if we're likely to add or remove a
	 * connection then first acquire pcbinfo lock.  There are three cases
	 * where we might discover later we need a write lock despite the
	 * flags: ACKs moving a connection out of the syncache, ACKs for a
	 * connection in TIMEWAIT and SYNs not targeting a listening socket.
	 */
#if 0 // Ignore synchronization code
	if ((thflags & (TH_FIN | TH_RST)) != 0) {
		INP_INFO_RLOCK(&V_tcbinfo);
		ti_locked = TI_RLOCKED;
	} else
		ti_locked = TI_UNLOCKED;
#endif
/* I BELIEVE THAT THIS IS ALREADY HANDLED AT A LOWER LAYER ON STORM */
#if 0
	/*
	 * Grab info from PACKET_TAG_IPFORWARD tag prepended to the chain.
	 */
        if (
#ifdef INET6
	    (isipv6 && (m->m_flags & M_IP6_NEXTHOP))
#ifdef INET
	    || (!isipv6 && (m->m_flags & M_IP_NEXTHOP))
#endif
#endif
#if defined(INET) && !defined(INET6)
	    (m->m_flags & M_IP_NEXTHOP)
#endif
	    )
		fwd_tag = m_tag_find(m, PACKET_TAG_IPFORWARD, NULL);
#endif
findpcb:
#if 0 // I DON'T NEED ANY OF THEIR SYNCHRONIZATION CODE
#ifdef INVARIANTS
	if (ti_locked == TI_RLOCKED) {
		INP_INFO_RLOCK_ASSERT(&V_tcbinfo);
	} else {
		INP_INFO_UNLOCK_ASSERT(&V_tcbinfo);
	}
#endif
#endif
#if 0 // THIS IS THE CODE TO LOOK UP THE SOCKET. I'VE ALREADY DONE THAT
#ifdef INET6
	if (isipv6 && fwd_tag != NULL) {
		struct sockaddr_in6 *next_hop6;

		next_hop6 = (struct sockaddr_in6 *)(fwd_tag + 1);
		/*
		 * Transparently forwarded. Pretend to be the destination.
		 * Already got one like this?
		 */
		inp = in6_pcblookup_mbuf(&V_tcbinfo,
		    &ip6->ip6_src, th->th_sport, &ip6->ip6_dst, th->th_dport,
		    INPLOOKUP_WLOCKPCB, m->m_pkthdr.rcvif, m);
		if (!inp) {
			/*
			 * It's new.  Try to find the ambushing socket.
			 * Because we've rewritten the destination address,
			 * any hardware-generated hash is ignored.
			 */
			inp = in6_pcblookup(&V_tcbinfo, &ip6->ip6_src,
			    th->th_sport, &next_hop6->sin6_addr,
			    next_hop6->sin6_port ? ntohs(next_hop6->sin6_port) :
			    th->th_dport, INPLOOKUP_WILDCARD |
			    INPLOOKUP_WLOCKPCB, m->m_pkthdr.rcvif);
		}
	} else if (isipv6) {
		inp = in6_pcblookup_mbuf(&V_tcbinfo, &ip6->ip6_src,
		    th->th_sport, &ip6->ip6_dst, th->th_dport,
		    INPLOOKUP_WILDCARD | INPLOOKUP_WLOCKPCB,
		    m->m_pkthdr.rcvif, m);
	}
#endif /* INET6 */
#if defined(INET6) && defined(INET)
	else
#endif
#ifdef INET
	if (fwd_tag != NULL) {
		struct sockaddr_in *next_hop;

		next_hop = (struct sockaddr_in *)(fwd_tag+1);
		/*
		 * Transparently forwarded. Pretend to be the destination.
		 * already got one like this?
		 */
		inp = in_pcblookup_mbuf(&V_tcbinfo, ip->ip_src, th->th_sport,
		    ip->ip_dst, th->th_dport, INPLOOKUP_WLOCKPCB,
		    m->m_pkthdr.rcvif, m);
		if (!inp) {
			/*
			 * It's new.  Try to find the ambushing socket.
			 * Because we've rewritten the destination address,
			 * any hardware-generated hash is ignored.
			 */
			inp = in_pcblookup(&V_tcbinfo, ip->ip_src,
			    th->th_sport, next_hop->sin_addr,
			    next_hop->sin_port ? ntohs(next_hop->sin_port) :
			    th->th_dport, INPLOOKUP_WILDCARD |
			    INPLOOKUP_WLOCKPCB, m->m_pkthdr.rcvif);
		}
	} else
		inp = in_pcblookup_mbuf(&V_tcbinfo, ip->ip_src,
		    th->th_sport, ip->ip_dst, th->th_dport,
		    INPLOOKUP_WILDCARD | INPLOOKUP_WLOCKPCB,
		    m->m_pkthdr.rcvif, m);
#endif /* INET */

	/*
	 * If the INPCB does not exist then all data in the incoming
	 * segment is discarded and an appropriate RST is sent back.
	 * XXX MRT Send RST using which routing table?
	 */
	if (inp == NULL) {
		/*
		 * Log communication attempts to ports that are not
		 * in use.
		 */
		if ((tcp_log_in_vain == 1 && (thflags & TH_SYN)) ||
		    tcp_log_in_vain == 2) {
			if ((s = tcp_log_vain(NULL, th, (void *)ip, ip6)))
				log(LOG_INFO, "%s; %s: Connection attempt "
				    "to closed port\n", s, __func__);
		}
		/*
		 * When blackholing do not respond with a RST but
		 * completely ignore the segment and drop it.
		 */
		if ((V_blackhole == 1 && (thflags & TH_SYN)) ||
		    V_blackhole == 2)
			goto dropunlock;

		rstreason = BANDLIM_RST_CLOSEDPORT;
		goto dropwithreset;
	}
	INP_WLOCK_ASSERT(inp);
	if ((inp->inp_flowtype == M_HASHTYPE_NONE) &&
	    (M_HASHTYPE_GET(m) != M_HASHTYPE_NONE) &&
	    ((inp->inp_socket == NULL) ||
	    (inp->inp_socket->so_options & SO_ACCEPTCONN) == 0)) {
		inp->inp_flowid = m->m_pkthdr.flowid;
		inp->inp_flowtype = M_HASHTYPE_GET(m);
	}
#ifdef IPSEC
#ifdef INET6
	if (isipv6 && ipsec6_in_reject(m, inp)) {
		goto dropunlock;
	} else
#endif /* INET6 */
	if (ipsec4_in_reject(m, inp) != 0) {
		goto dropunlock;
	}
#endif /* IPSEC */

	/*
	 * Check the minimum TTL for socket.
	 */
	if (inp->inp_ip_minttl != 0) {
#ifdef INET6
		if (isipv6 && inp->inp_ip_minttl > ip6->ip6_hlim)
			goto dropunlock;
		else
#endif
		if (inp->inp_ip_minttl > ip->ip_ttl)
			goto dropunlock;
	}
#endif
	/*
	 * A previous connection in TIMEWAIT state is supposed to catch stray
	 * or duplicate segments arriving late.  If this segment was a
	 * legitimate new connection attempt, the old INPCB gets removed and
	 * we can try again to find a listening socket.
	 *
	 * At this point, due to earlier optimism, we may hold only an inpcb
	 * lock, and not the inpcbinfo write lock.  If so, we need to try to
	 * acquire it, or if that fails, acquire a reference on the inpcb,
	 * drop all locks, acquire a global write lock, and then re-acquire
	 * the inpcb lock.  We may at that point discover that another thread
	 * has tried to free the inpcb, in which case we need to loop back
	 * and try to find a new inpcb to deliver to.
	 *
	 * XXXRW: It may be time to rethink timewait locking.
	 */
relocked:
	if (tp && /*inp->inp_flags & INP_TIMEWAIT*/tp->t_state == TCP6S_TIME_WAIT) {
#if 0 // REMOVE SYNCHRONIZATION
		if (ti_locked == TI_UNLOCKED) {
			if (INP_INFO_TRY_RLOCK(&V_tcbinfo) == 0) {
				in_pcbref(inp);
//				INP_WUNLOCK(inp);
//				INP_INFO_RLOCK(&V_tcbinfo);
//				ti_locked = TI_RLOCKED;
//				INP_WLOCK(inp);
				if (in_pcbrele_wlocked(inp)) {
					inp = NULL;
					goto findpcb;
				}
			} else
				ti_locked = TI_RLOCKED;
		}
		INP_INFO_RLOCK_ASSERT(&V_tcbinfo);
#endif
//		if (thflags & TH_SYN)
//			tcp_dooptions(&to, optp, optlen, TO_SYN);
		/*
		 * NB: tcp_twcheck unlocks the INP and frees the mbuf.
		 */
		if (tcp_twcheck(tp,/*inp, &to,*/ th, /*m,*/ tlen))
			//goto findpcb;
			return (RELOOKUP_REQUIRED);
//		INP_INFO_RUNLOCK(&V_tcbinfo);
		return (IPPROTO_DONE);
	}
	/*
	 * The TCPCB may no longer exist if the connection is winding
	 * down or it is in the CLOSED state.  Either way we drop the
	 * segment and send an appropriate response.
	 */
#if 0
	tp = intotcpcb(inp);
	if (tp == NULL || tp->t_state == TCPS_CLOSED) {
		rstreason = BANDLIM_RST_CLOSEDPORT;
		goto dropwithreset;
	}

#ifdef TCP_OFFLOAD
	if (tp->t_flags & TF_TOE) {
		tcp_offload_input(tp, m);
		m = NULL;	/* consumed by the TOE driver */
		goto dropunlock;
	}
#endif
#endif
	/*
	 * We've identified a valid inpcb, but it could be that we need an
	 * inpcbinfo write lock but don't hold it.  In this case, attempt to
	 * acquire using the same strategy as the TIMEWAIT case above.  If we
	 * relock, we have to jump back to 'relocked' as the connection might
	 * now be in TIMEWAIT.
	 */
#if 0
#ifdef INVARIANTS
	if ((thflags & (TH_FIN | TH_RST)) != 0)
		INP_INFO_RLOCK_ASSERT(&V_tcbinfo);
#endif
	if (!((tp->t_state == TCPS_ESTABLISHED && (thflags & TH_SYN) == 0) ||
	    (tp->t_state == TCPS_LISTEN && (thflags & TH_SYN)))) {
		if (ti_locked == TI_UNLOCKED) {
			if (INP_INFO_TRY_RLOCK(&V_tcbinfo) == 0) {
				in_pcbref(inp);
				INP_WUNLOCK(inp);
				INP_INFO_RLOCK(&V_tcbinfo);
				ti_locked = TI_RLOCKED;
				INP_WLOCK(inp);
				if (in_pcbrele_wlocked(inp)) {
					inp = NULL;
					goto findpcb;
				}
				goto relocked;
			} else
				ti_locked = TI_RLOCKED;
		}
		INP_INFO_RLOCK_ASSERT(&V_tcbinfo);
	}

#ifdef MAC
	INP_WLOCK_ASSERT(inp);
	if (mac_inpcb_check_deliver(inp, m))
		goto dropunlock;
#endif
	so = inp->inp_socket;
	KASSERT(so != NULL, ("%s: so == NULL", __func__));
#ifdef TCPDEBUG
	if (so->so_options & SO_DEBUG) {
		ostate = tp->t_state;
#ifdef INET6
		if (isipv6) {
			bcopy((char *)ip6, (char *)tcp_saveipgen, sizeof(*ip6));
		} else
#endif
			bcopy((char *)ip, (char *)tcp_saveipgen, sizeof(*ip));
		tcp_savetcp = *th;
	}
#endif /* TCPDEBUG */
#endif
	/*
	 * When the socket is accepting connections (the INPCB is in LISTEN
	 * state) we look into the SYN cache if this is a new connection
	 * attempt or the completion of a previous one.
	 */

	if (/*so->so_options & SO_ACCEPTCONN*/tp == NULL) {
		KASSERT(tpl->t_state == TCP6S_LISTEN, ("listen socket must be in listening state!"));
#if 0 // HANDLING OF SYN_RECEIVED HAPPENS NORMALLY, EVEN IF THIS BRANCH ISN'T TAKEN
		//struct in_conninfo inc;
		struct syncache_ent inc;

//		KASSERT(tp->t_state == TCPS_LISTEN, ("%s: so accepting but "
//		    "tp not listening", __func__));
//		bzero(&inc, sizeof(inc));
		memset(&inc, 0, sizeof(inc));
//#ifdef INET6
//		if (isipv6) {
//			inc.inc_flags |= INC_ISIPV6;
//			inc.inc6_faddr = ip6->ip6_src;
//			inc.inc6_laddr = ip6->ip6_dst;
			inc.faddr = ip6->ip6_src;
			inc.laddr = ip6->ip6_dst;
//		} else
//#endif
//		{
//			inc.inc_faddr = ip->ip_src;
//			inc.inc_laddr = ip->ip_dst;
//		}
//		inc.inc_fport = th->th_sport;
//		inc.inc_lport = th->th_dport;
//		inc.inc_fibnum = so->so_fibnum;
		inc.fport = th->th_sport;
		inc.lport = th->th_dport;	

		/*
		 * Check for an existing connection attempt in syncache if
		 * the flag is only ACK.  A successful lookup creates a new
		 * socket appended to the listen queue in SYN_RECEIVED state.
		 */
		if (tp->state == TCP6S_SYN_RECEIVED && (thflags & (TH_RST|TH_ACK|TH_SYN)) == TH_ACK) {

//			INP_INFO_RLOCK_ASSERT(&V_tcbinfo);
			/*
			 * Parse the TCP options here because
			 * syncookies need access to the reflected
			 * timestamp.
			 */
			tcp_dooptions(&to, optp, optlen, 0);
#if 0
			/*
			 * NB: syncache_expand() doesn't unlock
			 * inp and tcpinfo locks.
			 */
			if (!syncache_expand(&inc,/* &to, */th, tp, tp->acceptinto/*m*/)) {
				/*
				 * No syncache entry or ACK was not
				 * for our SYN/ACK.  Send a RST.
				 * NB: syncache did its own logging
				 * of the failure cause.
				 */
				rstreason = BANDLIM_RST_OPENPORT;
				goto dropwithreset;
			}
			if (so == NULL) {
				/*
				 * We completed the 3-way handshake
				 * but could not allocate a socket
				 * either due to memory shortage,
				 * listen queue length limits or
				 * global socket limits.  Send RST
				 * or wait and have the remote end
				 * retransmit the ACK for another
				 * try.
				 */
				if ((s = tcp_log_addrs(&inc, th, NULL, NULL)))
					log(LOG_DEBUG, "%s; %s: Listen socket: "
					    "Socket allocation failed due to "
					    "limits or memory shortage, %s\n",
					    s, __func__,
					    V_tcp_sc_rst_sock_fail ?
					    "sending RST" : "try again");
				if (V_tcp_sc_rst_sock_fail) {
					rstreason = BANDLIM_UNLIMITED;
					goto dropwithreset;
				} else
					goto dropunlock;
			}
#endif
			/*
			 * Socket is created in state SYN_RECEIVED.
			 * Unlock the listen socket, lock the newly
			 * created socket and update the tp variable.
			 */
//			INP_WUNLOCK(inp);	/* listen socket */
//			inp = sotoinpcb(so);
			/*
			 * New connection inpcb is already locked by
			 * syncache_expand().
			 */
//			INP_WLOCK_ASSERT(inp);
//			tp = intotcpcb(inp);
			tp = tp->acceptinto;
			KASSERT(tp->t_state == TCPS_SYN_RECEIVED,
			    ("%s: ", __func__));
#if 0
#ifdef TCP_SIGNATURE
			if (sig_checked == 0)  {
				tcp_dooptions(&to, optp, optlen,
				    (thflags & TH_SYN) ? TO_SYN : 0);
				if (!tcp_signature_verify_input(m, off0, tlen,
				    optlen, &to, th, tp->t_flags)) {

					/*
					 * In SYN_SENT state if it receives an
					 * RST, it is allowed for further
					 * processing.
					 */
					if ((thflags & TH_RST) == 0 ||
					    (tp->t_state == TCPS_SYN_SENT) == 0)
						goto dropunlock;
				}
				sig_checked = 1;
			}
#endif
#endif
			/*
			 * Process the segment and the data it
			 * contains.  tcp_do_segment() consumes
			 * the mbuf chain and unlocks the inpcb.
			 */
			tcp_do_segment(m, th, so, tp, drop_hdrlen, tlen,
			    iptos, ti_locked);
//			INP_INFO_UNLOCK_ASSERT(&V_tcbinfo);
			return (IPPROTO_DONE);
		}
		/*
		 * Segment flag validation for new connection attempts:
		 *
		 * Our (SYN|ACK) response was rejected.
		 * Check with syncache and remove entry to prevent
		 * retransmits.
		 *
		 * NB: syncache_chkrst does its own logging of failure
		 * causes.
		 */
		if (thflags & TH_RST) {
		    //syncache_chkrst(&inc, th);
			goto dropunlock;
		}
#endif
		/*
		 * We can't do anything without SYN.
		 */
		if ((thflags & TH_SYN) == 0) {
			//if ((s = tcp_log_addrs(&inc, th, NULL, NULL)))
				printf(/*log(LOG_DEBUG, */"%s; %s: Listen socket: "
				    "SYN is missing, segment ignored\n",
				    /*s*/"note", __func__);
//			TCPSTAT_INC(tcps_badsyn);
			goto dropunlock;
		}
		/*
		 * (SYN|ACK) is bogus on a listen socket.
		 */
		if (thflags & TH_ACK) {
			//if ((s = tcp_log_addrs(&inc, th, NULL, NULL)))
				printf(/*log(LOG_DEBUG, */"%s; %s: Listen socket: "
				    "SYN|ACK invalid, segment rejected\n",
				    /*s*/"note", __func__);
//			syncache_badack(&inc);	/* XXX: Not needed! */
//			TCPSTAT_INC(tcps_badsyn);
			rstreason = BANDLIM_RST_OPENPORT;
			goto dropwithreset;
		}
		/*
		 * If the drop_synfin option is enabled, drop all
		 * segments with both the SYN and FIN bits set.
		 * This prevents e.g. nmap from identifying the
		 * TCP/IP stack.
		 * XXX: Poor reasoning.  nmap has other methods
		 * and is constantly refining its stack detection
		 * strategies.
		 * XXX: This is a violation of the TCP specification
		 * and was used by RFC1644.
		 */
		if ((thflags & TH_FIN) && V_drop_synfin) {
			//if ((s = tcp_log_addrs(&inc, th, NULL, NULL)))
				printf(/*log(LOG_DEBUG, */"%s; %s: Listen socket: "
				    "SYN|FIN segment ignored (based on "
				    "sysctl setting)\n", /*s*/"note", __func__);
//			TCPSTAT_INC(tcps_badsyn);
			goto dropunlock;
		}
		/*
		 * Segment's flags are (SYN) or (SYN|FIN).
		 *
		 * TH_PUSH, TH_URG, TH_ECE, TH_CWR are ignored
		 * as they do not affect the state of the TCP FSM.
		 * The data pointed to by TH_URG and th_urp is ignored.
		 */
		KASSERT((thflags & (TH_RST|TH_ACK)) == 0,
		    ("%s: Listen socket: TH_RST or TH_ACK set", __func__));
		KASSERT(thflags & (TH_SYN),
		    ("%s: Listen socket: TH_SYN not set", __func__));
#if 0
#ifdef INET6
		/*
		 * If deprecated address is forbidden,
		 * we do not accept SYN to deprecated interface
		 * address to prevent any new inbound connection from
		 * getting established.
		 * When we do not accept SYN, we send a TCP RST,
		 * with deprecated source address (instead of dropping
		 * it).  We compromise it as it is much better for peer
		 * to send a RST, and RST will be the final packet
		 * for the exchange.
		 *
		 * If we do not forbid deprecated addresses, we accept
		 * the SYN packet.  RFC2462 does not suggest dropping
		 * SYN in this case.
		 * If we decipher RFC2462 5.5.4, it says like this:
		 * 1. use of deprecated addr with existing
		 *    communication is okay - "SHOULD continue to be
		 *    used"
		 * 2. use of it with new communication:
		 *   (2a) "SHOULD NOT be used if alternate address
		 *        with sufficient scope is available"
		 *   (2b) nothing mentioned otherwise.
		 * Here we fall into (2b) case as we have no choice in
		 * our source address selection - we must obey the peer.
		 *
		 * The wording in RFC2462 is confusing, and there are
		 * multiple description text for deprecated address
		 * handling - worse, they are not exactly the same.
		 * I believe 5.5.4 is the best one, so we follow 5.5.4.
		 */
		if (isipv6 && !V_ip6_use_deprecated) {
			struct in6_ifaddr *ia6;

			ia6 = in6ifa_ifwithaddr(&ip6->ip6_dst, 0 /* XXX */);
			if (ia6 != NULL &&
			    (ia6->ia6_flags & IN6_IFF_DEPRECATED)) {
				ifa_free(&ia6->ia_ifa);
				if ((s = tcp_log_addrs(&inc, th, NULL, NULL)))
				    log(LOG_DEBUG, "%s; %s: Listen socket: "
					"Connection attempt to deprecated "
					"IPv6 address rejected\n",
					s, __func__);
				rstreason = BANDLIM_RST_OPENPORT;
				goto dropwithreset;
			}
			if (ia6)
				ifa_free(&ia6->ia_ifa);
		}
#endif /* INET6 */
#endif
		/*
		 * Basic sanity checks on incoming SYN requests:
		 *   Don't respond if the destination is a link layer
		 *	broadcast according to RFC1122 4.2.3.10, p. 104.
		 *   If it is from this socket it must be forged.
		 *   Don't respond if the source or destination is a
		 *	global or subnet broad- or multicast address.
		 *   Note that it is quite possible to receive unicast
		 *	link-layer packets with a broadcast IP address. Use
		 *	in_broadcast() to find them.
		 */
/*
		if (m->m_flags & (M_BCAST|M_MCAST)) {
			if ((s = tcp_log_addrs(&inc, th, NULL, NULL)))
			    log(LOG_DEBUG, "%s; %s: Listen socket: "
				"Connection attempt from broad- or multicast "
				"link layer address ignored\n", s, __func__);
			goto dropunlock;
		}
*/
//#ifdef INET6
//		if (isipv6) {
			if (th->th_dport == th->th_sport &&
			    IN6_ARE_ADDR_EQUAL(&ip6->ip6_dst, &ip6->ip6_src)) {
				//if ((s = tcp_log_addrs(&inc, th, NULL, NULL)))
				    printf(/*log(LOG_DEBUG, */"%s; %s: Listen socket: "
					"Connection attempt to/from self "
					"ignored\n", /*s*/ "note", __func__);
				goto dropunlock;
			}
			if (IN6_IS_ADDR_MULTICAST(&ip6->ip6_dst) ||
			    IN6_IS_ADDR_MULTICAST(&ip6->ip6_src)) {
				//if ((s = tcp_log_addrs(&inc, th, NULL, NULL)))
				    printf(/*log(LOG_DEBUG, */"%s; %s: Listen socket: "
					"Connection attempt from/to multicast "
					"address ignored\n", /*s*/ "note", __func__);
				goto dropunlock;
			}
//		}
//#endif
#if 0
#if defined(INET) && defined(INET6)
		else
#endif
#ifdef INET
		{
			if (th->th_dport == th->th_sport &&
			    ip->ip_dst.s_addr == ip->ip_src.s_addr) {
				if ((s = tcp_log_addrs(&inc, th, NULL, NULL)))
				    log(LOG_DEBUG, "%s; %s: Listen socket: "
					"Connection attempt from/to self "
					"ignored\n", s, __func__);
				goto dropunlock;
			}
			if (IN_MULTICAST(ntohl(ip->ip_dst.s_addr)) ||
			    IN_MULTICAST(ntohl(ip->ip_src.s_addr)) ||
			    ip->ip_src.s_addr == htonl(INADDR_BROADCAST) ||
			    in_broadcast(ip->ip_dst, m->m_pkthdr.rcvif)) {
				if ((s = tcp_log_addrs(&inc, th, NULL, NULL)))
				    log(LOG_DEBUG, "%s; %s: Listen socket: "
					"Connection attempt from/to broad- "
					"or multicast address ignored\n",
					s, __func__);
				goto dropunlock;
			}
		}
#endif
		/*
		 * SYN appears to be valid.  Create compressed TCP state
		 * for syncache.
		 */
#ifdef TCPDEBUG
		if (so->so_options & SO_DEBUG)
			tcp_trace(TA_INPUT, ostate, tp,
			    (void *)tcp_saveipgen, &tcp_savetcp, 0);
#endif
		TCP_PROBE3(debug__input, tp, th, mtod(m, const char *));
		tcp_dooptions(&to, optp, optlen, TO_SYN);
#endif
		//syncache_add(&inc, &to, th, inp, &so, m, NULL, NULL);
		// INSTEAD OF ADDING TO TO THE SYNCACHE, INITIALIZE THE NEW SOCKET RIGHT AWAY
		// CODE IS TAKEN FROM THE syncache_socket FUNCTION
		tp = tpl->acceptinto;
		tcp_state_change(tp, TCPS_SYN_RECEIVED);
		tp->t_flags |= TF_ACKNOW; // my addition
		tp->iss = tcp_new_isn(tp);
		tp->irs = th->th_seq;
		tcp_rcvseqinit(tp);
		tcp_sendseqinit(tp);
		tp->snd_wl1 = th->th_seq;
		tp->snd_max = tp->iss/* + 1*/;
		tp->snd_nxt = tp->iss/* + 1*/;
		tp->rcv_up = th->th_seq + 1;
		tp->rcv_wnd = imin(imax(cbuf_free_space(tp->recvbuf), 0), TCP_MAXWIN);
		tp->rcv_adv += tp->rcv_wnd;
		tp->last_ack_sent = tp->rcv_nxt;
		memcpy(&tp->faddr, &ip6->ip6_src, sizeof(tp->faddr));
		tp->fport = th->th_sport;
		tp->lport = tpl->lport;

//		tp->t_flags = sototcpcb(lso)->t_flags & (TF_NOPUSH|TF_NODELAY);
//		if (sc->sc_flags & SCF_NOOPT)
			tp->t_flags |= TF_NOOPT;
#if 0 // Don't handle TCP options right now
		else {
			if (sc->sc_flags & SCF_WINSCALE) {
				tp->t_flags |= TF_REQ_SCALE|TF_RCVD_SCALE;
				tp->snd_scale = sc->sc_requested_s_scale;
				tp->request_r_scale = sc->sc_requested_r_scale;
			}
			if (sc->sc_flags & SCF_TIMESTAMP) {
				tp->t_flags |= TF_REQ_TSTMP|TF_RCVD_TSTMP;
				tp->ts_recent = sc->sc_tsreflect;
				tp->ts_recent_age = tcp_ts_getticks();
				tp->ts_offset = sc->sc_tsoff;
			}
#if 0
	#ifdef TCP_SIGNATURE
			if (sc->sc_flags & SCF_SIGNATURE)
				tp->t_flags |= TF_SIGNATURE;
	#endif
#endif
			if (sc->sc_flags & SCF_SACK)
				tp->t_flags |= TF_SACK_PERMIT;
		}
		if (sc->sc_flags & SCF_ECN)
			tp->t_flags |= TF_ECN_PERMIT;
#endif
		/*
		 * Entry added to syncache and mbuf consumed.
		 * Only the listen socket is unlocked by syncache_add().
		 */
//		if (ti_locked == TI_RLOCKED) {
//			INP_INFO_RUNLOCK(&V_tcbinfo);
//			ti_locked = TI_UNLOCKED;
//		}
//		INP_INFO_UNLOCK_ASSERT(&V_tcbinfo);
        printf("Sending SYN-ACK\n");
		tcp_output(tp); // to send the SYN-ACK
		return (IPPROTO_DONE);
	} else if (tp->t_state == TCPS_LISTEN) {
		/*
		 * When a listen socket is torn down the SO_ACCEPTCONN
		 * flag is removed first while connections are drained
		 * from the accept queue in a unlock/lock cycle of the
		 * ACCEPT_LOCK, opening a race condition allowing a SYN
		 * attempt go through unhandled.
		 */
		goto dropunlock;
	}
	
	KASSERT(tp, ("tp is still NULL!"));

#if 0 // DON'T DO TCP SIGNATURE
#ifdef TCP_SIGNATURE
	if (sig_checked == 0)  {
		tcp_dooptions(&to, optp, optlen,
		    (thflags & TH_SYN) ? TO_SYN : 0);
		if (!tcp_signature_verify_input(m, off0, tlen, optlen, &to,
		    th, tp->t_flags)) {

			/*
			 * In SYN_SENT state if it receives an RST, it is
			 * allowed for further processing.
			 */
			if ((thflags & TH_RST) == 0 ||
			    (tp->t_state == TCPS_SYN_SENT) == 0)
				goto dropunlock;
		}
		sig_checked = 1;
	}
#endif
#endif
//	TCP_PROBE5(receive, NULL, tp, mtod(m, const char *), tp, th);

	/*
	 * Segment belongs to a connection in SYN_SENT, ESTABLISHED or later
	 * state.  tcp_do_segment() always consumes the mbuf chain, unlocks
	 * the inpcb, and unlocks pcbinfo.
	 */
	tcp_do_segment(ip6, th, tp, drop_hdrlen, tlen, iptos);
//	INP_INFO_UNLOCK_ASSERT(&V_tcbinfo);
	return (IPPROTO_DONE);

dropwithreset:
/*
	TCP_PROBE5(receive, NULL, tp, mtod(m, const char *), tp, th);

	if (ti_locked == TI_RLOCKED) {
		INP_INFO_RUNLOCK(&V_tcbinfo);
		ti_locked = TI_UNLOCKED;
	}
*/
#if 0
#ifdef INVARIANTS
	else {
		KASSERT(ti_locked == TI_UNLOCKED, ("%s: dropwithreset "
		    "ti_locked: %d", __func__, ti_locked));
		INP_INFO_UNLOCK_ASSERT(&V_tcbinfo);
	}
#endif
#endif
//	if (inp != NULL) {
    if (tp) {
		tcp_dropwithreset(ip6, th, tp, tlen, rstreason);
	}
//		INP_WUNLOCK(inp);
//	} else
//		tcp_dropwithreset(ip6, th, NULL, tlen, rstreason);
//	m = NULL;	/* mbuf chain got consumed. */
	goto drop;

dropunlock:
#if 0
	if (m != NULL)
		TCP_PROBE5(receive, NULL, tp, mtod(m, const char *), tp, th);

	if (ti_locked == TI_RLOCKED) {
		INP_INFO_RUNLOCK(&V_tcbinfo);
		ti_locked = TI_UNLOCKED;
	}
#ifdef INVARIANTS
	else {
		KASSERT(ti_locked == TI_UNLOCKED, ("%s: dropunlock "
		    "ti_locked: %d", __func__, ti_locked));
		INP_INFO_UNLOCK_ASSERT(&V_tcbinfo);
	}
#endif

	if (inp != NULL)
		INP_WUNLOCK(inp);
#endif
drop:
#if 0 // I BELIEVE THAT THE MEMORY MANAGEMENT IS DONE FOR ME BY THE IP STACK
	INP_INFO_UNLOCK_ASSERT(&V_tcbinfo);
	if (s != NULL)
		free(s, M_TCPLOG);
	if (m != NULL)
		m_freem(m);
#endif
	return (IPPROTO_DONE);
}

/* Original signature
static void
tcp_do_segment(struct mbuf *m, struct tcphdr *th, struct socket *so,
    struct tcpcb *tp, int drop_hdrlen, int tlen, uint8_t iptos,
    int ti_locked)
*/
static void
tcp_do_segment(struct ip6_hdr* ip6, struct tcphdr *th,
    struct tcpcb *tp, int drop_hdrlen, int tlen, uint8_t iptos)
{
	int thflags, acked, ourfinisacked, needoutput = 0;
	int rstreason, todrop, win;
	u_long tiwin;
	char *s;
	struct in_conninfo *inc;
	struct mbuf *mfree;
	struct tcpopt to;
	uint32_t ticks = get_ticks();
#if 0
#ifdef TCPDEBUG
	/*
	 * The size of tcp_saveipgen must be the size of the max ip header,
	 * now IPv6.
	 */
	u_char tcp_saveipgen[IP6_HDR_LEN];
	struct tcphdr tcp_savetcp;
	short ostate = 0;
#endif
#endif
	thflags = th->th_flags;
	//inc = &tp->t_inpcb->inp_inc;
	//tp->sackhint.last_sack_ack = 0;

	/*
	 * If this is either a state-changing packet or current state isn't
	 * established, we require a write lock on tcbinfo.  Otherwise, we
	 * allow the tcbinfo to be in either alocked or unlocked, as the
	 * caller may have unnecessarily acquired a write lock due to a race.
	 */
#if 0 // OMIT THEIR SYNCHRONIZATION
	if ((thflags & (TH_SYN | TH_FIN | TH_RST)) != 0 ||
	    tp->t_state != TCPS_ESTABLISHED) {
		KASSERT(ti_locked == TI_RLOCKED, ("%s ti_locked %d for "
		    "SYN/FIN/RST/!EST", __func__, ti_locked));
		INP_INFO_RLOCK_ASSERT(&V_tcbinfo);
	} else {
#ifdef INVARIANTS
		if (ti_locked == TI_RLOCKED)
			INP_INFO_RLOCK_ASSERT(&V_tcbinfo);
		else {
			KASSERT(ti_locked == TI_UNLOCKED, ("%s: EST "
			    "ti_locked: %d", __func__, ti_locked));
			INP_INFO_UNLOCK_ASSERT(&V_tcbinfo);
		}
#endif
	}
	INP_WLOCK_ASSERT(tp->t_inpcb);
#endif
	KASSERT(tp->t_state > TCPS_LISTEN, ("%s: TCPS_LISTEN",
	    __func__));
	KASSERT(tp->t_state != TCPS_TIME_WAIT, ("%s: TCPS_TIME_WAIT",
	    __func__));
#if 0
#ifdef TCPPCAP
	/* Save segment, if requested. */
	tcp_pcap_add(th, m, &(tp->t_inpkts));
#endif
#endif
	/*
	 * Segment received on connection.
	 * Reset idle time and keep-alive timer.
	 * XXX: This should be done after segment
	 * validation to ignore broken/spoofed segs.
	 */
	tp->t_rcvtime = ticks;
	if (TCPS_HAVEESTABLISHED(tp->t_state))
		tcp_timer_activate(tp, TT_KEEP, TP_KEEPIDLE(tp));

	/*
	 * Scale up the window into a 32-bit value.
	 * For the SYN_SENT state the scale is zero.
	 */
	tiwin = th->th_win << tp->snd_scale;

	/*
	 * TCP ECN processing.
	 */
	if (tp->t_flags & TF_ECN_PERMIT) {
		if (thflags & TH_CWR)
			tp->t_flags &= ~TF_ECN_SND_ECE;
		switch (iptos & IPTOS_ECN_MASK) {
		case IPTOS_ECN_CE:
			tp->t_flags |= TF_ECN_SND_ECE;
//			TCPSTAT_INC(tcps_ecn_ce);
			break;
		case IPTOS_ECN_ECT0:
//			TCPSTAT_INC(tcps_ecn_ect0);
			break;
		case IPTOS_ECN_ECT1:
//			TCPSTAT_INC(tcps_ecn_ect1);
			break;
		}
#if 0
		/* Process a packet differently from RFC3168. */
		cc_ecnpkt_handler(tp, th, iptos);

		/* Congestion experienced. */
		if (thflags & TH_ECE) {
			cc_cong_signal(tp, th, CC_ECN);
		}
#endif
	}

	/*
	 * Parse options on any incoming segment.
	 */
#if 0 // Skip options for now
	tcp_dooptions(&to, (u_char *)(th + 1),
	    (th->th_off << 2) - sizeof(struct tcphdr),
	    (thflags & TH_SYN) ? TO_SYN : 0);
#endif
	/*
	 * If echoed timestamp is later than the current time,
	 * fall back to non RFC1323 RTT calculation.  Normalize
	 * timestamp if syncookies were used when this connection
	 * was established.
	 */
#if 0 // Skip timestamps for now
	if ((to.to_flags & TOF_TS) && (to.to_tsecr != 0)) {
		to.to_tsecr -= tp->ts_offset;
		if (TSTMP_GT(to.to_tsecr, tcp_ts_getticks()))
			to.to_tsecr = 0;
	}
	/*
	 * If timestamps were negotiated during SYN/ACK they should
	 * appear on every segment during this session and vice versa.
	 */
	if ((tp->t_flags & TF_RCVD_TSTMP) && !(to.to_flags & TOF_TS)) {
		if ((s = tcp_log_addrs(inc, th, NULL, NULL))) {
			log(LOG_DEBUG, "%s; %s: Timestamp missing, "
			    "no action\n", s, __func__);
			free(s, M_TCPLOG);
		}
	}
	if (!(tp->t_flags & TF_RCVD_TSTMP) && (to.to_flags & TOF_TS)) {
		if ((s = tcp_log_addrs(inc, th, NULL, NULL))) {
			log(LOG_DEBUG, "%s; %s: Timestamp not expected, "
			    "no action\n", s, __func__);
			free(s, M_TCPLOG);
		}
	}
#endif
	/*
	 * Process options only when we get SYN/ACK back. The SYN case
	 * for incoming connections is handled in tcp_syncache.
	 * According to RFC1323 the window field in a SYN (i.e., a <SYN>
	 * or <SYN,ACK>) segment itself is never scaled.
	 * XXX this is traditional behavior, may need to be cleaned up.
	 */
	if (tp->t_state == TCPS_SYN_SENT && (thflags & TH_SYN)) {
#if 0 // DON'T PROCESS OPTIONS YET
		if ((to.to_flags & TOF_SCALE) &&
		    (tp->t_flags & TF_REQ_SCALE)) {
			tp->t_flags |= TF_RCVD_SCALE;
			tp->snd_scale = to.to_wscale;
		}
#endif
		/*
		 * Initial send window.  It will be updated with
		 * the next incoming segment to the scaled value.
		 */
		tp->snd_wnd = th->th_win;
#if 0 // DON'T PROCESS OPTIONS YET
		if (to.to_flags & TOF_TS) {
			tp->t_flags |= TF_RCVD_TSTMP;
			tp->ts_recent = to.to_tsval;
			tp->ts_recent_age = tcp_ts_getticks();
		}
		if (to.to_flags & TOF_MSS)
			tcp_mss(tp, to.to_mss);
		if ((tp->t_flags & TF_SACK_PERMIT) &&
		    (to.to_flags & TOF_SACKPERM) == 0)
			tp->t_flags &= ~TF_SACK_PERMIT;
#endif
	}
	/*
	 * Header prediction: check for the two common cases
	 * of a uni-directional data xfer.  If the packet has
	 * no control flags, is in-sequence, the window didn't
	 * change and we're not retransmitting, it's a
	 * candidate.  If the length is zero and the ack moved
	 * forward, we're the sender side of the xfer.  Just
	 * free the data acked & wake any higher level process
	 * that was blocked waiting for space.  If the length
	 * is non-zero and the ack didn't move, we're the
	 * receiver side.  If we're getting packets in-order
	 * (the reassembly queue is empty), add the data to
	 * the socket buffer and note that we need a delayed ack.
	 * Make sure that the hidden state-flags are also off.
	 * Since we check for TCPS_ESTABLISHED first, it can only
	 * be TH_NEEDSYN.
	 */
	if (tp->t_state == TCPS_ESTABLISHED &&
	    th->th_seq == tp->rcv_nxt &&
	    (thflags & (TH_SYN|TH_FIN|TH_RST|TH_URG|TH_ACK)) == TH_ACK &&
	    tp->snd_nxt == tp->snd_max &&
	    tiwin && tiwin == tp->snd_wnd && 
	    ((tp->t_flags & (TF_NEEDSYN|TF_NEEDFIN)) == 0) /*&&
	    LIST_EMPTY(&tp->t_segq) &&
	    ((to.to_flags & TOF_TS) == 0 ||
	     TSTMP_GEQ(to.to_tsval, tp->ts_recent))*/ ) {

		/*
		 * If last ACK falls within this segment's sequence numbers,
		 * record the timestamp.
		 * NOTE that the test is modified according to the latest
		 * proposal of the tcplw@cray.com list (Braden 1993/04/26).
		 */
		/*if ((to.to_flags & TOF_TS) != 0 &&
		    SEQ_LEQ(th->th_seq, tp->last_ack_sent)) {
			tp->ts_recent_age = tcp_ts_getticks();
			tp->ts_recent = to.to_tsval;
		}*/

		if (tlen == 0) {
			if (SEQ_GT(th->th_ack, tp->snd_una) &&
			    SEQ_LEQ(th->th_ack, tp->snd_max) &&
			    !IN_RECOVERY(tp->t_flags)/* &&
			    (to.to_flags & TOF_SACK) == 0 &&
			    TAILQ_EMPTY(&tp->snd_holes)*/) {
				/*
				 * This is a pure ack for outstanding data.
				 */
/*
				if (ti_locked == TI_RLOCKED)
					INP_INFO_RUNLOCK(&V_tcbinfo);
				ti_locked = TI_UNLOCKED;

				TCPSTAT_INC(tcps_predack);
*/
				/*
				 * "bad retransmit" recovery.
				 */
/*				if (tp->t_rxtshift == 1 &&
				    tp->t_flags & TF_PREVVALID &&
				    (int)(ticks - tp->t_badrxtwin) < 0) {
					cc_cong_signal(tp, th, CC_RTO_ERR);
				}
*/
				/*
				 * Recalculate the transmit timer / rtt.
				 *
				 * Some boxes send broken timestamp replies
				 * during the SYN+ACK phase, ignore
				 * timestamps of 0 or we could calculate a
				 * huge RTT and blow up the retransmit timer.
				 */
#if 0 // Don't estimate RTT yet
				if ((to.to_flags & TOF_TS) != 0 &&
				    to.to_tsecr) {
					u_int t;

					t = tcp_ts_getticks() - to.to_tsecr;
					if (!tp->t_rttlow || tp->t_rttlow > t)
						tp->t_rttlow = t;
					tcp_xmit_timer(tp,
					    TCP_TS_TO_TICKS(t) + 1);
				} else if (tp->t_rtttime &&
				    SEQ_GT(th->th_ack, tp->t_rtseq)) {
					if (!tp->t_rttlow ||
					    tp->t_rttlow > ticks - tp->t_rtttime)
						tp->t_rttlow = ticks - tp->t_rtttime;
					tcp_xmit_timer(tp,
							ticks - tp->t_rtttime);
				}
#endif
				acked = BYTES_THIS_ACK(tp, th);

				/* Run HHOOK_TCP_ESTABLISHED_IN helper hooks. */
//				hhook_run_tcp_est_in(tp, th, &to);

//				TCPSTAT_INC(tcps_rcvackpack);
//				TCPSTAT_ADD(tcps_rcvackbyte, acked);
//				sbdrop(&so->so_snd, acked);
				cbuf_pop(tp->sendbuf, acked);
				if (SEQ_GT(tp->snd_una, tp->snd_recover) &&
				    SEQ_LEQ(th->th_ack, tp->snd_recover))
					tp->snd_recover = th->th_ack - 1;
				
				/*
				 * Let the congestion control algorithm update
				 * congestion control related information. This
				 * typically means increasing the congestion
				 * window.
				 */
//				cc_ack_received(tp, th, CC_ACK);

				tp->snd_una = th->th_ack;
				/*
				 * Pull snd_wl2 up to prevent seq wrap relative
				 * to th_ack.
				 */
				tp->snd_wl2 = th->th_ack;
//				tp->t_dupacks = 0;
//				m_freem(m);

				/*
				 * If all outstanding data are acked, stop
				 * retransmit timer, otherwise restart timer
				 * using current (possibly backed-off) value.
				 * If process is waiting for space,
				 * wakeup/selwakeup/signal.  If data
				 * are ready to send, let tcp_output
				 * decide between more output or persist.
				 */
#if 0
#ifdef TCPDEBUG
				if (so->so_options & SO_DEBUG)
					tcp_trace(TA_INPUT, ostate, tp,
					    (void *)tcp_saveipgen,
					    &tcp_savetcp, 0);
#endif
				TCP_PROBE3(debug__input, tp, th,
					mtod(m, const char *));
#endif
				if (tp->snd_una == tp->snd_max)
					tcp_timer_activate(tp, TT_REXMT, 0);
				else if (!tcp_timer_active(tp, TT_PERSIST))
					tcp_timer_activate(tp, TT_REXMT,
						      tp->t_rxtcur);
//				sowwakeup(so);
				if (cbuf_used_space(tp->sendbuf))
//				if (sbavail(&so->so_snd))
					(void) tcp_output(tp);
				goto check_delack;
			}
		} else if (th->th_ack == tp->snd_una &&
		    tlen <= /*sbspace(&so->so_rcv)*/cbuf_free_space(tp->recvbuf)) {
			int newsize = 0;	/* automatic sockbuf scaling */

			/*
			 * This is a pure, in-sequence data packet with
			 * nothing on the reassembly queue and we have enough
			 * buffer space to take it.
			 */
#if 0
			if (ti_locked == TI_RLOCKED)
				INP_INFO_RUNLOCK(&V_tcbinfo);
			ti_locked = TI_UNLOCKED;

			/* Clean receiver SACK report if present */
			if ((tp->t_flags & TF_SACK_PERMIT) && tp->rcv_numsacks)
				tcp_clean_sackreport(tp);
			TCPSTAT_INC(tcps_preddat);
#endif
			tp->rcv_nxt += tlen;
			/*
			 * Pull snd_wl1 up to prevent seq wrap relative to
			 * th_seq.
			 */
			tp->snd_wl1 = th->th_seq;
			/*
			 * Pull rcv_up up to prevent seq wrap relative to
			 * rcv_nxt.
			 */
			tp->rcv_up = tp->rcv_nxt;
#if 0
			TCPSTAT_INC(tcps_rcvpack);
			TCPSTAT_ADD(tcps_rcvbyte, tlen);
#ifdef TCPDEBUG
			if (so->so_options & SO_DEBUG)
				tcp_trace(TA_INPUT, ostate, tp,
				    (void *)tcp_saveipgen, &tcp_savetcp, 0);
#endif
			TCP_PROBE3(debug__input, tp, th, mtod(m, const char *));
#endif
		/*
		 * Automatic sizing of receive socket buffer.  Often the send
		 * buffer size is not optimally adjusted to the actual network
		 * conditions at hand (delay bandwidth product).  Setting the
		 * buffer size too small limits throughput on links with high
		 * bandwidth and high delay (eg. trans-continental/oceanic links).
		 *
		 * On the receive side the socket buffer memory is only rarely
		 * used to any significant extent.  This allows us to be much
		 * more aggressive in scaling the receive socket buffer.  For
		 * the case that the buffer space is actually used to a large
		 * extent and we run out of kernel memory we can simply drop
		 * the new segments; TCP on the sender will just retransmit it
		 * later.  Setting the buffer size too big may only consume too
		 * much kernel memory if the application doesn't read() from
		 * the socket or packet loss or reordering makes use of the
		 * reassembly queue.
		 *
		 * The criteria to step up the receive buffer one notch are:
		 *  1. Application has not set receive buffer size with
		 *     SO_RCVBUF. Setting SO_RCVBUF clears SB_AUTOSIZE.
		 *  2. the number of bytes received during the time it takes
		 *     one timestamp to be reflected back to us (the RTT);
		 *  3. received bytes per RTT is within seven eighth of the
		 *     current socket buffer size;
		 *  4. receive buffer size has not hit maximal automatic size;
		 *
		 * This algorithm does one step per RTT at most and only if
		 * we receive a bulk stream w/o packet losses or reorderings.
		 * Shrinking the buffer during idle times is not necessary as
		 * it doesn't consume any memory when idle.
		 *
		 * TODO: Only step up if the application is actually serving
		 * the buffer to better manage the socket buffer resources.
		 */
#if 0
			if (V_tcp_do_autorcvbuf &&
			    (to.to_flags & TOF_TS) &&
			    to.to_tsecr &&
			    (so->so_rcv.sb_flags & SB_AUTOSIZE)) {
				if (TSTMP_GT(to.to_tsecr, tp->rfbuf_ts) &&
				    to.to_tsecr - tp->rfbuf_ts < hz) {
					if (tp->rfbuf_cnt >
					    (so->so_rcv.sb_hiwat / 8 * 7) &&
					    so->so_rcv.sb_hiwat <
					    V_tcp_autorcvbuf_max) {
						newsize =
						    min(so->so_rcv.sb_hiwat +
						    V_tcp_autorcvbuf_inc,
						    V_tcp_autorcvbuf_max);
					}
					/* Start over with next RTT. */
					tp->rfbuf_ts = 0;
					tp->rfbuf_cnt = 0;
				} else
					tp->rfbuf_cnt += tlen;	/* add up */
			}
#endif
			/* Add data to socket buffer. */
//			SOCKBUF_LOCK(&so->so_rcv);
/*			if (so->so_rcv.sb_state & SBS_CANTRCVMORE) {
				m_freem(m);
			} else { */
				/*
				 * Set new socket buffer size.
				 * Give up when limit is reached.
				 */
#if 0 // The circular buffer isn't resizable
				if (newsize)
					if (!sbreserve_locked(&so->so_rcv,
					    newsize, so, NULL))
						so->so_rcv.sb_flags &= ~SB_AUTOSIZE;
				m_adj(m, drop_hdrlen);	/* delayed header drop */
#endif
//				sbappendstream_locked(&so->so_rcv, m, 0);
			if (!(tp->bufstate & TCB_CANTRCVMORE))
				cbuf_write(tp->recvbuf, ((uint8_t*) th) + (th->th_off << 2), tlen);
//			}
			/* NB: sorwakeup_locked() does an implicit unlock. */
//			sorwakeup_locked(so);
/*			if (DELAY_ACK(tp, tlen)) { // DON'T DELAY ACKNOWLEDGEMENTS
				tp->t_flags |= TF_DELACK;
			} else {*/
				tp->t_flags |= TF_ACKNOW;
				tcp_output(tp);
//			}
			goto check_delack;
		}
	}

	/*
	 * Calculate amount of space in receive window,
	 * and then do TCP input processing.
	 * Receive window is amount of space in rcv queue,
	 * but not less than advertised window.
	 */
//	win = sbspace(&so->so_rcv);
	win = cbuf_free_space(tp->recvbuf);
	if (win < 0)
		win = 0;
	tp->rcv_wnd = imax(win, (int)(tp->rcv_adv - tp->rcv_nxt));

	/* Reset receive buffer auto scaling when not in bulk receive mode. */
#if 0
	tp->rfbuf_ts = 0;
	tp->rfbuf_cnt = 0;
#endif

	switch (tp->t_state) {

	/*
	 * If the state is SYN_RECEIVED:
	 *	if seg contains an ACK, but not for our SYN/ACK, send a RST.
	 *  (Added by Sam) if seg is resending the original SYN, resend the SYN/ACK
	 */
	case TCPS_SYN_RECEIVED:
		if ((thflags & TH_ACK) &&
		    (SEQ_LEQ(th->th_ack, tp->snd_una) ||
		     SEQ_GT(th->th_ack, tp->snd_max))) {
				rstreason = BANDLIM_RST_OPENPORT;
				goto dropwithreset;
		} else if ((thflags & TH_SYN) && (th->th_seq == tp->irs)) { // this clause was added by Sam
		    tp->snd_nxt = tp->snd_una;
		    tp->t_flags |= TF_ACKNOW;//tcp_output(tp);
		}
		break;

	/*
	 * If the state is SYN_SENT:
	 *	if seg contains an ACK, but not for our SYN, drop the input.
	 *	if seg contains a RST, then drop the connection.
	 *	if seg does not contain SYN, then drop it.
	 * Otherwise this is an acceptable SYN segment
	 *	initialize tp->rcv_nxt and tp->irs
	 *	if seg contains ack then advance tp->snd_una
	 *	if seg contains an ECE and ECN support is enabled, the stream
	 *	    is ECN capable.
	 *	if SYN has been acked change to ESTABLISHED else SYN_RCVD state
	 *	arrange for segment to be acked (eventually)
	 *	continue processing rest of data/controls, beginning with URG
	 */
	case TCPS_SYN_SENT:
	    printf("Entering switch statement in 1885\n");
		if ((thflags & TH_ACK) &&
		    (SEQ_LEQ(th->th_ack, tp->iss) ||
		     SEQ_GT(th->th_ack, tp->snd_max))) {
			rstreason = BANDLIM_UNLIMITED;
			goto dropwithreset;
		}
		if ((thflags & (TH_ACK|TH_RST)) == (TH_ACK|TH_RST)) {
//			TCP_PROBE5(connect__refused, NULL, tp,
//			    mtod(m, const char *), tp, th);
			tp = tcp_drop(tp, ECONNREFUSED);
		}
		if (thflags & TH_RST)
			goto drop;
		if (!(thflags & TH_SYN))
			goto drop;

        printf("Got past the drops\n");
		tp->irs = th->th_seq;
		tcp_rcvseqinit(tp);
		if (thflags & TH_ACK) {
#if 0
			TCPSTAT_INC(tcps_connects);
			soisconnected(so);
#ifdef MAC
			mac_socketpeer_set_from_mbuf(m, so);
#endif
#endif
			/* Do window scaling on this connection? */
			if ((tp->t_flags & (TF_RCVD_SCALE|TF_REQ_SCALE)) ==
				(TF_RCVD_SCALE|TF_REQ_SCALE)) {
				tp->rcv_scale = tp->request_r_scale;
			}
			tp->rcv_adv += imin(tp->rcv_wnd,
			    TCP_MAXWIN << tp->rcv_scale);
			tp->snd_una++;		/* SYN is acked */
			/*
			 * If there's data, delay ACK; if there's also a FIN
			 * ACKNOW will be turned on later.
			 */
#if 0
			if (DELAY_ACK(tp, tlen) && tlen != 0)
				tcp_timer_activate(tp, TT_DELACK,
				    tcp_delacktime);
			else
#endif
				tp->t_flags |= TF_ACKNOW;

			if ((thflags & TH_ECE) && V_tcp_do_ecn) {
				tp->t_flags |= TF_ECN_PERMIT;
//				TCPSTAT_INC(tcps_ecn_shs);
			}
			
			/*
			 * Received <SYN,ACK> in SYN_SENT[*] state.
			 * Transitions:
			 *	SYN_SENT  --> ESTABLISHED
			 *	SYN_SENT* --> FIN_WAIT_1
			 */
			tp->t_starttime = ticks;
			if (tp->t_flags & TF_NEEDFIN) {
				tcp_state_change(tp, TCPS_FIN_WAIT_1);
				tp->t_flags &= ~TF_NEEDFIN;
				thflags &= ~TH_SYN;
			} else {
				tcp_state_change(tp, TCPS_ESTABLISHED);
//				TCP_PROBE5(connect__established, NULL, tp,
//				    mtod(m, const char *), tp, th);
//				cc_conn_init(tp);
				tcp_timer_activate(tp, TT_KEEP,
				    TP_KEEPIDLE(tp));
			}
		} else {
			/*
			 * Received initial SYN in SYN-SENT[*] state =>
			 * simultaneous open.
			 * If it succeeds, connection is * half-synchronized.
			 * Otherwise, do 3-way handshake:
			 *        SYN-SENT -> SYN-RECEIVED
			 *        SYN-SENT* -> SYN-RECEIVED*
			 */
			tp->t_flags |= (TF_ACKNOW | TF_NEEDSYN);
			tcp_timer_activate(tp, TT_REXMT, 0);
			tcp_state_change(tp, TCPS_SYN_RECEIVED);
		}
/*
		KASSERT(ti_locked == TI_RLOCKED, ("%s: trimthenstep6: "
		    "ti_locked %d", __func__, ti_locked));
		INP_INFO_RLOCK_ASSERT(&V_tcbinfo);
		INP_WLOCK_ASSERT(tp->t_inpcb);
*/
		/*
		 * Advance th->th_seq to correspond to first data byte.
		 * If data, trim to stay within window,
		 * dropping FIN if necessary.
		 */
		th->th_seq++;
		if (tlen > tp->rcv_wnd) {
			todrop = tlen - tp->rcv_wnd;
#if 0 // m_adj just trims an mbuf. We can just read less, so this isn't necessary
			m_adj(m, -todrop);
#endif
			tlen = tp->rcv_wnd;
			thflags &= ~TH_FIN;
//			TCPSTAT_INC(tcps_rcvpackafterwin);
//			TCPSTAT_ADD(tcps_rcvbyteafterwin, todrop);
		}
		tp->snd_wl1 = th->th_seq - 1;
		tp->rcv_up = th->th_seq;
		/*
		 * Client side of transaction: already sent SYN and data.
		 * If the remote host used T/TCP to validate the SYN,
		 * our data will be ACK'd; if so, enter normal data segment
		 * processing in the middle of step 5, ack processing.
		 * Otherwise, goto step 6.
		 */
		if (thflags & TH_ACK)
			goto process_ACK;

		goto step6;

	/*
	 * If the state is LAST_ACK or CLOSING or TIME_WAIT:
	 *      do normal processing.
	 *
	 * NB: Leftover from RFC1644 T/TCP.  Cases to be reused later.
	 */
	case TCPS_LAST_ACK:
	case TCPS_CLOSING:
		break;  /* continue normal processing */
	}

	/*
	 * States other than LISTEN or SYN_SENT.
	 * First check the RST flag and sequence number since reset segments
	 * are exempt from the timestamp and connection count tests.  This
	 * fixes a bug introduced by the Stevens, vol. 2, p. 960 bugfix
	 * below which allowed reset segments in half the sequence space
	 * to fall though and be processed (which gives forged reset
	 * segments with a random sequence number a 50 percent chance of
	 * killing a connection).
	 * Then check timestamp, if present.
	 * Then check the connection count, if present.
	 * Then check that at least some bytes of segment are within
	 * receive window.  If segment begins before rcv_nxt,
	 * drop leading data (and SYN); if nothing left, just ack.
	 */
	if (thflags & TH_RST) {
		/*
		 * RFC5961 Section 3.2
		 *
		 * - RST drops connection only if SEG.SEQ == RCV.NXT.
		 * - If RST is in window, we send challenge ACK.
		 *
		 * Note: to take into account delayed ACKs, we should
		 *   test against last_ack_sent instead of rcv_nxt.
		 * Note 2: we handle special case of closed window, not
		 *   covered by the RFC.
		 */
		if ((SEQ_GEQ(th->th_seq, tp->last_ack_sent) &&
		    SEQ_LT(th->th_seq, tp->last_ack_sent + tp->rcv_wnd)) ||
		    (tp->rcv_wnd == 0 && tp->last_ack_sent == th->th_seq)) {
/*
			INP_INFO_RLOCK_ASSERT(&V_tcbinfo);
			KASSERT(ti_locked == TI_RLOCKED,
			    ("%s: TH_RST ti_locked %d, th %p tp %p",
			    __func__, ti_locked, th, tp));
			KASSERT(tp->t_state != TCPS_SYN_SENT,
			    ("%s: TH_RST for TCPS_SYN_SENT th %p tp %p",
			    __func__, th, tp));
*/
			if (/*V_tcp_insecure_rst ||*/
			    tp->last_ack_sent == th->th_seq) {
//				TCPSTAT_INC(tcps_drops);
				/* Drop the connection. */
				switch (tp->t_state) {
				case TCPS_SYN_RECEIVED:
//					so->so_error = ECONNREFUSED;
					goto close;
				case TCPS_ESTABLISHED:
				case TCPS_FIN_WAIT_1:
				case TCPS_FIN_WAIT_2:
				case TCPS_CLOSE_WAIT:
//					so->so_error = ECONNRESET;
				close:
					tcp_state_change(tp, TCPS_CLOSED);
					/* FALLTHROUGH */
				default:
					tp = tcp_close(tp);
				}
			} else {
//				TCPSTAT_INC(tcps_badrst);
				/* Send challenge ACK. */
				tcp_respond(tp, ip6, th, tp->rcv_nxt, tp->snd_nxt, TH_ACK);
				tp->last_ack_sent = tp->rcv_nxt;
//				m = NULL;
			}
		}
		goto drop;
	}

	/*
	 * RFC5961 Section 4.2
	 * Send challenge ACK for any SYN in synchronized state.
	 * (Added by Sam) Don't send if in SYN-RECEIVED
	 */
	if ((thflags & TH_SYN) && tp->t_state != TCPS_SYN_SENT && tp->t_state != TCP6S_SYN_RECEIVED) {
/*		KASSERT(ti_locked == TI_RLOCKED,
		    ("tcp_do_segment: TH_SYN ti_locked %d", ti_locked));
		INP_INFO_RLOCK_ASSERT(&V_tcbinfo);

		TCPSTAT_INC(tcps_badsyn);*/
/*		DON'T BOTHER WITH THE ORIGINAL INSECURE WAY. ALWAYS SEND THE CHALLENGE ACK.
		if (V_tcp_insecure_syn &&
		    SEQ_GEQ(th->th_seq, tp->last_ack_sent) &&
		    SEQ_LT(th->th_seq, tp->last_ack_sent + tp->rcv_wnd)) {
			tp = tcp_drop(tp, ECONNRESET);
			rstreason = BANDLIM_UNLIMITED;
		} else {*/
			/* Send challenge ACK. */
			printf("Sending challenge ACK\n");
			tcp_respond(tp, ip6, th, tp->rcv_nxt, tp->snd_nxt, TH_ACK);
			tp->last_ack_sent = tp->rcv_nxt;
//			m = NULL;
//		}
		goto drop;
	}

	/*
	 * RFC 1323 PAWS: If we have a timestamp reply on this segment
	 * and it's less than ts_recent, drop it.
	 */
#if 0
	if ((to.to_flags & TOF_TS) != 0 && tp->ts_recent &&
	    TSTMP_LT(to.to_tsval, tp->ts_recent)) {

		/* Check to see if ts_recent is over 24 days old.  */
		if (tcp_ts_getticks() - tp->ts_recent_age > TCP_PAWS_IDLE) {
			/*
			 * Invalidate ts_recent.  If this segment updates
			 * ts_recent, the age will be reset later and ts_recent
			 * will get a valid value.  If it does not, setting
			 * ts_recent to zero will at least satisfy the
			 * requirement that zero be placed in the timestamp
			 * echo reply when ts_recent isn't valid.  The
			 * age isn't reset until we get a valid ts_recent
			 * because we don't want out-of-order segments to be
			 * dropped when ts_recent is old.
			 */
			tp->ts_recent = 0;
		} else {
			TCPSTAT_INC(tcps_rcvduppack);
			TCPSTAT_ADD(tcps_rcvdupbyte, tlen);
			TCPSTAT_INC(tcps_pawsdrop);
			if (tlen)
				goto dropafterack;
			goto drop;
		}
	}
#endif
	/*
	 * In the SYN-RECEIVED state, validate that the packet belongs to
	 * this connection before trimming the data to fit the receive
	 * window.  Check the sequence number versus IRS since we know
	 * the sequence numbers haven't wrapped.  This is a partial fix
	 * for the "LAND" DoS attack.
	 */
	if (tp->t_state == TCPS_SYN_RECEIVED && SEQ_LT(th->th_seq, tp->irs)) {
		rstreason = BANDLIM_RST_OPENPORT;
		goto dropwithreset;
	}

	todrop = tp->rcv_nxt - th->th_seq;
	if (todrop > 0) {
		if (thflags & TH_SYN) {
			thflags &= ~TH_SYN;
			th->th_seq++;
			if (th->th_urp > 1)
				th->th_urp--;
			else
				thflags &= ~TH_URG;
			todrop--;
		}
		/*
		 * Following if statement from Stevens, vol. 2, p. 960.
		 */
		if (todrop > tlen
		    || (todrop == tlen && (thflags & TH_FIN) == 0)) {
			/*
			 * Any valid FIN must be to the left of the window.
			 * At this point the FIN must be a duplicate or out
			 * of sequence; drop it.
			 */
			thflags &= ~TH_FIN;

			/*
			 * Send an ACK to resynchronize and drop any data.
			 * But keep on processing for RST or ACK.
			 */
			tp->t_flags |= TF_ACKNOW;
			todrop = tlen;
//			TCPSTAT_INC(tcps_rcvduppack);
//			TCPSTAT_ADD(tcps_rcvdupbyte, todrop);
		} else {
//			TCPSTAT_INC(tcps_rcvpartduppack);
//			TCPSTAT_ADD(tcps_rcvpartdupbyte, todrop);
		}
		drop_hdrlen += todrop;	/* drop from the top afterwards */
		th->th_seq += todrop;
		tlen -= todrop;
		if (th->th_urp > todrop)
			th->th_urp -= todrop;
		else {
			thflags &= ~TH_URG;
			th->th_urp = 0;
		}
	}

	/*
	 * If new data are received on a connection after the
	 * user processes are gone, then RST the other end.
	 */
#if 0 // I don't have to worry about user process state
	if ((so->so_state & SS_NOFDREF) &&
	    tp->t_state > TCPS_CLOSE_WAIT && tlen) {
		KASSERT(ti_locked == TI_RLOCKED, ("%s: SS_NOFDEREF && "
		    "CLOSE_WAIT && tlen ti_locked %d", __func__, ti_locked));
		INP_INFO_RLOCK_ASSERT(&V_tcbinfo);

		if ((s = tcp_log_addrs(inc, th, NULL, NULL))) {
			log(LOG_DEBUG, "%s; %s: %s: Received %d bytes of data "
			    "after socket was closed, "
			    "sending RST and removing tcpcb\n",
			    s, __func__, tcpstates[tp->t_state], tlen);
			free(s, M_TCPLOG);
		}
		tp = tcp_close(tp);
		TCPSTAT_INC(tcps_rcvafterclose);
		rstreason = BANDLIM_UNLIMITED;
		goto dropwithreset;
	}
#endif
	/*
	 * If segment ends after window, drop trailing data
	 * (and PUSH and FIN); if nothing left, just ACK.
	 */
	todrop = (th->th_seq + tlen) - (tp->rcv_nxt + tp->rcv_wnd);
//	printf("todrop is %d\n", todrop);
	if (todrop > 0) {
//		TCPSTAT_INC(tcps_rcvpackafterwin);
		if (todrop >= tlen) {
//			TCPSTAT_ADD(tcps_rcvbyteafterwin, tlen);
			/*
			 * If window is closed can only take segments at
			 * window edge, and have to drop data and PUSH from
			 * incoming segments.  Continue processing, but
			 * remember to ack.  Otherwise, drop segment
			 * and ack.
			 */
			if (tp->rcv_wnd == 0 && th->th_seq == tp->rcv_nxt) {
				tp->t_flags |= TF_ACKNOW;
//				TCPSTAT_INC(tcps_rcvwinprobe);
			} else
				goto dropafterack;
		}/* else
			TCPSTAT_ADD(tcps_rcvbyteafterwin, todrop);*/
#if 0 // Again, we don't need to trim an mbuf
		m_adj(m, -todrop);
#endif
		tlen -= todrop;
//		printf("Adjusted tlen from %d to %d\n", tlen + todrop, tlen);
		thflags &= ~(TH_PUSH|TH_FIN);
	}

	/*
	 * If last ACK falls within this segment's sequence numbers,
	 * record its timestamp.
	 * NOTE: 
	 * 1) That the test incorporates suggestions from the latest
	 *    proposal of the tcplw@cray.com list (Braden 1993/04/26).
	 * 2) That updating only on newer timestamps interferes with
	 *    our earlier PAWS tests, so this check should be solely
	 *    predicated on the sequence space of this segment.
	 * 3) That we modify the segment boundary check to be 
	 *        Last.ACK.Sent <= SEG.SEQ + SEG.Len  
	 *    instead of RFC1323's
	 *        Last.ACK.Sent < SEG.SEQ + SEG.Len,
	 *    This modified check allows us to overcome RFC1323's
	 *    limitations as described in Stevens TCP/IP Illustrated
	 *    Vol. 2 p.869. In such cases, we can still calculate the
	 *    RTT correctly when RCV.NXT == Last.ACK.Sent.
	 */
#if 0
	if ((to.to_flags & TOF_TS) != 0 &&
	    SEQ_LEQ(th->th_seq, tp->last_ack_sent) &&
	    SEQ_LEQ(tp->last_ack_sent, th->th_seq + tlen +
		((thflags & (TH_SYN|TH_FIN)) != 0))) {
		tp->ts_recent_age = tcp_ts_getticks();
		tp->ts_recent = to.to_tsval;
	}
#endif
	/*
	 * If the ACK bit is off:  if in SYN-RECEIVED state or SENDSYN
	 * flag is on (half-synchronized state), then queue data for
	 * later processing; else drop segment and return.
	 */
	if ((thflags & TH_ACK) == 0) {
		if (tp->t_state == TCPS_SYN_RECEIVED ||
		    (tp->t_flags & TF_NEEDSYN))
			goto step6;
		else if (tp->t_flags & TF_ACKNOW)
			goto dropafterack;
		else
			goto drop;
	}
	
	printf("Processing ACK\n");
	
	/*
	 * Ack processing.
	 */
	switch (tp->t_state) {

	/*
	 * In SYN_RECEIVED state, the ack ACKs our SYN, so enter
	 * ESTABLISHED state and continue processing.
	 * The ACK was checked above.
	 */
	case TCPS_SYN_RECEIVED:

//		TCPSTAT_INC(tcps_connects);
//		soisconnected(so);
		/* Do window scaling? */
		if ((tp->t_flags & (TF_RCVD_SCALE|TF_REQ_SCALE)) ==
			(TF_RCVD_SCALE|TF_REQ_SCALE)) {
			tp->rcv_scale = tp->request_r_scale;
			tp->snd_wnd = tiwin;
		}
		/*
		 * Make transitions:
		 *      SYN-RECEIVED  -> ESTABLISHED
		 *      SYN-RECEIVED* -> FIN-WAIT-1
		 */
		tp->t_starttime = ticks;
		if (tp->t_flags & TF_NEEDFIN) {
			tcp_state_change(tp, TCPS_FIN_WAIT_1);
			tp->t_flags &= ~TF_NEEDFIN;
		} else {
			tcp_state_change(tp, TCPS_ESTABLISHED);
//			TCP_PROBE5(accept__established, NULL, tp,
//			    mtod(m, const char *), tp, th);
//			cc_conn_init(tp);
			tcp_timer_activate(tp, TT_KEEP, TP_KEEPIDLE(tp));
		}
		/*
		 * If segment contains data or ACK, will call tcp_reass()
		 * later; if not, do so now to pass queued data to user.
		 */
/* Don't need to reassemble TCP segments
		if (tlen == 0 && (thflags & TH_FIN) == 0)
			(void) tcp_reass(tp, (struct tcphdr *)0, 0,
			    (struct mbuf *)0);
*/
		tp->snd_wl1 = th->th_seq - 1;
		/* FALLTHROUGH */

	/*
	 * In ESTABLISHED state: drop duplicate ACKs; ACK out of range
	 * ACKs.  If the ack is in the range
	 *	tp->snd_una < th->th_ack <= tp->snd_max
	 * then advance tp->snd_una to th->th_ack and drop
	 * data from the retransmission queue.  If this ACK reflects
	 * more up to date window information we update our window information.
	 */
	case TCPS_ESTABLISHED:
	case TCPS_FIN_WAIT_1:
	case TCPS_FIN_WAIT_2:
	case TCPS_CLOSE_WAIT:
	case TCPS_CLOSING:
	case TCPS_LAST_ACK:
		if (SEQ_GT(th->th_ack, tp->snd_max)) {
//			TCPSTAT_INC(tcps_rcvacktoomuch);
			goto dropafterack;
		}
#if 0
		if ((tp->t_flags & TF_SACK_PERMIT) &&
		    ((to.to_flags & TOF_SACK) ||
		     !TAILQ_EMPTY(&tp->snd_holes)))
			tcp_sack_doack(tp, &to, th->th_ack);
#endif
		/* Run HHOOK_TCP_ESTABLISHED_IN helper hooks. */
//		hhook_run_tcp_est_in(tp, th, &to);

		if (SEQ_LEQ(th->th_ack, tp->snd_una)) {
			if (tlen == 0 && tiwin == tp->snd_wnd) {
				/*
				 * If this is the first time we've seen a
				 * FIN from the remote, this is not a
				 * duplicate and it needs to be processed
				 * normally.  This happens during a
				 * simultaneous close.
				 */
				if ((thflags & TH_FIN) &&
				    (TCPS_HAVERCVDFIN(tp->t_state) == 0)) {
					tp->t_dupacks = 0;
					break;
				}
//				TCPSTAT_INC(tcps_rcvdupack);
				/*
				 * If we have outstanding data (other than
				 * a window probe), this is a completely
				 * duplicate ack (ie, window info didn't
				 * change and FIN isn't set),
				 * the ack is the biggest we've
				 * seen and we've seen exactly our rexmt
				 * threshhold of them, assume a packet
				 * has been dropped and retransmit it.
				 * Kludge snd_nxt & the congestion
				 * window so we send only this one
				 * packet.
				 *
				 * We know we're losing at the current
				 * window size so do congestion avoidance
				 * (set ssthresh to half the current window
				 * and pull our congestion window back to
				 * the new ssthresh).
				 *
				 * Dup acks mean that packets have left the
				 * network (they're now cached at the receiver)
				 * so bump cwnd by the amount in the receiver
				 * to keep a constant cwnd packets in the
				 * network.
				 *
				 * When using TCP ECN, notify the peer that
				 * we reduced the cwnd.
				 */
				if (!tcp_timer_active(tp, TT_REXMT) ||
				    th->th_ack != tp->snd_una)
					tp->t_dupacks = 0;
				else if (++tp->t_dupacks > tcprexmtthresh ||
				     IN_FASTRECOVERY(tp->t_flags)) {
//					cc_ack_received(tp, th, CC_DUPACK);
#if 0
					if ((tp->t_flags & TF_SACK_PERMIT) &&
					    IN_FASTRECOVERY(tp->t_flags)) {
						int awnd;
						
						/*
						 * Compute the amount of data in flight first.
						 * We can inject new data into the pipe iff 
						 * we have less than 1/2 the original window's
						 * worth of data in flight.
						 */
						awnd = (tp->snd_nxt - tp->snd_fack) +
							tp->sackhint.sack_bytes_rexmit;
						if (awnd < tp->snd_ssthresh) {
							tp->snd_cwnd += tp->t_maxseg;
							if (tp->snd_cwnd > tp->snd_ssthresh)
								tp->snd_cwnd = tp->snd_ssthresh;
						}
					} else
#endif
						tp->snd_cwnd += tp->t_maxseg;
					(void) tcp_output(tp);
					goto drop;
				} else if (tp->t_dupacks == tcprexmtthresh) {
					tcp_seq onxt = tp->snd_nxt;

					/*
					 * If we're doing sack, check to
					 * see if we're already in sack
					 * recovery. If we're not doing sack,
					 * check to see if we're in newreno
					 * recovery.
					 */
					if (tp->t_flags & TF_SACK_PERMIT) {
						if (IN_FASTRECOVERY(tp->t_flags)) {
							tp->t_dupacks = 0;
							break;
						}
					} else {
						if (SEQ_LEQ(th->th_ack,
						    tp->snd_recover)) {
							tp->t_dupacks = 0;
							break;
						}
					}
					/* Congestion signal before ack. */
//					cc_cong_signal(tp, th, CC_NDUPACK);
//					cc_ack_received(tp, th, CC_DUPACK);
					tcp_timer_activate(tp, TT_REXMT, 0);
					tp->t_rtttime = 0;
#if 0
					if (tp->t_flags & TF_SACK_PERMIT) {
//						TCPSTAT_INC(
//						    tcps_sack_recovery_episode);
						tp->sack_newdata = tp->snd_nxt;
						tp->snd_cwnd = tp->t_maxseg;
						(void) tcp_output(tp);
						goto drop;
					}
#endif
					tp->snd_nxt = th->th_ack;
					tp->snd_cwnd = tp->t_maxseg;
					(void) tcp_output(tp);
//					KASSERT(tp->snd_limited <= 2,
//					    ("%s: tp->snd_limited too big",
//					    __func__));
					tp->snd_cwnd = tp->snd_ssthresh +
					     tp->t_maxseg *
					     (tp->t_dupacks - tp->snd_limited);
					if (SEQ_GT(onxt, tp->snd_nxt))
						tp->snd_nxt = onxt;
					goto drop;
				} else if (V_tcp_do_rfc3042) {
					/*
					 * Process first and second duplicate
					 * ACKs. Each indicates a segment
					 * leaving the network, creating room
					 * for more. Make sure we can send a
					 * packet on reception of each duplicate
					 * ACK by increasing snd_cwnd by one
					 * segment. Restore the original
					 * snd_cwnd after packet transmission.
					 */
//					cc_ack_received(tp, th, CC_DUPACK);
					u_long oldcwnd = tp->snd_cwnd;
					tcp_seq oldsndmax = tp->snd_max;
					u_int sent;
					int avail;

					KASSERT(tp->t_dupacks == 1 ||
					    tp->t_dupacks == 2,
					    ("%s: dupacks not 1 or 2",
					    __func__));
					if (tp->t_dupacks == 1)
						tp->snd_limited = 0;
					tp->snd_cwnd =
					    (tp->snd_nxt - tp->snd_una) +
					    (tp->t_dupacks - tp->snd_limited) *
					    tp->t_maxseg;
					/*
					 * Only call tcp_output when there
					 * is new data available to be sent.
					 * Otherwise we would send pure ACKs.
					 */
//					SOCKBUF_LOCK(&so->so_snd);
//					avail = sbavail(&so->so_snd) -
//					    (tp->snd_nxt - tp->snd_una);
					avail = cbuf_used_space(tp->sendbuf) - (tp->snd_nxt - tp->snd_una);
//					SOCKBUF_UNLOCK(&so->so_snd);
					if (avail > 0)
						(void) tcp_output(tp);
					sent = tp->snd_max - oldsndmax;
					if (sent > tp->t_maxseg) {
						KASSERT((tp->t_dupacks == 2 &&
						    tp->snd_limited == 0) ||
						   (sent == tp->t_maxseg + 1 &&
						    tp->t_flags & TF_SENTFIN),
						    ("%s: sent too much",
						    __func__));
						tp->snd_limited = 2;
					} else if (sent > 0)
						++tp->snd_limited;
					tp->snd_cwnd = oldcwnd;
					goto drop;
				}
			} else
				tp->t_dupacks = 0;
			break;
		}

		KASSERT(SEQ_GT(th->th_ack, tp->snd_una),
		    ("%s: th_ack <= snd_una", __func__));

		/*
		 * If the congestion window was inflated to account
		 * for the other side's cached packets, retract it.
		 */
#if 0
		if (IN_FASTRECOVERY(tp->t_flags)) {
			if (SEQ_LT(th->th_ack, tp->snd_recover)) {
//				if (tp->t_flags & TF_SACK_PERMIT)
//					tcp_sack_partialack(tp, th);
//				else
					tcp_newreno_partial_ack(tp, th);
			}// else
//				cc_post_recovery(tp, th);
		}
#endif
		tp->t_dupacks = 0;
		/*
		 * If we reach this point, ACK is not a duplicate,
		 *     i.e., it ACKs something we sent.
		 */
		if (tp->t_flags & TF_NEEDSYN) {
			/*
			 * T/TCP: Connection was half-synchronized, and our
			 * SYN has been ACK'd (so connection is now fully
			 * synchronized).  Go to non-starred state,
			 * increment snd_una for ACK of SYN, and check if
			 * we can do window scaling.
			 */
			printf("Fully synchronized now!\n");
			tp->t_flags &= ~TF_NEEDSYN;
			tp->snd_una++;
			/* Do window scaling? */
			if ((tp->t_flags & (TF_RCVD_SCALE|TF_REQ_SCALE)) ==
				(TF_RCVD_SCALE|TF_REQ_SCALE)) {
				tp->rcv_scale = tp->request_r_scale;
				/* Send window already scaled. */
			}
		}

process_ACK:
//		INP_WLOCK_ASSERT(tp->t_inpcb);

		acked = BYTES_THIS_ACK(tp, th);
//		TCPSTAT_INC(tcps_rcvackpack);
//		TCPSTAT_ADD(tcps_rcvackbyte, acked);

		printf("Bytes acked: %d\n", acked);
		/*
		 * If we just performed our first retransmit, and the ACK
		 * arrives within our recovery window, then it was a mistake
		 * to do the retransmit in the first place.  Recover our
		 * original cwnd and ssthresh, and proceed to transmit where
		 * we left off.
		 */
/*		if (tp->t_rxtshift == 1 && tp->t_flags & TF_PREVVALID &&
		    (int)(ticks - tp->t_badrxtwin) < 0)
			cc_cong_signal(tp, th, CC_RTO_ERR);
*/
		/*
		 * If we have a timestamp reply, update smoothed
		 * round trip time.  If no timestamp is present but
		 * transmit timer is running and timed sequence
		 * number was acked, update smoothed round trip time.
		 * Since we now have an rtt measurement, cancel the
		 * timer backoff (cf., Phil Karn's retransmit alg.).
		 * Recompute the initial retransmit timer.
		 *
		 * Some boxes send broken timestamp replies
		 * during the SYN+ACK phase, ignore
		 * timestamps of 0 or we could calculate a
		 * huge RTT and blow up the retransmit timer.
		 */
#if 0
		if ((to.to_flags & TOF_TS) != 0 && to.to_tsecr) {
			u_int t;

			t = tcp_ts_getticks() - to.to_tsecr;
			if (!tp->t_rttlow || tp->t_rttlow > t)
				tp->t_rttlow = t;
			tcp_xmit_timer(tp, TCP_TS_TO_TICKS(t) + 1);
		} else if (tp->t_rtttime && SEQ_GT(th->th_ack, tp->t_rtseq)) {
			if (!tp->t_rttlow || tp->t_rttlow > ticks - tp->t_rtttime)
				tp->t_rttlow = ticks - tp->t_rtttime;
			tcp_xmit_timer(tp, ticks - tp->t_rtttime);
		}
#endif
		/*
		 * If all outstanding data is acked, stop retransmit
		 * timer and remember to restart (more output or persist).
		 * If there is more data to be acked, restart retransmit
		 * timer, using current (possibly backed-off) value.
		 */
		if (th->th_ack == tp->snd_max) {
			tcp_timer_activate(tp, TT_REXMT, 0);
			needoutput = 1;
		} else if (!tcp_timer_active(tp, TT_PERSIST))
			tcp_timer_activate(tp, TT_REXMT, tp->t_rxtcur);

		/*
		 * If no data (only SYN) was ACK'd,
		 *    skip rest of ACK processing.
		 */
		if (acked == 0)
			goto step6;

		/*
		 * Let the congestion control algorithm update congestion
		 * control related information. This typically means increasing
		 * the congestion window.
		 */
//		cc_ack_received(tp, th, CC_ACK);

//		SOCKBUF_LOCK(&so->so_snd);
		if (acked > /*sbavail(&so->so_snd)*/cbuf_used_space(tp->sendbuf)) {
			//tp->snd_wnd -= sbavail(&so->so_snd);
			tp->snd_wnd -= cbuf_used_space(tp->sendbuf);
//			mfree = sbcut_locked(&so->so_snd,
//			    (int)sbavail(&so->so_snd));
			cbuf_pop(tp->sendbuf, cbuf_used_space(tp->sendbuf));
			ourfinisacked = 1;
		} else {
//			mfree = sbcut_locked(&so->so_snd, acked);
			cbuf_pop(tp->sendbuf, acked);
			tp->snd_wnd -= acked;
			ourfinisacked = 0;
		}
		/* NB: sowwakeup_locked() does an implicit unlock. */
//		sowwakeup_locked(so);
//		m_freem(mfree);
		/* Detect una wraparound. */
		if (!IN_RECOVERY(tp->t_flags) &&
		    SEQ_GT(tp->snd_una, tp->snd_recover) &&
		    SEQ_LEQ(th->th_ack, tp->snd_recover))
			tp->snd_recover = th->th_ack - 1;
		/* XXXLAS: Can this be moved up into cc_post_recovery? */
		if (IN_RECOVERY(tp->t_flags) &&
		    SEQ_GEQ(th->th_ack, tp->snd_recover)) {
			EXIT_RECOVERY(tp->t_flags);
		}
		tp->snd_una = th->th_ack;
		if (tp->t_flags & TF_SACK_PERMIT) {
			if (SEQ_GT(tp->snd_una, tp->snd_recover))
				tp->snd_recover = tp->snd_una;
		}
		if (SEQ_LT(tp->snd_nxt, tp->snd_una))
			tp->snd_nxt = tp->snd_una;

		switch (tp->t_state) {

		/*
		 * In FIN_WAIT_1 STATE in addition to the processing
		 * for the ESTABLISHED state if our FIN is now acknowledged
		 * then enter FIN_WAIT_2.
		 */
		case TCPS_FIN_WAIT_1:
			if (ourfinisacked) {
				/*
				 * If we can't receive any more
				 * data, then closing user can proceed.
				 * Starting the timer is contrary to the
				 * specification, but if we don't get a FIN
				 * we'll hang forever.
				 *
				 * XXXjl:
				 * we should release the tp also, and use a
				 * compressed state.
				 */
				if (/*so->so_rcv.sb_state & SBS_CANTRCVMORE*/
				    tp->bufstate & TCB_CANTRCVMORE) {
//					soisdisconnected(so);
					tcp_timer_activate(tp, TT_2MSL,
					    (tcp_fast_finwait2_recycle ?
					    tcp_finwait2_timeout :
					    TP_MAXIDLE(tp)));
				}
				tcp_state_change(tp, TCPS_FIN_WAIT_2);
			}
			break;

		/*
		 * In CLOSING STATE in addition to the processing for
		 * the ESTABLISHED state if the ACK acknowledges our FIN
		 * then enter the TIME-WAIT state, otherwise ignore
		 * the segment.
		 */
		case TCPS_CLOSING:
			if (ourfinisacked) {
//				INP_INFO_RLOCK_ASSERT(&V_tcbinfo);
				tcp_twstart(tp);
//				INP_INFO_RUNLOCK(&V_tcbinfo);
//				m_freem(m);
				return;
			}
			break;

		/*
		 * In LAST_ACK, we may still be waiting for data to drain
		 * and/or to be acked, as well as for the ack of our FIN.
		 * If our FIN is now acknowledged, delete the TCB,
		 * enter the closed state and return.
		 */
		case TCPS_LAST_ACK:
			if (ourfinisacked) {
//				INP_INFO_RLOCK_ASSERT(&V_tcbinfo);
				tp = tcp_close(tp);
				goto drop;
			}
			break;
		}
	}

step6:
//	INP_WLOCK_ASSERT(tp->t_inpcb);

	/*
	 * Update window information.
	 * Don't look at window if no ACK: TAC's send garbage on first SYN.
	 */
	if ((thflags & TH_ACK) &&
	    (SEQ_LT(tp->snd_wl1, th->th_seq) ||
	    (tp->snd_wl1 == th->th_seq && (SEQ_LT(tp->snd_wl2, th->th_ack) ||
	     (tp->snd_wl2 == th->th_ack && tiwin > tp->snd_wnd))))) {
		/* keep track of pure window updates */
//		if (tlen == 0 &&
//		    tp->snd_wl2 == th->th_ack && tiwin > tp->snd_wnd)
//			TCPSTAT_INC(tcps_rcvwinupd);
		tp->snd_wnd = tiwin;
		tp->snd_wl1 = th->th_seq;
		tp->snd_wl2 = th->th_ack;
		if (tp->snd_wnd > tp->max_sndwnd)
			tp->max_sndwnd = tp->snd_wnd;
		needoutput = 1;
	}

	/*
	 * Process segments with URG.
	 */
#if 0 // IGNORE THE URG FOR NOW
	if ((thflags & TH_URG) && th->th_urp &&
	    TCPS_HAVERCVDFIN(tp->t_state) == 0) {
		/*
		 * This is a kludge, but if we receive and accept
		 * random urgent pointers, we'll crash in
		 * soreceive.  It's hard to imagine someone
		 * actually wanting to send this much urgent data.
		 */
//		SOCKBUF_LOCK(&so->so_rcv);
		if (th->th_urp + sbavail(&so->so_rcv) > sb_max) {
			th->th_urp = 0;			/* XXX */
			thflags &= ~TH_URG;		/* XXX */
//			SOCKBUF_UNLOCK(&so->so_rcv);	/* XXX */
			goto dodata;			/* XXX */
		}
		/*
		 * If this segment advances the known urgent pointer,
		 * then mark the data stream.  This should not happen
		 * in CLOSE_WAIT, CLOSING, LAST_ACK or TIME_WAIT STATES since
		 * a FIN has been received from the remote side.
		 * In these states we ignore the URG.
		 *
		 * According to RFC961 (Assigned Protocols),
		 * the urgent pointer points to the last octet
		 * of urgent data.  We continue, however,
		 * to consider it to indicate the first octet
		 * of data past the urgent section as the original
		 * spec states (in one of two places).
		 */
		if (SEQ_GT(th->th_seq+th->th_urp, tp->rcv_up)) {
			tp->rcv_up = th->th_seq + th->th_urp;
			so->so_oobmark = sbavail(&so->so_rcv) +
			    (tp->rcv_up - tp->rcv_nxt) - 1;
			if (so->so_oobmark == 0)
				so->so_rcv.sb_state |= SBS_RCVATMARK;
			sohasoutofband(so);
			tp->t_oobflags &= ~(TCPOOB_HAVEDATA | TCPOOB_HADDATA);
		}
//		SOCKBUF_UNLOCK(&so->so_rcv);
		/*
		 * Remove out of band data so doesn't get presented to user.
		 * This can happen independent of advancing the URG pointer,
		 * but if two URG's are pending at once, some out-of-band
		 * data may creep in... ick.
		 */
		if (th->th_urp <= (u_long)tlen &&
		    !(so->so_options & SO_OOBINLINE)) {
			/* hdr drop is delayed */
			tcp_pulloutofband(so, th, m, drop_hdrlen);
		}
	} else
#endif
	{
		/*
		 * If no out of band data is expected,
		 * pull receive urgent pointer along
		 * with the receive window.
		 */
		if (SEQ_GT(tp->rcv_nxt, tp->rcv_up))
			tp->rcv_up = tp->rcv_nxt;
	}
dodata:							/* XXX */
//	INP_WLOCK_ASSERT(tp->t_inpcb);

	/*
	 * Process the segment text, merging it into the TCP sequencing queue,
	 * and arranging for acknowledgment of receipt if necessary.
	 * This process logically involves adjusting tp->rcv_wnd as data
	 * is presented to the user (this happens in tcp_usrreq.c,
	 * case PRU_RCVD).  If a FIN has already been received on this
	 * connection then we just ignore the text.
	 */
	if ((tlen || (thflags & TH_FIN)) &&
	    TCPS_HAVERCVDFIN(tp->t_state) == 0) {
		tcp_seq save_start = th->th_seq;
#if 0 // Again, we don't need to explicitly drop the header
		m_adj(m, drop_hdrlen);	/* delayed header drop */
#endif
		/*
		 * Insert segment which includes th into TCP reassembly queue
		 * with control block tp.  Set thflags to whether reassembly now
		 * includes a segment with FIN.  This handles the common case
		 * inline (segment is the next to be received on an established
		 * connection, and the queue is empty), avoiding linkage into
		 * and removal from the queue and repetition of various
		 * conversions.
		 * Set DELACK for segments received in order, but ack
		 * immediately when segments are out of order (so
		 * fast retransmit can work).
		 */
		if (th->th_seq == tp->rcv_nxt &&
		    /*LIST_EMPTY(&tp->t_segq) &&*/ // NO SACK
		    TCPS_HAVEESTABLISHED(tp->t_state)) {
//			if (DELAY_ACK(tp, tlen))
//				tp->t_flags |= TF_DELACK;
//			else
				tp->t_flags |= TF_ACKNOW;
			tp->rcv_nxt += tlen;
			thflags = th->th_flags & TH_FIN;
//			TCPSTAT_INC(tcps_rcvpack);
//			TCPSTAT_ADD(tcps_rcvbyte, tlen);
//			SOCKBUF_LOCK(&so->so_rcv);
/*
			if (so->so_rcv.sb_state & SBS_CANTRCVMORE)
				m_freem(m);
			else
*/
				//sbappendstream_locked(&so->so_rcv, m, 0);
			//printf("Writing %d bytes to receive buffer\n", tlen);
			if (!(tp->bufstate & TCB_CANTRCVMORE))
				cbuf_write(tp->recvbuf, ((uint8_t*) th) + (th->th_off << 2), tlen);
			/* NB: sorwakeup_locked() does an implicit unlock. */
//			sorwakeup_locked(so);
		} else {
			/*
			 * XXX: Due to the header drop above "th" is
			 * theoretically invalid by now.  Fortunately
			 * m_adj() doesn't actually frees any mbufs
			 * when trimming from the head.
			 */
//			thflags = tcp_reass(tp, th, &tlen, m);    NO SACK
			tp->t_flags |= TF_ACKNOW;
		}
//		if (tlen > 0 && (tp->t_flags & TF_SACK_PERMIT))
//			tcp_update_sack_list(tp, save_start, save_start + tlen);
#if 0 // This was originally there, not me commenting out things
		/*
		 * Note the amount of data that peer has sent into
		 * our window, in order to estimate the sender's
		 * buffer size.
		 * XXX: Unused.
		 */
		if (SEQ_GT(tp->rcv_adv, tp->rcv_nxt))
			len = so->so_rcv.sb_hiwat - (tp->rcv_adv - tp->rcv_nxt);
		else
			len = so->so_rcv.sb_hiwat;
#endif
	} else {
//		m_freem(m);
		thflags &= ~TH_FIN;
	}

	/*
	 * If FIN is received ACK the FIN and let the user know
	 * that the connection is closing.
	 */
	if (thflags & TH_FIN) {
	    printf("FIN Processing start\n");
		if (TCPS_HAVERCVDFIN(tp->t_state) == 0) {
//			socantrcvmore(so);
            tpcantrcvmore(tp);
			/*
			 * If connection is half-synchronized
			 * (ie NEEDSYN flag on) then delay ACK,
			 * so it may be piggybacked when SYN is sent.
			 * Otherwise, since we received a FIN then no
			 * more input can be expected, send ACK now.
			 */
			if (tp->t_flags & TF_NEEDSYN)
				tp->t_flags |= TF_DELACK;
			else
				tp->t_flags |= TF_ACKNOW;
			tp->rcv_nxt++;
		}
		switch (tp->t_state) {

		/*
		 * In SYN_RECEIVED and ESTABLISHED STATES
		 * enter the CLOSE_WAIT state.
		 */
		case TCPS_SYN_RECEIVED:
			tp->t_starttime = ticks;
			/* FALLTHROUGH */
		case TCPS_ESTABLISHED:
			tcp_state_change(tp, TCPS_CLOSE_WAIT);
			break;

		/*
		 * If still in FIN_WAIT_1 STATE FIN has not been acked so
		 * enter the CLOSING state.
		 */
		case TCPS_FIN_WAIT_1:
			tcp_state_change(tp, TCPS_CLOSING);
			break;

		/*
		 * In FIN_WAIT_2 state enter the TIME_WAIT state,
		 * starting the time-wait timer, turning off the other
		 * standard timers.
		 */
		case TCPS_FIN_WAIT_2:
/*
			INP_INFO_RLOCK_ASSERT(&V_tcbinfo);
			KASSERT(ti_locked == TI_RLOCKED, ("%s: dodata "
			    "TCP_FIN_WAIT_2 ti_locked: %d", __func__,
			    ti_locked));
*/
			tcp_twstart(tp);
//			INP_INFO_RUNLOCK(&V_tcbinfo);
			return;
		}
	}
#if 0
	if (ti_locked == TI_RLOCKED)
		INP_INFO_RUNLOCK(&V_tcbinfo);
	ti_locked = TI_UNLOCKED;

#ifdef TCPDEBUG
	if (so->so_options & SO_DEBUG)
		tcp_trace(TA_INPUT, ostate, tp, (void *)tcp_saveipgen,
			  &tcp_savetcp, 0);
#endif
	TCP_PROBE3(debug__input, tp, th, mtod(m, const char *));
#endif
	/*
	 * Return any desired output.
	 */
	if (needoutput || (tp->t_flags & TF_ACKNOW))
		(void) tcp_output(tp);

check_delack:
#if 0
	KASSERT(ti_locked == TI_UNLOCKED, ("%s: check_delack ti_locked %d",
	    __func__, ti_locked));
	INP_INFO_UNLOCK_ASSERT(&V_tcbinfo);
	INP_WLOCK_ASSERT(tp->t_inpcb);
#endif
	if (tp->t_flags & TF_DELACK) {
		tp->t_flags &= ~TF_DELACK;
		tcp_timer_activate(tp, TT_DELACK, tcp_delacktime);
	}
//	INP_WUNLOCK(tp->t_inpcb);
	return;

dropafterack:
	/*
	 * Generate an ACK dropping incoming segment if it occupies
	 * sequence space, where the ACK reflects our state.
	 *
	 * We can now skip the test for the RST flag since all
	 * paths to this code happen after packets containing
	 * RST have been dropped.
	 *
	 * In the SYN-RECEIVED state, don't send an ACK unless the
	 * segment we received passes the SYN-RECEIVED ACK test.
	 * If it fails send a RST.  This breaks the loop in the
	 * "LAND" DoS attack, and also prevents an ACK storm
	 * between two listening ports that have been sent forged
	 * SYN segments, each with the source address of the other.
	 */
	if (tp->t_state == TCPS_SYN_RECEIVED && (thflags & TH_ACK) &&
	    (SEQ_GT(tp->snd_una, th->th_ack) ||
	     SEQ_GT(th->th_ack, tp->snd_max)) ) {
		rstreason = BANDLIM_RST_OPENPORT;
		goto dropwithreset;
	}
#if 0
#ifdef TCPDEBUG
	if (so->so_options & SO_DEBUG)
		tcp_trace(TA_DROP, ostate, tp, (void *)tcp_saveipgen,
			  &tcp_savetcp, 0);
#endif
#endif
//	TCP_PROBE3(debug__input, tp, th, mtod(m, const char *));
//	if (ti_locked == TI_RLOCKED)
//		INP_INFO_RUNLOCK(&V_tcbinfo);
//	ti_locked = TI_UNLOCKED;

	tp->t_flags |= TF_ACKNOW;
	(void) tcp_output(tp);
//	INP_WUNLOCK(tp->t_inpcb);
//	m_freem(m);
	return;

dropwithreset:
/*
	if (ti_locked == TI_RLOCKED)
		INP_INFO_RUNLOCK(&V_tcbinfo);
	ti_locked = TI_UNLOCKED;
*/
	if (tp != NULL) {
		tcp_dropwithreset(ip6, th, tp, tlen, rstreason);
//		INP_WUNLOCK(tp->t_inpcb);
	} else
		tcp_dropwithreset(ip6, th, NULL, tlen, rstreason);
	return;

drop:
#if 0
	if (ti_locked == TI_RLOCKED) {
		INP_INFO_RUNLOCK(&V_tcbinfo);
		ti_locked = TI_UNLOCKED;
	}
#ifdef INVARIANTS
	else
		INP_INFO_UNLOCK_ASSERT(&V_tcbinfo);
#endif

	/*
	 * Drop space held by incoming segment and return.
	 */
#ifdef TCPDEBUG
	if (tp == NULL || (tp->t_inpcb->inp_socket->so_options & SO_DEBUG))
		tcp_trace(TA_DROP, ostate, tp, (void *)tcp_saveipgen,
			  &tcp_savetcp, 0);
#endif
	TCP_PROBE3(debug__input, tp, th, mtod(m, const char *));
	if (tp != NULL)
		INP_WUNLOCK(tp->t_inpcb);
	m_freem(m);
#endif
}

