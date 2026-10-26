#pragma once
namespace guitar::radio {
// Probe once before USB enumeration. Availability remains fixed until reboot.
void detect();
bool available();
bool ensure_ready();
}
