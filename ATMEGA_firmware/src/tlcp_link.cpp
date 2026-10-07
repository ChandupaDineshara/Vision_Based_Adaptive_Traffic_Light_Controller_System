#include "tlcp_link.h"
#include <Arduino.h>
#include "rs485.h"
#include "power.h"
#include "tlcp_frame.h"
#include "tlc_protocol.h"

#define BUS_IDLE_MS  5      /* silence required before we start transmitting */

static TlcpParser parser;
static uint8_t    txSeq = 0;
static uint32_t   lastRxByteMs = 0;

/* reply matching for tlcp_send() */
static bool    awaiting = false;
static uint8_t awaitSeq = 0;
static uint8_t awaitCmd = 0;
static bool    replyGot = false;
static bool    replyAck = false;
static uint8_t replyStatus = 0;

/* duplicate suppression for commands from the ETLC */
static bool    haveLastRx = false;
static uint8_t lastRxSeq = 0;
static uint8_t lastRxCmd = 0;
static uint8_t lastRxStatus = ST_OK;

/* RED_STARTED event */
static bool     redPending = false;
static uint16_t redSecs = 0;
static uint32_t redRxMs = 0;

static void transmit(const TlcpFrame &f)
{
  uint8_t buf[TLC_MAX_FRAME];
  const uint8_t n = tlcp_encode(f, buf);
  if (n) rs485_write(buf, n);
}

static void send_reply(uint8_t seq, uint8_t cmd, uint8_t ackedCmd, uint8_t status)
{
  TlcpFrame f;
  f.seq = seq;
  f.cmd = cmd;
  f.len = 2;
  f.payload[0] = ackedCmd;
  f.payload[1] = status;
  delay(TLC_REPLY_DELAY_MS);          /* let the ETLC switch to receive */
  transmit(f);
}

static void handle_frame(const TlcpFrame &f)
{
  if (f.cmd == CMD_ACK || f.cmd == CMD_NAK) {
    if (awaiting && f.len >= 2 && f.seq == awaitSeq && f.payload[0] == awaitCmd) {
      replyGot = true;
      replyAck = (f.cmd == CMD_ACK) && (f.payload[1] == ST_OK);
      replyStatus = f.payload[1];
    }
    return;
  }

  /* a command from the ETLC */
  if (haveLastRx && f.seq == lastRxSeq && f.cmd == lastRxCmd) {
    send_reply(f.seq, lastRxStatus == ST_OK ? CMD_ACK : CMD_NAK, f.cmd, lastRxStatus);
    return;                           /* duplicate: answered again, not processed again */
  }

  uint8_t status = ST_OK;
  switch (f.cmd) {
    case CMD_RED_STARTED:
      if (f.len != 2) { status = ST_BAD_LEN; break; }
      redSecs = (uint16_t)(((uint16_t)f.payload[0] << 8) | f.payload[1]);
      redRxMs = millis();
      redPending = true;
      break;
    case CMD_PING:
      break;
    default:
      status = ST_UNKNOWN_CMD;
      break;
  }

  haveLastRx = true;
  lastRxSeq = f.seq;
  lastRxCmd = f.cmd;
  lastRxStatus = status;
  send_reply(f.seq, status == ST_OK ? CMD_ACK : CMD_NAK, f.cmd, status);
}

void tlcp_init()
{
  parser.reset();
  txSeq = 0;
  tlcp_reset_state();
}

void tlcp_reset_state()
{
  haveLastRx = false;
  redPending = false;
  awaiting = false;
  replyGot = false;
}

void tlcp_poll()
{
  uint8_t b;
  TlcpFrame f;
  while (rs485_read(b)) {
    lastRxByteMs = millis();
    if (parser.feed(b, lastRxByteMs, f)) handle_frame(f);
  }
}

bool tlcp_take_red_started(uint16_t &secsToGreen, uint32_t &rxMs)
{
  if (!redPending) return false;
  redPending = false;
  secsToGreen = redSecs;
  rxMs = redRxMs;
  return true;
}

TlcpResult tlcp_send(uint8_t cmd, const uint8_t *payload, uint8_t len,
                     uint8_t retries, uint8_t *nakReason)
{
  if (len > TLC_MAX_PAYLOAD) return TLCP_FAIL;

  TlcpFrame f;
  f.seq = txSeq++;
  f.cmd = cmd;
  f.len = len;
  for (uint8_t i = 0; i < len; i++) f.payload[i] = payload[i];

  awaiting = true;
  awaitSeq = f.seq;
  awaitCmd = cmd;

  for (uint8_t attempt = 0; attempt <= retries; attempt++) {
    /* wait for the bus to be quiet (never longer than 50 ms) */
    const uint32_t t0 = millis();
    do {
      tlcp_poll();
      power_feed();
    } while ((parser.busy() || (millis() - lastRxByteMs) < BUS_IDLE_MS) && (millis() - t0) < 50UL);

    replyGot = false;
    transmit(f);

    const uint32_t sent = millis();
    while ((millis() - sent) < TLC_ACK_TIMEOUT_MS && !replyGot) {
      tlcp_poll();
      power_feed();
    }

    if (replyGot) {
      awaiting = false;
      if (replyAck) return TLCP_OK;
      if (nakReason) *nakReason = replyStatus;
      return TLCP_NAK;
    }
  }

  awaiting = false;
  return TLCP_FAIL;
}
