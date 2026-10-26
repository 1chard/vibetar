#pragma once
#include <stdbool.h>
#include <stdint.h>
#ifdef __cplusplus
extern "C" {
#endif
void guitar_xsm3_reset(void);
void guitar_xsm3_identify(const uint8_t identity[29]);
bool guitar_xsm3_init(uint8_t packet[34]);
bool guitar_xsm3_verify(uint8_t packet[22]);
extern uint8_t xsm3_challenge_response[48];
extern const uint8_t xsm3_id_data_ms_controller[29];
#ifdef __cplusplus
}
#endif
