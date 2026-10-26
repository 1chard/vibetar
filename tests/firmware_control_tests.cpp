#include "firmware_control.hpp"
using namespace guitar;
namespace {
constexpr bool command_test() {
    FirmwareControl control;
    if (control.report()[6] != 0 || control.report()[7] != 3 || control.due(999999)) return false;
    control.submit(bootsel_command.data(), bootsel_command.size(), 123, false);
    if (control.report()[6] != 1 || control.due(1122) || !control.due(1123)) return false;
    control.submit(bootsel_command.data(), bootsel_command.size(), 1100, false);
    return control.due(1123); // duplicates must not postpone reboot
}
constexpr bool reject_test() {
    FirmwareControl control;
    control.submit(bootsel_command.data(), 15, 0, false);
    if (control.report()[6] != 2 || control.due(1000)) return false;
    for (size_t i = 0; i < bootsel_command.size(); ++i) {
        auto invalid = bootsel_command;
        invalid[i] ^= 0xff;
        control.submit(invalid.data(), invalid.size(), 0, false);
        if (control.report()[6] != 2 || control.pending()) return false;
    }
    control.submit(bootsel_command.data(), 16, 0, true);
    if (control.report()[6] != 3 || control.due(1000)) return false;
    control.submit(bootsel_command.data(), 16, 0xfffffff0u, false);
    return !control.due(983) && control.due(984);
}
static_assert(command_test(), "HID reboot scheduling/duplicate regression");
static_assert(reject_test(), "Malformed/busy requests must not reboot; timer handles wrap");
constexpr bool restart_test() {
    FirmwareControl control;
    auto command = bootsel_command;
    command[5] = 2;
    control.submit(command.data(), command.size(), 100, false);
    return control.restart() && control.pending() && control.report()[6] == 4 &&
           !control.due(1099) && control.due(1100);
}
static_assert(restart_test(), "Normal restart is distinct from BOOTSEL");
static_assert(firmware_report_id == 4 && firmware_payload_size == 16);
static_assert(FirmwareControl{}.report()[8] == 0xe8 && FirmwareControl{}.report()[9] == 3);
}
