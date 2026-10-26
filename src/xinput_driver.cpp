#include "xinput_driver.hpp"
#include "xsm3_auth.hpp"
#include "xsm3_protocol.hpp"
#include "usb_device.hpp"
#include "xinput_profile.hpp"
#include "tusb.h"
#include "device/usbd_pvt.h"
#include <cstring>

namespace {
constexpr uint8_t ep_in = 0x81, ep_out = 0x02;
bool opened = false;
alignas(4) uint8_t input_buffer[32]{};
alignas(4) uint8_t output_buffer[32]{};

void reset(uint8_t) {
    guitar::usb_host_bus_reset();
    guitar::auth::reset();
    opened = false;
    std::memset(input_buffer, 0, sizeof(input_buffer));
    std::memset(output_buffer, 0, sizeof(output_buffer));
}
void init() { reset(0); }
bool deinit() { reset(0); return true; }

uint16_t open(uint8_t rhport, const tusb_desc_interface_t* interface, uint16_t max_length) {
    if(guitar::is_xinput_mode(guitar::usb_mode()) && interface->bInterfaceNumber==guitar::auth::interface_id &&
       interface->bInterfaceClass==0xff && interface->bInterfaceSubClass==0xfd && interface->bInterfaceProtocol==0x13 &&
       !interface->bNumEndpoints && max_length>=15){
        const auto* bytes=reinterpret_cast<const uint8_t*>(interface);
        return bytes[9]==6 && bytes[10]==0x41?15:0;
    }
    if (!guitar::is_xinput_mode(guitar::usb_mode()) || interface->bInterfaceClass != 0xff ||
        interface->bInterfaceSubClass != 0x5d || interface->bInterfaceProtocol != 1 ||
        interface->bNumEndpoints != 2 || max_length < 39) return 0;
    const auto* bytes = reinterpret_cast<const uint8_t*>(interface);
    if (bytes[9] != 16 || bytes[10] != 0x21) return 0;
    const auto* in = reinterpret_cast<const tusb_desc_endpoint_t*>(bytes + 25);
    const auto* out = reinterpret_cast<const tusb_desc_endpoint_t*>(bytes + 32);
    if (in->bLength != 7 || out->bLength != 7 ||
        in->bDescriptorType != TUSB_DESC_ENDPOINT || out->bDescriptorType != TUSB_DESC_ENDPOINT ||
        in->bEndpointAddress != ep_in || out->bEndpointAddress != ep_out) return 0;
    if (!usbd_edpt_open(rhport, in) || !usbd_edpt_open(rhport, out)) return 0;
    // Keep consuming Windows LED/rumble packets, even though no actuators are connected.
    opened = usbd_edpt_xfer(rhport, ep_out, output_buffer, sizeof(output_buffer));
    return opened ? 39 : 0;
}
bool control(uint8_t, uint8_t, const tusb_control_request_t*) {
    // Standard enumeration is handled by TinyUSB. Vendor requests are routed
    // through tud_vendor_control_xfer_cb(), not this class callback.
    return false;
}
bool transfer(uint8_t rhport, uint8_t endpoint, xfer_result_t, uint32_t) {
    if (endpoint == ep_out && opened)
        return usbd_edpt_xfer(rhport, ep_out, output_buffer, sizeof(output_buffer));
    return endpoint == ep_in;
}
const usbd_class_driver_t driver = {
    "XInput", init, deinit, reset, open, control, transfer, nullptr
};
}

extern "C" const usbd_class_driver_t* usbd_app_driver_get_cb(uint8_t* count) {
    *count = 1;
    return &driver;
}
extern "C" bool tud_vendor_control_xfer_cb(uint8_t rhport, uint8_t stage,
                                            const tusb_control_request_t* request) {
    using namespace guitar::xinput;
    if(stage==CONTROL_STAGE_SETUP)
        guitar::usb_observe_host_request(request->bmRequestType,request->bRequest,request->wValue,request->wIndex,request->wLength);
    if (!guitar::is_xinput_mode(guitar::usb_mode())) return false;
    if(guitar::auth::command(request->bmRequestType,request->bRequest,request->wIndex,request->wLength)!=guitar::auth::Command::None)
        return guitar::auth::control(rhport,stage,request);
    const auto reply = control_reply(request->bmRequestType, request->bRequest, request->wValue, request->wIndex);
    if (reply == ControlReply::Unsupported) return false;
    if (stage != CONTROL_STAGE_SETUP) return true;
    // Static buffers remain valid for the entire asynchronous EP0 transfer.
    switch (reply) {
    case ControlReply::InputCapabilities:
        return tud_control_xfer(rhport, request, const_cast<uint8_t*>(profile_for(guitar::usb_mode()).input_capabilities.data()), input_capabilities.size());
    case ControlReply::VibrationCapabilities:
        return tud_control_xfer(rhport, request, const_cast<uint8_t*>(vibration_capabilities.data()), vibration_capabilities.size());
    case ControlReply::CompatibleId:
        return tud_control_xfer(rhport, request, const_cast<uint8_t*>(compatible_id.data()), compatible_id.size());
    default: return false;
    }
}
namespace guitar {
bool xinput_ready() {
    return is_xinput_mode(usb_mode()) && opened && tud_mounted() &&
           !tud_suspended() && !usbd_edpt_busy(0, ep_in);
}
bool xinput_send(const XInputReport& report) {
    if (!xinput_ready() || !usbd_edpt_claim(0, ep_in)) return false;
    std::memcpy(input_buffer, &report, sizeof(report));
    if (usbd_edpt_xfer(0, ep_in, input_buffer, sizeof(report))) return true;
    usbd_edpt_release(0, ep_in);
    return false;
}
}
