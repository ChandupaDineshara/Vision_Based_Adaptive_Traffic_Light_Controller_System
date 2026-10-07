#include "wake_line.h"
#include <Arduino.h>
#include "pins.h"
#include "power.h"
#include "tlc_protocol.h"

static bool     lowSeen = false;
static uint32_t lowSince = 0;

void wake_line_init()
{
  pinMode(PIN_WAKE_LINE, INPUT);     /* released, no internal pull-up */
  wake_line_done_reset();
}

bool wake_line_is_low()
{
  return digitalRead(PIN_WAKE_LINE) == LOW;
}

bool wake_line_pulse_wake()
{
  digitalWrite(PIN_WAKE_LINE, LOW);  /* latch LOW first ... */
  pinMode(PIN_WAKE_LINE, OUTPUT);    /* ... then pull the line LOW */
  delay(WAKE_PULSE_ATMEGA_MS);
  pinMode(PIN_WAKE_LINE, INPUT);     /* release */

  const uint32_t start = millis();
  while (wake_line_is_low()) {
    if (millis() - start > 500UL) return false;
    power_feed();
  }
  return true;
}

void wake_line_done_reset()
{
  lowSeen = false;
}

bool wake_line_done_seen()
{
  const bool low = wake_line_is_low();

  if (!lowSeen) {
    if (low) { lowSeen = true; lowSince = millis(); }
    return false;
  }
  if (low) return false;              /* still LOW, keep timing */

  lowSeen = false;
  const uint32_t width = millis() - lowSince;
  return width >= DONE_PULSE_MIN_MS && width <= DONE_PULSE_MAX_MS;
}
