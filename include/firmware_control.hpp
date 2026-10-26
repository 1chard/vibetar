#pragma once
#include <array>
#include <cstddef>
#include <cstdint>

namespace guitar {
inline constexpr uint8_t firmware_report_id = 4;
inline constexpr size_t firmware_payload_size = 16;
inline constexpr uint16_t firmware_boot_delay_ms = 1000;
using FirmwarePayload = std::array<uint8_t, firmware_payload_size>;
enum class FirmwareStatus : uint8_t { Ready, PendingBootsel, Invalid, Busy, PendingRestart };
inline constexpr FirmwarePayload bootsel_command = {'X', 'C', 'B', 'T', 1, 1};

// Separate from GPIO settings: commands never write flash. The USB callback
// only schedules a reboot, giving EP0 and the client time to complete the ACK.
class FirmwareControl {
public:
    constexpr void submit(const uint8_t* bytes, size_t length, uint32_t now, bool busy) {
        if (pending()) return; // do not extend deadline
        if (length != firmware_payload_size) { status_ = FirmwareStatus::Invalid; return; }
        for (size_t i = 0; i < firmware_payload_size; ++i) {
            if ((i == 5 && bytes[i] != 1 && bytes[i] != 2) ||
                (i != 5 && bytes[i] != bootsel_command[i])) { status_ = FirmwareStatus::Invalid; return; }
        }
        if (busy) { status_ = FirmwareStatus::Busy; return; }
        requested_at_ = now;
        status_ = bytes[5] == 1 ? FirmwareStatus::PendingBootsel : FirmwareStatus::PendingRestart;
    }
    constexpr bool pending() const { return status_ == FirmwareStatus::PendingBootsel || status_ == FirmwareStatus::PendingRestart; }
    constexpr bool restart() const { return status_ == FirmwareStatus::PendingRestart; }
    constexpr bool due(uint32_t now) const {
        return pending() && uint32_t(now - requested_at_) >= firmware_boot_delay_ms;
    }
    constexpr FirmwarePayload report() const {
        FirmwarePayload data{'X', 'C', 'B', 'T', 1, 0, static_cast<uint8_t>(status_), 3};
        data[8] = static_cast<uint8_t>(firmware_boot_delay_ms);
        data[9] = static_cast<uint8_t>(firmware_boot_delay_ms >> 8);
        return data;
    }
private:
    FirmwareStatus status_ = FirmwareStatus::Ready;
    uint32_t requested_at_ = 0;
};
}
