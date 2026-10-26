#include "usb_device.hpp"
#include "xsm3_auth.hpp"
#include "feature_protocol.hpp"
#include "outputs.hpp"
#include "battery.hpp"
#include "bluetooth.hpp"
#include "bluetooth_discovery.hpp"
#include "radio.hpp"
#include "config.hpp"
#include "controller_preview.hpp"
#include "settings.hpp"
#include "firmware_control.hpp"
#include "pico/time.h"
#include "xinput_driver.hpp"
#include "tusb.h"
#include <algorithm>
#include <cstring>

namespace {
guitar::UsbMode active_mode = guitar::UsbMode::Hid;
guitar::HostDetector host_detector;
bool force_hid=false, self_transition=false;
guitar::HidReport latest_hid{};
guitar::State latest_state{};
guitar::FirmwareControl firmware_control;
guitar::GpioPayload latest_gpio{};
guitar::GpioScanPayload latest_scan{};
std::array<uint8_t,20> latest_tilt{};
}
namespace guitar {
UsbMode usb_mode() { return active_mode; }
uint8_t usb_mode_mask() {
    return (xinput_only(settings::current().host,host_detector.kind(),force_hid)?6:7) | (1<<static_cast<uint8_t>(active_mode));
}
void usb_observe_host_request(uint8_t type,uint8_t request,uint16_t value,uint16_t index,uint16_t length) {
    host_detector.request(type,request,value,index,length);
}
void usb_observe_os_string() { host_detector.os_string(); }
void usb_host_bus_reset() {
    if(!self_transition) host_detector.reset(to_ms_since_boot(get_absolute_time()));
}
void UsbDevice::configure_boot(bool recovery,uint32_t now) {
    force_hid=recovery;self_transition=false;host_detector.reset(now);
    active_mode=policy_boot_mode(settings::current().host,recovery);
}
bool usb_bootsel_due(uint32_t now) { return firmware_control.due(now) && !firmware_control.restart(); }
bool usb_restart_due(uint32_t now) { return firmware_control.due(now) && firmware_control.restart(); }
bool UsbDevice::init() {
    return tud_init(0);
}
void UsbDevice::request_toggle(uint32_t now) {
    request_mode(policy_next_mode(active_mode,xinput_only(settings::current().host,host_detector.kind(),force_hid)),now);
}
void UsbDevice::request_mode(UsbMode mode,uint32_t now) {
    if (detached_) return; // One transition at a time; holding button never repeats.
    if(mode==active_mode) return;
    // Deinit stops endpoints and IRQ before any descriptor is changed.
    self_transition=true;
    tud_disconnect();
    if (!tud_deinit(0)) { self_transition=false;tud_connect();return; }
    pending_mode_=mode;
    detached_ = true;
    detached_at_ = now;
}
void UsbDevice::task(uint32_t now, const State& state, const GpioPayload& gpio, const GpioScanPayload& scan, const std::array<uint8_t,20>& tilt) {
    latest_tilt = tilt;
    latest_scan = scan;
    latest_gpio = gpio;
    latest_state=state;
    latest_hid = make_hid_report(state);
    if (detached_) {
        if (uint32_t(now - detached_at_) < config::usb_detach_ms) return;
        const auto previous_mode = active_mode;
        active_mode = pending_mode_;
        if (!init()) {
            active_mode = previous_mode;
            return; // Retry later with descriptors still stable while detached.
        }
        detached_ = false;
        last_report_ = now - config::report_interval_ms;
        tud_connect();
    }
    // OPT_OS_PICO supports blocking waits: never block the GPIO scan loop.
    tud_task_ext(0, false);
    if(is_xinput_mode(active_mode))auth::task();
    // Do not suppress physical reconnection resets indefinitely if a host
    // never mounts the personality selected by MODE.
    if(self_transition && !tud_mounted() && uint32_t(now-detached_at_)>=5000) self_transition=false;
    if(!force_hid && settings::current().host.policy==ModePolicy::Auto && !firmware_control.pending()) {
        if(host_detector.kind()==HostKind::Xbox360 && active_mode==UsbMode::Hid)
            request_mode(settings::current().host.boot_rock_band ? UsbMode::RockBand : UsbMode::XInput,now);
        if(detached_) return;
    }
    if (!tud_mounted() || tud_suspended() || uint32_t(now - last_report_) < config::report_interval_ms) return;
    bool sent = false;
    if (active_mode == UsbMode::Hid) {
        if (tud_hid_ready()) sent = tud_hid_report(gamepad_report_id, &latest_hid, sizeof(latest_hid));
    } else if (xinput_ready()) {
        sent = xinput_send(make_xinput_report(state, active_mode));
    }
    if (sent) last_report_ = now;
}
}

