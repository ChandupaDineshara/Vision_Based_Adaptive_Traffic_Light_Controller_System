/*
 * esp_link.c - ATmega side of the link to the ESP32-CAM. Based on the team-mate's prototype.
 */
#include <avr/io.h>
#include <util/delay.h>
#include "esp_link.h"
#include "hal_sleep.h"
#include "config.h"

void esp_wake(void)
{
    /* Open-drain pulse on PB1. Pulling LOW takes two register writes, and the ORDER matters:
     *   1. PORT bit = 0 first, so that when the pin becomes an output it drives LOW (not HIGH)
     *   2. DDR  bit = 1, which turns the pin into an output = the line is pulled LOW
     * Letting go = DDR bit 0 (pin becomes an input, the pull-ups bring the line HIGH). */
    PORTB &= ~(1 << PORTB1);
    DDRB  |=  (1 << DDB1);
    hal_delay_ms(WAKE_PULSE_MS);
    DDRB  &= ~(1 << DDB1);

    /* Our own pulse also triggered our pin-change interrupt: wait for the line to be HIGH
     * again and throw that event away, so it is not mistaken for an answer from the ESP32. */
    esp_wait_line_released(50);
    hal_sleep_clear_events(HAL_EVT_ESP);
}

uint8_t esp_wait_line_released(uint16_t timeout_ms)
{
    while (!(PINB & (1 << PINB1))) {           /* PIN register = the real voltage; 0 = LOW */
        if (timeout_ms-- == 0) return 0;
        _delay_ms(1);
    }
    return 1;
}

twi_err_t esp_get_density(uint8_t *density)
{
    /* Step 1: tell the ESP32 we want its result. This is "address+W, command byte, STOP". */
    twi_err_t err = twi_write_regs(ESP_I2C_ADDR, ESP_CMD_GET_DATA, 0, 0);
    if (err != TWI_OK) return err;

    _delay_ms(5);                              /* give the ESP32's I2C callback time to run */

    /* Step 2: read one byte back: the density level. */
    return twi_read_bytes(ESP_I2C_ADDR, density, 1);
}
