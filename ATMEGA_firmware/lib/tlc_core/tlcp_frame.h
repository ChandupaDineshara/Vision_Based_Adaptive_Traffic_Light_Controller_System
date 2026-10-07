/*
 * tlcp_frame.h - TLCP frame encode and byte-wise parser (docs/02_protocol_spec.md).
 *   [0xA5][SEQ][CMD][LEN][PAYLOAD 0..4][CRC8 over SEQ..PAYLOAD]
 * Pure logic: time is passed in, no UART access.
 */
#ifndef TLCP_FRAME_H
#define TLCP_FRAME_H

#include <stdint.h>
#include "tlc_protocol.h"

struct TlcpFrame {
  uint8_t seq;
  uint8_t cmd;
  uint8_t len;
  uint8_t payload[TLC_MAX_PAYLOAD];
};

/* Encode into out[] (needs TLC_MAX_FRAME bytes). Returns the frame length, or 0 if len is too big. */
uint8_t tlcp_encode(const TlcpFrame &f, uint8_t *out);

class TlcpParser {
public:
  TlcpParser() { reset(); }
  void reset();

  /* Feed one received byte with the current time in ms. Returns true when `out` holds a
   * complete frame with a valid CRC. A gap longer than TLC_INTERBYTE_TIMEOUT_MS inside a
   * frame drops the partial frame; the byte is then treated as a possible start of frame. */
  bool feed(uint8_t b, uint32_t nowMs, TlcpFrame &out);

  /* True while the middle of a frame is being received. */
  bool busy() const { return state_ != S_SOF; }

private:
  enum State : uint8_t { S_SOF, S_SEQ, S_CMD, S_LEN, S_PAYLOAD, S_CRC };
  State    state_;
  TlcpFrame f_;
  uint8_t  idx_;
  uint32_t lastByteMs_;
};

#endif /* TLCP_FRAME_H */
