#pragma once
#include "battery_config.hpp"
namespace guitar::battery {
void configure(const BatteryConfig& config,uint32_t now);
void task(uint32_t now);
bool low();
std::array<uint8_t,20> live_report();
}
