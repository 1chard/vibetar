// SPDX-License-Identifier: LGPL-2.1-or-later
// Adapter for pinned libxsm3 a085709; upstream sources/license stay in the submodule.
#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include "xsm3_port.h"
#include "pico/rand.h"
static bool validation_failed;
static uint32_t random_word;
static unsigned random_remaining;
static int random_byte(void){
    if(!random_remaining){random_word=get_rand_32();random_remaining=4;}
    int value=random_word&255;random_word>>=8;--random_remaining;return value;
}
static int validation_error(const char* message,...){(void)message;validation_failed=true;return 0;}
// Include after standard headers: replace only the library's random/log calls.
// Its four diagnostic calls signal bad checksum or bad MAC; do not print keys.
#define printf validation_error
#define srand(seed) ((void)0)
#define rand() random_byte()
#include "../third_party/libxsm3/xsm3.c"
#undef printf
#undef srand
#undef rand
void guitar_xsm3_reset(void){
    xsm3_initialise_state();
    // Upstream clears key bytes but forgets this flag: the next session must derive keys again.
    xsm3_has_kv_keys=false;
    validation_failed=false;random_remaining=0;
}
void guitar_xsm3_identify(const uint8_t identity[29]){guitar_xsm3_reset();xsm3_set_identification_data(identity);}
bool guitar_xsm3_init(uint8_t packet[34]){
    validation_failed=false;random_remaining=0;xsm3_do_challenge_init(packet);
    if(validation_failed){guitar_xsm3_reset();return false;}return true;
}
bool guitar_xsm3_verify(uint8_t packet[22]){
    validation_failed=false;xsm3_do_challenge_verify(packet);
    if(validation_failed){guitar_xsm3_reset();return false;}return true;
}
