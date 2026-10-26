#include "xsm3_auth.hpp"
#include "xsm3_protocol.hpp"
#include "xsm3_port.h"
#include "xinput_profile.hpp"
#include "usb_device.hpp"
#include "pico/unique_id.h"
#include "hardware/sync.h"
#include "device/usbd_pvt.h"
#include "device/dcd.h"
#include <algorithm>
#include <cstring>
namespace guitar::auth {
namespace {
Session session{};
alignas(8) std::array<uint8_t,34> receive{};
alignas(8) std::array<uint8_t,48> response{};
std::array<uint8_t,29> identity{};
std::array<uint8_t,2> status{};
Command pending=Command::None;
volatile uint32_t setup_epoch=0;
uint32_t deferred_epoch=0;
bool deferred=false;
uint8_t deferred_port=0;
tusb_control_request_t deferred_request{};
void make_identity(){
    std::memcpy(identity.data(),xsm3_id_data_ms_controller,identity.size());
    char serial[PICO_UNIQUE_BOARD_ID_SIZE_BYTES*2+1]{};
    pico_get_unique_board_id_string(serial,sizeof(serial));
    std::memcpy(identity.data()+5,serial+std::strlen(serial)-12,12);
    const auto& profile=xinput::profile_for(usb_mode());
    identity[20]=profile.vendor_id;identity[21]=profile.vendor_id>>8;
    identity[22]=profile.product_id;identity[23]=profile.product_id>>8;
    identity[28]=0;for(unsigned i=5;i<28;++i)identity[28]^=identity[i];
}
bool send_response(uint8_t port,const tusb_control_request_t* request){
    if(!session.ready())return false;
    return tud_control_xfer(port,request,response.data(),static_cast<uint16_t>(response[4]+6));
}
}
void reset(){session.reset();pending=Command::None;deferred=false;response.fill(0);receive.fill(0);guitar_xsm3_reset();}
bool control(uint8_t port,uint8_t stage,const tusb_control_request_t* request){
    const auto cmd=command(request->bmRequestType,request->bRequest,request->wIndex,request->wLength);
    if(cmd==Command::None)return false;
    if(stage==CONTROL_STAGE_SETUP){
        deferred=false;
        switch(cmd){
        case Command::Identify:
            reset();make_identity();guitar_xsm3_identify(identity.data());session.phase=Phase::Identified;
            return tud_control_xfer(port,request,identity.data(),identity.size());
        case Command::Status:
            status={static_cast<uint8_t>(session.status()),0};return tud_control_xfer(port,request,status.data(),status.size());
        case Command::Response:
            if(session.busy()){
                // Leave EP0 NAKing until the main loop completes the challenge.
                deferred=true;deferred_epoch=setup_epoch;deferred_port=port;deferred_request=*request;return true;
            }
            return send_response(port,request);
        case Command::Init:case Command::Verify:
            if(session.busy() || (cmd==Command::Init && session.phase!=Phase::Identified && !session.ready()) ||
               (cmd==Command::Verify && !session.ready()))return false;
            receive.fill(0);return tud_control_xfer(port,request,receive.data(),request->wLength);
        case Command::Acknowledge:
            return tud_control_status(port,request);
        default:return false;
        }
    }
    if(stage==CONTROL_STAGE_DATA && (cmd==Command::Init || cmd==Command::Verify)){
        if(!valid_packet(receive.data(),request->wLength,cmd) || !session.begin(cmd))return false;
        pending=cmd;return true;
    }
    return true;
}
void task(){
    if(pending==Command::None)return;
    const auto cmd=pending;pending=Command::None;
    if(cmd==Command::Init)guitar_xsm3_identify(identity.data());
    const bool valid=cmd==Command::Init?guitar_xsm3_init(receive.data()):guitar_xsm3_verify(receive.data());
    session.finish(valid);
    if(valid)std::memcpy(response.data(),xsm3_challenge_response,response.size());else response.fill(0);
    // Do not answer an abandoned EP0 request if a new SETUP/reset arrived during crypto.
    const auto irq=save_and_disable_interrupts();
    if(deferred && deferred_epoch==setup_epoch){
        if(!send_response(deferred_port,&deferred_request)){
            usbd_edpt_stall(deferred_port,0);usbd_edpt_stall(deferred_port,0x80);
        }
    }
    deferred=false;restore_interrupts(irq);
}
}
extern "C" void tud_event_hook_cb(uint8_t,uint32_t event,bool){
    if(event==DCD_EVENT_SETUP_RECEIVED || event==DCD_EVENT_BUS_RESET || event==DCD_EVENT_UNPLUGGED)++guitar::auth::setup_epoch;
}
