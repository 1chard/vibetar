#pragma once
#include "btstack.h"
#include <array>
#include <cstring>
namespace guitar::bluetooth {
// Caller holds the CYW43 lock. Addresses identify the actual HCI peer.
inline std::array<uint8_t,24> make_link_report(hci_con_handle_t handle,uint8_t flags,uint8_t role){
    std::array<uint8_t,24> p{'X','C','L','P',1,flags,255,0};
    bd_addr_t local;gap_local_bd_addr(local);std::memcpy(p.data()+8,local,6);
    auto* peer=hci_connection_for_handle(handle);
    if(peer){p[6]=static_cast<uint8_t>(peer->address_type);std::memcpy(p.data()+14,peer->address,6);}
    else if(handle!=HCI_CON_HANDLE_INVALID)p[5]|=8; // stale application handle
    p[20]=static_cast<uint8_t>(handle);p[21]=static_cast<uint8_t>(handle>>8);p[22]=role;
    return p;
}
}
