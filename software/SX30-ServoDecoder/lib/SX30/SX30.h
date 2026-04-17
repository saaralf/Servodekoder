/*
 * SX30.h
 *
 *  Created on: 02.01.2012
 *  Changed on: 10.11.2013
 *  Version:    2.1
 *  Copyright:  Michael Blank
 *
 *  Changed on: 11.01.2015 by Reinhard Thamm, referring to comments
 *  Version:    2.2
 *
 *    This faster version uses more direct bitwise port and pin operations
 *    and needs to be used if an original TRIX ZENTRALEINHEIT 1 is in use.
 *    Only important for SX-Writing issues.
 *
 *  Changed on: 09.05.2015 by Reinhard Thamm
 *  Version:    2.2b
 *  
 *    Due to a timing problem when writing the very first Bit0 of a 
 *    SX-address on the SX-bus, the handling of the SCOPE-trigger-
 *    signal has been postponed. The SCOPE-trigger is now activated 
 *    after writing Bit0.
 *
 *  Changed on: 24.11.2015, 16.12.2015 by Reinhard Thamm
 *  Version:    3.0
 *  
 *    Hardware: this new version now supports a dual transistor output stage 
 *      for SX_OUT, which is now driven by TWO processor outputs instead of 
 *      a single R100 resistor connected to processor pin 6 (SX_OUT).
 *    Software: writing data on the SX bus is now initiated BEFORE the
 *      T0 (clock) signal rises, while reading data from the 
 *      SX bus remains unchanged in the trailing part of the ISR.
 *      To accomplish both operations, the ISR has to be attached via CHANGE 
 *      condition in the Setup of the calling module.
 *
 *  interface hardware needed ! see www.opensx.net

 Read SX Signal - SX_CLK (T0) must be connected to Pin2=INT0.
 SX_DATA (T1) should be connected according to SX30.h.
 SX_OUT (D) needs two lines to drive the output stage: a high-side signal
 and a low-side signal according to SX30.h.
 For triggering a scope, a signal can be generated at Pin SCOPE (see SX30.h) 
 at a defined base address.


 This library is free software; you can redistribute it and/or
 modify it under the terms of the GNU Lesser General Public
 License as published by the Free Software Foundation; either
 version 2.1 of the License, or (at your option) any later version.
 
 This library is distributed in the hope that it will be useful,
 but WITHOUT ANY WARRANTY; without even the implied warranty of
 MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 Lesser General Public License for more details.
 
 You should have received a copy of the GNU Lesser General Public
 License along with this library; if not, write to the Free Software
 Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA

 */

#ifndef SX30_H_
#define SX30_H_

#include <inttypes.h>

#define _SXBASISBOARD_ 2       // important for RT-Basisboard:
                               // 1 == V01.0x uses a R100 Resistor for SX-Writing
                               // 2 == V02.00 uses a dual transistor output stage

//#define _SXSINGLEOUTPUTLINE_   // can be used if SX_OUT is realized via
                               // a R100 resistor. This #define is automatically
                               // set when _SXBASISBOARD_==V01
  
//#define _SXISRTIMECHK_         // to enable an extra scope signal 
                               // at Pin A5 when entering and leaving the
                               // ISR. This #define should ONLY be used for
                               // temporary measurement purposes.


// define arduino pins, ports and bits 

/* #if defined (__AVR_ATtiny45__)||(__AVR_ATtiny85__)  // settings for ATtiny45/85

                                // do not use this section, actually ATtinys @8MHz are too slow
                                // for this lib when writing on the SX-Bus. It's only a relict from an
                                // unsuccessful experiment. 
                                // Perhaps sometimes in future...?
  
  #define SX_CLK            PB2     // must be INT0 !!
  #define SX_CLK_DDR        DDRB                      
  #define SX_CLK_PORT       PORTB                     
  #define SX_CLK_PORTPIN    PORTB2                    
  #define SX_CLK_PINREG     PINB                      

  #define SCOPE             PB0                                               
  #define SCOPE_DDR         DDRB                      
  #define SCOPE_PORT        PORTB                     
  #define SCOPE_PORTPIN     PORTB0                    

  #define SX_DATA           PB4                         
  #define SX_DATA_DDR       DDRB                      
  #define SX_DATA_PORT      PORTB                     
  #define SX_DATA_PORTPIN   PORTB4                    
  #define SX_DATA_PINREG    PINB                      

  #define _SXSINGLEOUTPUTLINE_
  
  #define SX_OUT            PB3                         
  #define SX_OUT_DDR        DDRB                      
  #define SX_OUT_PORT       PORTB                     
  #define SX_OUT_PORTPIN    PORTB3                    
*/

