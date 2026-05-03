/*
 * SX30.cpp
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
 *  Changed on: 24.11.2015, 16.12.201  by Reinhard Thamm
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


#include <Arduino.h> 
#include "SX30.h"


SX30::SX30() {
    _scopeFlag = 0;
}

void SX30::init() {
     // initialize function
     // initialize pins and variables
     // and start looking for a SYNC signal

    pinMode(SX_CLK, INPUT);      // SX-clock is an input
    pinMode(SX_DATA,INPUT);      // SX-data is also an input
    //digitalWrite(SX_CLK, HIGH);  // = enable pullup resistor
    //digitalWrite(SX_DATA, HIGH); // = enable pullup resistor

#ifndef _SXSINGLEOUTPUTLINE_                      //Tha: new:
    pinMode(SX_OUTH, OUTPUT);
    digitalWrite(SX_OUTH, HIGH);
    pinMode(SX_OUTL, OUTPUT); 
    digitalWrite(SX_OUTL, HIGH); 
#endif

#ifdef _SXISRTIMECHK_              //Tha: new, for purposes only
    pinMode(TIMECHK, OUTPUT);
    digitalWrite(TIMECHK, LOW);    
#endif

    for (int i=0; i<112;i++) {
          // reset sx variable to zero
        _sx[i]=0;
    }
     _toggle=0;
    _adrCount =0;

    // start always with search for SYNC
    _state = SYNC;
    _zeroCount = 0;
    _sx_write_busy = 0;    
    _sx_writing = 0;
    _newTrackBit = 2;
}

void SX30::init(uint8_t tba) {
    // special init which enable a scope trigger
    // at a Selectrix address tba (must be 0..112)
    // scope will go high at bit 12 of this address!!
    // i.e. interesting data are shortly BEFORE 
    // this trigger
    _scopeFlag=1;
     pinMode(SCOPE,OUTPUT);
    _triggerAdr = tba;
    init();
}

void SX30::setSX_OUT(uint8_t val) {              //Tha: new for output stage
#ifdef _SXSINGLEOUTPUTLINE_
    switch(val) {
    case 0:          // writing a 0 to SX_OUT
        bitWrite(SX_OUT_DDR, SX_OUT_PORTPIN, HIGH);
        bitWrite(SX_OUT_PORT, SX_OUT_PORTPIN, LOW);
        break;
    case 1:          // writing a 1 to SX_OUT
        bitWrite(SX_OUT_DDR, SX_OUT_PORTPIN, HIGH);
        bitWrite(SX_OUT_PORT, SX_OUT_PORTPIN, HIGH);
        break;
    case TRISTATE:   // setting SX_OUT to high impedance
        bitWrite(SX_OUT_DDR, SX_OUT_PORTPIN, LOW);  
        break;
    }
#else
    switch(val) {
    case 0:          // writing a 0 to SX_OUT (active LOW!)
        bitWrite(SX_OUTH_PORT, SX_OUTH_PORTPIN, HIGH);
        bitWrite(SX_OUTL_PORT, SX_OUTL_PORTPIN, LOW);
        break;
    case 1:          // writing a 1 to SX_OUT
        bitWrite(SX_OUTH_PORT, SX_OUTH_PORTPIN, LOW);
        bitWrite(SX_OUTL_PORT, SX_OUTL_PORTPIN, HIGH);
        break;
    case TRISTATE:   // setting SX_OUT to high impedance (both lines to 1)
        bitWrite(SX_OUTH_PORT, SX_OUTH_PORTPIN, HIGH);
        bitWrite(SX_OUTL_PORT, SX_OUTL_PORTPIN, HIGH);    
        break;
    }
#endif
}

uint8_t SX30::get(uint8_t channel) {
     // returns the value of a SX channel
    if (channel < MAX_CHANNEL_NUMBER)
       return _sx[channel];
    else
       return 0;
}

uint8_t SX30::set(uint8_t ch, uint8_t d) {
    if (_sx_write_busy) return 1;  // dont accept new data

    _sx_write_channel = ch;
    _sx_write_data = d;
    _sx_write_busy=1;
    return 0;
}

uint8_t SX30::writing() {                                       
    return _sx_write_busy;
}

uint8_t SX30::getTrackBit() {               //Tha: getting the Track-Bit (track 
    return _trackBit;                       //        voltage on/off), due to an
}                                           //        idea from Uli Beyenbach 

void SX30::setTrackBit(uint8_t val) {       //Tha: setting the Track-Bit (track 
    if (val==0 || val==1) {                 //        voltage on/off), due to an
        _newTrackBit = val;                 //        idea from Uli Beyenbach
    }
}


void SX30::checkAndWrite(void) {            //Tha: NEW: for SX data writing
    switch (_state) {                       //  purposes @FALLING slope of ISR
    case ADDR:                              //  triggering
        switch (_adrCount) {
        case 0:  // this is the GLEISSPANNUNG bit
            if (_newTrackBit < 2) 
                setSX_OUT(_newTrackBit);
            else 
                setSX_OUT(TRISTATE);
            break;
        
        case 1:
            if (_newTrackBit != 2) {
                _newTrackBit = 2;
                setSX_OUT(TRISTATE);
            }
            break;
        }  // switch (adrCount)
    
        break;  // case ADDR
        
    case DATA:
        // prepare for data writing:
        // calc sx channel from baseAdr and dataFrameCount
        _channel = (15-_baseAdr) + ((6-_dataFrameCount)<<4);
        
        if ((_sx_write_busy) && (_channel == _sx_write_channel)) { 
            switch (_dataBitCount) {
            case 2:        // Trennbits
            case 5:
            case 8:
                setSX_OUT(TRISTATE);
                break;
            case 0:    // D0
                if (bitRead(_sx_write_data, 0) != bitRead(_sx[_channel], 0))
                    setSX_OUT(bitRead(_sx_write_data, 0));
                else
                    setSX_OUT(TRISTATE);
                _sx_writing = 1;   
                break;
            case 1:    // D1
                if (bitRead(_sx_write_data, 1) != bitRead(_sx[_channel], 1))
                    setSX_OUT(bitRead(_sx_write_data, 1));
                else
                    setSX_OUT(TRISTATE);
                break;
            case 3:    // D2
                if (bitRead(_sx_write_data, 2) != bitRead(_sx[_channel], 2))
                    setSX_OUT(bitRead(_sx_write_data, 2));
                else
                    setSX_OUT(TRISTATE);
                break;
            case 4:    // D3
                if (bitRead(_sx_write_data, 3) != bitRead(_sx[_channel], 3))
                    setSX_OUT(bitRead(_sx_write_data, 3));
                else
                    setSX_OUT(TRISTATE);
                break;
            case 6:    // D4
                if (bitRead(_sx_write_data, 4) != bitRead(_sx[_channel], 4))
                    setSX_OUT(bitRead(_sx_write_data, 4));    
                else
                    setSX_OUT(TRISTATE);
                break;
            case 7:    // D5
                if (bitRead(_sx_write_data, 5) != bitRead(_sx[_channel], 5))
                    setSX_OUT(bitRead(_sx_write_data, 5));
                else
                    setSX_OUT(TRISTATE);
                break;
            case 9:    // D6
                if (bitRead(_sx_write_data, 6) != bitRead(_sx[_channel], 6))
                    setSX_OUT(bitRead(_sx_write_data, 6));
                else
                    setSX_OUT(TRISTATE);
                break;
            case 10: // D7
                if (bitRead(_sx_write_data, 7) != bitRead(_sx[_channel], 7))
                    setSX_OUT(bitRead(_sx_write_data, 7));
                else
                    setSX_OUT(TRISTATE);
                break;
            case 11:  // Trennbit
                setSX_OUT(TRISTATE);
                _sx_writing = 0;           // was the last bit
                _sx_write_busy = 0;        // write has ended, can accept new data
                break;
            }   // switch (_dataBitCount)
        }    // if _sx_write_busy && ...
        
        if (_scopeFlag) {
            if (_channel == _triggerAdr) {
                bitWrite(SCOPE_PORT, SCOPE_PORTPIN, HIGH);      // activate scope signal
            } 
        }    
        break;  // case DATA
    }  // switch (_state)
}    // checkAndWrite()


void SX30::switchAdr() {
     // a SYNC signal was received, now look for a valid
     // base address
    switch(_adrCount) {
    case 0:   // this is the GLEISSPANNUNG bit
        _trackBit = _bit;    
        break;    
    case 1:
        break;
    case 4:
        break; // ignore
    case 2:  // B3
        bitWrite(_baseAdr,3,_bit);
        break;
    case 3:  // B2
        bitWrite(_baseAdr,2,_bit);
        break;    
    case 5:  // B1
        bitWrite(_baseAdr,1,_bit);
        break;
    case 6:  // B0
        bitWrite(_baseAdr,0,_bit);
        break;
    case 7: // last "1"
        // _baseAdr is complete !
                 
        // advance to next state - next we are looking
        // for the 7 data bytes (i.e. 7 SX Channels)
        _state = DATA;  
        _dataFrameCount = 0;
        _dataBitCount = 0;
        _data=0;
        break;
    }
}

void SX30::switchData() {
    // continue reading _data
    // a total of 7 DATA blocks will be received
    // for a certain base-address
    // calc sx channel from baseAdr and dataFrameCount
    
    switch(_dataBitCount) {
    case 2:  // "Trenn_bits"
    case 5:
    case 8:
        _dataBitCount++;
        break; // ignore
    case 0:  // D0
        _data=0;
        bitWrite(_data,0,_bit);
        _dataBitCount++;

        if (_scopeFlag) {                                           //Tha: also watching _scopeFlag..
            if (_channel ==  _triggerAdr) {                         //     (bugfix 30.01.2015)
                bitWrite(SCOPE_PORT, SCOPE_PORTPIN, HIGH);          //Tha: digitalWrite(SCOPE,HIGH);
            }
            else {
                bitWrite(SCOPE_PORT, SCOPE_PORTPIN, LOW);           //Tha: digitalWrite(SCOPE,LOW);
            }
        }
        break;
    case 1:  // D1
        bitWrite(_data,1,_bit);
        _dataBitCount++;
        break;
    case 3:  // D2
        bitWrite(_data,2,_bit);
        _dataBitCount++;
        break;
    case 4:  // D3
        bitWrite(_data,3,_bit);
        _dataBitCount++;
        break;
    case 6:  // D4
        bitWrite(_data,4,_bit);
        _dataBitCount++;
        break;
    case 7:  // D5
        bitWrite(_data,5,_bit);
        _dataBitCount++;
        break;
    case 9:  // D6
        bitWrite(_data,6,_bit);
        _dataBitCount++;
        break;
    case 10: // D7
        bitWrite(_data,7,_bit);
        _dataBitCount++;
        break;
    case 11:  // == MAX_DATABITCOUNT
        // _bit value should always equal HIGH, not tested here.
        
        if (_scopeFlag) {                            
            if (_channel ==  _triggerAdr) {    
                bitWrite(SCOPE_PORT, SCOPE_PORTPIN, LOW);       // deactivate scope signal
            } 
        }
        
        // copy _data byte to SX _channel
        _sx[_channel] = _data;

        // increment dataFrameCount to move on the next DATA byte
        // check, if we already reached the last DATA block - in this
        // case move on to the next SX-Datenpaket, i.e. look for SYNC
        _dataFrameCount ++;
        if (_dataFrameCount == MAX_DATACOUNT) {
            // and move on to SYNC _state
            _dataFrameCount=0;
            _state =SYNC;
            _zeroCount = 0;
            _dataBitCount=0;
        } 
        else {
            _dataBitCount = 0;  // reset _bit counter
            _data = 0;
            // continue with reading next _data uint8_t
        }
    }  //end switch/case _dataBitCount
}


void SX30::isr() {

    // interrupt service routine (AVR INT0)
    // driven by CHANGING clock signal T0 (SX-plug pin1)

     // 3 different states are distinguished
     //     1. SNYC = looking for a SYNC signal
     //     2. ADDR = (after SYNC received) look for base address (0..15)
     //     3. DATA = (after ADDR decoded) decode the 7 data-bytes
     

#ifdef _SXISRTIMECHK_
    bitWrite(TIMECHK_PORT, TIMECHK_PORTPIN, HIGH);     // for TEST purposes only
#endif 

    // check if the calling slope was the falling or the rising one:
    if (!bitRead(SX_CLK_PINREG, SX_CLK_PORTPIN)) {    // falling slope

        checkAndWrite(); 
    }
    else {                                            // rising slope
        _bit = bitRead(SX_DATA_PINREG, SX_DATA_PORTPIN);    // reading the T1 data pin

        switch(_state) {
        case SYNC:
            if (_bit == LOW)  {
                _zeroCount++;
            } else {
                if (_zeroCount == 3)  {        // sync bits 0 0 0 1 found
                    _state = ADDR;         // advance to next state
                    _baseAdr = 0;   //init
                    _adrCount = 0;  //init
                } else {  // no valid sync, try again ...
                    _zeroCount = 0;       // reset _zeroCounter
                } // endif _zeroCount
            }  // endif _bit==LOW
            break;
        case ADDR:
            switchAdr();
            _adrCount++;
            break;
        case DATA:
            switchData();
        }
    }

#ifdef _SXISRTIMECHK_
    bitWrite(TIMECHK_PORT, TIMECHK_PORTPIN, LOW);     // for TEST purposes only
#endif


}




