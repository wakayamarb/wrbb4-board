/*
SoftwareSerial.cpp (formerly NewSoftSerial.cpp) - 
Multi-instance software serial library for Arduino/Wiring
-- Interrupt-driven receive and other improvements by ladyada
   (http://ladyada.net)
-- Tuning, circular buffer, derivation from class Print/Stream,
   multi-instance support, porting to 8MHz processors,
   various optimizations, PROGMEM delay tables, inverse logic and 
   direct port writing by Mikal Hart (http://www.arduiniana.org)
-- Pin change interrupt macros by Paul Stoffregen (http://www.pjrc.com)
-- 20MHz processor support by Garrett Mace (http://www.macetech.com)
-- ATmega1280/2560 support by Brett Hagman (http://www.roguerobotics.com/)

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

The latest version of this library can always be found at
http://arduiniana.org.

Modified 11 August 2014 by Nozomu Fujita for GR-SAKURA
*/

// When set, _DEBUG co-opts pins 11 and 13 for debugging with an
// oscilloscope or logic analyzer.  Beware: it also slightly modifies
// the bit times, so don't rely on it too much at high baud rates
#define _DEBUG 0
#define _DEBUG_PIN1 11
#define _DEBUG_PIN2 13
// 
// Includes
// 
#ifndef GRSAKURA
#include <avr/interrupt.h>
#include <avr/pgmspace.h>
#else /*GRSAKURA*/
#include "Arduino.h"
#include "iodefine.h"
#include "interrupt_handlers.h"
#define UseCmt2 1
#define UseTpu5 0
#endif/*GRSAKURA*/
#include <SoftwareSerial.h>
#ifndef GRSAKURA
//
// Lookup table
//
typedef struct _DELAY_TABLE
{
  long baud;
  unsigned short rx_delay_centering;
  unsigned short rx_delay_intrabit;
  unsigned short rx_delay_stopbit;
  unsigned short tx_delay;
} DELAY_TABLE;

#if F_CPU == 16000000

static const DELAY_TABLE PROGMEM table[] = 
{
  //  baud    rxcenter   rxintra    rxstop    tx
  { 115200,   1,         17,        17,       12,    },
  { 57600,    10,        37,        37,       33,    },
  { 38400,    25,        57,        57,       54,    },
  { 31250,    31,        70,        70,       68,    },
  { 28800,    34,        77,        77,       74,    },
  { 19200,    54,        117,       117,      114,   },
  { 14400,    74,        156,       156,      153,   },
  { 9600,     114,       236,       236,      233,   },
  { 4800,     233,       474,       474,      471,   },
  { 2400,     471,       950,       950,      947,   },
  { 1200,     947,       1902,      1902,     1899,  },
  { 600,      1902,      3804,      3804,     3800,  },
  { 300,      3804,      7617,      7617,     7614,  },
};

const int XMIT_START_ADJUSTMENT = 5;

#elif F_CPU == 8000000

static const DELAY_TABLE table[] PROGMEM = 
{
  //  baud    rxcenter    rxintra    rxstop  tx
  { 115200,   1,          5,         5,      3,      },
  { 57600,    1,          15,        15,     13,     },
  { 38400,    2,          25,        26,     23,     },
  { 31250,    7,          32,        33,     29,     },
  { 28800,    11,         35,        35,     32,     },
  { 19200,    20,         55,        55,     52,     },
  { 14400,    30,         75,        75,     72,     },
  { 9600,     50,         114,       114,    112,    },
  { 4800,     110,        233,       233,    230,    },
  { 2400,     229,        472,       472,    469,    },
  { 1200,     467,        948,       948,    945,    },
  { 600,      948,        1895,      1895,   1890,   },
  { 300,      1895,       3805,      3805,   3802,   },
};

const int XMIT_START_ADJUSTMENT = 4;

#elif F_CPU == 20000000

// 20MHz support courtesy of the good people at macegr.com.
// Thanks, Garrett!

