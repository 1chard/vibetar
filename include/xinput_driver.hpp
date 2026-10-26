#pragma once
#include "controller.hpp"

namespace guitar {
bool xinput_ready();
bool xinput_send(const XInputReport& report);
}
