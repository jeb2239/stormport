/*
 * "Copyright (c) 2016 The Regents of the University  of California.
 * All rights reserved."
 *
 * Permission to use, copy, modify, and distribute this software and its
 * documentation for any purpose, without fee, and without written agreement is
 * hereby granted, provided that the above copyright notice, the following
 * two paragraphs and the author appear in all copies of this software.
 *
 * IN NO EVENT SHALL THE UNIVERSITY OF CALIFORNIA BE LIABLE TO ANY PARTY FOR
 * DIRECT, INDIRECT, SPECIAL, INCIDENTAL, OR CONSEQUENTIAL DAMAGES ARISING OUT
 * OF THE USE OF THIS SOFTWARE AND ITS DOCUMENTATION, EVEN IF THE UNIVERSITY OF
 * CALIFORNIA HAS BEEN ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 * THE UNIVERSITY OF CALIFORNIA SPECIFICALLY DISCLAIMS ANY WARRANTIES,
 * INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY
 * AND FITNESS FOR A PARTICULAR PURPOSE.  THE SOFTWARE PROVIDED HEREUNDER IS
 * ON AN "AS IS" BASIS, AND THE UNIVERSITY OF CALIFORNIA HAS NO OBLIGATION TO
 * PROVIDE MAINTENANCE, SUPPORT, UPDATES, ENHANCEMENTS, OR MODIFICATIONS."
 *
 */
#ifndef RPL_STATISTICS_H
#define RPL_STATISTICS_H

/* Different RPL components provide statistics about their operation. 
 *
 * Structures with this information are available here.
 */

typedef nx_struct {
    nx_uint16_t dio_sent_count; // Number of DIO messages sent
    nx_uint16_t dio_recv_count; // Number of DIO messages received
    nx_uint16_t dis_sent_count; // Number of DIS messages sent
    nx_uint16_t dis_recv_count; // Number of DIS messages received
} rpl_statistics_t;

typedef nx_struct {
    nx_uint16_t dao_sent_count; // Number of DAO messages sent
    nx_uint16_t dao_recv_count; // Number of DAO messages received
} rpl_dao_statistics_t;

#endif // RPL_STATISTICS_H
