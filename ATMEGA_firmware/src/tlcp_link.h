/*
 * tlcp_link.h - ATmega side of the ETLC protocol (docs/02_protocol_spec.md).
 *
 * Reliability: every command except ACK/NAK is answered. tlcp_send() retransmits the identical
 * frame (same SEQ) after TLC_ACK_TIMEOUT_MS, up to `retries` times. Incoming commands are ACKed
 * from tlcp_poll(); a repeated SEQ+CMD is re-ACKed but not processed twice.
 * While waiting for an ACK, tlcp_send() keeps polling, so a RED_STARTED arriving in the
 * meantime is still ACKed at once.
 */
#ifndef TLCP_LINK_H
#define TLCP_LINK_H

#include <stdint.h>

enum TlcpResult : uint8_t {
  TLCP_OK,     /* ACK with status OK */
  TLCP_NAK,    /* ETLC refused (NAK, or ACK with a non-OK status); reason in *nakReason */
  TLCP_FAIL    /* no reply after all retries */
};

void tlcp_init();

/* Forget the duplicate-detection state and any pending event (call at peak start). */
void tlcp_reset_state();

/* Service the receiver: parse bytes, ACK PING / RED_STARTED, match replies. Call often. */
void tlcp_poll();

/* If a new RED_STARTED has arrived, returns true once and gives secsToGreen and the time
 * (millis) at which it was received. */
bool tlcp_take_red_started(uint16_t &secsToGreen, uint32_t &rxMs);

/* Send a command and wait for its ACK/NAK. `retries` = extra attempts after the first. */
TlcpResult tlcp_send(uint8_t cmd, const uint8_t *payload, uint8_t len,
                     uint8_t retries, uint8_t *nakReason = nullptr);

#endif /* TLCP_LINK_H */
