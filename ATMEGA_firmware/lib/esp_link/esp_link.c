/*
 * esp_link.c - ATmega side of the link to the ESP32-CAM node
 */

#include "hal_f_cpu.h"
#include <avr/io.h>
#include <util/delay.h>
#include "esp_link.h"
#include "hal_sleep.h"

#define ESP_WAKE_PULSE_MS  100

void esp_wake(void)
{
    PORTB &= ~(1 << PORTB1);        /* latch LOW first ...            */
    DDRB  |=  (1 << DDB1);          /* ... then pull the line LOW     */
    hal_delay_ms(ESP_WAKE_PULSE_MS);
    DDRB  &= ~(1 << DDB1);          /* release                        */

    /* our own pulse triggered the pin-change interrupt: discard it */
    esp_wait_line_released(50);
    hal_sleep_clear_events(HAL_EVT_ESP);
}

uint8_t esp_wait_line_released(uint16_t timeout_ms)
{
    while (!(PINB & (1 << PINB1))) {
        if (timeout_ms-- == 0) return 0;
        _delay_ms(1);
    }
    return 1;
}

twi_err_t esp_get_density(uint8_t *density)
{
    /* command byte only: SLA+W, cmd, STOP */
    twi_err_t err = twi_write_regs(ESP_I2C_ADDR, ESP_CMD_GET_DATA, 0, 0);
    if (err != TWI_OK) return err;

    _delay_ms(5);                   /* let the ESP32 slave callback run */

    return twi_read_bytes(ESP_I2C_ADDR, density, 1);
}
