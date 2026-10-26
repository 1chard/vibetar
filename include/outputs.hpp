#pragma once
#include "output_config.hpp"
namespace guitar::outputs {
void init(const OutputsConfig& config,UsbMode mode);
void release();
void update(UsbMode mode);
OutputsPayload live_report();
}
