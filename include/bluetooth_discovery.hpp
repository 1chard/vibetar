#pragma once
#include <array>
#include <cstdint>
#include <algorithm>
namespace guitar::bluetooth {
using DiscoveryPayload=std::array<uint8_t,32>;
using ConnectPayload=std::array<uint8_t,16>;
struct Candidate {
    std::array<uint8_t,6> address{};
    uint32_t seen=0;
    uint8_t type=0;
    int8_t rssi=0;
    bool valid=false;
};
struct Discovery {
    std::array<Candidate,3> candidates{};
    constexpr void clear(){candidates={};}
    constexpr void observe(const uint8_t* address,uint8_t type,int8_t rssi,uint32_t now){
        if(type>1)return;
        size_t target=0;
        for(size_t i=0;i<3;++i){
            const auto& c=candidates[i];bool same=c.valid && c.type==type;
            for(size_t j=0;j<6;++j)if(c.address[j]!=address[j])same=false;
            if(same){target=i;break;}
            if(!c.valid){target=i;break;}
            if(uint32_t(now-c.seen)>uint32_t(now-candidates[target].seen))target=i;
        }
        auto& c=candidates[target];c.valid=true;c.type=type;c.rssi=rssi;c.seen=now;
        for(size_t i=0;i<6;++i)c.address[i]=address[i];
    }
    constexpr bool contains(const uint8_t* address,uint8_t type,uint32_t now)const{
        for(const auto& c:candidates){
            bool same=c.valid && c.type==type && uint32_t(now-c.seen)<15000;
            for(size_t i=0;i<6;++i)if(c.address[i]!=address[i])same=false;
            if(same)return true;
        }
        return false;
    }
    constexpr DiscoveryPayload report(uint32_t now,uint8_t flags,uint8_t gatt_status)const{
        DiscoveryPayload p{'X','C','D','V',1,0,gatt_status,flags};
        for(const auto& c:candidates)if(c.valid && uint32_t(now-c.seen)<15000){
            const auto offset=8+8*p[5]++;
            for(size_t j=0;j<6;++j)p[offset+j]=c.address[j];
            p[offset+6]=c.type;p[offset+7]=static_cast<uint8_t>(c.rssi);
        }
        return p;
    }
};
constexpr bool valid_connect(const uint8_t* p,uint16_t size){
    return size==16 && p[0]=='X' && p[1]=='C' && p[2]=='C' && p[3]=='N' && p[4]==1 &&
        p[5]==0 && p[6]<=1 && p[7]==0 && p[14]==0 && p[15]==0;
}
DiscoveryPayload discovery_report();
ConnectPayload connect_report();
ConnectPayload health_report();
void submit_connect(const uint8_t*,uint16_t);
}
