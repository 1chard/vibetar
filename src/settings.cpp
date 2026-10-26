#include "settings.hpp"
#include "settings_record.hpp"
#include "hardware/flash.h"
#include "hardware/regs/addressmap.h"
#include "pico/flash.h"
#include <cstring>

namespace guitar::settings {
namespace {
// Physical Pico W flash is 2 MiB. The linker reserves its final two 4 KiB sectors.
constexpr uint32_t physical_flash_size = 2u * 1024 * 1024;
constexpr uint32_t settings_offset = physical_flash_size - 2 * FLASH_SECTOR_SIZE;
static_assert(FLASH_PAGE_SIZE == 256);
static_assert(PICO_FLASH_SIZE_BYTES == physical_flash_size);
RuntimeConfig active{}, pending{};
ConfigStatus status = ConfigStatus::Ready;
bool write_pending = false;
int active_slot = -1;
uint32_t sequence = 0;

FlashRecord read_slot(unsigned slot) {
    FlashRecord record{};
    std::memcpy(record.data(), reinterpret_cast<const void*>(XIP_BASE + settings_offset + slot * FLASH_SECTOR_SIZE), record.size());
    return record;
}
struct WriteJob { uint32_t offset; const uint8_t* page; };
void write_flash(void* parameter) {
    const auto& job = *static_cast<const WriteJob*>(parameter);
    flash_range_erase(job.offset, FLASH_SECTOR_SIZE);
    flash_range_program(job.offset, job.page, FLASH_PAGE_SIZE);
}
}

void load() {
    active = RuntimeConfig{};
    active_slot = -1;
    sequence = 0;
    write_pending = false;
    status = ConfigStatus::Ready;
    for (unsigned slot = 0; slot < 2; ++slot) {
        const auto record = read_slot(slot);
        RuntimeConfig candidate{};
        if (!parse_record(record, candidate)) continue;
        const uint32_t candidate_sequence = read_u32(record, 4);
        if (active_slot < 0 || newer_sequence(candidate_sequence, sequence)) {
            active = candidate;
            active_slot = static_cast<int>(slot);
            sequence = candidate_sequence;
        }
    }
}
const RuntimeConfig& current() { return active; }
ConfigPayload report() { return encode_config(active, status); }
ExtrasPayload extras_report() { return encode_extras(active, status); }
HostPayload host_report() { return encode_host(active.host,static_cast<uint8_t>(status)); }
OutputsPayload outputs_report() { return encode_outputs(active.outputs,static_cast<uint8_t>(status)); }
BatteryPayload battery_report(){return encode_battery(active.battery,static_cast<uint8_t>(status));}
BluetoothPayload bluetooth_report(){return encode_bluetooth(active.bluetooth,static_cast<uint8_t>(status));}
void submit_bluetooth(const uint8_t* payload,uint16_t length){
    if(write_pending)return;
    BluetoothPayload p{};
    if(length!=p.size()){status=ConfigStatus::Invalid;return;}
    std::memcpy(p.data(),payload,p.size());auto candidate=active;
    if(!decode_bluetooth(p,candidate.bluetooth)){status=ConfigStatus::Invalid;return;}
    if(same_config(candidate,active)){status=ConfigStatus::Ready;return;}
    pending=candidate;write_pending=true;status=ConfigStatus::Pending;
}
void submit_battery(const uint8_t* payload,uint16_t length){
    if(write_pending)return;
    BatteryPayload p{};
    if(length!=p.size()){status=ConfigStatus::Invalid;return;}
    std::memcpy(p.data(),payload,p.size());auto candidate=active;
    if(p[0]==1&&p[1]==1){
        for(size_t i=2;i<p.size();++i)if(p[i]){status=ConfigStatus::Invalid;return;}
        candidate.battery={};
    }else if(!decode_battery(p,candidate.battery)){status=ConfigStatus::Invalid;return;}
    if(same_config(candidate,active)){status=ConfigStatus::Ready;return;}
    pending=candidate;write_pending=true;status=ConfigStatus::Pending;
}
void submit_outputs(const uint8_t* payload,uint16_t length) {
    if(write_pending) return;
    OutputsPayload bytes{};
    if(length!=bytes.size()) {status=ConfigStatus::Invalid;return;}
    std::memcpy(bytes.data(),payload,bytes.size());
    auto candidate=active;
    if(bytes[0]==1 && bytes[1]==1) {
        for(size_t i=2;i<bytes.size();++i) if(bytes[i]) {status=ConfigStatus::Invalid;return;}
        candidate.outputs={};
    } else if(!decode_outputs(bytes,candidate.outputs)) {status=ConfigStatus::Invalid;return;}
    if(!valid_config(candidate)) {status=ConfigStatus::Invalid;return;}
    if(same_config(candidate,active)) {status=ConfigStatus::Ready;return;}
    pending=candidate;write_pending=true;status=ConfigStatus::Pending;
}
void submit_host(const uint8_t* payload,uint16_t length) {
    if(write_pending) return;
    HostPayload bytes{};
    if(length!=bytes.size()) { status=ConfigStatus::Invalid;return; }
    std::memcpy(bytes.data(),payload,bytes.size());
    auto candidate=active;
    if(bytes[0]==1 && bytes[1]==1) {
        for(size_t i=2;i<bytes.size();++i) if(bytes[i]) { status=ConfigStatus::Invalid;return; }
        candidate.host=HostConfig{};
    } else if(!decode_host(bytes,candidate.host)) { status=ConfigStatus::Invalid;return; }
    if(same_config(candidate,active)) { status=ConfigStatus::Ready;return; }
    pending=candidate;write_pending=true;status=ConfigStatus::Pending;
}
TiltPayload tilt_report() { return encode_tilt(active.tilt, static_cast<uint8_t>(status)); }
void submit_tilt(const uint8_t* payload, uint16_t length) {
    if (write_pending) return;
    TiltPayload bytes{};
    if (length != bytes.size()) { status = ConfigStatus::Invalid; return; }
    std::memcpy(bytes.data(), payload, bytes.size());
    auto candidate = active;
    if (bytes[0] == 1 && bytes[1] == 1) {
        for (size_t i = 2; i < bytes.size(); ++i)
            if (bytes[i]) { status = ConfigStatus::Invalid; return; }
        candidate.tilt = TiltConfig{};
    } else if (!decode_tilt(bytes, candidate.tilt) || !valid_config(candidate)) {
        status = ConfigStatus::Invalid; return;
    }
    if (same_config(candidate, active)) { status = ConfigStatus::Ready; return; }
    pending = candidate; write_pending = true; status = ConfigStatus::Pending;
}
void submit_extras(const uint8_t* payload, uint16_t length) {
    if (write_pending) return;
    ExtrasPayload bytes{};
    if (length != bytes.size()) { status = ConfigStatus::Invalid; return; }
    std::memcpy(bytes.data(), payload, bytes.size());
    auto candidate = active;
    if (bytes[0] == 1 && bytes[1] == 1) {
        for (size_t i = 2; i < bytes.size(); ++i)
            if (bytes[i]) { status = ConfigStatus::Invalid; return; }
        candidate.extras = ExtrasConfig{};
    } else if (!decode_extras(bytes, candidate)) { status = ConfigStatus::Invalid; return; }
    if (same_config(candidate, active)) { status = ConfigStatus::Ready; return; }
    pending = candidate; write_pending = true; status = ConfigStatus::Pending;
}
void submit(const uint8_t* payload, uint16_t length) {
    if (write_pending) return; // Keep the first transaction and Pending status.
    ConfigPayload bytes{};
    if (length != bytes.size()) { status = ConfigStatus::Invalid; return; }
    std::memcpy(bytes.data(), payload, bytes.size());
    RuntimeConfig candidate = active;
    if (bytes[0] == 1 && bytes[1] == 1) {
        // Explicit reset command: version 1, command 1, zero-filled remainder.
        for (size_t i = 2; i < bytes.size(); ++i)
            if (bytes[i]) { status = ConfigStatus::Invalid; return; }
        candidate = RuntimeConfig{}; // Factory reset clears both base and optional inputs.
    } else if (!decode_config(bytes, candidate)) {
        status = ConfigStatus::Invalid;
        return;
    }
    if (same_config(candidate, active)) { status = ConfigStatus::Ready; return; }
    pending = candidate;
    write_pending = true;
    status = ConfigStatus::Pending;
}
bool process() {
    if (!write_pending) return false;
    write_pending = false;
    const unsigned slot = active_slot == 0 ? 1 : 0;
    const auto record = make_record(pending, sequence + 1);
    WriteJob job{settings_offset + slot * FLASH_SECTOR_SIZE, record.data()};
    // Core 1 is unused; flash_safe_execute disables core 0 IRQs while XIP is unavailable.
    const int result = flash_safe_execute(write_flash, &job, 1000);
    const auto verified = read_slot(slot);
    RuntimeConfig readback{};
    if (result != PICO_OK || !parse_record(verified, readback) ||
        read_u32(verified, 4) != sequence + 1 || !same_config(pending, readback)) {
        status = ConfigStatus::FlashError;
        return false; // Previous sector and active GPIOs remain valid.
    }
    active = pending;
    active_slot = static_cast<int>(slot);
    ++sequence;
    status = ConfigStatus::Ready;
    return true;
}
}
