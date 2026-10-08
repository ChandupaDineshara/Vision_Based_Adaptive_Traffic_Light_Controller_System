#include "i2c_link.h"
#include <Arduino.h>
#include <Wire.h>
#include "config.h"

/* Shared between the I2C callbacks (which run in an interrupt-like context) and the main code. */
static volatile uint8_t density = 0;
static volatile bool getDataSeen = false;
static volatile bool resultRead = false;

/* Called when the ATmega WRITES to us: look for the GET_DATA command byte. */
static void onReceive(int numberOfBytes)
{
    (void)numberOfBytes;
    while (Wire.available()) {
        const int command = Wire.read();
        if (command == GET_DATA_COMMAND) getDataSeen = true;
    }
}

/* Called when the ATmega READS from us: answer with the density byte. */
static void onRequest(void)
{
    Wire.write((uint8_t)density);
    resultRead = true;
}

bool i2c_link_begin(uint8_t value)
{
    density = value;
    getDataSeen = false;
    resultRead = false;

    Wire.onReceive(onReceive);
    Wire.onRequest(onRequest);
    return Wire.begin((uint8_t)I2C_SLAVE_ADDRESS, I2C_SDA, I2C_SCL, (uint32_t)I2C_FREQUENCY);
}

bool i2c_link_get_data_seen(void) { return getDataSeen; }
bool i2c_link_result_read(void)   { return resultRead; }
