#pragma once
#include <array>
#include <cstdint>
namespace guitar::bluetooth {
constexpr uint8_t feature_size(uint8_t id){
    constexpr uint8_t sizes[]{0,0,24,24,16,20,30,24,24,16,16,20,8,8,16,16,16,16,16,16,20,8,8,24,8,8};
    if(id==33)return 24;
    return id<sizeof(sizes)?sizes[id]:0;
}
constexpr bool writable_feature(uint8_t id){
    return id==2 || id==4 || id==7 || id==9 || id==12 || id==15 || id==18 || id==21 || id==24;
}
// ATT prepare writes can split a feature report at arbitrary offsets.
// One connection / one prepared report; never publish an incomplete payload.
struct FeatureTransfer {
    std::array<uint8_t,30> data{};
    uint32_t received=0;
    uint8_t id=0;
    constexpr void clear(){id=0;received=0;}
    constexpr bool append(uint8_t report,uint16_t offset,const uint8_t* bytes,uint16_t size,bool allow_readonly=false){
        if(!feature_size(report) || (!allow_readonly && !writable_feature(report)) || (id && id!=report) || !size || offset+size>feature_size(report))return false;
        id=report;
        for(unsigned i=0;i<size;++i){data[offset+i]=bytes[i];received|=uint32_t(1)<<(offset+i);}
        return true;
    }
    constexpr bool complete()const{return id && received==((uint32_t(1)<<feature_size(id))-1);}
};
}
