/*
 * i2c_link.h - the ESP32 as an I2C slave (address 0x08) for the ATmega.
 *
 * Protocol (see ATMEGA_firmware/docs/03_esp32_interface.md):
 *   ATmega writes 1 byte:  GET_DATA (1)
 *   ATmega reads  1 byte:  the traffic density 0 LOW, 1 MEDIUM, 2 HIGH, 3 FULL
 */
#ifndef I2C_LINK_H
#define I2C_LINK_H

#include <stdint.h>

/* Start the I2C slave and set the density byte it will return. */
bool i2c_link_begin(uint8_t density);

/* True after the ATmega has sent GET_DATA. */
bool i2c_link_get_data_seen(void);

/* True after the ATmega has read the density byte. */
bool i2c_link_result_read(void);

#endif /* I2C_LINK_H */
