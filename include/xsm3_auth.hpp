#pragma once
#include <cstdint>
#include "tusb.h"
namespace guitar::auth {
void reset();
void task();
bool control(uint8_t rhport,uint8_t stage,const tusb_control_request_t* request);
}
