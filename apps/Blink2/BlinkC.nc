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
#define ENABLE_TRACE 1

#include "printf.h"
#include <usarthardware.h>



module BlinkC @safe()
{
  uses interface Timer<TMilli> as Timer0;
  uses interface GeneralIO as Led;
  uses interface Boot;
  uses interface HplSam4lUSART as SpiHPL;
  uses interface SpiPacket;
}
implementation
{
  event void Boot.booted()
  {
    call Timer0.startPeriodic( 500 );
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

    //edfc was 1<<24
    //*((volatile uint32_t*) 0xE000EDFC) = 1<<24; //Enable debug port
    //*((volatile uint32_t*) 0xE00400F0) = 0x00000002; //Wire protocol
    //*((volatile uint32_t*) 0xE0000FB0) = 0xC5ACCE55; //Access code
    //*((volatile uint32_t*) 0xE0000E80) = 0x00010015; //Forgot what this does but it seems NB
    //*((volatile uint32_t*) 0xE0040010) = 7; //Set speed to 6 Mhz
    //*((volatile uint32_t*) 0xE0000E00) = 0b11; //Enable stim0 and stim1
    //*((volatile uint32_t*) 0xE0000E40) = 0x0; //Disable priorities
	//*((volatile uint32_t*) 0xE0040304) = 0x00000100; // Formatter and Flush Control Register


  }
  async event void SpiPacket.sendDone(uint8_t* txBuf, uint8_t* rxBuf, uint16_t len, error_t error)
  {
    printf("got: '%s'",rxBuf);
  }
  uint8_t txbuf [80];
  uint8_t rxbuf [80];
  event void Timer0.fired()
  {
    
    uint8_t txlen;
    trace("hello world\n");
    trace16(500);
    trace32(501);
    txlen = snprintf(txbuf, 80, "Toggled LED") + 1;
    call SpiPacket.send(txbuf, rxbuf, txlen);
    call Led.toggle();
  }
}

