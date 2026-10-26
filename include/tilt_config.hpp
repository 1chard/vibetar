#pragma once
#include <array>
#include <cstddef>
#include <cstdint>
namespace guitar {
enum class TiltSource : uint8_t { Button, Adc, Adxl345 };
struct TiltConfig {
    TiltSource source = TiltSource::Button;
    uint8_t adc_pin = 28, sda = 20, scl = 21, address = 0x53, axis = 0;
    bool below = false;
    int16_t threshold = 2000; // ADC counts or signed milli-g, according to source
    uint16_t hysteresis = 100;
};
inline constexpr uint8_t tilt_report_id = 9, tilt_defaults_report_id = 10, tilt_live_report_id = 11;
using TiltPayload = std::array<uint8_t, 16>;
constexpr bool valid_tilt(const TiltConfig& t) {
    if (static_cast<uint8_t>(t.source) > 2 || t.adc_pin < 26 || t.adc_pin > 28 ||
        t.sda > 20 || (t.sda % 4 != 0 && t.sda % 4 != 2) || t.scl != t.sda + 1 ||
        (t.address != 0x53 && t.address != 0x1d) || t.axis > 2) return false;
    const int lo = t.source == TiltSource::Adxl345 ? -16000 : 0;
    const int hi = t.source == TiltSource::Adxl345 ? 16000 : 4095;
    return t.threshold >= lo && t.threshold <= hi && t.hysteresis <= hi - lo &&
           (t.below ? int(t.threshold) + t.hysteresis <= hi : int(t.threshold) - t.hysteresis >= lo);
}
constexpr TiltPayload encode_tilt(const TiltConfig& t, uint8_t status = 0) {
    return {1,status,static_cast<uint8_t>(t.source),t.adc_pin,t.sda,t.scl,t.address,t.axis,
            static_cast<uint8_t>(t.below),static_cast<uint8_t>(t.threshold),static_cast<uint8_t>(t.threshold >> 8),
            static_cast<uint8_t>(t.hysteresis),static_cast<uint8_t>(t.hysteresis >> 8),0,0,0};
}
constexpr bool decode_tilt(const TiltPayload& p, TiltConfig& t) {
    if (p[0] != 1 || p[1] || p[8] > 1 || p[13] || p[14] || p[15]) return false;
    TiltConfig v{static_cast<TiltSource>(p[2]),p[3],p[4],p[5],p[6],p[7],p[8] != 0,
                 static_cast<int16_t>(uint16_t(p[9]) | uint16_t(p[10]) << 8),
                 static_cast<uint16_t>(uint16_t(p[11]) | uint16_t(p[12]) << 8)};
    if (!valid_tilt(v)) return false;
    t = v; return true;
}
constexpr bool same_tilt(const TiltConfig& a, const TiltConfig& b) {
    const auto x = encode_tilt(a), y = encode_tilt(b);
    for (size_t i = 0; i < x.size(); ++i) if (x[i] != y[i]) return false;
    return true;
}
constexpr bool tilt_active(int value, const TiltConfig& t, bool previous, bool valid) {
    if (!valid) return false;
    const int limit = t.threshold + (previous ? (t.below ? int(t.hysteresis) : -int(t.hysteresis)) : 0);
    return t.below ? value <= limit : value >= limit;
}
constexpr int16_t adxl_mg(int16_t raw) { return static_cast<int16_t>(int32_t(raw) * 1000 / 256); }
}
