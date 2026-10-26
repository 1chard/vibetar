#include "runtime_config.hpp"
#include "settings_record.hpp"

using namespace guitar;
namespace {
constexpr bool serialization_test() {
    RuntimeConfig config{};
    config.whammy_enabled = true;
    config.whammy_inverted = true;
    config.whammy_min = 237;
    config.whammy_max = 3815;
    config.whammy_deadzone = 50;
    const auto payload = encode_config(config);
    RuntimeConfig decoded{};
    return decode_config(payload, decoded) && same_config(config, decoded) &&
           payload[15] == 3 && payload[18] == 0xe7 && payload[19] == 0x0e;
}
constexpr bool validation_test() {
    RuntimeConfig config{}, decoded{};
    config.button_pins[1] = config.button_pins[0];
    if (valid_config(config) || decode_config(encode_config(config), decoded)) return false;
    config = RuntimeConfig{};
    config.mode_pin = config.button_pins[0];
    if (valid_config(config)) return false;
    config = RuntimeConfig{};
    config.button_pins[0] = 23;
    if (valid_config(config)) return false;
    config = RuntimeConfig{};
    config.whammy_pin = 29;
    if (valid_config(config)) return false;
    config = RuntimeConfig{};
    config.whammy_min = config.whammy_max;
    if (valid_config(config)) return false;
    config = RuntimeConfig{};
    config.whammy_max = 4096;
    if (valid_config(config)) return false;
    config = RuntimeConfig{};
    config.whammy_deadzone = config.whammy_max - config.whammy_min;
    if (valid_config(config)) return false;
    auto payload = encode_config(RuntimeConfig{});
    payload[0] = 2;
    if (decode_config(payload, decoded)) return false;
    payload = encode_config(RuntimeConfig{});
    payload[15] = 4;
    if (decode_config(payload, decoded)) return false;
    payload = encode_config(RuntimeConfig{});
    payload[23] = 1;
    return !decode_config(payload, decoded);
}
constexpr bool flash_record_test() {
    const RuntimeConfig config{};
    RuntimeConfig decoded{};
    auto record = settings::make_record(config, 7);
    if (!settings::parse_record(record, decoded) || !same_config(config, decoded) ||
        settings::read_u32(record, 4) != 7) return false;
    record[8] ^= 1;
    if (settings::parse_record(record, decoded)) return false;
    record = settings::make_record(config, 7);
    record[32] ^= 1;
    if (settings::parse_record(record, decoded)) return false;
    for (auto& byte : record) byte = 0xff; // erased or incomplete alternative slot
    return !settings::parse_record(record, decoded);
}
static_assert(serialization_test(), "Config wire format regression");
static_assert(validation_test(), "Invalid configuration must never reach GPIO or flash");
static_assert(flash_record_test(), "Flash corruption / incomplete record detection");
static_assert(settings::crc32(settings::FlashRecord{}) == 0x190a55ad, "Standard CRC32 check");
static_assert(settings::newer_sequence(0, 0xffffffffu), "Journal sequence rollover");
static_assert(!settings::newer_sequence(7, 7) && !settings::newer_sequence(6, 7));
}
