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

int V_nolocaltimewait = 1; // For now, to keep things simple

/*
 * Move a TCP connection into TIME_WAIT state.
 *    tcbinfo is locked.
 *    inp is locked, and is unlocked before returning.
 */
void
tcp_twstart(struct tcpcb *tp)
{
#if 0
	struct tcptw *tw;
	struct inpcb *inp = tp->t_inpcb;
	int acknow;
	struct socket *so;
#ifdef INET6
	int isipv6 = inp->inp_inc.inc_flags & INC_ISIPV6;
#endif

	INP_INFO_RLOCK_ASSERT(&V_tcbinfo);
	INP_WLOCK_ASSERT(inp);
#endif
	if (V_nolocaltimewait) {
//		int error = 0;
//#ifdef INET6
//		if (isipv6)
//			error = in6_localaddr(&inp->in6p_faddr);
//#endif
//#if defined(INET6) && defined(INET)
//		else
//#endif
//#ifdef INET
//			error = in_localip(inp->inp_faddr);
//#endif
//		if (error) {
			tp = tcp_close(tp);
//			if (tp != NULL)
//				INP_WUNLOCK(inp);
			return;
//		}
	}
#if 0

	/*
	 * For use only by DTrace.  We do not reference the state
	 * after this point so modifying it in place is not a problem.
	 */
	tcp_state_change(tp, TCPS_TIME_WAIT);

	tw = uma_zalloc(V_tcptw_zone, M_NOWAIT);
	if (tw == NULL) {
		/*
		 * Reached limit on total number of TIMEWAIT connections
		 * allowed. Remove a connection from TIMEWAIT queue in LRU
		 * fashion to make room for this connection.
		 *
		 * XXX:  Check if it possible to always have enough room
		 * in advance based on guarantees provided by uma_zalloc().
		 */
		tw = tcp_tw_2msl_scan(1);
		if (tw == NULL) {
			tp = tcp_close(tp);
			if (tp != NULL)
				INP_WUNLOCK(inp);
			return;
		}
	}
	/*
	 * The tcptw will hold a reference on its inpcb until tcp_twclose
	 * is called
	 */
	tw->tw_inpcb = inp;
	in_pcbref(inp);	/* Reference from tw */

	/*
	 * Recover last window size sent.
	 */
	if (SEQ_GT(tp->rcv_adv, tp->rcv_nxt))
		tw->last_win = (tp->rcv_adv - tp->rcv_nxt) >> tp->rcv_scale;
	else
		tw->last_win = 0;

	/*
	 * Set t_recent if timestamps are used on the connection.
	 */
	if ((tp->t_flags & (TF_REQ_TSTMP|TF_RCVD_TSTMP|TF_NOOPT)) ==
	    (TF_REQ_TSTMP|TF_RCVD_TSTMP)) {
		tw->t_recent = tp->ts_recent;
		tw->ts_offset = tp->ts_offset;
	} else {
		tw->t_recent = 0;
		tw->ts_offset = 0;
	}

	tw->snd_nxt = tp->snd_nxt;
	tw->rcv_nxt = tp->rcv_nxt;
	tw->iss     = tp->iss;
	tw->irs     = tp->irs;
	tw->t_starttime = tp->t_starttime;
	tw->tw_time = 0;

/* XXX
 * If this code will
 * be used for fin-wait-2 state also, then we may need
 * a ts_recent from the last segment.
 */
	acknow = tp->t_flags & TF_ACKNOW;

	/*
	 * First, discard tcpcb state, which includes stopping its timers and
	 * freeing it.  tcp_discardcb() used to also release the inpcb, but
	 * that work is now done in the caller.
	 *
	 * Note: soisdisconnected() call used to be made in tcp_discardcb(),
	 * and might not be needed here any longer.
	 */
	tcp_discardcb(tp);
	so = inp->inp_socket;
	soisdisconnected(so);
	tw->tw_cred = crhold(so->so_cred);
	SOCK_LOCK(so);
	tw->tw_so_options = so->so_options;
	SOCK_UNLOCK(so);
	if (acknow)
		tcp_twrespond(tw, TH_ACK);
	inp->inp_ppcb = tw;
	inp->inp_flags |= INP_TIMEWAIT;
	tcp_tw_2msl_reset(tw, 0);

	/*
	 * If the inpcb owns the sole reference to the socket, then we can
	 * detach and free the socket as it is not needed in time wait.
	 */
	if (inp->inp_flags & INP_SOCKREF) {
		KASSERT(so->so_state & SS_PROTOREF,
		    ("tcp_twstart: !SS_PROTOREF"));
		inp->inp_flags &= ~INP_SOCKREF;
		INP_WUNLOCK(inp);
		ACCEPT_LOCK();
		SOCK_LOCK(so);
		so->so_state &= ~SS_PROTOREF;
		sofree(so);
	} else
		INP_WUNLOCK(inp);
#endif
}
