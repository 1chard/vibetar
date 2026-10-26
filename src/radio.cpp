#include "radio.hpp"
#include "radio_probe.hpp"
#include "pico/cyw43_arch.h"
#include "hardware/structs/padsbank0.h"
#include "hardware/structs/sio.h"
#include "cyw43_ll.h"
namespace guitar::radio {
namespace {
Probe probe;
struct Operations {
    // All pins used by the Pico W driver are onboard-only on both RP2040 boards.
    static constexpr unsigned pins[]{23,24,25,29};
    gpio_function_t functions[4];uint32_t pads[4];
    uint32_t output=0,direction=0;
    bool init(){
        output=sio_hw->gpio_out;direction=sio_hw->gpio_oe;
        for(unsigned i=0;i<4;++i){functions[i]=gpio_get_function(pins[i]);pads[i]=padsbank0_hw->io[pins[i]];}
        return cyw43_arch_init()==0;
    }
    bool query(){
        // arch_init only creates software state. This transaction actually starts
        // the chip and verifies its SPI test pattern, firmware and GPIO response.
        bool vbus=false;
        cyw43_thread_enter();
        const bool ready=cyw43_gpio_get(&cyw43_state,CYW43_WL_GPIO_VBUS_PIN,&vbus)==0;
        if(!ready && cyw43_poll==nullptr){
            // SDK cyw43_deinit returns early if bring-up failed, leaving PIO/DMA
            // claimed. Release that partial bus before destroying its context.
            cyw43_ll_deinit(&cyw43_state.cyw43_ll);
        }
        cyw43_thread_exit();
        return ready;
    }
    void shutdown(){cyw43_arch_deinit();}
    void restore(){
        for(unsigned i=0;i<4;++i){
            const unsigned pin=pins[i];
            gpio_put(pin,(output>>pin)&1u);gpio_set_dir(pin,(direction>>pin)&1u);
            gpio_set_function(pin,functions[i]);padsbank0_hw->io[pin]=pads[i];
        }
    }
};
}
void detect(){Operations operations{};probe.detect(operations);}
bool available(){return probe.available();}
bool ensure_ready(){detect();return available();}
}
