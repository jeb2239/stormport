/* Copyright (c) 2014, Regents of the University of California
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 *  - Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 *  - Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the
 *    distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS
 * FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL
 * THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT,
 * INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
 * SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT,
 * STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED
 * OF THE POSSIBILITY OF SUCH DAMAGE.
 */

/**
 * Definitions specific to the Cortex-M4 MCU
 * Copied from those for the Cortex-M3, and verified
 * Includes interrupt enable/disable routines for nesC.
 *
 * @author Michael Andersen <m.andersen@cs.berkeley.edu>
 * @author Wanja Hofer <wanja@cs.fau.de>
 * @author Thomas Schmid
 */

#ifndef CORTEXM4_HARDWARE_H
#define CORTEXM4_HARDWARE_H

#define ROUNDDOWN(a, n)                                         \
({                                                              \
        uint32_t __a = (uint32_t) (a);                          \
        (typeof(a)) (__a - __a % (n));                          \
})
// Round up to the nearest multiple of n
#define ROUNDUP(a, n)                                           \
({                                                              \
        uint32_t __n = (uint32_t) (n);                          \
        (typeof(a)) (ROUNDDOWN((uint32_t) (a) + __n - 1, __n)); \
})

typedef uint32_t __nesc_atomic_t;

inline __nesc_atomic_t __nesc_atomic_start() @spontaneous() __attribute__((always_inline))
{
	__nesc_atomic_t oldState = 0;
	__nesc_atomic_t newState = 1;
	asm volatile(
		"mrs %[old], primask\n"
		"msr primask, %[new]\n"
		: [old] "=&r" (oldState) // output, assure write only!
		: [new] "r"  (newState)  // input
        : "cc", "memory"         // clobber condition code flag and memory
	);
	return oldState;
}
 
inline void __nesc_atomic_end(__nesc_atomic_t oldState) @spontaneous() __attribute__((always_inline))
{
	asm volatile("" : : : "memory"); // memory barrier
 
	asm volatile(
		"msr primask, %[old]"
		:                      // no output
		: [old] "r" (oldState) // input
	);
}

// Enables all exceptions except hard fault and NMI
inline void __nesc_enable_interrupt() __attribute__((always_inline))
{
	__nesc_atomic_t newState = 0;

	asm volatile(
		"msr primask, %0"
		: // output
		: "r" (newState) // input
	);
}

// Disables all exceptions except hard fault and NMI
inline void __nesc_disable_interrupt() __attribute__((always_inline))
{
	__nesc_atomic_t newState = 1;

	asm volatile(
		"msr primask, %0"
		: // output
		: "r" (newState) // input
	);
}


#endif // CORTEXM4_HARDWARE_H
