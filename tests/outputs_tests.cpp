#include "settings_record.hpp"
#include "gpio_report.hpp"
using namespace guitar;
namespace {
constexpr bool checks() {
    RuntimeConfig c{},r{};c.outputs.rules={{{0,1,false},{1,2,true},{14,4,false}}};
    if(!valid_config(c) || !settings::parse_record(settings::make_record(c,5),r) || !same_config(c,r)) return false;
    if(!output_active(c.outputs.rules[0],UsbMode::Hid) || output_active(c.outputs.rules[0],UsbMode::XInput)) return false;
    if(output_level(c.outputs.rules[1],UsbMode::XInput) || !output_level(c.outputs.rules[1],UsbMode::RockBand)) return false;
    c.outputs.rules[2].modes=6;
    if(!output_active(c.outputs.rules[2],UsbMode::XInput) || !output_active(c.outputs.rules[2],UsbMode::RockBand) || output_active(c.outputs.rules[2],UsbMode::Hid)) return false;
    auto p=encode_outputs(c.outputs);p[5]=5;if(decode_outputs(p,r.outputs)) return false;
    p=encode_outputs(c.outputs);p[14]=1;if(decode_outputs(p,r.outputs)) return false;
    c.outputs.rules[0].pin=2;if(valid_config(c)) return false; // fret
    c.outputs.rules[0].pin=13;if(valid_config(c)) return false; // mode
    c.outputs.rules[0].pin=26;if(valid_config(c)) return false; // ADC
    c.outputs.rules[0].pin=1;if(valid_config(c)) return false; // duplicate output
    c.outputs.rules[0].pin=20;c.tilt.source=TiltSource::Adxl345;if(valid_config(c)) return false;
    c.tilt.source=TiltSource::Button;c.extras.digital_pins[0]=20;if(valid_config(c)) return false;
    c.extras.digital_pins[0]=255;c.outputs.rules[0].pin=0;
    const auto scan=encode_gpio_scan(c,{},false,digital_scan_mask,{0,0,0});
    if(!(scan[12]&3) || !(scan[13]&64)) return false;
    auto record=settings::make_record(c,5);record[83]^=1;if(settings::parse_record(record,r)) return false;
    // Existing XFC4 retains host policy and migrates with all LED outputs disabled.
    c.outputs={};c.host={ModePolicy::XInputOnly,true};record=settings::make_record(c,6);
    settings::write_u32(record,0,0x34434658);settings::write_u32(record,80,settings::crc32(record,80));
    return settings::parse_record(record,r) && same_config(c,r);
}
constexpr bool combined_event_checks(){
    using bluetooth::Phase;
    RuntimeConfig c{},decoded{};c.outputs.rules[0]={0,1,false,uint8_t(128|1|2|4|8|16|32)};
    if(!valid_config(c) || !settings::parse_record(settings::make_record(c,32),decoded) || !same_config(c,decoded))return false;
    const auto& r=c.outputs.rules[0];
    if(output_brightness(r,UsbMode::Hid,false,Phase::Searching,800)!=0)return false;
    if(output_brightness(r,UsbMode::Hid,false,Phase::Searching,0)!=32)return false;
    if(output_brightness(r,UsbMode::Hid,false,Phase::Pairing,150)!=0)return false;
    if(output_brightness(r,UsbMode::Hid,false,Phase::Connected)!=255)return false;
    if(output_brightness(r,UsbMode::Hid,false,Phase::Waiting,150)!=0)return false;
    if(output_brightness(r,UsbMode::Hid,true,Phase::Searching,800)!=255)return false;
    if(output_brightness(r,UsbMode::Hid,false,Phase::Idle)!=255)return false;
    c.outputs.rules[0].event=128;if(output_brightness(c.outputs.rules[0],UsbMode::Hid))return false;
    c.outputs.rules[0].event=192;return !valid_config(c) && output_events(4)==28;
}
static_assert(combined_event_checks(),"Combine LED events, preserve blink-off periods, battery priority, idle USB fallback and flash persistence");
static_assert(checks(),"LED mode events, polarity, collisions, GPIO reports, CRC and XFC4 migration");
}