static const DELAY_TABLE PROGMEM table[] =
{
  //  baud    rxcenter    rxintra    rxstop  tx
  { 115200,   3,          21,        21,     18,     },
  { 57600,    20,         43,        43,     41,     },
  { 38400,    37,         73,        73,     70,     },
  { 31250,    45,         89,        89,     88,     },
  { 28800,    46,         98,        98,     95,     },
  { 19200,    71,         148,       148,    145,    },
  { 14400,    96,         197,       197,    194,    },
  { 9600,     146,        297,       297,    294,    },
  { 4800,     296,        595,       595,    592,    },
  { 2400,     592,        1189,      1189,   1186,   },
  { 1200,     1187,       2379,      2379,   2376,   },
  { 600,      2379,       4759,      4759,   4755,   },
  { 300,      4759,       9523,      9523,   9520,   },
};

const int XMIT_START_ADJUSTMENT = 6;

#else

#error This version of SoftwareSerial supports only 20, 16 and 8MHz processors

#endif
#endif/*GRSAKURA*/

//
// Statics
//
SoftwareSerial *SoftwareSerial::active_object = 0;
#ifndef GRSAKURA
char SoftwareSerial::_receive_buffer[_SS_MAX_RX_BUFF]; 
volatile uint8_t SoftwareSerial::_receive_buffer_tail = 0;
volatile uint8_t SoftwareSerial::_receive_buffer_head = 0;
#endif/*GRSAKURA*/

#ifndef GRSAKURA
//
// Debugging
//
// This function generates a brief pulse
// for debugging or measuring on an oscilloscope.
inline void DebugPulse(uint8_t pin, uint8_t count)
{
#if _DEBUG
  volatile uint8_t *pport = portOutputRegister(digitalPinToPort(pin));

  uint8_t val = *pport;
  while (count--)
  {
    *pport = val | digitalPinToBitMask(pin);
    *pport = val;
  }
#endif
}
#endif/*GRSAKURA*/

//
// Private methods
//

/* static */ 
#ifndef GRSAKURA
inline void SoftwareSerial::tunedDelay(uint16_t delay) { 
  uint8_t tmp=0;

  asm volatile("sbiw    %0, 0x01 \n\t"
    "ldi %1, 0xFF \n\t"
    "cpi %A0, 0xFF \n\t"
    "cpc %B0, %1 \n\t"
    "brne .-10 \n\t"
    : "+r" (delay), "+a" (tmp)
    : "0" (delay)
    );
}
#else /*GRSAKURA*/
/*
inline void SoftwareSerial::tunedDelay(uint16_t delay)
{
}
*/
#endif/*GRSAKURA*/

// This function sets the current object as the "listening"
// one and returns true if it replaces another 
bool SoftwareSerial::listen()
{
  if (active_object != this)
  {
#ifndef GRSAKURA
    _buffer_overflow = false;
    uint8_t oldSREG = SREG;
    cli();
    _receive_buffer_head = _receive_buffer_tail = 0;
    active_object = this;
    SREG = oldSREG;
#else /*GRSAKURA*/
    _transmit_buffer_head = _transmit_buffer_tail = 0;
    _receive_buffer_head = _receive_buffer_tail = 0;
    active_object = this;
    _transmit_bit_pos = -1;
    _transmit_bit_length_count = 0;
    _receive_bit_pos = -1;
    _receive_bit_length_count = 0;
#endif/*GRSAKURA*/
    return true;
  }

  return false;
}

