#include "battery.hpp"
#include "hardware/adc.h"
#include "hardware/gpio.h"
#include "hardware/structs/padsbank0.h"
#include "radio.hpp"
#include "pico/stdlib.h"
#include "pico/cyw43_arch.h"
namespace guitar::battery
{
    namespace
    {
        constexpr uint32_t sample_interval_ms = 5000;
        BatteryConfig active{};
        BatteryState state{};
        bool ready = false;
        uint32_t last_sample = 0;
    }
    void configure(const BatteryConfig &config, uint32_t now)
    {
        active = config;
        state = {};
        last_sample = now - sample_interval_ms;
        ready = true;
        if (!radio::available())
        {
            gpio_init(24);
            gpio_set_dir(24, false);
        } // Pico VBUS sense
    }
    void task(uint32_t now)
    {
        if (!active.enabled || uint32_t(now - last_sample) < sample_interval_ms)
            return;
        last_sample = now;
        if (!ready)
        {
            state.sample(active, 0, false, false, false);
            return;
        }
        // Same core owns the ADC. Hold the CYW43 lock while borrowing its clock pin.
        const bool wireless = radio::available();
        bool usb = false;
        if (wireless)
        {
            cyw43_thread_enter();
            if (cyw43_gpio_get(&cyw43_state, CYW43_WL_GPIO_VBUS_PIN, &usb) != 0)
            {
                cyw43_thread_exit();
                state.sample(active, 0, false, false, false);
                return;
            }
        }
        else
            usb = gpio_get(24);
        const auto function = gpio_get_function(PICO_VSYS_PIN);
        const auto pad = padsbank0_hw->io[PICO_VSYS_PIN];
        const auto channel = adc_get_selected_input();
        adc_gpio_init(PICO_VSYS_PIN);
        adc_select_input(3);
        sleep_us(20);
        for (unsigned i = 0; i < 4; ++i)
            (void)adc_read(); // discard settling conversions
        uint32_t sum = 0;
        for (unsigned i = 0; i < 8; ++i)
            sum += adc_read();
        adc_select_input(channel);
        gpio_set_function(PICO_VSYS_PIN, function);
        padsbank0_hw->io[PICO_VSYS_PIN] = pad;
        if (wireless)
            cyw43_thread_exit();
        state.sample(active, (sum + 4) / 8, true, usb, true);
    }
    bool low() { return state.low; }
    std::array<uint8_t, 20> live_report()
    {
        std::array<uint8_t, 20> p{'X', 'C', 'B', 'T', 1, static_cast<uint8_t>((active.enabled ? 1 : 0) | (state.valid ? 2 : 0) | (state.usb ? 4 : 0) | (state.source_known ? 8 : 0) | (state.low ? 16 : 0) | (state.error ? 32 : 0))};
        const uint16_t v[]{state.vsys_mv, state.battery_mv, state.raw};
        for (size_t i = 0; i < 3; ++i)
        {
            p[8 + 2 * i] = v[i];
            p[9 + 2 * i] = v[i] >> 8;
        }
        return p;
    }
}
