#include "rs485.h"
#include <Arduino.h>
#include "pins.h"
#include "tlc_protocol.h"

static void rx_mode()
{
  digitalWrite(PIN_RS485_DE, LOW);
  digitalWrite(PIN_RS485_NRE, LOW);
}

static void tx_mode()
{
  digitalWrite(PIN_RS485_DE, HIGH);
  digitalWrite(PIN_RS485_NRE, HIGH);
}

void rs485_init()
{
  pinMode(PIN_RS485_DE, OUTPUT);
  pinMode(PIN_RS485_NRE, OUTPUT);
  rx_mode();
  Serial.begin(TLC_UART_BAUD);
}

bool rs485_read(uint8_t &b)
{
  if (!Serial.available()) return false;
  b = (uint8_t)Serial.read();
  return true;
}

void rs485_write(const uint8_t *data, uint8_t len)
{
  tx_mode();
  Serial.write(data, len);
  Serial.flush();            /* waits for the transmit-complete flag (TXC0) */
  rx_mode();
  rs485_flush_rx();          /* drop any glitch from the direction change */
}

void rs485_flush_rx()
{
  while (Serial.available()) Serial.read();
}
