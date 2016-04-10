// $Id: BlinkC.nc,v 1.6 2010-06-29 22:07:16 scipio Exp $

/*									tab:4
 * Copyright (c) 2000-2005 The Regents of the University  of California.  
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * - Redistributions of source code must retain the above copyright
 *   notice, this list of conditions and the following disclaimer.
 * - Redistributions in binary form must reproduce the above copyright
 *   notice, this list of conditions and the following disclaimer in the
 *   documentation and/or other materials provided with the
 *   distribution.
 * - Neither the name of the University of California nor the names of
 *   its contributors may be used to endorse or promote products derived
 *   from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS
 * FOR A PARTICULAR PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL
 * THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT,
 * INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
 * SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT,
 * STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED
 * OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 * Copyright (c) 2002-2003 Intel Corporation
 * All rights reserved.
 *
 * This file is distributed under the terms in the attached INTEL-LICENSE     
 * file. If you do not find these files, copies can be found by writing to
 * Intel Research Berkeley, 2150 Shattuck Avenue, Suite 1300, Berkeley, CA, 
 * 94704.  Attention:  Intel License Inquiry.
 */

/**
 * Implementation for Blink application.  Toggle the red LED when a
 * Timer fires.
 **/

#include "Timer.h"
#include "printf.h"
#include <usarthardware.h>
#include <stdint.h>



module BlinkC @safe()
{
  uses interface Timer<TMilli> as Timer0;
  uses interface Timer<TMilli> as Timer1;
  uses interface Timer<TMilli> as Timer2;
  uses interface GeneralIO as Led;
  uses interface HplSam4lUSART as SpiHPL;
  uses interface Boot;
  uses interface SpiPacket;
}
implementation
{
  event void Boot.booted()
  {
    call Timer0.startPeriodic( 250 );
    call Timer1.startPeriodic( 500 );
    call Timer2.startPeriodic( 1000 );
    call Led.makeOutput();
    printf("Configuring SPI\n");
    //Because you can have one usart present on multiple pins (like multiple TX pins)
    //you need to speak to the HPL directly. Not sure what the best way to implement
    //this is.
    call SpiHPL.enableUSARTPin(USART2_TX_PC12);
    call SpiHPL.enableUSARTPin(USART2_RX_PC11);
    call SpiHPL.enableUSARTPin(USART2_CLK_PA18);
    call SpiHPL.enableUSARTPin(USART2_RTS_PC07);
    call SpiHPL.initSPIMaster();
    call SpiHPL.setSPIMode(0,0);
    call SpiHPL.setSPIBaudRate(20000);
    call SpiHPL.enableTX();
    call SpiHPL.enableRX();

  }

  async event void SpiPacket.sendDone(uint8_t* txBuf, uint8_t* rxBuf, uint16_t len, error_t error)
  {
    printf("got: '%s'",rxBuf);
  }
  uint8_t txbuf [80];
  uint8_t rxbuf [80];
  volatile uint32_t count = 0;

// addresses of registers
  volatile uint32_t *DWT_CONTROL = (uint32_t *)0xE0001000;
  volatile uint32_t *DWT_CYCCNT = (uint32_t *)0xE0001004; 
  volatile uint32_t *DEMCR = (uint32_t *)0xE000EDFC; 

  extern uint32_t __start_count()
{


// enable the use DWT
*DEMCR = *DEMCR | 0x01000000;

// Reset cycle counter
*DWT_CYCCNT = 0; 

// enable cycle counter
*DWT_CONTROL = *DWT_CONTROL | 1 ; 

}

// some code here
// .....

extern uint32_t __end_count()
{
// number of cycles stored in count variable
count = *DWT_CYCCNT;
return count;
}
uint32_t a=0;
  event void Timer0.fired()
  {

     __start_count(); //this interval is 4
    //dbg("BlinkC", "Timer 0 fired @ %s.\n", sim_time_string());
    //call Led.toggle();
     call Led.toggle();
     
     a=__end_count(); 
    
    printf("%d\n",a);
  }
  
  event void Timer1.fired()
  {
    dbg("BlinkC", "Timer 1 fired @ %s \n", sim_time_string());
    call Led.toggle();
  }
  
  event void Timer2.fired()
  {
    dbg("BlinkC", "Timer 2 fired @ %s.\n", sim_time_string());
    call Led.toggle();
  }
}