#ifndef GRSAKURA
//
// The receive routine called by the interrupt handler
//
void SoftwareSerial::recv()
{

#if GCC_VERSION < 40302
// Work-around for avr-gcc 4.3.0 OSX version bug
// Preserve the registers that the compiler misses
// (courtesy of Arduino forum user *etracer*)
  asm volatile(
    "push r18 \n\t"
    "push r19 \n\t"
    "push r20 \n\t"
    "push r21 \n\t"
    "push r22 \n\t"
    "push r23 \n\t"
    "push r26 \n\t"
    "push r27 \n\t"
    ::);
#endif  

  uint8_t d = 0;

  // If RX line is high, then we don't see any start bit
  // so interrupt is probably not for us
  if (_inverse_logic ? rx_pin_read() : !rx_pin_read())
  {
    // Wait approximately 1/2 of a bit width to "center" the sample
    tunedDelay(_rx_delay_centering);
    DebugPulse(_DEBUG_PIN2, 1);

    // Read each of the 8 bits
    for (uint8_t i=0x1; i; i <<= 1)
    {
      tunedDelay(_rx_delay_intrabit);
      DebugPulse(_DEBUG_PIN2, 1);
      uint8_t noti = ~i;
      if (rx_pin_read())
        d |= i;
      else // else clause added to ensure function timing is ~balanced
        d &= noti;
    }

    // skip the stop bit
    tunedDelay(_rx_delay_stopbit);
    DebugPulse(_DEBUG_PIN2, 1);

    if (_inverse_logic)
      d = ~d;

    // if buffer full, set the overflow flag and return
    if ((_receive_buffer_tail + 1) % _SS_MAX_RX_BUFF != _receive_buffer_head) 
    {
      // save new data in buffer: tail points to where byte goes
      _receive_buffer[_receive_buffer_tail] = d; // save new byte
      _receive_buffer_tail = (_receive_buffer_tail + 1) % _SS_MAX_RX_BUFF;
    } 
    else 
    {
#if _DEBUG // for scope: pulse pin as overflow indictator
      DebugPulse(_DEBUG_PIN1, 1);
#endif
      _buffer_overflow = true;
    }
  }

#if GCC_VERSION < 40302
// Work-around for avr-gcc 4.3.0 OSX version bug
// Restore the registers that the compiler misses
  asm volatile(
    "pop r27 \n\t"
    "pop r26 \n\t"
    "pop r23 \n\t"
    "pop r22 \n\t"
    "pop r21 \n\t"
    "pop r20 \n\t"
    "pop r19 \n\t"
    "pop r18 \n\t"
    ::);
#endif
}
#else /*GRSAKURA*/
#if UseCmt2
static const int SamplingMultiples = 4;
#elif UseTpu5
static const int SamplingMultiples = 3;
#endif
void SoftwareSerial::recv()
{
  int received_value = _inverse_logic ? !_receive_bit : _receive_bit;
  if (_receive_bit_pos < 0) {
    if (!received_value) {
      if (++_receive_bit_length_count >= (SamplingMultiples - 1)) { // If a start bit is received.
        _receive_bit_length_count = (SamplingMultiples - 1) / 2;
        _receive_bit_pos = 0;
        _receive_byte = 0;
      }
    } else {
      _receive_bit_length_count = 0;
    }
  } else if (_receive_bit_pos < _format_data_bits) { // data bit.
    if (++_receive_bit_length_count >= SamplingMultiples) {
      _receive_bit_length_count = 0;
      if (received_value) {
        _receive_byte |= (1 << _receive_bit_pos);
      }
      _receive_bit_pos++;
    }
  } else if (_receive_bit_pos == _format_data_bits && (_format_parity == _format_parity_even || _format_parity == _format_parity_odd)) { // The parity bit.
    if (++_receive_bit_length_count >= SamplingMultiples) {
      _receive_bit_length_count = 0;
      _receive_bit_pos++;
      for (int i = 0; i < _format_data_bits; i++) {
        if (_receive_byte & (1 << i)) {
          received_value = !received_value;
        }
      }
      if ((_format_parity == _format_parity_even) != (!received_value)) { // parity error
        _receive_bit_pos = -1;
      }
    }
  } else { // The stop bit
    if (++_receive_bit_length_count >= SamplingMultiples) {
      _receive_bit_length_count = 0;
      if (received_value) {
        if ((_receive_buffer_tail + 1) % _SS_MAX_RX_BUFF != _receive_buffer_head) 
        {
          // save new data in buffer: tail points to where byte goes
          _receive_buffer[_receive_buffer_tail] = _receive_byte; // save new byte
          _receive_buffer_tail = (_receive_buffer_tail + 1) % _SS_MAX_RX_BUFF;
        } 
        else 
        {
#if _DEBUG // for scope: pulse pin as overflow indictator
          DebugPulse(_DEBUG_PIN1, 1);
#endif
          _buffer_overflow = true;
        }
      }
      // Wait one extra period before reading again to de-bounce
      _receive_bit_pos = -1;
    }
  }
}

