#pragma once
#include "bluetooth_config.hpp"
#include "controller.hpp"
#include "controller_preview.hpp"
namespace guitar::bluetooth {
void configure(const BluetoothConfig& config,uint32_t now);
void task(uint32_t now,const State& state,bool usb_mounted);
std::array<uint8_t,24> live_report();
std::array<uint8_t,24> link_report();
void mode_press();
UsbMode output_mode(UsbMode local,uint32_t now);
#if VIBETAR_RECEIVER
State received_state(uint32_t now);
PreviewPayload received_preview(UsbMode mode);
bool take_mode_press(uint32_t now);
#endif
BluetoothPayload command_report();
void submit_command(const uint8_t* payload,uint16_t length);
}
