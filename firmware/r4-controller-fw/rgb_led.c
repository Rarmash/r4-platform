#include "rgb_led.h"

#include "hardware/clocks.h"
#include "hardware/pio.h"
#include "pico/stdlib.h"
#include "ws2812.pio.h"

#define WS2812_FREQUENCY_HZ 800000.0f
#define WS2812_CYCLES_PER_BIT 10.0f

static PIO led_pio;
static uint led_state_machine;

void rgb_led_init(uint pin) {
    led_pio = pio0;
    led_state_machine = pio_claim_unused_sm(led_pio, true);

    const uint program_offset =
        pio_add_program(led_pio, &r4_ws2812_program);

    pio_gpio_init(led_pio, pin);

    pio_sm_set_consecutive_pindirs(
        led_pio,
        led_state_machine,
        pin,
        1,
        true
    );

    pio_sm_config config =
        r4_ws2812_program_get_default_config(program_offset);

    sm_config_set_sideset_pins(&config, pin);

    sm_config_set_out_shift(
        &config,
        false,
        true,
        24
    );

    sm_config_set_fifo_join(
        &config,
        PIO_FIFO_JOIN_TX
    );

    const float clock_divider =
        (float)clock_get_hz(clk_sys) /
        (WS2812_FREQUENCY_HZ * WS2812_CYCLES_PER_BIT);

    sm_config_set_clkdiv(
        &config,
        clock_divider
    );

    pio_sm_init(
        led_pio,
        led_state_machine,
        program_offset,
        &config
    );

    pio_sm_set_enabled(
        led_pio,
        led_state_machine,
        true
    );
}

void rgb_led_set(
    uint8_t red,
    uint8_t green,
    uint8_t blue
) {
    // WS2812 ожидает каналы в порядке GRB.
    const uint32_t color =
        ((uint32_t)green << 16) |
        ((uint32_t)red << 8) |
        (uint32_t)blue;

    pio_sm_put_blocking(
        led_pio,
        led_state_machine,
        color << 8
    );

    // Пауза для фиксации нового цвета светодиодом.
    sleep_us(80);
}