void SoftwareSerial::send()
{
    int transmit_value = HIGH;

    if (_transmit_bit_pos == -1) { // The start bit.
        if (_transmit_bit_length_count == 0) {
            if (_transmit_buffer_head != _transmit_buffer_tail) {
                _transmit_byte = _transmit_buffer[_transmit_buffer_head];
                _transmit_buffer_head = (_transmit_buffer_head + 1) % _SS_MAX_TX_BUFF;
                transmit_value = LOW;
                _transmit_bit_length_count = 1;
            }
        } else {
            transmit_value = LOW;
            if (++_transmit_bit_length_count >= SamplingMultiples) {
                _transmit_bit_length_count = 0;
                _transmit_bit_pos++;
            }
        }
    } else if (_transmit_bit_pos < _format_data_bits) { // data bit.
        transmit_value = ((_transmit_byte >> _transmit_bit_pos) & 0x01);
        if (++_transmit_bit_length_count >= SamplingMultiples) {
            _transmit_bit_length_count = 0;
            _transmit_bit_pos++;
        }
    } else if (_transmit_bit_pos == _format_data_bits && (_format_parity == _format_parity_even || _format_parity == _format_parity_odd)) { // The parity bit.
        transmit_value = _format_parity == _format_parity_even ? LOW : HIGH;
        for (int i = 0; i < _format_data_bits; i++) {
            if (_transmit_byte & (1 << i)) {
                transmit_value = !transmit_value;
            }
        }
        if (++_transmit_bit_length_count >= SamplingMultiples) {
            _transmit_bit_length_count = 0;
            _transmit_bit_pos++;
        }
    } else { // The stop bit.
        transmit_value = HIGH;
        if (++_transmit_bit_length_count >= (SamplingMultiples * _format_stop_bits)) {
            _transmit_bit_length_count = 0;
            _transmit_bit_pos = -1;
        }
    }

    _transmit_bit = _inverse_logic ? !transmit_value : transmit_value;
}
#endif/*GRSAKURA*/

void SoftwareSerial::tx_pin_write(uint8_t pin_state)
{
  if (pin_state == LOW)
#ifndef GRSAKURA
    *_transmitPortRegister &= ~_transmitBitMask;
#else /*GRSAKURA*/
    BCLR(_transmitPortRegister, _transmitBit);
#endif/*GRSAKURA*/
  else
#ifndef GRSAKURA
    *_transmitPortRegister |= _transmitBitMask;
#else /*GRSAKURA*/
    BSET(_transmitPortRegister, _transmitBit);
#endif/*GRSAKURA*/
}

uint8_t SoftwareSerial::rx_pin_read()
{
  return *_receivePortRegister & _receiveBitMask;
}

//
// Interrupt handling
//

/* static */
inline void SoftwareSerial::handle_interrupt()
{
  if (active_object)
  {
#ifndef GRSAKURA
    active_object->recv();
#else /*GRSAKURA*/
    active_object->_receive_bit = active_object->rx_pin_read() ? HIGH : LOW;
    active_object->tx_pin_write(active_object->_transmit_bit);
    active_object->recv();
    active_object->send();
#endif/*GRSAKURA*/
  }
}

#ifndef GRSAKURA
#if defined(PCINT0_vect)
ISR(PCINT0_vect)
{
  SoftwareSerial::handle_interrupt();
}
#endif

#if defined(PCINT1_vect)
ISR(PCINT1_vect)
{
  SoftwareSerial::handle_interrupt();
}
#endif

#if defined(PCINT2_vect)
ISR(PCINT2_vect)
{
  SoftwareSerial::handle_interrupt();
}
#endif

