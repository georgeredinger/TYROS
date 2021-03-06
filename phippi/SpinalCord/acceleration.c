/*
 *  Copyright (c) 2011,2012 Tonu Samuel
 *  All rights reserved.
 *
 *  This file is part of TYROS.
 *
 *  TYROS is free software: you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation, either version 3 of the License, or
 *  (at your option) any later version.
 *
 *  TYROS is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with TYROS.  If not, see <http://www.gnu.org/licenses/>.
 *
 */

#include "ior32c111.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include "main.h"
#include "uart.h"
#include "hwsetup.h"
#include "gyro.h"
#include "SPI.h"
#include "mma7455l.h"

int accok=0;
int accwhoami=0;
int accx=0,accy=0,accz=0,acctout=0;
int accstatus=0;
/* 
 * During startup we read CALIBRATIONSAMPLES values from sensor for all X,Y,Z acces to 
 * calculate and negate error, gravitation etc. 
 */

//int acccalcnt=0;
//static int avgx=0,avgy=0,avgz=0;

void
SPI2_Init(void) { // Accel sensor
#define f1_CLK_SPEED 24000000
    /* 
     * MMA7455L datasheet allows up to 8MHz clock
     */
    // u2brg =  (unsigned char)(((f1_CLK_SPEED)/(2*4000000))-1);
    u2brg =  (unsigned char)(((f1_CLK_SPEED)/(2*500000))-1);

    CS2d = PD_OUTPUT;
    CS2=1;
    CLOCK2d = PD_OUTPUT;
    CLOCK2s = PF_UART;
    TX2d = PD_OUTPUT;
    TX2s = PF_UART;
    RX2s = PF_UART;

    smd0_u2mr  = 1;                                        // \ 
    smd1_u2mr  = 0;                                        //  | Synchronous Serial Mode
    smd2_u2mr  = 0;                                        // /

    ckdir_u2mr = 0;                                        // 0=internal clock   
    stps_u2mr  = 0;                                        // 0=1 stop bit, 0 required
    pry_u2mr   = 0;                                        // Parity, 0=odd, 0 required 
    prye_u2mr  = 0;                                        // Parity Enable? 0=disable, 0 required 
    iopol_u2mr = 0;                                        // IO Polarity, 0=not inverted, 0 required

    clk0_u2c0 = 0;                                         // Clock source f1 for u4brg
    clk1_u2c0 = 0;                                         // 
    txept_u2c0 = 0;                                        // Transmit register empty flag 
    crd_u2c0 = 1;                                          // CTS disabled when 1
    nch_u2c0 = 1;                                          // 0=Output mode "open drain" for TXD and CLOCK pin 
    ckpol_u2c0 = 0;                                        // CLK Polarity 0 rising edge, 1 falling edge
    uform_u2c0 = 1;                                        // 1=MSB first

    te_u2c1 = 1;                                           // 1=Transmission Enable
    ti_u2c1 = 0;                                           // Must be 0 to send or receive
    re_u2c1 = 1;                                           // Reception Enable when 1
    ri_u2c1 = 0;                                           // Receive complete flag - U2RB is empty.
    u2irs_u2c1 = 1;                                        // Interrupt  when transmission is completed. 
    u2rrm_u2c1 = 0;                                        // Continuous receive mode off
    u2lch_u2c1 = 0;                                        // Logical inversion off 

    u2smr = 0x00;
    u2smr2 = 0x00;

    sse_u2smr3 = 0;                                        // SS is disabled when 0
    ckph_u2smr3 = 0;                                       // Non clock delayed 
    dinc_u2smr3 = 0;                                       // Master mode when 0
    nodc_u2smr3 = 0;                                       // Select a clock output  mode "push-pull" when 0 
    err_u2smr3 = 0;                                        // Error flag, no error when 0 
    dl0_u2smr3 = 0;                                        // Set 0 for no  delay 
    dl1_u2smr3 = 0;                                        // Set 0 for no  delay 
    dl2_u2smr3 = 0;                                        // Set 0 for no  delay 

    u2smr4 = 0x00;

    DISABLE_IRQ
    /* 
     * Lowest interrupt priority
     * we do not care about speed
     */
    ilvl_s2ric =1;       
    ir_s2ric   =0;            
    ENABLE_IRQ
}

void
accelerometer_read_reg(unsigned char c) {
    u2tb = c << 1;
}

void
accelerometer_write_reg(unsigned char c) {
    u2tb = (c << 1) |  MMA7455L_WRITE_BIT;
}

void
accelerometer_write_data(unsigned char c) {
    uDelay(16);
    u2tb = c ; 
}

#pragma vector = UART2_RX
__interrupt void _uart2_receive(void) {
    /* 
     * This is done in main() to start whole process:
     * accelerometer_write_reg( MMA7455L_REG_I2CAD ); 
     */
    signed char b=u2rb & 0xFF;
    switch(accstatus) {
    case 0: // Writing bit to disable I2C
        accelerometer_write_data(MMA7455L_GSELECT_2 | MMA7455L_MODE_MEASUREMENT); 
        break;
    case 1:
        uDelay(16);
        CS2=1;
        uDelay(16);
   /*     for(int i=0;i<1000;i++){
            uDelay(200);
        }*/
        CS2=0;
        uDelay(16);
        accelerometer_read_reg(MMA7455L_REG_MCTL); 
        break;
    case 3: // REG_WHOAMI answer received. Trying to get XOUTL
        accwhoami=(int) b;
        accstatus=0-1; // 2 after accstatus++ later
        uDelay(16);
        CS2=1;
        uDelay(16);
        CS2=0;
        uDelay(16);
        accelerometer_write_reg(MMA7455L_REG_MCTL);
        break;
    default:    
        uDelay(16);
        u2tb=0xff;
        uDelay(16);
        break;
    } 
    accstatus++;

    /* Clear the 'reception complete' flag.	*/
    ir_s2ric = 0;
}
