#include "outputs.hpp"
#include "battery.hpp"
#include "radio.hpp"
#include "pico/cyw43_arch.h"
#include "hardware/gpio.h"
#include "hardware/pwm.h"
#include "pico/time.h"
namespace guitar::outputs {
namespace {
OutputsConfig active{}; OutputsPayload live{'X','C','L','S',2,3};
bluetooth::Phase previous_phase=bluetooth::Phase::Idle;
uint32_t phase_at=0,last_update=0;
bool uses_bluetooth=false;
bool onboard_known=false,onboard_level=false;
void drive(const OutputRule& r,uint8_t brightness){
    if(r.pin==onboard_led_pin && radio::available()){
        const bool level=brightness!=0;
        if(!onboard_known || level!=onboard_level){
            cyw43_arch_gpio_put(CYW43_WL_GPIO_LED_PIN,level);
            onboard_known=true;onboard_level=level;
        }
        return;
    }
    const auto pin=output_gpio(r.pin);
    if((output_events(r.event)&44))pwm_set_gpio_level(pin,r.active_low?255-brightness:brightness);
    else gpio_put(pin,bool(brightness)!=r.active_low);
}
}
void release() {
    for(const auto& r:active.rules) if(r.pin!=255) {
        if(r.pin==onboard_led_pin && radio::available()){drive(r,0);continue;}
        const auto pin=output_gpio(r.pin);
        if((output_events(r.event)&44)){drive(r,0);gpio_set_function(pin,GPIO_FUNC_SIO);pwm_set_enabled(pwm_gpio_to_slice_num(pin),false);}
        gpio_put(pin,r.active_low); // inactive before relinquishing the pin
        gpio_set_dir(pin,GPIO_IN);gpio_disable_pulls(pin);gpio_deinit(pin);
    }
    active={};uses_bluetooth=false;live={'X','C','L','S',2,3};
}
void init(const OutputsConfig& config,UsbMode mode) {
    active=config;
    previous_phase=bluetooth::connection_phase();phase_at=to_ms_since_boot(get_absolute_time());
    last_update=phase_at-1;
    uses_bluetooth=false;
    for(const auto& r:active.rules) if(r.pin!=255) {
        if(output_events(r.event)&60)uses_bluetooth=true;
        if(r.pin==onboard_led_pin && radio::available()){drive(r,0);continue;}
        const auto pin=output_gpio(r.pin);
        gpio_init(pin);gpio_disable_pulls(pin);
        gpio_put(pin,r.active_low); // preload inactive latch before enabling output
        gpio_set_dir(pin,GPIO_OUT);
        if((output_events(r.event)&44)){
            const auto slice=pwm_gpio_to_slice_num(pin);
            pwm_set_wrap(slice,254);pwm_set_clkdiv(slice,125.0f);
            drive(r,0);gpio_set_function(pin,GPIO_FUNC_PWM);pwm_set_enabled(slice,true);
        }
    }
    update(mode);
}
void update(UsbMode mode) {
    const uint32_t now=to_ms_since_boot(get_absolute_time());
    if(now==last_update)return;
    last_update=now;
    const auto phase=uses_bluetooth?bluetooth::connection_phase():bluetooth::Phase::Idle;
    if(phase!=previous_phase){previous_phase=phase;phase_at=now;}
    live={'X','C','L','S',2,3};live[9]=static_cast<uint8_t>(mode);
    for(size_t i=0;i<3;++i) {
        const auto& r=active.rules[i];live[10+i]=r.pin;
        if(r.pin==255) continue;
        const auto brightness=effective_brightness(r.pin,radio::available(),output_brightness(r,mode,battery::low(),phase,uint32_t(now-phase_at)));
        drive(r,brightness);live[13+i]=brightness;
        live[6]|=1u<<i;if(brightness)live[7]|=1u<<i;
        if(r.active_low?brightness<255:brightness>0)live[8]|=1u<<i;
    }
}
OutputsPayload live_report() { return live; }
}
