#pragma once
#include <array>
#include <cstdint>
#ifndef XCONTROLLER_GIT_COMMIT
#define XCONTROLLER_GIT_COMMIT "unknown"
#define XCONTROLLER_GIT_DIRTY 0
#endif
namespace guitar {
constexpr std::array<uint8_t,24> firmware_identity(bool receiver,bool wireless) {
    std::array<uint8_t,24> p{'X','C','F','W',1,0,1,0,static_cast<uint8_t>(receiver),static_cast<uint8_t>(wireless?3:2),XCONTROLLER_GIT_DIRTY,0};
    constexpr char commit[]=XCONTROLLER_GIT_COMMIT;
    for(unsigned i=0;i<12 && i<sizeof(commit)-1;++i)p[12+i]=commit[i];
    return p;
}
static_assert(firmware_identity(false,false)[9]==2);
}
