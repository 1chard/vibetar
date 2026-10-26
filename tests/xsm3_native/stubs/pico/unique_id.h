#pragma once
#include <stddef.h>
#define PICO_UNIQUE_BOARD_ID_SIZE_BYTES 8
#ifdef __cplusplus
extern "C" {
#endif
void pico_get_unique_board_id_string(char*,size_t);
#ifdef __cplusplus
}
#endif
