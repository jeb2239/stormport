/*-
 * Copyright (c) 1982, 1986, 1988, 1993
 *	The Regents of the University of California.
 * Copyright (c) 2006-2007 Robert N. M. Watson
 * Copyright (c) 2010-2011 Juniper Networks, Inc.
 * All rights reserved.
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
 *	From: @(#)tcp_usrreq.c	8.2 (Berkeley) 1/3/94
 */

#include "socket.h"
#include "ip6.h"

/* For compatibility between BSD's in6_addr struct and TinyOS's in6_addr struct. */
#define __u6_addr in6_u
#define __u6_addr32 u6_addr32

#if 0
static int
tcp6_usr_bind(struct socket *so, struct sockaddr *nam, struct thread *td)
{
	int error = 0;
	struct inpcb *inp;
	struct tcpcb *tp = NULL;
	struct sockaddr_in6 *sin6p;

	sin6p = (struct sockaddr_in6 *)nam;
	if (nam->sa_len != sizeof (*sin6p))
		return (EINVAL);
	/*
	 * Must check for multicast addresses and disallow binding
	 * to them.
	 */
	if (sin6p->sin6_family == AF_INET6 &&
	    IN6_IS_ADDR_MULTICAST(&sin6p->sin6_addr))
		return (EAFNOSUPPORT);

	TCPDEBUG0;
	inp = sotoinpcb(so);
	KASSERT(inp != NULL, ("tcp6_usr_bind: inp == NULL"));
	INP_WLOCK(inp);
	if (inp->inp_flags & (INP_TIMEWAIT | INP_DROPPED)) {
		error = EINVAL;
		goto out;
	}
	tp = intotcpcb(inp);
	TCPDEBUG1();
	INP_HASH_WLOCK(&V_tcbinfo);
	inp->inp_vflag &= ~INP_IPV4;
	inp->inp_vflag |= INP_IPV6;
#ifdef INET
	if ((inp->inp_flags & IN6P_IPV6_V6ONLY) == 0) {
		if (IN6_IS_ADDR_UNSPECIFIED(&sin6p->sin6_addr))
			inp->inp_vflag |= INP_IPV4;
		else if (IN6_IS_ADDR_V4MAPPED(&sin6p->sin6_addr)) {
			struct sockaddr_in sin;

			in6_sin6_2_sin(&sin, sin6p);
			inp->inp_vflag |= INP_IPV4;
			inp->inp_vflag &= ~INP_IPV6;
			error = in_pcbbind(inp, (struct sockaddr *)&sin,
			    td->td_ucred);
			INP_HASH_WUNLOCK(&V_tcbinfo);
			goto out;
		}
	}
#endif
	error = in6_pcbbind(inp, nam, td->td_ucred);
	INP_HASH_WUNLOCK(&V_tcbinfo);
out:
	TCPDEBUG2(PRU_BIND);
	TCP_PROBE2(debug__user, tp, PRU_BIND);
	INP_WUNLOCK(inp);
	return (error);
}
#endif

/* Based on a function in in6_pcb.c. */
static int in6_pcbconnect(struct tcpcb* tp, struct sockaddr_in6* nam) {
    register struct sockaddr_in6 *sin6 = nam;
    tp->faddr = sin6->sin6_addr;
	tp->fport = sin6->sin6_port;
	return 0;
}

/*
 * Initiate connection to peer.
 * Create a template for use in transmissions on this connection.
 * Enter SYN_SENT state, and mark socket as connecting.
 * Start keep-alive timer, and seed output sequence space.
 * Send initial segment on connection.
 */
/* Signature used to be
static int
tcp6_connect(struct tcpcb *tp, struct sockaddr *nam, struct thread *td)
*/
static int
tcp6_connect(struct tcpcb *tp, struct sockaddr_in6 *nam)
{
//	struct inpcb *inp = tp->t_inpcb;
	int error;
	
	int sb_max = cbuf_free_space(tp->recvbuf); // same as sendbuf
//	INP_WLOCK_ASSERT(inp);
//	INP_HASH_WLOCK(&V_tcbinfo);
	if (/*inp->inp_lport == 0*/tp->lport == 0) {
		/*error = in6_pcbbind(inp, (struct sockaddr *)0, td->td_ucred);
		if (error)
			goto out;*/
		error = EINVAL; // First, the socket must be bound
		goto out;
	}
	error = in6_pcbconnect(/*inp*/tp, nam/*, td->td_ucred*/);
	if (error != 0)
		goto out;
//	INP_HASH_WUNLOCK(&V_tcbinfo);

	/* Compute window scaling to request.  */
	while (tp->request_r_scale < TCP_MAX_WINSHIFT &&
	    (TCP_MAXWIN << tp->request_r_scale) < sb_max)
		tp->request_r_scale++;

//	soisconnecting(inp->inp_socket);
//	TCPSTAT_INC(tcps_connattempt);
	tcp_state_change(tp, TCPS_SYN_SENT);
	tp->iss = tcp_new_isn(tp);
	tcp_sendseqinit(tp);

	return 0;

out:
//	INP_HASH_WUNLOCK(&V_tcbinfo);
	return error;
}

