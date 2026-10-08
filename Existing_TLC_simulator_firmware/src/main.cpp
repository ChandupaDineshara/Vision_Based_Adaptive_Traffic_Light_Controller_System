/*
 * Traffic-light controller (Uno) - talks to the ATmega over RS485 (MAX485).
 * This Uno never sleeps. It only counts.
 *
 * Every CYCLE_PERIOD_MS:
 *   1. wake pulse: hold the bus LOW for WAKE_LOW_MS (wakes the sleeping ATmega;
 *      the ATmega only listens for it during peak time, otherwise it is ignored)
 *   2. switch to receive and wait up to RESULT_TIMEOUT_MS for the ATmega's
 *      result line   "DENSITY <0|1|2>"
 *   3. answer "ACK <d>" and go back to driving the bus (so it idles HIGH)
 *
 * MAX485 wiring on this Uno:
 *   RO -> D10 (RX)    DI -> D11 (TX)    DE + RE tied together -> D2
 *   A <-> A,  B <-> B,  common GND with the ATmega board.
 * An undriven bus can float and produce junk bytes while we listen; lines that
 * are not a valid DENSITY message are ignored. 120 ohm termination at both ends
 * and bias resistors (A pull-up, B pull-down) make the bus quiet.
 * The USB serial monitor (9600) shows what happens.
 */

#include <Arduino.h>
#include <SoftwareSerial.h>

#define RS485_RX    10
#define RS485_TX    11
#define RS485_DIR    2
#define BAUD         9600

#define CYCLE_PERIOD_MS    20000UL   // time between wake pulses
#define WAKE_LOW_MS        10        // longer than the ATmega wake-up time (~1 ms)
#define RESULT_TIMEOUT_MS  70000UL   // ESP32 photo + Wi-Fi upload can take ~40 s

SoftwareSerial rs485(RS485_RX, RS485_TX);

unsigned long cycleCount = 0;
unsigned long lastPulse = 0;

void driverOn()  { digitalWrite(RS485_DIR, HIGH); }   // transmit, bus idles HIGH
void driverOff() { digitalWrite(RS485_DIR, LOW);  }   // receive

void wakePulse()
{
  driverOn();
  delay(2);
  digitalWrite(RS485_TX, LOW);         // TX pin as a plain pin, held LOW
  delay(WAKE_LOW_MS);
  digitalWrite(RS485_TX, HIGH);
}

// Wait for "DENSITY <d>". Returns 0..2, or -1 on timeout.
int waitForDensity(unsigned long timeoutMs)
{
  String line = "";
  unsigned long t0 = millis();

  while (millis() - t0 < timeoutMs)
  {
    if (!rs485.available()) continue;

    char c = rs485.read();
    if (c != '\n')
    {
      if (c != '\r' && line.length() < 20) line += c;
      continue;
    }

    line.trim();
    if (line.startsWith("DENSITY ") && line.length() == 9)
    {
      int d = line.charAt(8) - '0';
      if (d >= 0 && d <= 2) return d;
    }
    line = "";                          // junk or empty line: start over
  }
  return -1;
}

void sendAck(int density)
{
  delay(5);                             // the ATmega switches to receive after its frame
  driverOn();
  delay(2);
  rs485.print("ACK ");
  rs485.println(density);
  rs485.flush();
  delay(2);
}

void setup()
{
  Serial.begin(9600);
  pinMode(RS485_DIR, OUTPUT);
  driverOn();
  rs485.begin(BAUD);
  pinMode(LED_BUILTIN, OUTPUT);

  Serial.println(F("Traffic light controller (Uno) ready"));
  lastPulse = millis() - CYCLE_PERIOD_MS;   // first pulse right away
}

void loop()
{
  if (millis() - lastPulse < CYCLE_PERIOD_MS) return;
  lastPulse = millis();

  cycleCount++;
  digitalWrite(LED_BUILTIN, HIGH);

  Serial.print(F("Cycle "));
  Serial.print(cycleCount);
  Serial.print(F(": wake pulse -> "));

  wakePulse();
  driverOff();                          // listen for the result
  while (rs485.available()) rs485.read();

  int density = waitForDensity(RESULT_TIMEOUT_MS);

  if (density >= 0)
  {
    Serial.print(F("density "));
    Serial.print(density);
    Serial.println(F(", sending ACK"));
    sendAck(density);
  }
  else
  {
    Serial.println(F("no result (ATmega off-peak, or no answer)"));
  }

  driverOn();                           // idle HIGH again
  digitalWrite(LED_BUILTIN, LOW);
}
