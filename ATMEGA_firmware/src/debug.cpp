#include "debug.h"

#if defined(TLC_DEBUG) && TLC_DEBUG

#include <Arduino.h>
#include "pins.h"

/* Transmit-only bit-bang, 8N1 at about 57600 baud. SoftwareSerial is not used because it owns the
 * pin-change interrupt vectors, which the RTC alarm input needs (see power.cpp). Interrupts are
 * off for the 0.17 ms of one byte, like SoftwareSerial. */
#define DBG_BIT_US  17

static volatile uint8_t *outReg;
static uint8_t outMask;

void dbg_init()
{
  pinMode(PIN_DEBUG_TX, OUTPUT);
  digitalWrite(PIN_DEBUG_TX, HIGH);
  outReg = portOutputRegister(digitalPinToPort(PIN_DEBUG_TX));
  outMask = digitalPinToBitMask(PIN_DEBUG_TX);
}

static void send_byte(uint8_t b)
{
  const uint8_t sreg = SREG;
  cli();
  *outReg &= ~outMask;                       /* start bit */
  delayMicroseconds(DBG_BIT_US);
  for (uint8_t i = 0; i < 8; i++) {
    if (b & 1) *outReg |= outMask; else *outReg &= ~outMask;
    b >>= 1;
    delayMicroseconds(DBG_BIT_US);
  }
  *outReg |= outMask;                        /* stop bit */
  delayMicroseconds(DBG_BIT_US);
  SREG = sreg;
}

void dbg_trace(char tag, uint16_t value)
{
  static const char hex[] = "0123456789ABCDEF";
  send_byte((uint8_t)tag);
  send_byte((uint8_t)hex[(value >> 12) & 0xF]);
  send_byte((uint8_t)hex[(value >> 8) & 0xF]);
  send_byte((uint8_t)hex[(value >> 4) & 0xF]);
  send_byte((uint8_t)hex[value & 0xF]);
  send_byte('\n');
}

#endif
