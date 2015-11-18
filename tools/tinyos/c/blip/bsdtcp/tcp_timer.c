/*-
 * Copyright (c) 1982, 1986, 1993
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
 *	@(#)tcp_timer.h	8.1 (Berkeley) 6/10/93
 * $FreeBSD$
 */

int	tcp_keepcnt = TCPTV_KEEPCNT;

int
tcp_timer_active(struct tcpcb *tp, uint32_t timer_type)
{
	return (tp->activetimers & timer_type) != 0;
}

void
tcp_timer_activate(struct tcpcb *tp, uint32_t timer_type, u_int delta) {
	uint8_t tos_timer;
	tp->activetimers |= timer_type;
	switch (timer_type) {
	case TT_REXMT:
		tos_timer = TOS_REXMT;
		break;
	case TT_PERSIST:
		tos_timer = TOS_PERSIST;
		break;
	case TT_KEEP:
		tos_timer = TOS_KEEP;
		break;
	case TT_2MSL:
		tos_timer = TOS_2MSL;
		break;
	default:
		printf("Invalid timer 0x%x: skipping\n", timer_type);
		return;
    }
    set_timer(tp, timer_type, (uint32_t) delta);
}

