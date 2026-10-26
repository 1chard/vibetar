#pragma once
#include "controller.hpp"
#include <array>
namespace guitar {
using PreviewPayload=std::array<uint8_t,24>;
constexpr PreviewPayload encode_preview(State state,UsbMode mode,bool receiver,bool fresh,uint32_t now){
    if(!fresh)state={};
    return {'X','C','S','T',1,static_cast<uint8_t>(receiver),static_cast<uint8_t>(fresh),static_cast<uint8_t>(mode),
        static_cast<uint8_t>(state.buttons),static_cast<uint8_t>(state.buttons>>8),
        static_cast<uint8_t>((state.left?1:0)|(state.right?2:0)),state.solo_frets,
        static_cast<uint8_t>(state.whammy),static_cast<uint8_t>(state.whammy>>8),state.pickup,0,
        static_cast<uint8_t>(now),static_cast<uint8_t>(now>>8),static_cast<uint8_t>(now>>16),static_cast<uint8_t>(now>>24)};
}
}
