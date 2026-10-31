#include "keyboard.hpp"
#include "settings_record.hpp"
using namespace guitar;
constexpr bool keyboard_test(){
    KeyboardConfig c{};KeyboardState encoder{};State s{};
    s.buttons=31|bit(Button::StrumUp)|bit(Button::StrumDown)|bit(Button::Tilt);
    s.whammy=10000;const auto p=encoder.report(s,c);
    if(p[0]!=0x30 || !(p[2+40/8]&(1<<(40%8))) || !(p[2+78/8]&(1<<(78%8))))return false;
    for(unsigned key=30;key<=34;++key)if(!(p[2+key/8]&(1<<(key%8))))return false;
    s.whammy=7000;if(!(encoder.report(s,c)[0]&0x10))return false;
    s.whammy=6000;if(encoder.report(s,c)[0]&0x10)return false;
    const auto released=encoder.report({},c);for(auto b:released)if(b)return false;
    c.keys[0]=c.keys[1]=4;s={};s.buttons=3;
    if(encoder.report(s,c)[2]!=16)return false;
    s.buttons=2;if(encoder.report(s,c)[2]!=16)return false;
    s={};s.solo_frets=1;if(encoder.report(s,c)[2]!=16)return false;
    RuntimeConfig config{},decoded{};config.keyboard=c;
    auto record=settings::make_record(config,1);
    if(!settings::parse_record(record,decoded)||!same_config(config,decoded))return false;
    record[123]^=1;if(settings::parse_record(record,decoded))return false;
    record=settings::make_record(config,1);settings::write_u32(record,0,0x37434658);settings::write_u32(record,120,settings::crc32(record,120));
    if(!settings::parse_record(record,decoded)||!same_keyboard(decoded.keyboard,KeyboardConfig{}))return false;
    auto bad=encode_keyboard(c);bad[2]=1;return !decode_keyboard(bad,c);
}
static_assert(keyboard_test(),"NKRO chords, modifiers, whammy hysteresis, release, solo, duplicate binds and XFC7 migration");