/*
The signature used to be
static int
tcp6_usr_connect(struct socket *so, struct sockaddr *nam, struct thread *td)
*/
static int
tcp6_usr_connect(struct tcpcb* tp, struct sockaddr_in6* sin6p)
{
	int error = 0;
//	struct inpcb *inp;
//	struct tcpcb *tp = NULL;
//	struct sockaddr_in6 *sin6p;

//	TCPDEBUG0;

//	sin6p = (struct sockaddr_in6 *)nam;
//	if (nam->sa_len != sizeof (*sin6p))
//		return (EINVAL);
	/*
	 * Must disallow TCP ``connections'' to multicast addresses.
	 */
	if (/*sin6p->sin6_family == AF_INET6
	    && */IN6_IS_ADDR_MULTICAST(&sin6p->sin6_addr))
		return (EAFNOSUPPORT);
#if 0 // We already have the TCB
	inp = sotoinpcb(so);
	KASSERT(inp != NULL, ("tcp6_usr_connect: inp == NULL"));
	INP_WLOCK(inp);
	if (inp->inp_flags & INP_TIMEWAIT) {
		error = EADDRINUSE;
		goto out;
	}
	if (inp->inp_flags & INP_DROPPED) {
		error = ECONNREFUSED;
		goto out;
	}
	tp = intotcpcb(inp);
#endif
//	TCPDEBUG1();
//#ifdef INET
	/*
	 * XXXRW: Some confusion: V4/V6 flags relate to binding, and
	 * therefore probably require the hash lock, which isn't held here.
	 * Is this a significant problem?
	 */
	if (IN6_IS_ADDR_V4MAPPED(&sin6p->sin6_addr)) {
//		struct sockaddr_in sin;

		if (/*(inp->inp_flags & IN6P_IPV6_V6ONLY) != 0*/1) {
			error = EINVAL;
			goto out;
		}
#if 0 // Not needed since we'll take the if branch anyway
		in6_sin6_2_sin(&sin, sin6p);
		inp->inp_vflag |= INP_IPV4;
		inp->inp_vflag &= ~INP_IPV6;
		if ((error = prison_remote_ip4(td->td_ucred,
		    &sin.sin_addr)) != 0)
			goto out;
		if ((error = tcp_connect(tp, (struct sockaddr *)&sin, td)) != 0)
			goto out;
#endif
#if 0
#ifdef TCP_OFFLOAD
		if (registered_toedevs > 0 &&
		    (so->so_options & SO_NO_OFFLOAD) == 0 &&
		    (error = tcp_offload_connect(so, nam)) == 0)
			goto out;
#endif
#endif
		error = tcp_output(tp);
		goto out;
	}
//#endif
//	inp->inp_vflag &= ~INP_IPV4;
//	inp->inp_vflag |= INP_IPV6;
//	inp->inp_inc.inc_flags |= INC_ISIPV6;
//	if ((error = prison_remote_ip6(td->td_ucred, &sin6p->sin6_addr)) != 0)
//		goto out;
	if ((error = tcp6_connect(tp, sin6p/*, td*/)) != 0)
		goto out;
#if 0
#ifdef TCP_OFFLOAD
	if (registered_toedevs > 0 &&
	    (so->so_options & SO_NO_OFFLOAD) == 0 &&
	    (error = tcp_offload_connect(so, nam)) == 0)
		goto out;
#endif
#endif
	tcp_timer_activate(tp, TT_KEEP, TP_KEEPINIT(tp));
	error = tcp_output(tp);

out:
#if 0
	TCPDEBUG2(PRU_CONNECT);
	TCP_PROBE2(debug__user, tp, PRU_CONNECT);
	INP_WUNLOCK(inp);
#endif
	return (error);
}
