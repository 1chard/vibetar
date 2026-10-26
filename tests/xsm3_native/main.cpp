#include "xsm3_auth.hpp"
#include "xsm3_protocol.hpp"
#include "usb_device.hpp"
#include "excrypt.h"
#include <cstdio>
#include <cstring>
#include <vector>
extern "C" void fixture_init(uint8_t*,unsigned);
extern "C" bool fixture_check_init(const uint8_t*);
extern "C" void fixture_verify(uint8_t*,unsigned);
extern "C" bool fixture_check_verify(const uint8_t*);
extern "C" void tud_event_hook_cb(uint8_t,uint32_t,bool);
static guitar::UsbMode mode=guitar::UsbMode::XInput;
namespace guitar {UsbMode usb_mode(){return mode;}}
static std::vector<uint8_t> reply;
static uint8_t* receive_buffer;
static unsigned stalled;
static tusb_control_request_t request{};
extern "C" uint32_t get_rand_32(){static uint32_t state=0x12345678;state^=state<<13;state^=state>>17;state^=state<<5;return state;}
extern "C" void pico_get_unique_board_id_string(char* p,size_t size){std::snprintf(p,size,"E6614C311B934032");}
extern "C" bool tud_control_xfer(uint8_t,const tusb_control_request_t* r,void* p,uint16_t n){
    if(r->bmRequestType&128){auto* bytes=static_cast<uint8_t*>(p);reply.assign(bytes,bytes+(r->wLength<n?r->wLength:n));}
    else receive_buffer=static_cast<uint8_t*>(p);
    return true;
}
extern "C" bool tud_control_status(uint8_t,const tusb_control_request_t*){return true;}
extern "C" void usbd_edpt_stall(uint8_t,uint8_t){++stalled;}
static bool setup(uint8_t type,uint8_t cmd,uint16_t len,uint16_t index=0x101){
    request={type,cmd,cmd==0x81?uint16_t(0x5b17):uint16_t(3),index,len};reply.clear();receive_buffer=nullptr;
    tud_event_hook_cb(0,6,false);return guitar::auth::control(0,CONTROL_STAGE_SETUP,&request);
}
static bool write(uint8_t cmd,const uint8_t* packet,uint16_t len){
    if(!setup(0x41,cmd,len))return false;
    std::memcpy(receive_buffer,packet,len);
    return guitar::auth::control(0,CONTROL_STAGE_DATA,&request);
}
static void checksum(uint8_t* p,size_t n){p[n-1]=0;for(size_t i=5;i+1<n;++i)p[n-1]^=p[i];}
#define CHECK(x) do {if(!(x)){std::fprintf(stderr,"FAIL line %d: %s\n",__LINE__,#x);return 1;}}while(0)
int main(){
    uint8_t digest[20];ExCryptSha(reinterpret_cast<const uint8_t*>("abc"),3,digest);
    const uint8_t sha[]{0xa9,0x99,0x3e,0x36,0x47,0x06,0x81,0x6a,0xba,0x3e,0x25,0x71,0x78,0x50,0xc2,0x6c,0x9c,0xd0,0xd8,0x9d};
    CHECK(!std::memcmp(digest,sha,20));
    for(unsigned console=1;console<=2;++console){
        mode=console==1?guitar::UsbMode::XInput:guitar::UsbMode::RockBand;
        guitar::auth::reset();CHECK(!setup(0xc1,0x83,46));CHECK(!setup(0xc1,0x81,29,0));
        CHECK(setup(0xc1,0x81,29));CHECK(reply.size()==29 && reply[0]==0x49 && reply[1]==0x4b);
        CHECK(reply[20]==(console==1?0x30:0xad) && reply[21]==(console==1?0x14:0x1b));
        CHECK(reply[22]==(console==1?0x48:2) && reply[23]==(console==1?0x47:0));
        uint8_t check=0;for(unsigned i=5;i<28;++i)check^=reply[i];CHECK(check==reply[28]);
        uint8_t init[34];fixture_init(init,console);
        CHECK(!write(0x82,init,33));
        init[33]^=1;CHECK(!write(0x82,init,34));init[33]^=1;
        CHECK(write(0x82,init,34));CHECK(setup(0xc1,0x86,2));CHECK(reply==std::vector<uint8_t>({1,0}));
        CHECK(setup(0xc1,0x83,46));CHECK(reply.empty()); // deferred until crypto task
        guitar::auth::task();CHECK(reply.size()==46 && fixture_check_init(reply.data()));
        for(unsigned round=1;round<=2;++round){
            uint8_t verify[22];fixture_verify(verify,round);CHECK(write(0x87,verify,22));
            guitar::auth::task();CHECK(setup(0xc1,0x83,22));CHECK(reply.size()==22 && fixture_check_verify(reply.data()));
        }
        CHECK(setup(0x41,0x84,0));CHECK(setup(0xc1,0x86,2));CHECK(reply==std::vector<uint8_t>({2,0}));
        CHECK(setup(0x41,0x87,22));guitar::auth::reset();CHECK(setup(0xc1,0x86,2));CHECK(reply==std::vector<uint8_t>({0,0}));
    }
    // A valid checksum does not excuse a bad MAC.
    CHECK(setup(0xc1,0x81,29));uint8_t bad[34];fixture_init(bad,3);bad[29]^=1;checksum(bad,34);
    CHECK(write(0x82,bad,34));CHECK(setup(0xc1,0x83,46));guitar::auth::task();CHECK(stalled==2);
    CHECK(setup(0xc1,0x86,2));CHECK(reply==std::vector<uint8_t>({0,0}));CHECK(!setup(0xc1,0x83,46));
    // New USB SETUP abandons a deferred response safely.
    CHECK(setup(0xc1,0x81,29));fixture_init(bad,4);CHECK(write(0x82,bad,34));CHECK(setup(0xc1,0x83,46));
    tud_event_hook_cb(0,6,true);guitar::auth::task();CHECK(reply.empty());CHECK(setup(0xc1,0x83,46));CHECK(fixture_check_init(reply.data()));
    // Verify MAC failure, then reset while a challenge is queued.
    uint8_t verify[22];fixture_verify(verify,3);verify[13]^=1;checksum(verify,22);CHECK(write(0x87,verify,22));
    guitar::auth::task();CHECK(!setup(0xc1,0x83,22));
    CHECK(setup(0xc1,0x81,29));fixture_init(bad,5);CHECK(write(0x82,bad,34));guitar::auth::reset();guitar::auth::task();CHECK(!setup(0xc1,0x83,46));
    std::puts("XSM3 native tests passed: SHA vector, two consoles/profiles, challenges/MAC/ACR, deferred EP0, malformed packets and resets.");
    return 0;
}