#if defined(PCINT3_vect)
ISR(PCINT3_vect)
{
  SoftwareSerial::handle_interrupt();
}
#endif
#else /*GRSAKURA*/
#if UseCmt2
void INT_Excep_CMT2_CMI2(void)
#elif UseTpu5
void INT_Excep_TPU5_TGI5A(void)
#endif
{
  SoftwareSerial::handle_interrupt();
}
#endif/*GRSAKURA*/

//
// Constructor
//
#ifndef GRSAKURA
SoftwareSerial::SoftwareSerial(uint8_t receivePin, uint8_t transmitPin, bool inverse_logic /* = false */) : 
  _rx_delay_centering(0),
  _rx_delay_intrabit(0),
  _rx_delay_stopbit(0),
  _tx_delay(0),
  _buffer_overflow(false),
  _inverse_logic(inverse_logic)
{
  setTX(transmitPin);
  setRX(receivePin);
}
#else /*GRSAKURA*/
SoftwareSerial::SoftwareSerial(uint8_t receivePin, uint8_t transmitPin, bool inverse_logic)
{
    setTX(transmitPin);
    setRX(receivePin);
    active_object = 0;
    _inverse_logic = inverse_logic;
    _receive_byte = 0;
    _transmit_byte = 0;
    _transmit_bit = _inverse_logic ? LOW : HIGH;
}
#endif/*GRSAKURA*/

//
// Destructor
//
SoftwareSerial::~SoftwareSerial()
{
  end();
}

void SoftwareSerial::setTX(uint8_t tx)
{
  pinMode(tx, OUTPUT);
  digitalWrite(tx, HIGH);
#ifndef GRSAKURA
  _transmitBitMask = digitalPinToBitMask(tx);
#else /*GRSAKURA*/
  _transmitBit = digitalPinToBit(tx);
#endif/*GRSAKURA*/
  uint8_t port = digitalPinToPort(tx);
  _transmitPortRegister = portOutputRegister(port);
}

void SoftwareSerial::setRX(uint8_t rx)
{
  pinMode(rx, INPUT);
  if (!_inverse_logic)
    digitalWrite(rx, HIGH);  // pullup for normal logic!
  _receivePin = rx;
  _receiveBitMask = digitalPinToBitMask(rx);
  uint8_t port = digitalPinToPort(rx);
  _receivePortRegister = portInputRegister(port);
}

//
// Public methods
//

