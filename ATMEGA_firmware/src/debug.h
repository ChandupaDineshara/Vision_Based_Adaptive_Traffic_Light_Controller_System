/*
 * debug.h - trace output for a logic analyzer (the only UART is used by RS-485).
 *
 * Release build: every macro compiles to nothing.
 * Debug build (-DTLC_DEBUG=1): TX-only bit-bang on PIN_DEBUG_TX at about 57600 baud,
 * one short line per event: "<tag><4 hex digits>" plus a newline. Keep events few: each byte blocks
 * interrupts for about 0.17 ms, which can drop bytes on the 9600 baud RS-485 link.
 */
#ifndef DEBUG_H
#define DEBUG_H

#include <stdint.h>

#if defined(TLC_DEBUG) && TLC_DEBUG
  void dbg_init();
  void dbg_trace(char tag, uint16_t value);
  #define DBG_INIT()            dbg_init()
  #define DBG_STATE(s)          dbg_trace('S', (uint16_t)(s))
  #define DBG_FAULT(f)          dbg_trace('F', (uint16_t)(f))
  #define DBG_VAL(tag, v)       dbg_trace((tag), (uint16_t)(v))
#else
  #define DBG_INIT()            ((void)0)
  #define DBG_STATE(s)          ((void)0)
  #define DBG_FAULT(f)          ((void)0)
  #define DBG_VAL(tag, v)       ((void)0)
#endif

#endif /* DEBUG_H */
