#pragma once
namespace guitar::radio {
// Keep absent hardware out of the main loop: no repeated initialization attempts.
class Probe {
public:
    constexpr bool attempted()const{return attempted_;}
    constexpr bool available()const{return available_;}
    template<class Operations> constexpr void detect(Operations& operations){
        if(attempted_)return;
        attempted_=true;
        if(operations.init()){
            available_=operations.query();
            if(!available_)operations.shutdown();
        }
        if(!available_)operations.restore();
    }
private:
    bool attempted_=false,available_=false;
};
}
