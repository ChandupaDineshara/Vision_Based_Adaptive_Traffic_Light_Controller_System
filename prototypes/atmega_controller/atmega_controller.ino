#include <Wire.h>
#include <avr/sleep.h>
#include <avr/wdt.h>
#include <avr/interrupt.h>

// ============================================================
// Shared wake line (open-drain style, active LOW)
//   ATmega physical pin 15 = PB1 = Arduino D9  <->  ESP32 GPIO13
//   LOW    = assert signal
//   INPUT  = release (external pull-ups take the line HIGH)
//   NEVER drive this pin HIGH.
//
// I2C: A4 = SDA, A5 = SCL  <->  ESP32 GPIO15 (SDA), GPIO14 (SCL)
// ============================================================

#define ESP32_ADDRESS     0x08
#define SHARED_WAKE_PIN   9        // PB1 / PCINT1
#define GET_DATA_COMMAND  1

#define WAKE_PULSE_MS     100      // how long we hold the line LOW to wake ESP32
#define STARTUP_DELAY_MS  3000     // let ESP32 boot and enter deep sleep after power-up
#define CYCLE_DELAY_MS    5000     // ATmega waits this long before waking ESP32 again
#define MAX_WAIT_TICKS    2        // each tick = 8 s watchdog -> give up after ~16 s

// ============================================================
// RTC (DS3231 or DS1307) on the same I2C bus
// ============================================================

#define RTC_ADDRESS                0x68
#define SET_RTC_FROM_COMPILE_TIME  0   // set to 1, upload once, then set back to 0 and upload again

struct RtcTime
{
  uint8_t  sec, min, hour, day, month;
  uint16_t year;
};

volatile bool    esp32Ready = false;
volatile uint8_t wdtTicks   = 0;


// ============================================================
// Interrupt handlers
// ============================================================

// Pin change on PB1. Fires on both edges; we only care about LOW.
ISR(PCINT0_vect)
{
  if (digitalRead(SHARED_WAKE_PIN) == LOW)
  {
    esp32Ready = true;
  }
}

// Watchdog in interrupt-only mode: used as a timeout while sleeping.
ISR(WDT_vect)
{
  wdtTicks++;
}


// ============================================================
// Helpers
// ============================================================

void enableSharedInterrupt()
{
  cli();
  PCIFR |= _BV(PCIF0);      // clear stale flag
  PCMSK0 |= _BV(PCINT1);    // enable PB1
  PCICR  |= _BV(PCIE0);     // enable port B pin-change interrupts
  sei();
}

void disableSharedInterrupt()
{
  cli();
  PCMSK0 &= ~_BV(PCINT1);
  PCIFR  |= _BV(PCIF0);
  sei();
}

void startWatchdogInterrupt()   // 8 s, interrupt only (no reset)
{
  cli();
  wdt_reset();
  MCUSR &= ~_BV(WDRF);
  WDTCSR |= _BV(WDCE) | _BV(WDE);
  WDTCSR = _BV(WDIE) | _BV(WDP3) | _BV(WDP0);
  sei();
}

void stopWatchdog()
{
  cli();
  wdt_reset();
  MCUSR &= ~_BV(WDRF);
  WDTCSR |= _BV(WDCE) | _BV(WDE);
  WDTCSR = 0;
  sei();
}

bool waitForLineHigh(uint16_t timeoutMs)
{
  uint32_t start = millis();
  while (digitalRead(SHARED_WAKE_PIN) == LOW)
  {
    if (millis() - start > timeoutMs) return false;
  }
  return true;
}


// ============================================================
// STEP 1: ATmega wakes ESP32
// ============================================================