namespace guitar {
uint16_t read_feature_report(uint8_t report_id,uint8_t* buffer,uint16_t requested) {
#if VIBETAR_RECEIVER
    if(radio::available() && report_id==31){
        const auto report=bluetooth::health_report();const auto size=std::min<uint16_t>(requested,report.size());
        std::memcpy(buffer,report.data(),size);return size;
    }
    if(radio::available() && report_id==29){
        const auto report=bluetooth::discovery_report();const auto size=std::min<uint16_t>(requested,report.size());
        std::memcpy(buffer,report.data(),size);return size;
    }
    if(radio::available() && report_id==30){
        const auto report=bluetooth::connect_report();const auto size=std::min<uint16_t>(requested,report.size());
        std::memcpy(buffer,report.data(),size);return size;
    }
#endif
    if(radio::available() && report_id==32){
        const auto report=bluetooth::link_report();const auto size=std::min<uint16_t>(requested,report.size());
        std::memcpy(buffer,report.data(),size);return size;
    }
    if(report_id==33){
#if VIBETAR_RECEIVER
        const auto report=bluetooth::received_preview(active_mode);
#else
        const auto report=encode_preview(latest_state,active_mode,false,true,to_ms_since_boot(get_absolute_time()));
#endif
        const auto size=std::min<uint16_t>(requested,report.size());std::memcpy(buffer,report.data(),size);return size;
    }
    if(report_id==28){
        const std::array<uint8_t,8> report{'X','C','R','L',1,VIBETAR_RECEIVER,static_cast<uint8_t>(radio::available()),0};
        const auto size=std::min<uint16_t>(requested,report.size());std::memcpy(buffer,report.data(),size);return size;
    }
    if(guitar::radio::available()){
        if(report_id==guitar::bluetooth_live_report_id){
            const auto report=guitar::bluetooth::live_report();
            const auto size=std::min<uint16_t>(requested,report.size());std::memcpy(buffer,report.data(),size);return size;
        }
        if(report_id==guitar::bluetooth_command_report_id || report_id==guitar::bluetooth_report_id || report_id==guitar::bluetooth_defaults_report_id){
            const auto report=report_id==guitar::bluetooth_command_report_id?guitar::bluetooth::command_report():
                report_id==guitar::bluetooth_report_id?guitar::settings::bluetooth_report():guitar::encode_bluetooth(guitar::BluetoothConfig{});
            const auto size=std::min<uint16_t>(requested,report.size());std::memcpy(buffer,report.data(),size);return size;
        }
    }
    if(report_id==guitar::battery_live_report_id){
        const auto report=guitar::battery::live_report();
        const auto size=std::min<uint16_t>(requested,report.size());std::memcpy(buffer,report.data(),size);return size;
    }
    if((report_id==guitar::battery_report_id || report_id==guitar::battery_defaults_report_id)){
        const auto report=report_id==guitar::battery_report_id?guitar::settings::battery_report():guitar::encode_battery(guitar::BatteryConfig{});
        const auto size=std::min<uint16_t>(requested,report.size());std::memcpy(buffer,report.data(),size);return size;
    }
    if(report_id==guitar::outputs_live_report_id) {
        const auto report=guitar::outputs::live_report();
        const auto size=std::min<uint16_t>(requested,report.size());std::memcpy(buffer,report.data(),size);return size;
    }
    if((report_id==guitar::outputs_report_id || report_id==guitar::outputs_defaults_report_id)) {
        const auto report=report_id==guitar::outputs_report_id ? guitar::settings::outputs_report() : guitar::encode_outputs(guitar::OutputsConfig{});
        const auto size=std::min<uint16_t>(requested,report.size());std::memcpy(buffer,report.data(),size);return size;
    }
    if(report_id==guitar::host_live_report_id) {
        const auto& policy=guitar::settings::current().host;
        const std::array<uint8_t,16> report{'X','C','H','S',1,static_cast<uint8_t>(host_detector.kind()),
            static_cast<uint8_t>(active_mode),static_cast<uint8_t>((force_hid?1:0)|(guitar::xinput_only(policy,host_detector.kind(),force_hid)?2:0)),
            static_cast<uint8_t>(policy.policy),static_cast<uint8_t>(policy.boot_rock_band),host_detector.evidence()};
        const auto size=std::min<uint16_t>(requested,report.size());std::memcpy(buffer,report.data(),size);return size;
    }
    if((report_id==guitar::host_config_report_id || report_id==guitar::host_defaults_report_id)) {
        const auto report=report_id==guitar::host_config_report_id ? guitar::settings::host_report() : guitar::encode_host(guitar::HostConfig{});
        const auto size=std::min<uint16_t>(requested,report.size());std::memcpy(buffer,report.data(),size);return size;
    }
    if (report_id == guitar::tilt_live_report_id) {
        const auto size = std::min<uint16_t>(requested, latest_tilt.size());
        std::memcpy(buffer, latest_tilt.data(), size); return size;
    }
    if ((report_id == guitar::tilt_report_id || report_id == guitar::tilt_defaults_report_id)) {
        const auto report = report_id == guitar::tilt_report_id ? guitar::settings::tilt_report() : guitar::encode_tilt(guitar::TiltConfig{});
        const auto size = std::min<uint16_t>(requested, report.size());
        std::memcpy(buffer, report.data(), size); return size;
    }
    if ((report_id == guitar::extras_report_id || report_id == guitar::extras_defaults_report_id)) {
        const auto report = report_id == guitar::extras_report_id ? guitar::settings::extras_report() :
                            guitar::encode_extras(guitar::RuntimeConfig{});
        const auto size = std::min<uint16_t>(requested, report.size());
        std::memcpy(buffer, report.data(), size); return size;
    }
    if (report_id == guitar::gpio_scan_report_id) {
        const auto size = std::min<uint16_t>(requested, latest_scan.size());
        std::memcpy(buffer, latest_scan.data(), size);
        return size;
    }
    if (report_id == guitar::gpio_report_id) {
        const auto size = std::min<uint16_t>(requested, latest_gpio.size());
        std::memcpy(buffer, latest_gpio.data(), size);
        return size;
    }
    if (report_id == guitar::firmware_report_id) {
        const auto report = firmware_control.report();
        const auto size = std::min<uint16_t>(requested, report.size());
        std::memcpy(buffer, report.data(), size);
        return size;
    }
    if (report_id == guitar::defaults_report_id) {
        const auto report = guitar::encode_config(guitar::RuntimeConfig{});
        const auto size = std::min<uint16_t>(requested, report.size());
        std::memcpy(buffer, report.data(), size);
        return size;
    }
    if (report_id == guitar::config_report_id) {
        const auto report = guitar::settings::report();
        const auto size = std::min<uint16_t>(requested, report.size());
        std::memcpy(buffer, report.data(), size);
        return size;
    }
    return 0;
}
void submit_feature_report(uint8_t report_id,const uint8_t* buffer,uint16_t length) {
#if VIBETAR_RECEIVER
    if(radio::available() && report_id==30){bluetooth::submit_connect(buffer,length);return;}
#endif
    if (report_id == guitar::firmware_report_id) {
        const bool busy = guitar::settings::report()[1] == static_cast<uint8_t>(guitar::ConfigStatus::Pending);
        firmware_control.submit(buffer, length, to_ms_since_boot(get_absolute_time()), busy);
    } else if (report_id == guitar::config_report_id && !firmware_control.pending()) {
        guitar::settings::submit(buffer, length);
    } else if (report_id == guitar::extras_report_id && !firmware_control.pending()) {
        guitar::settings::submit_extras(buffer, length);
    } else if (report_id == guitar::tilt_report_id && !firmware_control.pending()) {
        guitar::settings::submit_tilt(buffer, length);
    } else if (report_id == guitar::host_config_report_id && !firmware_control.pending()) {
        guitar::settings::submit_host(buffer,length);
    } else if (report_id == guitar::outputs_report_id && !firmware_control.pending()) {
        guitar::settings::submit_outputs(buffer,length);
    } else if (report_id == guitar::battery_report_id && !firmware_control.pending()) {
        guitar::settings::submit_battery(buffer,length);
    } else if(report_id==guitar::bluetooth_report_id && guitar::radio::available() && !firmware_control.pending()){
        guitar::settings::submit_bluetooth(buffer,length);
    } else if(report_id==guitar::bluetooth_command_report_id && guitar::radio::available() && !firmware_control.pending()){
        guitar::bluetooth::submit_command(buffer,length);
    }
}
}
extern "C" uint16_t tud_hid_get_report_cb(uint8_t instance,uint8_t report_id,hid_report_type_t type,uint8_t* buffer,uint16_t requested) {
    if(instance!=0)return 0;
    if(type==HID_REPORT_TYPE_FEATURE)return guitar::read_feature_report(report_id,buffer,requested);
    if(report_id!=guitar::gamepad_report_id || type!=HID_REPORT_TYPE_INPUT)return 0;
    const auto size=std::min<uint16_t>(requested,sizeof(latest_hid));
    std::memcpy(buffer,&latest_hid,size);return size;
}
extern "C" void tud_hid_set_report_cb(uint8_t instance,uint8_t report_id,hid_report_type_t type,const uint8_t* buffer,uint16_t length) {
    if(instance==0 && type==HID_REPORT_TYPE_FEATURE)guitar::submit_feature_report(report_id,buffer,length);
}
extern "C" void tud_mount_cb() { self_transition=false; }
extern "C" void tud_umount_cb() { guitar::usb_host_bus_reset(); }
