#include "controller.hpp"
#include "settings_record.hpp"
#include "gpio_report.hpp"
using namespace guitar;
namespace {
constexpr bool migration_test() {
    RuntimeConfig old{}; old.button_pins[0] = 0; old.whammy_min = 234; old.whammy_max = 3876;
    settings::FlashRecord record{};
    settings::write_u32(record, 0, 0x31434658); settings::write_u32(record, 4, 99);
    const auto payload = encode_config(old);
    for (size_t i = 0; i < payload.size(); ++i) record[8 + i] = payload[i];
    settings::write_u32(record, 32, settings::crc32(record));
    RuntimeConfig migrated{};
    return settings::parse_record(record, migrated) && same_config(old, migrated);
}
constexpr bool extras_test() {
    RuntimeConfig c{};
    c.extras.digital_pins = {0,1,14,15,16,17,18,19}; c.extras.pickup_pin = 27;
    if (!valid_config(c)) return false;
    RuntimeConfig decoded{};
    if (!decode_extras(encode_extras(c), decoded) || !same_config(c, decoded)) return false;
    // Base updates must preserve extras, including the legacy editor.
    c.whammy_min = 123;
    if (!decode_config(encode_config(c), decoded) || !same_config(c, decoded)) return false;
    auto record = settings::make_record(c, 1);
    if (!settings::parse_record(record, decoded) || !same_config(c, decoded)) return false;
    record[42] ^= 1; if (settings::parse_record(record, decoded)) return false;
    c.extras.digital_pins[0] = 2; if (valid_config(c)) return false;
    c.extras.digital_pins[0] = 1; if (valid_config(c)) return false;
    c.extras.digital_pins[0] = 23; if (valid_config(c)) return false;
    c.extras.digital_pins[0] = 0; c.whammy_enabled = true; c.extras.pickup_pin = 26;
    if (valid_config(c)) return false;
    return true;
}
constexpr bool reports_test() {
    State s{}; s.buttons = bit(Button::StrumUp); s.right = true;
    if (make_hid_report(s).hat != 2 || make_xinput_report(s).buttons != 9) return false;
    s.left = true; if (make_hid_report(s).hat != 1) return false;
    s.buttons |= bit(Button::StrumDown); if (make_hid_report(s).hat != 0 || make_xinput_report(s).buttons) return false;
    if (resolve_solo(1, false, 2) != 2 || resolve_solo(1, true, 2) != 3 || resolve_solo(0, true, 0)) return false;
    s = {}; s.buttons = bit(Button::Red); s.solo_frets = 1; s.pickup = 0xab;
    const auto rb = make_xinput_report(s, UsbMode::RockBand), gh = make_xinput_report(s);
    return rb.buttons == 0x3040 && rb.left_trigger == 0xab && gh.buttons == 0x3000 && !gh.left_trigger &&
           (make_hid_report(s).buttons & 0x603) == 0x603;
}
static_assert(migration_test(), "Existing flash configuration must survive firmware update");
static_assert(extras_test(), "Optional pin configuration validation and persistence");
static_assert(reports_test(), "D-pad / solo / pickup mode-specific encoding");
static_assert(pickup_position(100,100,4000,false) == 0x17);
static_assert(pickup_position(4000,100,4000,false) == 0xe0);
static_assert(pickup_position(100,100,4000,true) == 0xe0);
static_assert(pickup_position(1000,0,4095,false) == 0x4b);
static_assert(pickup_position(2000,0,4095,false) == 0x79);
static_assert(pickup_position(3000,0,4095,false) == 0xab);
constexpr std::array<uint16_t,5> nonlinear={90,340,1200,2900,3950};
static_assert(pickup_position(500,nonlinear,false)==0x4b);
static_assert(pickup_position(770,nonlinear,false)==0x4b); // tie: first position
static_assert(pickup_position(771,nonlinear,false)==0x79);
static_assert(pickup_position(0,nonlinear,false)==0x17);
static_assert(pickup_position(4095,nonlinear,true)==0x17);
static_assert(pickup_position(100,std::array<uint16_t,5>{3950,2900,1200,340,90},false)==0xe0);
constexpr bool calibrated_test() {
    RuntimeConfig c{}, decoded{};c.extras.pickup_calibrated=true;c.extras.pickup_values=nonlinear;
    auto p=encode_extras(c);
    if(p[0]!=2 || !decode_extras(p,decoded) || !same_config(c,decoded))return false;
    if(!settings::parse_record(settings::make_record(c,7),decoded) || !same_config(c,decoded))return false;
    p[22]=1;if(decode_extras(p,decoded))return false;p[22]=0;
    p[14]=p[12];p[15]=p[13];if(decode_extras(p,decoded))return false;
    if(!decode_extras(encode_extras(RuntimeConfig{}),decoded) || decoded.extras.pickup_calibrated)return false;
    c.extras.pickup_values[0]=4096;return !valid_config(c);
}
static_assert(calibrated_test(), "Nonlinear selector persistence, validation and legacy configuration");
}