#if defined (__AVR_ATmega328P__) // default settings for Arduino Pro Mini 5V 16MHz

  #define SX_CLK            2     // must be INT0 !!
  #define SX_CLK_DDR        DDRD                      //Tha: new for SX30:
  #define SX_CLK_PORT       PORTD                     //Tha
  #define SX_CLK_PORTPIN    PORTD2                    //Tha
  #define SX_CLK_PINREG     PIND                      //Tha

  #define SCOPE             A4                        //Tha: changed for SX30:                         
  #define SCOPE_DDR         DDRC                      //Tha
  #define SCOPE_PORT        PORTC                     //Tha
  #define SCOPE_PORTPIN     PORTC4                    //Tha

  #define SX_DATA           4                         
  #define SX_DATA_DDR       DDRD                      
  #define SX_DATA_PORT      PORTD                     
  #define SX_DATA_PORTPIN   PORTD4                    
  #define SX_DATA_PINREG    PIND                      

  #if (_SXBASISBOARD_ == 2)
    #define SX_OUTL           3                       //Tha: new for SX30
    #define SX_OUTL_DDR       DDRD                    //Tha
    #define SX_OUTL_PORT      PORTD                   //Tha
    #define SX_OUTL_PORTPIN   PORTD3                  //Tha

    #define SX_OUTH           6                         
    #define SX_OUTH_DDR       DDRD                      
    #define SX_OUTH_PORT      PORTD                     
    #define SX_OUTH_PORTPIN   PORTD6
  #else                                // this is the RT-Basisboard V01..
    #define _SXSINGLEOUTPUTLINE_
    #define SX_OUT            6                         
    #define SX_OUT_DDR        DDRD                      
    #define SX_OUT_PORT       PORTD                     
    #define SX_OUT_PORTPIN    PORTD6                    
  #endif

  #if defined (_SXISRTIMECHK_)         // to enable an extra scope signal 
    #define TIMECHK         A5         // at Pin A5
    #define TIMECHK_DDR	    DDRC
    #define TIMECHK_PORT   	PORTC
    #define TIMECHK_PORTPIN PORTC5
  #endif                    

#else
  #error "--> CPU settings not supported in SX30.h <--"
#endif


// define for SX_OUT
#define TRISTATE        3

// defines for state machine
#define SYNC  0
#define ADDR  1
#define DATA  2

#define MAX_DATACOUNT    7    // 7 dataframes in 1 SYNC Channel
#define MAX_DATABITCOUNT 12   // 12 bits in 1 frame

#define MAX_CHANNEL_NUMBER 112   // SX channels

class SX30 {
public:
    SX30();
    void init(void);
    void init(uint8_t);    
    uint8_t get(uint8_t);
    uint8_t set(uint8_t, uint8_t);
    uint8_t writing();            //Tha
    uint8_t getTrackBit();        //Tha
    void setTrackBit(uint8_t);    //Tha
    void isr(void);           


private:
    void setSX_OUT(uint8_t);
    void checkAndWrite(void);
    void switchAdr(void);
    void switchData(void);

    uint8_t _toggle;
    uint8_t _zeroCount;
    uint8_t _adrCount;
    uint8_t _state;
    uint8_t _dataBitCount;    // bit counting
    uint8_t _dataFrameCount;  // frame counting
    uint8_t _trackBit;        //Tha
    uint8_t _newTrackBit;     //Tha

    uint8_t _data;            // 1 data uint8_t
    uint8_t _baseAdr;         // base address
    uint8_t _triggerAdr;      // SX address to trigger scope
    uint8_t _scopeFlag;       // generate scope trigger signal if != 0

    uint8_t _sx_write_channel;   // stores channel number to write to
    uint8_t _sx_write_data;      // data to write
    volatile uint8_t _sx_write_busy;      // waiting to send data, cannot accept new data
    uint8_t _sx_writing;         // currently writing a byte
    
    uint8_t _bit;
    uint8_t _sx[MAX_CHANNEL_NUMBER];   // to store the SX data

    uint8_t _channel;   // channel from 0 to 15, B3..B0 in sync data
    // 0  0  0  1  X   1  B3  B2  1  B1  B0  1 == sync frame of 12 bits
    

    /* SX Timing
     1   Bit             50 µs
     1   Kanal          600 µs (= 12 Bit)
     1   Grundrahmen    ca. 4,8 ms
     1   Gesamtrahmen   ca.  80 ms (= 16 Grundrahmen)  */
};

#endif /* SX30_H_ */
