#include <Wire.h>
#include "esp_sleep.h"
#include "driver/rtc_io.h"

// ============================================================
// Shared wake line (open-drain style, active LOW)
//   ESP32 GPIO13  <->  ATmega pin 15 (D9)
//   OUTPUT LOW = assert signal
//   INPUT      = release (external 3.3 V pull-up)
//   NEVER drive this pin HIGH.
//
// I2C slave: SDA = GPIO15, SCL = GPIO14
// Note: GPIO13/14/15 are SD-card pins on the ESP32-CAM, so do
// not insert an SD card while using them.
// ============================================================

#define I2C_SLAVE_ADDRESS   0x08
#define I2C_SDA             15
#define I2C_SCL             14

#define SHARED_WAKE_GPIO      13
#define SHARED_WAKE_RTC_GPIO  GPIO_NUM_13

#define GET_DATA_COMMAND    1

#define WORK_TIME_MS            5000    // simulated work before signalling ATmega
#define WAKE_PULSE_MS           100
#define ATMEGA_REQUEST_TIMEOUT_MS 10000 // sleep anyway if ATmega never asks for data

volatile bool getDataCommandReceived = false;
volatile bool dataWasSent            = false;

// Survives deep sleep: first result will be 1235, then 1236, ...
RTC_DATA_ATTR uint16_t valueToSend = 1234;

unsigned long waitingSince = 0;


// ============================================================
// I2C callbacks
// ============================================================

void onReceive(int numberOfBytes)
{
  while (Wire.available())
  {
    byte command = Wire.read();
    if (command == GET_DATA_COMMAND)
    {
      getDataCommandReceived = true;
    }
  }
}

void onRequest()
{
  uint16_t value = valueToSend;

  Wire.write((byte)(value & 0xFF));         // low byte first
  Wire.write((byte)((value >> 8) & 0xFF));  // then high byte

  dataWasSent = true;
}


// ============================================================
// Helpers
// ============================================================

bool waitForLineHigh(uint16_t timeoutMs)
{
  unsigned long start = millis();
  while (digitalRead(SHARED_WAKE_GPIO) == LOW)
  {
    if (millis() - start > timeoutMs) return false;
    delay(1);
  }
  return true;
}


// ESP32 -> ATmega signal: pull the line LOW briefly, then release
void wakeATmega()
{
  Serial.println();
  Serial.println("Waking ATmega...");

  digitalWrite(SHARED_WAKE_GPIO, LOW);   // latch LOW
  pinMode(SHARED_WAKE_GPIO, OUTPUT);     // pull line LOW

  delay(WAKE_PULSE_MS);

  pinMode(SHARED_WAKE_GPIO, INPUT);      // release

  waitForLineHigh(500);

  Serial.println("ATmega wake pulse finished.");
}


void goToDeepSleep()
{
  Serial.println();
  Serial.println("Preparing for deep sleep...");

  // Release the shared line and make sure it's HIGH,
  // otherwise the LOW-level wake source would fire immediately.
  pinMode(SHARED_WAKE_GPIO, INPUT);
  waitForLineHigh(2000);

  // Wake when GPIO13 goes LOW
  esp_sleep_enable_ext0_wakeup(SHARED_WAKE_RTC_GPIO, 0);

  // External pull-up is present; disable internal RTC pulls
  rtc_gpio_pullup_dis(SHARED_WAKE_RTC_GPIO);
  rtc_gpio_pulldown_dis(SHARED_WAKE_RTC_GPIO);

  Serial.println("ESP32 entering deep sleep.");
  Serial.flush();
  delay(20);

  esp_deep_sleep_start();
  // never returns
}


// ============================================================
// SETUP  (runs after every wake, because deep sleep resets the chip)
// ============================================================

void setup()
{
  Serial.begin(115200);
  delay(100);

  pinMode(SHARED_WAKE_GPIO, INPUT);

  esp_sleep_wakeup_cause_t cause = esp_sleep_get_wakeup_cause();

  Serial.println();
  Serial.println("==============================");
  Serial.println("ESP32-CAM START");
  Serial.println("==============================");

  // Power-on / reset: don't work, just go to sleep and wait for ATmega
  if (cause != ESP_SLEEP_WAKEUP_EXT0)
  {
    Serial.println("Initial power-on/reset -> deep sleep.");
    goToDeepSleep();
  }

  Serial.println("Wake signal received from ATmega!");

  // Wait for ATmega to release the line
  waitForLineHigh(1000);
  Serial.println("Shared line released.");

  // Start I2C slave
  if (!Wire.begin(I2C_SLAVE_ADDRESS, I2C_SDA, I2C_SCL, 100000))
  {
    Serial.println("ERROR: I2C failed to start!");
    delay(1000);
    goToDeepSleep();
  }

  Wire.onReceive(onReceive);
  Wire.onRequest(onRequest);

  Serial.print("I2C slave started, address 0x");
  Serial.println(I2C_SLAVE_ADDRESS, HEX);

  // ----------------------------------------------------------
  // Simulated work. Replace with camera init / capture / processing.
  // ----------------------------------------------------------
  Serial.println("ESP32 working for 5 seconds...");
  delay(WORK_TIME_MS);

  valueToSend++;
  Serial.print("Result prepared = ");
  Serial.println(valueToSend);

  // Tell the sleeping ATmega the result is ready
  wakeATmega();

  waitingSince = millis();
  Serial.println("Waiting for ATmega I2C request...");
}


// ============================================================
// LOOP
// ============================================================

void loop()
{
  if (getDataCommandReceived)
  {
    Serial.println("GET_DATA command received from ATmega.");
    getDataCommandReceived = false;
  }

  // Number has been read by ATmega -> sleep
  if (dataWasSent)
  {
    dataWasSent = false;

    Serial.print("Number sent to ATmega: ");
    Serial.println(valueToSend);

    delay(100);            // let the I2C transfer finish
    goToDeepSleep();
  }

  // Safety: never stay awake forever (we'd miss the next wake pulse)
  if (millis() - waitingSince > ATMEGA_REQUEST_TIMEOUT_MS)
  {
    Serial.println("Timeout waiting for ATmega request.");
    goToDeepSleep();
  }

  delay(10);
}
