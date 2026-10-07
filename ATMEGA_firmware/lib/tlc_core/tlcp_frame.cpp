#include "tlcp_frame.h"

uint8_t tlcp_encode(const TlcpFrame &f, uint8_t *out)
{
  if (f.len > TLC_MAX_PAYLOAD) return 0;

  uint8_t n = 0;
  out[n++] = TLC_SOF;
  out[n++] = f.seq;
  out[n++] = f.cmd;
  out[n++] = f.len;
  for (uint8_t i = 0; i < f.len; i++) out[n++] = f.payload[i];
  out[n] = tlc_crc8(&out[1], (uint8_t)(n - 1));   /* SEQ..PAYLOAD */
  n++;
  return n;
}

void TlcpParser::reset()
{
  state_ = S_SOF;
  idx_ = 0;
  lastByteMs_ = 0;
  f_.seq = f_.cmd = f_.len = 0;
}

bool TlcpParser::feed(uint8_t b, uint32_t nowMs, TlcpFrame &out)
{
  if (state_ != S_SOF && (uint32_t)(nowMs - lastByteMs_) > TLC_INTERBYTE_TIMEOUT_MS) {
    state_ = S_SOF;                       /* stale partial frame */
  }
  lastByteMs_ = nowMs;

  switch (state_) {
    case S_SOF:
      if (b == TLC_SOF) state_ = S_SEQ;
      break;

    case S_SEQ:
      f_.seq = b;
      state_ = S_CMD;
      break;

    case S_CMD:
      f_.cmd = b;
      state_ = S_LEN;
      break;

    case S_LEN:
      if (b > TLC_MAX_PAYLOAD) {
        state_ = (b == TLC_SOF) ? S_SEQ : S_SOF;
        break;
      }
      f_.len = b;
      idx_ = 0;
      state_ = (b == 0) ? S_CRC : S_PAYLOAD;
      break;

    case S_PAYLOAD:
      f_.payload[idx_++] = b;
      if (idx_ >= f_.len) state_ = S_CRC;
      break;

    case S_CRC: {
      uint8_t tmp[TLC_MAX_PAYLOAD + 3];
      tmp[0] = f_.seq; tmp[1] = f_.cmd; tmp[2] = f_.len;
      for (uint8_t i = 0; i < f_.len; i++) tmp[3 + i] = f_.payload[i];
      const bool ok = (tlc_crc8(tmp, (uint8_t)(3 + f_.len)) == b);
      state_ = S_SOF;
      if (ok) {
        out = f_;
        return true;
      }
      break;
    }
  }
  return false;
}