void wakeESP32()
{
  Serial.println();
  Serial.println("Pulling shared line LOW -> waking ESP32.");

  // Our own pulse must not trigger our own interrupt
  disableSharedInterrupt();

  digitalWrite(SHARED_WAKE_PIN, LOW);   // latch LOW first
  pinMode(SHARED_WAKE_PIN, OUTPUT);     // now pull the line LOW

  delay(WAKE_PULSE_MS);

  pinMode(SHARED_WAKE_PIN, INPUT);      // release; pull-ups bring it HIGH

  if (!waitForLineHigh(500))
  {
    Serial.println("WARNING: line did not return HIGH after wake pulse.");
  }

  cli();
  PCIFR |= _BV(PCIF0);
  sei();

  Serial.println("Wake pulse finished.");
}


// ============================================================
// STEP 2: ATmega sleeps until ESP32 pulls the line LOW
// Returns false if nothing arrived within the timeout.
// ============================================================

bool sleepUntilESP32Ready()
{
  esp32Ready = false;
  wdtTicks   = 0;

  pinMode(SHARED_WAKE_PIN, INPUT);
  enableSharedInterrupt();
  startWatchdogInterrupt();

  // If the line is already LOW there will be no edge, so handle it now
  if (digitalRead(SHARED_WAKE_PIN) == LOW)
  {
    esp32Ready = true;
  }

  Serial.println("ATmega entering sleep...");
  Serial.flush();

  while (!esp32Ready && wdtTicks < MAX_WAIT_TICKS)
  {
    set_sleep_mode(SLEEP_MODE_PWR_DOWN);

    cli();
    if (!esp32Ready && wdtTicks < MAX_WAIT_TICKS)
    {
      sleep_enable();
      sei();          // the instruction after sei() always runs before an interrupt
      sleep_cpu();    // sleeps here
      sleep_disable();
    }
    else
    {
      sei();
    }
  }

  stopWatchdog();
  disableSharedInterrupt();

  if (!esp32Ready)
  {
    Serial.println("Timeout: no signal from ESP32.");
    return false;
  }

  // ESP32 may still be holding the line LOW; wait for release
  waitForLineHigh(500);
  delay(20);

  Serial.println("ATmega woke up! ESP32 says data is ready.");
  return true;
}


// ============================================================
// STEP 3: Request number from ESP32 over I2C
// ============================================================

bool requestNumberFromESP32(uint16_t &receivedValue)
{
  Wire.beginTransmission(ESP32_ADDRESS);
  Wire.write(GET_DATA_COMMAND);
  byte error = Wire.endTransmission();

  if (error != 0)
  {
    Serial.print("I2C command error: ");
    Serial.println(error);
    return false;
  }

  Serial.println("GET_DATA command sent.");
  delay(20);

  uint8_t count = Wire.requestFrom(ESP32_ADDRESS, (uint8_t)2);

  if (count != 2)
  {
    Serial.print("Expected 2 bytes, received: ");
    Serial.println(count);
    while (Wire.available()) Wire.read();
    return false;
  }

  byte lowByte  = Wire.read();
  byte highByte = Wire.read();

  receivedValue = ((uint16_t)highByte << 8) | lowByte;
  return true;
}


// ============================================================
// RTC helpers
// ============================================================

static uint8_t bcd2dec(uint8_t v) { return (v >> 4) * 10 + (v & 0x0F); }
static uint8_t dec2bcd(uint8_t v) { return ((v / 10) << 4) | (v % 10); }

bool readRTC(RtcTime &t)
{
  Wire.beginTransmission((uint8_t)RTC_ADDRESS);
  Wire.write((uint8_t)0x00);                 // start at the seconds register
  if (Wire.endTransmission() != 0)
  {
    return false;
  }

  if (Wire.requestFrom((uint8_t)RTC_ADDRESS, (uint8_t)7) != 7)
  {
    while (Wire.available()) Wire.read();
    return false;
  }

  uint8_t s  = Wire.read();
  uint8_t m  = Wire.read();
  uint8_t h  = Wire.read();
  Wire.read();                               // day of week (unused)
  uint8_t d  = Wire.read();
  uint8_t mo = Wire.read();
  uint8_t y  = Wire.read();

  t.sec   = bcd2dec(s  & 0x7F);
  t.min   = bcd2dec(m  & 0x7F);
  t.hour  = bcd2dec(h  & 0x3F);              // 24-hour mode
  t.day   = bcd2dec(d  & 0x3F);
  t.month = bcd2dec(mo & 0x1F);
  t.year  = 2000 + bcd2dec(y);

  return true;
}

