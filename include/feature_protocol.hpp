#pragma once
#include <cstdint>
namespace guitar {
// Transport independent; only call from the main loop. BLE uses an IRQ-safe cache.
uint16_t read_feature_report(uint8_t id,uint8_t* buffer,uint16_t requested);
void submit_feature_report(uint8_t id,const uint8_t* buffer,uint16_t length);
}
