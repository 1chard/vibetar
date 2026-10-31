#include "input.hpp"
#include "usb_device.hpp"
#include "settings.hpp"
#include "outputs.hpp"
#include "battery.hpp"
#include "bluetooth.hpp"
#include "radio.hpp"
#include "tusb.h"
#include "pico/stdlib.h"
#include "pico/bootrom.h"
#include "hardware/watchdog.h"

int main() {
    guitar::Input input;
    guitar::UsbDevice usb;
    guitar::settings::load();
    guitar::radio::detect(); // Select hardware capabilities before USB descriptors.
    input.init(guitar::settings::current());
    // Sample saved GPIOs before USB enumeration. Start+Back (without MODE)
    // forces HID for this power cycle, including when XInput-only is saved.
    for(unsigned i=0;i<30;++i) {
        input.sample(to_ms_since_boot(get_absolute_time()));sleep_ms(1);
    }
    usb.configure_boot(input.recovery_chord(),to_ms_since_boot(get_absolute_time()));
    input.take_mode_press(); // Do not advance a mode from a button held at boot.
    guitar::battery::configure(guitar::settings::current().battery,to_ms_since_boot(get_absolute_time()));
    guitar::outputs::init(guitar::settings::current().outputs,guitar::usb_mode());
    if (!usb.init()) panic("USB init failed");
    guitar::bluetooth::configure(guitar::settings::current().bluetooth,to_ms_since_boot(get_absolute_time()));
    guitar::BootGesture receiver_chord{guitar::config::hid_hold_ms};
    uint32_t last_sample = to_ms_since_boot(get_absolute_time());
    while (true) {
        if (guitar::settings::process()) {
            guitar::outputs::release();
            input.reconfigure(guitar::settings::current());
            guitar::battery::configure(guitar::settings::current().battery,to_ms_since_boot(get_absolute_time()));
            guitar::outputs::init(guitar::settings::current().outputs,guitar::usb_mode());
            guitar::bluetooth::configure(guitar::settings::current().bluetooth,to_ms_since_boot(get_absolute_time()));
        }
        const uint32_t now = to_ms_since_boot(get_absolute_time());
        guitar::battery::task(now);
        if (uint32_t(now - last_sample) >= guitar::config::sample_interval_ms) {
            last_sample = now;
            input.sample(now);
            // Runtime HID recovery preserves settings and overrides mode restrictions.
            // Called outside USB callbacks and after synchronous settings writes.
            if (input.take_force_hid_request()) usb.request_force_hid(now);
            if (input.take_mode_press()) {
                usb.request_toggle(now);
                guitar::bluetooth::mode_press();
            }
        }
#if VIBETAR_RECEIVER
        if(!usb.transitioning() && guitar::bluetooth::take_mode_press(now))usb.request_toggle(now);
        const auto state=guitar::bluetooth::received_state(now);
        if(receiver_chord.update(false,state.pressed(guitar::Button::Start),state.pressed(guitar::Button::Back),now)==guitar::ModeAction::ForceHid)
            usb.request_force_hid(now);
#else
        const auto state=input.state();
#endif
        usb.task(now, state, input.gpio_report(), input.gpio_scan(), input.tilt_report());
        guitar::bluetooth::task(now,input.state(),tud_mounted());
        guitar::outputs::update(guitar::bluetooth::output_mode(guitar::usb_mode(),now));
        if (guitar::usb_bootsel_due(to_ms_since_boot(get_absolute_time()))) reset_usb_boot(0, 0);
        if (guitar::usb_restart_due(to_ms_since_boot(get_absolute_time()))) {
            watchdog_reboot(0, 0, 0);
            while (true) tight_loop_contents();
        }
        tight_loop_contents();
    }
}