void printRTC(const RtcTime &t)
{
  char buf[24];
  snprintf(buf, sizeof(buf), "%04u-%02u-%02u %02u:%02u:%02u",
           (unsigned)t.year, (unsigned)t.month, (unsigned)t.day,
           (unsigned)t.hour, (unsigned)t.min,   (unsigned)t.sec);
  Serial.print(buf);
}

// Sets the RTC to the PC time at the moment the sketch was compiled
// (accurate to a few seconds; the upload delay is not compensated).
void setRTCFromCompileTime()
{
  const char *months = "JanFebMarAprMayJunJulAugSepOctNovDec";
  char mon[4] = { __DATE__[0], __DATE__[1], __DATE__[2], 0 };

  uint8_t  month = (strstr(months, mon) - months) / 3 + 1;
  uint8_t  day   = atoi(__DATE__ + 4);
  uint16_t year  = atoi(__DATE__ + 7);

  uint8_t hh = atoi(__TIME__);
  uint8_t mm = atoi(__TIME__ + 3);
  uint8_t ss = atoi(__TIME__ + 6);

  Wire.beginTransmission((uint8_t)RTC_ADDRESS);
  Wire.write((uint8_t)0x00);
  Wire.write(dec2bcd(ss));
  Wire.write(dec2bcd(mm));
  Wire.write(dec2bcd(hh));
  Wire.write((uint8_t)1);                    // day of week placeholder
  Wire.write(dec2bcd(day));
  Wire.write(dec2bcd(month));
  Wire.write(dec2bcd(year - 2000));
  byte err = Wire.endTransmission();

  Serial.print("RTC set from compile time, I2C status: ");
  Serial.println(err);
}


// ============================================================
// SETUP / LOOP
// ============================================================

void setup()
{
  Serial.begin(9600);

  pinMode(SHARED_WAKE_PIN, INPUT);   // released
  disableSharedInterrupt();

  Wire.begin();
  Wire.setClock(100000);

#if SET_RTC_FROM_COMPILE_TIME
  setRTCFromCompileTime();
#endif

  Serial.println();
  Serial.println("==============================");
  Serial.println("ATmega started");
  Serial.println("==============================");
  RtcTime now;
  if (readRTC(now))
  {
    Serial.print("RTC time: ");
    printRTC(now);
    Serial.println();
  }
  else
  {
    Serial.println("WARNING: RTC not found at 0x68 (check wiring).");
  }

  Serial.println("Waiting for ESP32 to boot and enter deep sleep...");

  delay(STARTUP_DELAY_MS);
}

void loop()
{
  Serial.println();
  Serial.println("========== NEW CYCLE ==========");

  // 1. Wake ESP32
  wakeESP32();

  // 2. Sleep until ESP32 signals (about 5 s later)
  if (!sleepUntilESP32Ready())
  {
    delay(1000);
    return;               // start a new cycle: pulse again
  }

  // Timestamp from the RTC (millis() stops during sleep, the RTC does not)
  RtcTime stamp;
  if (readRTC(stamp))
  {
    Serial.print("RTC time: ");
    printRTC(stamp);
    Serial.println();
  }
  else
  {
    Serial.println("RTC read failed.");
  }

  // 3. Ask for the number
  uint16_t result;
  if (requestNumberFromESP32(result))
  {
    Serial.print("Received number from ESP32: ");
    Serial.println(result);
  }
  else
  {
    Serial.println("Failed to receive number.");
  }

  // 4. Wait, then repeat
  Serial.println("Waiting 5 seconds before next cycle...");
  delay(CYCLE_DELAY_MS);
}