#ifndef GRSAKURA
void SoftwareSerial::begin(long speed)
{
  _rx_delay_centering = _rx_delay_intrabit = _rx_delay_stopbit = _tx_delay = 0;

  for (unsigned i=0; i<sizeof(table)/sizeof(table[0]); ++i)
  {
    long baud = pgm_read_dword(&table[i].baud);
    if (baud == speed)
    {
      _rx_delay_centering = pgm_read_word(&table[i].rx_delay_centering);
      _rx_delay_intrabit = pgm_read_word(&table[i].rx_delay_intrabit);
      _rx_delay_stopbit = pgm_read_word(&table[i].rx_delay_stopbit);
      _tx_delay = pgm_read_word(&table[i].tx_delay);
      break;
    }
  }

  // Set up RX interrupts, but only if we have a valid RX baud rate
  if (_rx_delay_stopbit)
  {
    if (digitalPinToPCICR(_receivePin))
    {
      *digitalPinToPCICR(_receivePin) |= _BV(digitalPinToPCICRbit(_receivePin));
      *digitalPinToPCMSK(_receivePin) |= _BV(digitalPinToPCMSKbit(_receivePin));
    }
    tunedDelay(_tx_delay); // if we were low this establishes the end
  }

#if _DEBUG
  pinMode(_DEBUG_PIN1, OUTPUT);
  pinMode(_DEBUG_PIN2, OUTPUT);
#endif

  listen();
}
#else /*GRSAKURA*/
void SoftwareSerial::begin(uint32_t speed, uint8_t config)
{
  #define SERIAL_DM  0b0001
  #define SERIAL_PM  0b0110
  #define SERIAL_SM  0b1000
  #define SERIAL_D8  0b0000
  #define SERIAL_D7  0b0001
  #define SERIAL_PN  0b0000
  #define SERIAL_PE  0b0010
  #define SERIAL_PO  0b0100
  #define SERIAL_S1  0b0000
  #define SERIAL_S2  0b1000

  switch (config & SERIAL_DM) {
  case SERIAL_D8:
    _format_data_bits = 8;
    config &= ~SERIAL_DM;
    break;
  case SERIAL_D7:
    _format_data_bits = 7;
    config &= ~SERIAL_DM;
    break;
  default:
    break;
  }
  switch (config & SERIAL_PM) {
  case SERIAL_PN:
    _format_parity = _format_parity_none;
    config &= ~SERIAL_PM;
    break;
  case SERIAL_PE:
    _format_parity = _format_parity_even;
    config &= ~SERIAL_PM;
    break;
  case SERIAL_PO:
    _format_parity = _format_parity_odd;
    config &= ~SERIAL_PM;
    break;
  default:
    break;
  }
  switch (config & SERIAL_SM) {
  case SERIAL_S1:
    _format_stop_bits = 1;
    config &= ~SERIAL_SM;
    break;
  case SERIAL_S2:
    _format_stop_bits = 2;
    config &= ~SERIAL_SM;
    break;
  default:
    break;
  }
  if (config != 0) {
    _format_data_bits = 8;
    _format_parity = _format_parity_none;
    _format_stop_bits = 1;
  }

  _transmit_bit_pos = -1;
  _transmit_bit_length_count = 0;

#if UseCmt2
  st_cmt0_cmcr cmcr;
  startModule(MstpIdCMT2);

  CMT.CMSTR1.BIT.STR2 = 0;
  if (speed < 110 || speed > 115200) {
    speed = 9600;
  }
  cmcr.WORD = 0;
  cmcr.BIT.b7 = 1;
  for (int i = 0; i <= 3; i++) {
    int divider = 8 << (2 * i);
    int32_t cmcor = (2 * PCLK / (divider * SamplingMultiples * speed) + 1) / 2 - 1;
    if (cmcor >= 0 && cmcor <= 0xffff) {
      cmcr.BIT.CKS = i;
      CMT2.CMCR.WORD = cmcr.WORD;
      CMT2.CMCOR = cmcor;
      break;
    }
  }

  IPR(CMT2, CMI2) = 0x7;
  IEN(CMT2, CMI2) = 0x1;
  IR(CMT2, CMI2) = 0x0;

  cmcr.WORD = CMT2.CMCR.WORD;
  cmcr.BIT.CMIE = 1;
  cmcr.BIT.b7 = 1;
  CMT2.CMCR.WORD = cmcr.WORD;
  CMT.CMSTR1.BIT.STR2 = 1;
#elif UseTpu5
  startModule(MstpIdTPU5);

  TPUA.TSTR.BIT.CST5 = 0;
  TPU5.TCR.BIT.CKEG = 0b01;
  TPU5.TCR.BIT.CCLR = 0b001;
  TPU5.TMDR.BIT.MD = 0b0000;
  if (speed < 110 || speed > 115200) {
    speed = 9600;
  }
  TPU5.TCNT = 0;
  for (int i = 0; i <= 3; i++) {
    int divider = 1 << (2 * i);
    int32_t tgra = (2 * PCLK / (divider * SamplingMultiples * speed) + 1) / 2 - 1;
    if (tgra >= 0 && tgra <= 0xffff) {
      TPU5.TCR.BIT.TPSC = i;
      TPU5.TGRA = tgra;
      break;
    }
  }

  IPR(TPU5, TGI5A) = 0x7;
  IEN(TPU5, TGI5A) = 0x1;
  IR(TPU5, TGI5A) = 0x0;

  TPU5.TIER.BIT.TGIEA = 1;
  TPUA.TSTR.BIT.CST5 = 1;
#endif

  _receive_bit_pos = -1;
  _receive_bit_length_count = 0;

  listen();
}
#endif/*GRSAKURA*/

