#pragma once
#include <stdint.h>
#include <stdbool.h>
#ifdef __cplusplus
extern "C" {
#endif
#define CONTROL_STAGE_SETUP 0
#define CONTROL_STAGE_DATA 1
#define CONTROL_STAGE_ACK 2
struct tusb_control_request_t {uint8_t bmRequestType,bRequest;uint16_t wValue,wIndex,wLength;};
bool tud_control_xfer(uint8_t,const struct tusb_control_request_t*,void*,uint16_t);
bool tud_control_status(uint8_t,const struct tusb_control_request_t*);
#ifdef __cplusplus
}
#endif
