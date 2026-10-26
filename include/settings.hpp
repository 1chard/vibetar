#pragma once
#include "runtime_config.hpp"

namespace guitar::settings {
void load();
const RuntimeConfig& current();
// Feature callbacks only queue a write. Flash operations run from the main loop.
void submit(const uint8_t* payload, uint16_t length);
ConfigPayload report();
ExtrasPayload extras_report();
TiltPayload tilt_report();
HostPayload host_report();
OutputsPayload outputs_report();
BatteryPayload battery_report();
BluetoothPayload bluetooth_report();
void submit_bluetooth(const uint8_t* payload,uint16_t length);
void submit_battery(const uint8_t* payload,uint16_t length);
void submit_outputs(const uint8_t* payload,uint16_t length);
void submit_host(const uint8_t* payload,uint16_t length);
void submit_tilt(const uint8_t* payload, uint16_t length);
void submit_extras(const uint8_t* payload, uint16_t length);
bool process(); // true only when a saved configuration must be applied to GPIOs
}
