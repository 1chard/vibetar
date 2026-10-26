// Fixture constructs valid console packets independently from session/EP0 code.
#include "../../src/xsm3_port.c"
static uint8_t console_random[16],console_id[8],controller_random[16],key1[16],key2[16],salt[16],hash[20];
static void checksum(uint8_t* p,unsigned len){p[len-1]=0;for(unsigned i=5;i<len-1;++i)p[len-1]^=p[i];}
void fixture_init(uint8_t packet[34],unsigned console){
    uint8_t plain[24],mac[8],digest[20];
    for(unsigned i=0;i<16;++i)console_random[i]=(uint8_t)(i+console);
    for(unsigned i=0;i<8;++i)console_id[i]=(uint8_t)(console*17+i);
    memcpy(plain,console_random,16);memcpy(plain+16,console_id,8);
    memset(packet,0,34);packet[0]=9;packet[1]=0x40;packet[4]=28;
    UsbdSecXSM3AuthenticationCrypt(xsm3_key_0x1D,plain,24,packet+5,1);
    UsbdSecXSM3AuthenticationMac(xsm3_key_0x1E,NULL,packet+5,24,mac);
    memcpy(packet+29,mac+4,4);checksum(packet,34);
    ExCryptSha(console_id,8,digest);
    uint8_t kv1[16],kv2[16],swapped[16];
    UsbdSecXSM3AuthenticationCrypt(xsm3_root_key_0x23,digest,16,kv1,1);
    UsbdSecXSM3AuthenticationCrypt(xsm3_root_key_0x24,digest+4,16,kv2,1);
    memcpy(swapped,console_random+8,8);memcpy(swapped+8,console_random,8);
    UsbdSecXSM3AuthenticationCrypt(kv1,console_random,16,key1,1);
    UsbdSecXSM3AuthenticationCrypt(kv2,swapped,16,key2,1);
}
bool fixture_check_init(const uint8_t response[46]){
    uint8_t plain[32],mac[8],acr[8];
    if(response[0]!=0x49 || response[1]!=0x4c || response[4]!=40)return false;
    UsbdSecXSM3AuthenticationCrypt(key1,response+5,32,plain,0);
    if(memcmp(plain+16,console_random,16))return false;
    memcpy(controller_random,plain,16);ExCryptSha(plain,32,hash);
    UsbdSecXSM3AuthenticationMac(key2,NULL,(uint8_t*)response+5,32,mac);
    UsbdSecXSMAuthenticationAcr(console_id,xsm3_identification_data,mac,acr);
    if(memcmp(acr,response+37,8))return false;
    memcpy(salt,controller_random+12,4);memcpy(salt+4,console_random+12,4);
    return true;
}
void fixture_verify(uint8_t packet[22],unsigned round){
    uint8_t plain[8],mac[8];for(unsigned i=0;i<8;++i)plain[i]=(uint8_t)(round+i);
    memcpy(salt+8,plain,8);memset(packet,0,22);packet[0]=9;packet[1]=0x41;packet[4]=16;
    UsbdSecXSM3AuthenticationCrypt(controller_random,plain,8,packet+5,1);
    UsbdSecXSM3AuthenticationMac(hash,salt,packet+5,8,mac);
    memcpy(packet+13,mac,8);checksum(packet,22);
}
bool fixture_check_verify(const uint8_t response[22]){
    uint8_t mac[8],acr[8],plain[8];
    if(response[0]!=0x49 || response[1]!=0x4c || response[4]!=16)return false;
    UsbdSecXSM3AuthenticationMac(key2,salt,(uint8_t*)response+5,8,mac);
    if(memcmp(mac,response+13,8))return false;
    UsbdSecXSM3AuthenticationCrypt(key1,response+5,8,plain,0);
    UsbdSecXSMAuthenticationAcr(console_id,xsm3_identification_data,salt+8,acr);
    return !memcmp(acr,plain,8);
}
