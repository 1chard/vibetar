#pragma once
#include "controller.hpp"
#include "bluetooth_status.hpp"
#include <array>

namespace guitar {
inline constexpr uint8_t outputs_report_id=15, outputs_defaults_report_id=16, outputs_live_report_id=17;
inline constexpr uint8_t onboard_led_pin=254; // logical target, never a raw RP2040 GPIO
constexpr uint8_t output_gpio(uint8_t pin) { return pin==onboard_led_pin?25:pin; }
// Extended event byte: bit 7 marks a mask; bits 0..5 combine independent events.
constexpr uint8_t output_events(uint8_t event){
    if(event&128)return event&63;
    return event==0?1:event==1?2:event==2?4:event==3?8:event==4?28:0;
}
struct OutputRule {
    uint8_t pin=255;
    uint8_t modes=1; // bit 0 HID, bit 1 Guitar Hero, bit 2 Rock Band
    bool active_low=false;
    uint8_t event=0; // Legacy 0..4, or 0x80 | event mask (USB, battery, search, pairing, connected, waiting)
};
struct OutputsConfig { std::array<OutputRule,3> rules{{{255,1,false},{255,2,false},{255,4,false}}}; };
using OutputsPayload=std::array<uint8_t,16>;
constexpr bool valid_outputs(const OutputsConfig& c) {
    for(size_t i=0;i<c.rules.size();++i) {
        const auto& r=c.rules[i];
        if((r.pin!=255 && r.pin!=onboard_led_pin && r.pin>22) || r.modes>7 || (r.event>4 && (!(r.event&128) || (r.event&64)))) return false;
        if(r.pin==onboard_led_pin && r.active_low)return false;
        if(r.pin!=255) for(size_t j=i+1;j<c.rules.size();++j) if(r.pin==c.rules[j].pin) return false;
        // RP2040 GPIOs separated by 16 share a PWM channel.
        if(r.pin!=255 && (output_events(r.event)&44))for(size_t j=i+1;j<c.rules.size();++j)
            if(c.rules[j].pin!=255 && (output_events(c.rules[j].event)&44) && (output_gpio(r.pin)&15)==(output_gpio(c.rules[j].pin)&15))return false;
    }
    return true;
}
constexpr bool output_owns(const OutputsConfig& c,uint8_t pin) {
    for(const auto& r:c.rules) if(r.pin!=255 && r.pin==pin) return true;
    return false;
}
constexpr uint8_t output_brightness(const OutputRule& r,UsbMode mode,bool battery_low=false,
                                    bluetooth::Phase phase=bluetooth::Phase::Idle,uint32_t elapsed=0) {
    if(r.pin==255)return 0;
    const auto events=output_events(r.event);
    if((events&2) && battery_low)return 255;
    if((events&4) && phase==bluetooth::Phase::Searching)return elapsed%1600<800?32:0;
    if((events&8) && phase==bluetooth::Phase::Pairing)return elapsed%300<150?255:0;
    if((events&16) && phase==bluetooth::Phase::Connected)return 255;
    if((events&32) && phase==bluetooth::Phase::Waiting)return elapsed%3000<150?255:0;
    return (events&1) && (r.modes & (1u<<static_cast<uint8_t>(mode)))?255:0;
}
constexpr bool output_active(const OutputRule& r,UsbMode mode,bool battery_low=false) { return output_brightness(r,mode,battery_low)!=0; }
constexpr uint8_t effective_brightness(uint8_t pin,bool wireless,uint8_t brightness) {
    return pin==onboard_led_pin && wireless?(brightness?255:0):brightness;
}
constexpr bool output_level(const OutputRule& r,UsbMode mode,bool battery_low=false) { return output_active(r,mode,battery_low)!=r.active_low; }
constexpr OutputsPayload encode_outputs(const OutputsConfig& c,uint8_t status=0) {
    OutputsPayload p{1,status};
    for(size_t i=0;i<3;++i) {p[2+4*i]=c.rules[i].pin;p[3+4*i]=c.rules[i].modes;p[4+4*i]=c.rules[i].active_low;p[5+4*i]=c.rules[i].event;}
    return p;
}
constexpr bool decode_outputs(const OutputsPayload& p,OutputsConfig& c) {
    if(p[0]!=1 || p[1] || p[14] || p[15]) return false;
    OutputsConfig candidate{};
    for(size_t i=0;i<3;++i) {
        if(p[4+4*i]>1 || (p[5+4*i]>4 && (!(p[5+4*i]&128) || (p[5+4*i]&64)))) return false;
        candidate.rules[i]={p[2+4*i],p[3+4*i],bool(p[4+4*i]),p[5+4*i]};
    }
    if(!valid_outputs(candidate)) return false;
    c=candidate;return true;
}
constexpr bool same_outputs(const OutputsConfig& a,const OutputsConfig& b) {
    const auto x=encode_outputs(a),y=encode_outputs(b);
    for(size_t i=0;i<x.size();++i) if(x[i]!=y[i]) return false;
    return true;
}
}
