#pragma once
#include "controller.hpp"
#include "gpio_report.hpp"

namespace guitar {
// Descriptor callbacks and the driver always consult this same active mode.
UsbMode usb_mode();
uint8_t usb_mode_mask();
bool usb_bootsel_due(uint32_t now);
bool usb_restart_due(uint32_t now);
void usb_observe_host_request(uint8_t type,uint8_t request,uint16_t value,uint16_t index,uint16_t length);
void usb_observe_os_string();
void usb_host_bus_reset();
class UsbDevice {
public:
    bool init();
    void configure_boot(bool recovery,uint32_t now);
    void request_toggle(uint32_t now);
    bool transitioning() const { return detached_; }
    void task(uint32_t now, const State& state, const GpioPayload& gpio, const GpioScanPayload& scan, const std::array<uint8_t,20>& tilt);
private:
    void request_mode(UsbMode mode,uint32_t now);
    UsbMode pending_mode_=UsbMode::Hid;
    bool detached_ = false;
    uint32_t detached_at_ = 0, last_report_ = 0;
};
}
