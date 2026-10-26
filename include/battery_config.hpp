#pragma once
#include <array>
#include <cstdint>
#include <cstddef>
namespace guitar {
inline constexpr uint8_t battery_report_id=18,battery_defaults_report_id=19,battery_live_report_id=20;
struct BatteryConfig { bool enabled=false; uint16_t threshold_mv=3500,hysteresis_mv=150,diode_mv=0; };
using BatteryPayload=std::array<uint8_t,16>;
constexpr bool valid_battery(const BatteryConfig& c) {
    return c.threshold_mv>=2000 && c.threshold_mv<=4500 && c.hysteresis_mv<=1000 &&
           uint32_t(c.threshold_mv)+c.hysteresis_mv<=4500 && c.diode_mv<=1000;
}
constexpr BatteryPayload encode_battery(const BatteryConfig& c,uint8_t status=0) {
    BatteryPayload p{1,status,static_cast<uint8_t>(c.enabled)};
    const uint16_t v[]{c.threshold_mv,c.hysteresis_mv,c.diode_mv};
    for(size_t i=0;i<3;++i){p[4+2*i]=v[i];p[5+2*i]=v[i]>>8;}return p;
}
constexpr bool decode_battery(const BatteryPayload& p,BatteryConfig& c) {
    if(p[0]!=1 || p[1] || p[2]>1 || p[3])return false;
    for(size_t i=10;i<p.size();++i)if(p[i])return false;
    BatteryConfig v{bool(p[2]),uint16_t(p[4]|uint16_t(p[5])<<8),uint16_t(p[6]|uint16_t(p[7])<<8),uint16_t(p[8]|uint16_t(p[9])<<8)};
    if(!valid_battery(v))return false;
    c=v;return true;
}
constexpr bool same_battery(const BatteryConfig& a,const BatteryConfig& b){return a.enabled==b.enabled && a.threshold_mv==b.threshold_mv && a.hysteresis_mv==b.hysteresis_mv && a.diode_mv==b.diode_mv;}
constexpr uint16_t vsys_millivolts(uint16_t raw){return (uint32_t(raw)*9900+2048)/4096;}
struct BatteryState {
    bool valid=false,source_known=false,usb=false,low=false,error=false;
    uint16_t vsys_mv=0,battery_mv=0,raw=0;
    uint8_t low_samples=0;
    constexpr void sample(const BatteryConfig& c,uint16_t adc,bool known,bool usb_present,bool ok){
        raw=adc;vsys_mv=vsys_millivolts(adc);source_known=known;usb=known&&usb_present;
        valid=c.enabled&&ok&&adc<=4095&&vsys_mv>=1800&&vsys_mv<=5500;
        error=c.enabled&&(!known||!valid);battery_mv=valid&&known&&!usb ? vsys_mv+c.diode_mv : 0;
        if(!c.enabled||!valid||!known||usb){low=false;low_samples=0;return;}
        if(low){if(battery_mv>=c.threshold_mv+c.hysteresis_mv){low=false;low_samples=0;}}
        else if(battery_mv<=c.threshold_mv){if(++low_samples>=3){low=true;low_samples=3;}}
        else low_samples=0;
    }
};
}