#ifndef GRSAKURA
void SoftwareSerial::end()
{
  if (digitalPinToPCMSK(_receivePin))
    *digitalPinToPCMSK(_receivePin) &= ~_BV(digitalPinToPCMSKbit(_receivePin));
}
#else /*GRSAKURA*/
void SoftwareSerial::end()
{
#if UseCmt2
  IR(CMT2, CMI2) = 0x0;
  st_cmt0_cmcr cmcr;
  cmcr.WORD = CMT2.CMCR.WORD;
  cmcr.BIT.CMIE = 0;
  cmcr.BIT.b7 = 1;
  CMT2.CMCR.WORD = cmcr.WORD;
  IPR(CMT2, CMI2) = 0x0;
  IEN(CMT2, CMI2) = 0x0;
  stopModule(MstpIdCMT2);
#elif UseTpu5
  IR(TPU5, TGI5A) = 0x0;
  TPUA.TSTR.BIT.CST5 = 0;
  IPR(TPU5, TGI5A) = 0x0;
  IEN(TPU5, TGI5A) = 0x0;
  stopModule(MstpIdTPU5);
#endif
}
#endif/*GRSAKURA*/


// Read data from buffer
int SoftwareSerial::read()
{
  if (!isListening())
    return -1;

  // Empty buffer?
  if (_receive_buffer_head == _receive_buffer_tail)
    return -1;

  // Read from "head"
  uint8_t d = _receive_buffer[_receive_buffer_head]; // grab next byte
  _receive_buffer_head = (_receive_buffer_head + 1) % _SS_MAX_RX_BUFF;
  return d;
}

int SoftwareSerial::available()
{
  if (!isListening())
    return 0;

  return (_receive_buffer_tail + _SS_MAX_RX_BUFF - _receive_buffer_head) % _SS_MAX_RX_BUFF;
}

size_t SoftwareSerial::write(uint8_t b)
{
#ifndef GRSAKURA
  if (_tx_delay == 0) {
    setWriteError();
    return 0;
  }

  uint8_t oldSREG = SREG;
  cli();  // turn off interrupts for a clean txmit

  // Write the start bit
  tx_pin_write(_inverse_logic ? HIGH : LOW);
  tunedDelay(_tx_delay + XMIT_START_ADJUSTMENT);

  // Write each of the 8 bits
  if (_inverse_logic)
  {
    for (byte mask = 0x01; mask; mask <<= 1)
    {
      if (b & mask) // choose bit
        tx_pin_write(LOW); // send 1
      else
        tx_pin_write(HIGH); // send 0
    
      tunedDelay(_tx_delay);
    }

    tx_pin_write(LOW); // restore pin to natural state
  }
  else
  {
    for (byte mask = 0x01; mask; mask <<= 1)
    {
      if (b & mask) // choose bit
        tx_pin_write(HIGH); // send 1
      else
        tx_pin_write(LOW); // send 0
    
      tunedDelay(_tx_delay);
    }

    tx_pin_write(HIGH); // restore pin to natural state
  }

  SREG = oldSREG; // turn interrupts back on
  tunedDelay(_tx_delay);
#else /*GRSAKURA*/
  uint8_t _transmit_buffer_tail_next = (_transmit_buffer_tail + 1) % _SS_MAX_TX_BUFF;
  while (_transmit_buffer_tail_next == _transmit_buffer_head)
    ;
  _transmit_buffer[_transmit_buffer_tail] = b;
  _transmit_buffer_tail = _transmit_buffer_tail_next;
#endif/*GRSAKURA*/
  
  return 1;
}

void SoftwareSerial::flush()
{
  if (!isListening())
    return;

#ifndef GRSAKURA
  uint8_t oldSREG = SREG;
  cli();
  _receive_buffer_head = _receive_buffer_tail = 0;
  SREG = oldSREG;
#else
  while (_transmit_buffer_head != _transmit_buffer_tail || _transmit_bit_pos >= 0)
    ;
  _receive_buffer_head = _receive_buffer_tail = 0;
#endif
}

int SoftwareSerial::peek()
{
  if (!isListening())
    return -1;

  // Empty buffer?
  if (_receive_buffer_head == _receive_buffer_tail)
    return -1;

  // Read from "head"
  return _receive_buffer[_receive_buffer_head];
}
