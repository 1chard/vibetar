// SDK 2.2.0 maps most ATT failures to HCI 0x1f, losing the useful cause.
// Include the pinned SDK implementation; keep its client storage ABI unchanged.
#include "btstack.h"
static uint8_t receiver_att_status(uint8_t status,uint8_t state);
extern void vibetar_receiver_att_diagnostic(uint8_t status,uint8_t stage);
#define gatt_client_att_status_to_error_code(status) receiver_att_status(status,client->state)
#include "ble/gatt-service/hids_client.c"
#undef gatt_client_att_status_to_error_code
static uint8_t receiver_att_status(uint8_t status,uint8_t state){
    uint8_t stage=0;
    switch(state){
    case HIDS_CLIENT_STATE_W4_SERVICE_RESULT:stage=1;break;
    case HIDS_CLIENT_STATE_W4_CHARACTERISTIC_RESULT:stage=2;break;
    case HIDS_CLIENT_STATE_W4_REPORT_MAP_HID_DESCRIPTOR:stage=3;break;
    case HIDS_CLIENT_STATE_W4_REPORT_MAP_CHARACTERISTIC_DESCRIPTORS_RESULT:stage=4;break;
    case HIDS_CLIENT_STATE_W4_REPORT_FOUND:stage=5;break;
    case HIDS_CLIENT_STATE_W4_REPORT_ID_AND_TYPE:stage=6;break;
    case HIDS_CLIENT_STATE_W4_INPUT_REPORTS_ENABLED:stage=7;break;
    case HIDS_CLIENT_W4_WRITE_REPORT_DONE:stage=8;break;
    default:break;
    }
    vibetar_receiver_att_diagnostic(status,stage);
    return gatt_client_att_status_to_error_code(status);
}
