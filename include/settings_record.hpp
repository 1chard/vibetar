#pragma once
#include "runtime_config.hpp"

namespace guitar::settings {
using FlashRecord = std::array<uint8_t, 256>;
constexpr uint32_t read_u32(const FlashRecord& r, size_t offset) {
    return uint32_t(r[offset]) | uint32_t(r[offset + 1]) << 8 |
           uint32_t(r[offset + 2]) << 16 | uint32_t(r[offset + 3]) << 24;
}
constexpr void write_u32(FlashRecord& r, size_t offset, uint32_t value) {
    for (size_t i = 0; i < 4; ++i) r[offset + i] = static_cast<uint8_t>(value >> (8 * i));
}
constexpr uint32_t crc32(const FlashRecord& r, size_t length = 32) {
    uint32_t crc = 0xffffffffu;
    for (size_t i = 0; i < length; ++i) {
        crc ^= r[i];
        for (unsigned j = 0; j < 8; ++j) crc = (crc >> 1) ^ ((crc & 1) ? 0xedb88320u : 0);
    }
    return ~crc;
}
constexpr FlashRecord make_record(const RuntimeConfig& c, uint32_t sequence) {
    FlashRecord r{};
    for (auto& byte : r) byte = 0xff;
    write_u32(r, 0, 0x37434658); // XFC7; all earlier versions still load
    write_u32(r, 4, sequence);
    const auto payload = encode_config(c);
    for (size_t i = 0; i < payload.size(); ++i) r[8 + i] = payload[i];
    const auto extras = encode_extras(c);
    for (size_t i = 0; i < extras.size(); ++i) r[32 + i] = extras[i];
    const auto tilt = encode_tilt(c.tilt);
    for (size_t i = 0; i < tilt.size(); ++i) r[56 + i] = tilt[i];
    const auto host=encode_host(c.host);
    for(size_t i=0;i<host.size();++i) r[72+i]=host[i];
    const auto outputs=encode_outputs(c.outputs);
    for(size_t i=0;i<outputs.size();++i) r[80+i]=outputs[i];
    const auto battery=encode_battery(c.battery);
    for(size_t i=0;i<battery.size();++i)r[96+i]=battery[i];
    const auto bluetooth=encode_bluetooth(c.bluetooth);
    for(size_t i=0;i<bluetooth.size();++i)r[112+i]=bluetooth[i];
    write_u32(r,120,crc32(r,120));
    return r;
}
constexpr bool parse_record(const FlashRecord& r, RuntimeConfig& c) {
    const bool legacy = read_u32(r, 0) == 0x31434658;
    const bool with_bluetooth=read_u32(r,0)==0x37434658;
    const bool with_battery = read_u32(r,0)==0x36434658 || with_bluetooth;
    const bool with_outputs = read_u32(r,0)==0x35434658 || with_battery;
    const bool with_host = read_u32(r,0)==0x34434658 || with_outputs;
    const bool extended = read_u32(r, 0) == 0x33434658 || with_host;
    const size_t checksum = legacy ? 32 : with_bluetooth ? 120 : with_battery ? 112 : with_outputs ? 96 : with_host ? 80 : extended ? 72 : 56;
    if ((!legacy && !extended && read_u32(r, 0) != 0x32434658) ||
        read_u32(r, checksum) != crc32(r, checksum)) return false;
    ConfigPayload payload{};
    for (size_t i = 0; i < payload.size(); ++i) payload[i] = r[8 + i];
    RuntimeConfig candidate{};
    if (!decode_config(payload, candidate)) return false;
    if (!legacy) {
        ExtrasPayload extras{};
        for (size_t i = 0; i < extras.size(); ++i) extras[i] = r[32 + i];
        if (!decode_extras(extras, candidate)) return false;
    }
    if(with_host) {
        HostPayload host{};
        for(size_t i=0;i<host.size();++i) host[i]=r[72+i];
        if(!decode_host(host,candidate.host)) return false;
    }
    if (extended) {
        TiltPayload tilt{};
        for (size_t i = 0; i < tilt.size(); ++i) tilt[i] = r[56 + i];
        if (!decode_tilt(tilt, candidate.tilt) || !valid_config(candidate)) return false;
    }
    if(with_outputs) {
        OutputsPayload outputs{};
        for(size_t i=0;i<outputs.size();++i) outputs[i]=r[80+i];
        if(!decode_outputs(outputs,candidate.outputs) || !valid_config(candidate)) return false;
    }
    if(with_battery){
        BatteryPayload battery{};for(size_t i=0;i<battery.size();++i)battery[i]=r[96+i];
        if(!decode_battery(battery,candidate.battery))return false;
    }
    if(with_bluetooth){
        BluetoothPayload p{};for(size_t i=0;i<p.size();++i)p[i]=r[112+i];
        if(!decode_bluetooth(p,candidate.bluetooth))return false;
    }
    c = candidate;
    return true;
}
constexpr bool newer_sequence(uint32_t a, uint32_t b) {
    return a != b && uint32_t(a - b) < 0x80000000u;
}
}
