#pragma once
#include "controller.hpp"
namespace guitar {
struct KeyboardConfig {
    // Eleven base buttons, left, right, whammy. Zero disables an input.
    std::array<uint8_t,14> keys={30,31,32,33,34,229,40,41,42,78,0,80,79,228};
    uint16_t threshold=8192,hysteresis=2048;
};
using KeyboardPayload=std::array<uint8_t,24>;
constexpr bool valid_keyboard(const KeyboardConfig& c) {
    for(auto key:c.keys)if(key && (key<4 || key>231))return false;
    return c.threshold>0 && c.hysteresis<c.threshold;
}
constexpr KeyboardPayload encode_keyboard(const KeyboardConfig& c,uint8_t status=0) {
    KeyboardPayload p{1,status};for(size_t i=0;i<14;++i)p[2+i]=c.keys[i];
    p[16]=c.threshold;p[17]=c.threshold>>8;p[18]=c.hysteresis;p[19]=c.hysteresis>>8;return p;
}
constexpr bool decode_keyboard(const KeyboardPayload& p,KeyboardConfig& c) {
    if(p[0]!=1 || p[1])return false;
    for(size_t i=20;i<24;++i)if(p[i])return false;
    KeyboardConfig next{};for(size_t i=0;i<14;++i)next.keys[i]=p[2+i];
    next.threshold=p[16]|uint16_t(p[17])<<8;next.hysteresis=p[18]|uint16_t(p[19])<<8;
    if(!valid_keyboard(next))return false;
    c=next;return true;
}
constexpr bool same_keyboard(const KeyboardConfig& a,const KeyboardConfig& b) {
    const auto x=encode_keyboard(a),y=encode_keyboard(b);
    for(size_t i=0;i<24;++i)if(x[i]!=y[i])return false;
    return true;
}
struct KeyboardState {
    bool whammy=false;
    constexpr std::array<uint8_t,32> report(const State& s,const KeyboardConfig& c) {
        whammy=s.whammy >= (whammy ? c.threshold-c.hysteresis : c.threshold);
        std::array<uint8_t,32> p{};
        for(size_t i=0;i<14;++i) {
            const bool down=i<11 ? ((s.buttons | (s.solo_frets&31))&(1u<<i))!=0 : i==11?s.left:i==12?s.right:whammy;
            const auto key=c.keys[i];if(!down || !key)continue;
            if(key>=224)p[0]|=1u<<(key-224);else p[2+key/8]|=1u<<(key%8);
        }
        return p;
    }
};
}
