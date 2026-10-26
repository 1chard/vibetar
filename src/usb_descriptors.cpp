#include "usb_device.hpp"
#include "xsm3_protocol.hpp"
#include "radio.hpp"
#include "runtime_config.hpp"
#include "firmware_control.hpp"
#include "gpio_report.hpp"
#include "xinput_profile.hpp"
#include "tusb.h"
#include "pico/unique_id.h"
#include <cstring>

namespace {
static_assert(CFG_TUD_HID_EP_BUFSIZE>=33,"Discovery feature needs 32 bytes plus USB report ID");
// Prototype IDs: HID uses a development VID; XInput uses Xplorer/Harmonix
// identity for binding to the Windows driver. See README before distribution.
constexpr tusb_desc_device_t hid_device = {
    sizeof(tusb_desc_device_t), TUSB_DESC_DEVICE, 0x0200, 0, 0, 0,
    CFG_TUD_ENDPOINT0_SIZE, 0xcafe, 0x4005, 0x020d, 1, 2, 3, 1
};
constexpr tusb_desc_device_t make_xinput_device(const guitar::xinput::Profile& profile) {
    return {
    sizeof(tusb_desc_device_t), TUSB_DESC_DEVICE, 0x0200, 0xff, 0xff, 0xff,
    CFG_TUD_ENDPOINT0_SIZE, profile.vendor_id, profile.product_id,
    profile.revision, 1, 2, 3, 1
    };
}
constexpr auto xinput_device = make_xinput_device(guitar::xinput::guitar_hero);
constexpr auto rock_band_device = make_xinput_device(guitar::xinput::rock_band);
constexpr uint8_t hid_report_descriptor_base[] = {
    TUD_HID_REPORT_DESC_GAMEPAD(HID_REPORT_ID(guitar::gamepad_report_id)),
    // Vendor collection exposed as a separate HID top-level collection on Windows.
    0x06, 0x00, 0xff, // Usage Page 0xFF00 (vendor)
    0x09, 0x01,       // Usage 1
    0xa1, 0x01,       // Application collection
    0x85, guitar::config_report_id,
    0x09, 0x02,       // Feature usage
    0x15, 0x00, 0x26, 0xff, 0x00, // Logical min 0, max 255
    0x75, 0x08, 0x95, static_cast<uint8_t>(guitar::config_payload_size),
    0xb1, 0x02,       // Feature: Data, Variable, Absolute
    0x85, guitar::defaults_report_id,
    0x09, 0x03, 0xb1, 0x02, // Read-only compiled defaults, same 24-byte layout
    0x85, guitar::firmware_report_id,
    0x09, 0x04, 0x95, static_cast<uint8_t>(guitar::firmware_payload_size),
    0xb1, 0x02, // Firmware control/status, 16 bytes, separate from settings
    0x85, guitar::gpio_report_id,
    0x09, 0x05, 0x95, static_cast<uint8_t>(guitar::GpioPayload{}.size()),
    0xb1, 0x02, // Read-only live digital GPIO and ADC snapshot
    0x85, guitar::gpio_scan_report_id,
    0x09, 0x06, 0x95, static_cast<uint8_t>(guitar::GpioScanPayload{}.size()),
    0xb1, 0x02, // Read-only scan of all exposed digital pins and three ADCs
    0x85, guitar::extras_report_id,
    0x09, 0x07, 0x95, static_cast<uint8_t>(guitar::ExtrasPayload{}.size()), 0xb1, 0x02,
    0x85, guitar::extras_defaults_report_id,
    0x09, 0x08, 0xb1, 0x02,
    0x85, guitar::tilt_report_id, 0x09, 0x09, 0x95, 16, 0xb1, 0x02,
    0x85, guitar::tilt_defaults_report_id, 0x09, 0x0a, 0xb1, 0x02,
    0x85, guitar::tilt_live_report_id, 0x09, 0x0b, 0x95, 20, 0xb1, 0x02,
    0x85, guitar::host_config_report_id, 0x09, 0x0c, 0x95, 8, 0xb1, 0x02,
    0x85, guitar::host_defaults_report_id, 0x09, 0x0d, 0xb1, 0x02,
    0x85, guitar::host_live_report_id, 0x09, 0x0e, 0x95, 16, 0xb1, 0x02,
    0x85, guitar::outputs_report_id, 0x09, 0x0f, 0x95, 16, 0xb1, 0x02,
    0x85, guitar::outputs_defaults_report_id, 0x09, 0x10, 0xb1, 0x02,
    0x85, guitar::outputs_live_report_id, 0x09, 0x11, 0xb1, 0x02,
    0x85, guitar::battery_report_id, 0x09, 0x12, 0xb1, 0x02,
    0x85, guitar::battery_defaults_report_id, 0x09, 0x13, 0xb1, 0x02,
    0x85, guitar::battery_live_report_id, 0x09, 0x14, 0x95, 20, 0xb1, 0x02,
    0x85, 33, 0x09, 33, 0x95, 24, 0xb1, 0x02,
    0x85, 28, 0x09, 28, 0x95, 8, 0xb1, 0x02,
    0xc0
};
constexpr uint8_t bluetooth_features[] = {
    0x85,32,0x09,32,0x95,24,0xb1,0x02,
#if VIBETAR_RECEIVER
    0x85,29,0x09,29,0x95,32,0xb1,0x02,
    0x85,30,0x09,30,0x95,16,0xb1,0x02,
    0x85,31,0x09,31,0x95,16,0xb1,0x02,
#endif
    0x85, guitar::bluetooth_report_id, 0x09, 0x15, 0x95, 8, 0xb1, 0x02,
    0x85, guitar::bluetooth_defaults_report_id, 0x09, 0x16, 0xb1, 0x02,
    0x85, guitar::bluetooth_live_report_id, 0x09, 0x17, 0x95, 24, 0xb1, 0x02,
    0x85, guitar::bluetooth_command_report_id, 0x09, 0x18, 0x95, 8, 0xb1, 0x02,
};
constexpr auto make_wireless_descriptor(){
    std::array<uint8_t,sizeof(hid_report_descriptor_base)+sizeof(bluetooth_features)> d{};
    for(size_t i=0;i<sizeof(hid_report_descriptor_base)-1;++i)d[i]=hid_report_descriptor_base[i];
    for(size_t i=0;i<sizeof(bluetooth_features);++i)d[sizeof(hid_report_descriptor_base)-1+i]=bluetooth_features[i];
    d.back()=0xc0;return d;
}
constexpr auto hid_report_descriptor_wireless=make_wireless_descriptor();
constexpr auto make_hid_configuration(uint16_t length){
    return std::array<uint8_t,TUD_CONFIG_DESC_LEN+TUD_HID_DESC_LEN>{
    TUD_CONFIG_DESCRIPTOR(1, 1, 0, TUD_CONFIG_DESC_LEN + TUD_HID_DESC_LEN, 0, 100),
    TUD_HID_DESCRIPTOR(0, 0, HID_ITF_PROTOCOL_NONE, length, 0x81, 32, 4)
    };
}
constexpr auto hid_configuration_base=make_hid_configuration(sizeof(hid_report_descriptor_base));
constexpr auto hid_configuration_wireless=make_hid_configuration(hid_report_descriptor_wireless.size());
// Both wired XUSB interfaces use the same endpoints, with different subtypes.
constexpr std::array<uint8_t, 63> make_xinput_configuration(uint8_t subtype) {
    return {
    9, TUSB_DESC_CONFIGURATION, 63, 0, 2, 1, 0, 0x80, 50,
    9, TUSB_DESC_INTERFACE, 0, 0, 2, 0xff, 0x5d, 0x01, 0,
    // XUSB descriptor: subtype 7 (GH) / 6 (RB), IN 0x81 / 20 bytes, OUT 0x02.
    16, 0x21, 0x10, 0x01, subtype, 0x24, 0x81, 0x14,
    0x03, 0x00, 0x03, 0x13, 0x02, 0x00, 0x03, 0x00,
    7, TUSB_DESC_ENDPOINT, 0x81, TUSB_XFER_INTERRUPT, 32, 0, 4,
    7, TUSB_DESC_ENDPOINT, 0x02, TUSB_XFER_INTERRUPT, 32, 0, 8,
    9, TUSB_DESC_INTERFACE, guitar::auth::interface_id, 0, 0, 0xff, 0xfd, 0x13, 4,
    6, 0x41, 0, 1, 1, 3
    };
}
constexpr auto xinput_configuration = make_xinput_configuration(guitar::xinput::guitar_hero.subtype);
constexpr auto rock_band_configuration = make_xinput_configuration(guitar::xinput::rock_band.subtype);
static_assert(sizeof(xinput_configuration) == 63);
static_assert(xinput_configuration[22] == 0x07, "Guitar subtype belongs in the XUSB descriptor");
static_assert(rock_band_configuration[22] == 0x06 && rock_band_configuration[24] == 0x81);
static_assert(rock_band_device.idVendor == 0x1bad && rock_band_device.idProduct == 0x0002);
static_assert(sizeof(hid_configuration_base) == TUD_CONFIG_DESC_LEN + TUD_HID_DESC_LEN);
}

