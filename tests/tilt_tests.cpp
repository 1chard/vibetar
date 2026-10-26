#include "settings_record.hpp"
#include "controller.hpp"
#include "gpio_report.hpp"
#include "adxl_sample.hpp"
#include "receiver_protocol.hpp"
using namespace guitar;
namespace {
constexpr bool checks() {
    RuntimeConfig c{}, restored{};
    c.tilt.source = TiltSource::Adc; c.tilt.threshold = 3000;
    if (!valid_config(c) || !settings::parse_record(settings::make_record(c,9),restored) || !same_config(c,restored)) return false;
    c.whammy_enabled = true; c.whammy_pin = 28; if (valid_config(c)) return false;
    c.whammy_enabled = false; c.extras.pickup_pin = 28; if (valid_config(c)) return false;
    c.extras.pickup_pin = 255; c.tilt.source = TiltSource::Adxl345; c.tilt.threshold = -500; c.tilt.below = true;
    if (!valid_config(c)) return false;
    c.tilt.sda = 2; c.tilt.scl = 3; if (valid_config(c)) return false;
    c.tilt.sda = 20; c.tilt.scl = 21; c.extras.digital_pins[0]=20; if (valid_config(c)) return false;
    c.extras.digital_pins[0]=255;
    if (!settings::parse_record(settings::make_record(c,10),restored) || !same_config(c,restored)) return false;
    auto bytes = encode_tilt(c.tilt); bytes[13]=1; if (decode_tilt(bytes,restored.tilt)) return false;
    auto record = settings::make_record(c,10); record[66]^=1; if (settings::parse_record(record,restored)) return false;
    // Earlier XFC2 records retain D-pad/solo/ADC effects, with default button tilt.
    c.tilt = {}; c.extras.digital_pins[0]=0;
    record = settings::make_record(c,10); settings::write_u32(record,0,0x32434658);
    settings::write_u32(record,56,settings::crc32(record,56));
    return settings::parse_record(record,restored) && same_config(c,restored);
}
constexpr TiltConfig high{TiltSource::Adc,28,20,21,0x53,0,false,2000,100};
constexpr bool optional_button_checks() {
    RuntimeConfig c{}, restored{};
    c.button_pins[9] = unused_pin;
    for (auto source : {TiltSource::Button, TiltSource::Adc, TiltSource::Adxl345}) {
        c.tilt.source = source;
        if (!valid_config(c) || !decode_config(encode_config(c), restored)) return false;
        if (!settings::parse_record(settings::make_record(c,12),restored) || !same_config(c,restored)) return false;
        const auto gpio = encode_gpio_scan(c,{},false,digital_scan_mask,{0,0,0});
        const auto assigned = uint32_t(gpio[12]) | uint32_t(gpio[13])<<8 | uint32_t(gpio[14])<<16 | uint32_t(gpio[15])<<24;
        if (assigned & (1u<<11)) return false;
    }
    c.tilt.source = TiltSource::Adxl345; c.tilt.sda=10; c.tilt.scl=11;
    if (valid_config(c)) return false; // Back still owns GP10.
    c.tilt.sda=20; c.tilt.scl=21; c.extras.digital_pins[0]=11;
    if (!valid_config(c)) return false; // Old tilt GPIO is reusable.
    c.button_pins[0]=unused_pin;
    return !valid_config(c);
}
static_assert(optional_button_checks(), "Optional tilt GPIO, report masks, persistence and other required buttons");
constexpr TiltConfig low{TiltSource::Adxl345,28,20,21,0x53,1,true,-500,100};
static_assert(checks(), "Tilt persistence, migrations, CRC and pin conflicts");
static_assert(!tilt_active(1999,high,false,true) && tilt_active(2000,high,false,true));
static_assert(tilt_active(1950,high,true,true) && !tilt_active(1899,high,true,true));
static_assert(tilt_active(-500,low,false,true) && !tilt_active(-499,low,false,true));
static_assert(tilt_active(-450,low,true,true) && !tilt_active(-399,low,true,true));
static_assert(!tilt_active(-1000,low,true,false), "Sensor failure releases Tilt immediately");
static_assert(adxl_mg(256)==1000 && adxl_mg(-256)==-1000 && adxl_mg(4096)==16000);
static_assert(make_xinput_report({bit(Button::Tilt)}).right_y==32767);
constexpr bool interrupted_samples() {
    AdxlSample s{};
    if(s.fresh(0))return false;
    const std::array<uint8_t,6> tilted={0,1,0,0,0,0}; // X = 1 g
    if(!s.accept(tilted,1000) || s.axes[0]!=1000)return false;
    const TiltConfig t{TiltSource::Adxl345,28,20,21,0x53,0,false,600,100};
    // Radio-related missing reads do not release a fresh tilted sample.
    if(!tilt_active(s.axes[0],t,true,s.fresh(1099)))return false;
    if(tilt_active(s.axes[0],t,true,s.fresh(1100)))return false;
    auto bad=tilted;bad[5]=0x20; // Invalid Z must not partly replace X/Y.
    if(s.accept(bad,1090) || s.updated!=1000 || s.axes[0]!=1000)return false;
    if(!s.accept(tilted,1095) || !s.fresh(1194) || s.fresh(1195))return false;
    if(!s.accept({},1200) || tilt_active(s.axes[0],t,true,s.fresh(1200)))return false;
    // Millisecond wrap remains valid; real loss still expires after 100 ms.
    return s.accept(tilted,0xfffffff0u) && s.fresh(50) && !s.fresh(84);
}
static_assert(interrupted_samples(), "ADXL transient loss, atomic axes, recovery, real release and wraparound");
constexpr bool wireless_tilt_interruption() {
    AdxlSample sample{};wireless::Link receiver{};
    const TiltConfig t{TiltSource::Adxl345,28,20,21,0x53,0,false,600,100};
    sample.accept({0,1,0,0,0,0},1000);
    bool active=false;
    for(uint16_t step=0;step<=10;++step) {
        const uint32_t now=1000+step*10;
        // Simulate missing I2C reads while BLE still sends fresh packets.
        active=tilt_active(sample.axes[0],t,active,sample.fresh(now));
        State state{};if(active)state.buttons=bit(Button::Tilt);
        const auto packet=wireless::encode(state,0,step);
        if(!receiver.accept(packet.data(),packet.size(),now))return false;
        const auto received=receiver.state(now);
        if(received.pressed(Button::Tilt)!=(step<10))return false;
        if(make_xinput_report(received,UsbMode::RockBand).right_y!=(step<10?32767:0))return false;
    }
    return !receiver.state(1350).pressed(Button::Tilt);
}
static_assert(wireless_tilt_interruption(), "Held sensor sample survives BLE receiver path; persistent failure releases XInput Tilt");
}