namespace {
constexpr bool bluetooth_led_checks(){
    using bluetooth::Phase;
    if(bluetooth::phase(false,true,true,true,true,true)!=Phase::Idle)return false;
    if(bluetooth::phase(true,true,true,false,false,false)!=Phase::Searching)return false;
    if(bluetooth::phase(true,true,false,true,false,false)!=Phase::Pairing)return false;
    if(bluetooth::phase(true,true,false,false,true,false)!=Phase::Pairing)return false;
    if(bluetooth::phase(true,true,false,false,true,true)!=Phase::Connected)return false;
    RuntimeConfig c{},decoded{};c.outputs.rules={{{0,1,false,2},{1,1,true,3},{14,1,false,4}}};
    if(!valid_config(c) || !settings::parse_record(settings::make_record(c,17),decoded) || !same_config(c,decoded))return false;
    const auto& search=c.outputs.rules[0];const auto& pairing=c.outputs.rules[1];const auto& status=c.outputs.rules[2];
    if(output_brightness(search,UsbMode::Hid,false,Phase::Searching,0)!=32 || output_brightness(search,UsbMode::Hid,false,Phase::Searching,800)!=0)return false;
    if(output_brightness(pairing,UsbMode::Hid,false,Phase::Pairing,149)!=255 || output_brightness(pairing,UsbMode::Hid,false,Phase::Pairing,150)!=0)return false;
    if(output_brightness(status,UsbMode::Hid,false,Phase::Searching,1600)!=32 || output_brightness(status,UsbMode::Hid,false,Phase::Pairing,300)!=255)return false;
    if(output_brightness(status,UsbMode::Hid,false,Phase::Connected)!=255 || output_brightness(status,UsbMode::Hid,false,Phase::Idle)!=0)return false;
    if(output_brightness(search,UsbMode::Hid,false,Phase::Pairing) || output_brightness(pairing,UsbMode::Hid,false,Phase::Searching))return false;
    c.outputs.rules[2].pin=16;if(valid_outputs(c.outputs))return false; // shares PWM channel with GP0
    c.outputs.rules[2].event=0;return valid_config(c);
}
static_assert(bluetooth_led_checks(),"Bluetooth phases, dim/bright blinking, steady connection, persistence and PWM aliases");
}

namespace {
constexpr bool onboard_checks(){
    RuntimeConfig c{},decoded{};c.outputs.rules[0]={onboard_led_pin,1,false,4};
    if(!valid_config(c) || !settings::parse_record(settings::make_record(c,23),decoded) || !same_config(c,decoded))return false;
    const auto scan=encode_gpio_scan(c,{},false,digital_scan_mask,{0,0,0});
    if(scan[15] || output_gpio(onboard_led_pin)!=25)return false;
    if(effective_brightness(onboard_led_pin,true,32)!=255 || effective_brightness(onboard_led_pin,false,32)!=32)return false;
    if(effective_brightness(onboard_led_pin,true,0) || effective_brightness(14,true,32)!=32)return false;
    c.outputs.rules[1]={onboard_led_pin,1,false,0};if(valid_config(c))return false;
    c.outputs.rules[1]={14,1,false,4};if(!valid_config(c))return false;
    c.outputs.rules[1].pin=9;if(valid_outputs(c.outputs))return false;
    c.outputs.rules[1].pin=255;c.outputs.rules[0].active_low=true;if(valid_config(c))return false;
    c.outputs.rules[0]={25,1,false,0};return !valid_config(c);
}
static_assert(onboard_checks(),"Onboard target, board-specific brightness, flash, duplicate/polarity/PWM validation, safe GPIO scan");
}
