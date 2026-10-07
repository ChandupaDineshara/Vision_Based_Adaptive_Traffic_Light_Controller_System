/*
 * params.h - ATmega parameter file (docs/04_register_maps.md, section A).
 * The struct is stored byte for byte in EEPROM; offsets in the doc match.
 * Pure logic: no EEPROM access here (see src/params_store.*).
 */
#ifndef PARAMS_H
#define PARAMS_H

#include <stdint.h>

#define PARAMS_MAGIC      0x544C   /* "TL" */
#define PARAMS_VERSION    1
#define SCHED_MAX         4
#define SCHED_FLAG_ENABLED 0x01

struct SchedEntry {
  uint8_t dowMask;     /* bit0 = Sunday ... bit6 = Saturday */
  uint8_t startHour;
  uint8_t startMin;
  uint8_t endHour;
  uint8_t endMin;
  uint8_t flags;       /* bit0 = enabled */
} __attribute__((packed));

struct Params {
  uint16_t magic;               /* 0x00 */
  uint8_t  version;             /* 0x02 */
  uint16_t accumDelayS;         /* 0x03 A: wait after red starts */
  uint16_t accumMinS;           /* 0x05 A_MIN: below this the cycle is skipped */
  uint8_t  guardS;              /* 0x07 G: safety margin before green */
  uint8_t  espTimeoutS;         /* 0x08 max time from wake pulse to a valid result */
  uint16_t espFallbackReads;    /* 0x09 extra I2C reads after the timeout */
  uint16_t greenMinS;           /* 0x0B */
  uint16_t greenMaxS;           /* 0x0D */
  uint16_t greenS[4];           /* 0x0F green seconds for density level 0..3 */
  uint16_t pingRetryS;          /* 0x17 */
  uint8_t  peakStartRetries;    /* 0x19 */
  uint8_t  schedCount;          /* 0x1A */
  SchedEntry sched[SCHED_MAX];  /* 0x1B */
  uint8_t  crc;                 /* 0x33 CRC-8 over bytes 0x00..0x32 */
} __attribute__((packed));

/* Fill with the defaults of docs/04_register_maps.md (green times are placeholders). */
void    params_set_defaults(Params &p);

/* CRC-8 over everything except the crc byte. */
uint8_t params_compute_crc(const Params &p);

/* Recompute and store the crc byte. */
void    params_seal(Params &p);

/* Magic, version, crc and sanity of every field and schedule entry. */
bool    params_valid(const Params &p);

#endif /* PARAMS_H */