extern "C" const uint8_t* tud_descriptor_device_cb() {
    const auto mode = guitar::usb_mode();
    const auto& descriptor = mode == guitar::UsbMode::Hid ? hid_device :
                             (mode == guitar::UsbMode::RockBand ? rock_band_device : xinput_device);
    return reinterpret_cast<const uint8_t*>(&descriptor);
}
extern "C" const uint8_t* tud_descriptor_configuration_cb(uint8_t index) {
    if (index != 0) return nullptr;
    if (guitar::usb_mode() == guitar::UsbMode::Hid) return guitar::radio::available()?hid_configuration_wireless.data():hid_configuration_base.data();
    return guitar::usb_mode() == guitar::UsbMode::RockBand ? rock_band_configuration.data() : xinput_configuration.data();
}
extern "C" const uint8_t* tud_hid_descriptor_report_cb(uint8_t instance) {
    if(instance!=0 || guitar::usb_mode()!=guitar::UsbMode::Hid)return nullptr;
    return guitar::radio::available()?hid_report_descriptor_wireless.data():hid_report_descriptor_base;
}
extern "C" const uint16_t* tud_descriptor_string_cb(uint8_t index, uint16_t) {
    static uint16_t descriptor[127];
    if (index == 0) {
        descriptor[0] = (TUSB_DESC_STRING << 8) | 4;
        descriptor[1] = 0x0409;
        return descriptor;
    }
    const bool hid = guitar::usb_mode() == guitar::UsbMode::Hid;
    const bool rock_band = guitar::usb_mode() == guitar::UsbMode::RockBand;
    if (index == 0xee && !hid) {
        guitar::usb_observe_os_string();
        // OS string advertises vendor request 0x20, followed by a padding byte.
        static constexpr uint16_t os_string[] = {0x0312, 'M', 'S', 'F', 'T', '1', '0', '0', guitar::xinput::os_vendor_code};
        return os_string;
    }
    char serial[PICO_UNIQUE_BOARD_ID_SIZE_BYTES * 2 + 3]{};
    const char* value = nullptr;
    switch (index) {
    case 1: value = "Vibetar DIY"; break;
    case 2:
#if VIBETAR_RECEIVER
        value = hid ? "Vibetar Receiver HID" : (rock_band ? "Vibetar Receiver Rock Band" : "Vibetar Receiver Guitar Hero");
#else
        value = hid ? "Vibetar Guitar HID" : (rock_band ? "Vibetar Rock Band XInput" : "Vibetar Guitar Hero XInput");
#endif
        break;
    case 3: {
        pico_get_unique_board_id_string(serial, sizeof(serial));
        const size_t length = std::strlen(serial);
        serial[length] = '-';
        serial[length + 1] = hid ? 'H' : (rock_band ? 'R' : 'X');
        serial[length + 2] = '\0';
        value = serial;
        break;
    }
    case 4: if(hid)return nullptr;value=guitar::auth::security_string;break;
    default: return nullptr;
    }
    const size_t length = std::strlen(value);
    if (length > 126) return nullptr;
    descriptor[0] = static_cast<uint16_t>((TUSB_DESC_STRING << 8) | (2 + length * 2));
    for (size_t i = 0; i < length; ++i) descriptor[i + 1] = static_cast<uint8_t>(value[i]);
    return descriptor;
}
