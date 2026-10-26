#include "bluetooth.hpp"
#include "bluetooth_link.hpp"
#include "bluetooth_status.hpp"
#include "receiver_protocol.hpp"
#include "receiver_health.hpp"
#include "radio.hpp"
#include "feature_protocol.hpp"
#include "ble_feature_transfer.hpp"
#include "pico/cyw43_arch.h"
#include "btstack.h"
#include "guitar_ble.h" // generated GATT database (do not shadow BTstack bluetooth.h)
#include <algorithm>
#include <cstring>
namespace guitar::bluetooth {
namespace {
BluetoothConfig active{};
bool raw_subscribed=false;
uint8_t mode_counter=0,report_queue=0;
uint16_t wireless_sequence=0;
wireless::Packet raw{};
wireless::Feedback feedback{};
UsbMode remote_mode=UsbMode::Hid;
uint32_t feedback_at=0;
bool feedback_valid=false;
bool initialized=false,working=false,advertising=false,encrypted=false,subscribed=false,
    suspended=false,paused=false,error=false,send_pending=false;
hci_con_handle_t connection=HCI_CON_HANDLE_INVALID;
uint32_t pair_until=0,last_send=0,connection_at=0;
bool parameters_checked=false;
uint16_t interval=0;
uint8_t pending_command=0,command_status=0,last_command=0;
HidReport latest{};
btstack_packet_callback_registration_t hci_registration{},sm_registration{};
hids_device_report_t report_storage[28]{};
std::array<std::array<uint8_t,30>,34> feature_cache{};
FeatureTransfer prepared{}, queued{};
std::array<uint8_t,30> read_snapshot{};
uint8_t read_id=0;
uint32_t stop_at=0,action_at=0;
uint32_t last_cache=0;
uint8_t deferred_command=0;
// 0xff means no result yet; expose exact failures without a UART console.
uint8_t last_connection_status=0xff,last_pairing_status=0xff,last_pairing_reason=0,last_disconnect_reason=0;
void clear_diagnostics(){
    last_connection_status=0xff;last_pairing_status=0xff;last_pairing_reason=0;last_disconnect_reason=0;error=false;
}

// Same 11-byte gamepad layout as USB; BLE uses Report Reference ID, not a prefix.
constexpr uint8_t report_map[]{
    0x05,0x01,0x09,0x05,0xa1,0x01,0x85,0x01,
    0x05,0x01,0x09,0x30,0x09,0x31,0x09,0x32,0x09,0x35,0x09,0x33,0x09,0x34,
    0x15,0x81,0x25,0x7f,0x95,0x06,0x75,0x08,0x81,0x02,
    0x09,0x39,0x15,0x01,0x25,0x08,0x35,0x00,0x46,0x3b,0x01,
    0x95,0x01,0x75,0x08,0x81,0x42, // null-state hat: zero is neutral
    0x05,0x09,0x19,0x01,0x29,0x20,0x15,0x00,0x25,0x01,
    0x95,0x20,0x75,0x01,0x81,0x02,0xc0
    ,0x06,0x00,0xff,0x09,0x01,0xa1,0x01,0x15,0x00,0x26,0xff,0x00,0x75,0x08,
    0x85,26,0x09,26,0x95,16,0x81,0x02,
    0x85,27,0x09,27,0x95,8,0x91,0x02,
    0x85,2,0x09,2,0x95,24,0xb1,0x02,
    0x85,3,0x09,3,0x95,24,0xb1,0x02,
    0x85,4,0x09,4,0x95,16,0xb1,0x02,
    0x85,5,0x09,5,0x95,20,0xb1,0x02,
    0x85,6,0x09,6,0x95,30,0xb1,0x02,
    0x85,7,0x09,7,0x95,24,0xb1,0x02,
    0x85,8,0x09,8,0x95,24,0xb1,0x02,
    0x85,9,0x09,9,0x95,16,0xb1,0x02,
    0x85,10,0x09,10,0x95,16,0xb1,0x02,
    0x85,11,0x09,11,0x95,20,0xb1,0x02,
    0x85,12,0x09,12,0x95,8,0xb1,0x02,
    0x85,13,0x09,13,0x95,8,0xb1,0x02,
    0x85,14,0x09,14,0x95,16,0xb1,0x02,
    0x85,15,0x09,15,0x95,16,0xb1,0x02,
    0x85,16,0x09,16,0x95,16,0xb1,0x02,
    0x85,17,0x09,17,0x95,16,0xb1,0x02,
    0x85,18,0x09,18,0x95,16,0xb1,0x02,
    0x85,19,0x09,19,0x95,16,0xb1,0x02,
    0x85,20,0x09,20,0x95,20,0xb1,0x02,
    0x85,21,0x09,21,0x95,8,0xb1,0x02,
    0x85,22,0x09,22,0x95,8,0xb1,0x02,
    0x85,23,0x09,23,0x95,24,0xb1,0x02,
    0x85,24,0x09,24,0x95,8,0xb1,0x02,
    0x85,25,0x09,25,0x95,8,0xb1,0x02,
    0x85,33,0x09,33,0x95,24,0xb1,0x02,
    0xc0
};
constexpr uint8_t advertisement[]{2,1,6,3,3,0x12,0x18,3,0x19,0xc4,0x03,8,0xff,0xff,0xff,'X','C','G','T',1};
constexpr uint8_t scan_response[]{19,9,'X','C','o','n','t','r','o','l','l','e','r',' ','G','u','i','t','a','r'};
uint32_t now_ms(){return to_ms_since_boot(get_absolute_time());}
bool pairing_open(){return pair_until && int32_t(pair_until-now_ms())>0;}
void advertise(){
    advertising=working && active.enabled && connection==HCI_CON_HANDLE_INVALID;
    if(working)gap_advertisements_enable(advertising);
}
void get_report(hci_con_handle_t,hid_report_type_t type,uint16_t id,uint16_t size,uint8_t* out){
    if(type==HID_REPORT_TYPE_INPUT && id==1)std::memcpy(out,&latest,std::min<size_t>(size,sizeof(latest)));
    if(type==HID_REPORT_TYPE_INPUT && id==26)std::memcpy(out,raw.data(),std::min<size_t>(size,raw.size()));
    if(type==HID_REPORT_TYPE_OUTPUT && id==27)std::memcpy(out,feedback.data(),std::min<size_t>(size,feedback.size()));
}
void packet_handler(uint8_t type,uint16_t,uint8_t* packet,uint16_t){
    if(type!=HCI_EVENT_PACKET)return;
    switch(hci_event_packet_get_type(packet)){
    case BTSTACK_EVENT_STATE:
        working=btstack_event_state_get_state(packet)==HCI_STATE_WORKING;
        if(working){
            bd_addr_t zero{};
            gap_advertisements_set_params(0x30,0x60,0,0,zero,7,0);
            gap_advertisements_set_data(sizeof(advertisement),const_cast<uint8_t*>(advertisement));
            gap_scan_response_set_data(sizeof(scan_response),const_cast<uint8_t*>(scan_response));
            advertise();
        }
        break;
    case HCI_EVENT_META_GAP:
        if(hci_event_gap_meta_get_subevent_code(packet)==GAP_SUBEVENT_LE_CONNECTION_COMPLETE){
            last_connection_status=gap_subevent_le_connection_complete_get_status(packet);
            if(last_connection_status){error=true;advertise();break;}
            connection=gap_subevent_le_connection_complete_get_connection_handle(packet);
            interval=gap_subevent_le_connection_complete_get_conn_interval(packet);
            advertising=false;encrypted=false;subscribed=false;raw_subscribed=false;suspended=false;send_pending=false;report_queue=0;feedback_valid=false;
            if(!active.enabled){gap_disconnect(connection);break;}
            connection_at=now_ms();parameters_checked=false;
        }
        break;
    case HCI_EVENT_LE_META:
        if(hci_event_le_meta_get_subevent_code(packet)==HCI_SUBEVENT_LE_CONNECTION_UPDATE_COMPLETE &&
           hci_subevent_le_connection_update_complete_get_status(packet)==0)
            interval=hci_subevent_le_connection_update_complete_get_conn_interval(packet);
        break;
    case HCI_EVENT_ENCRYPTION_CHANGE:
        if(hci_event_encryption_change_get_connection_handle(packet)==connection)
            encrypted=hci_event_encryption_change_get_status(packet)==0 && hci_event_encryption_change_get_encryption_enabled(packet)!=0;
        if(encrypted)error=false;
        break;
    case HCI_EVENT_ENCRYPTION_CHANGE_V2:
        if(hci_event_encryption_change_v2_get_connection_handle(packet)==connection)
            encrypted=hci_event_encryption_change_v2_get_status(packet)==0 && hci_event_encryption_change_v2_get_encryption_enabled(packet)!=0;
        if(encrypted)error=false;
        break;
    case SM_EVENT_JUST_WORKS_REQUEST:
        if(active.enabled && pairing_open())sm_just_works_confirm(sm_event_just_works_request_get_handle(packet));
        else {
            const auto handle=sm_event_just_works_request_get_handle(packet);
            sm_bonding_decline(handle);gap_disconnect(handle);
        }
        break;
    case SM_EVENT_PAIRING_COMPLETE:
        last_pairing_status=sm_event_pairing_complete_get_status(packet);
        last_pairing_reason=sm_event_pairing_complete_get_reason(packet);
        if(!last_pairing_status){pair_until=0;error=false;}
        else {error=true;gap_disconnect(sm_event_pairing_complete_get_handle(packet));}
        break;
    case SM_EVENT_REENCRYPTION_COMPLETE:
        last_pairing_status=sm_event_reencryption_complete_get_status(packet);
        last_pairing_reason=0;
        if(last_pairing_status){error=true;gap_disconnect(sm_event_reencryption_complete_get_handle(packet));}
        break;
    case HCI_EVENT_DISCONNECTION_COMPLETE:
        if(hci_event_disconnection_complete_get_connection_handle(packet)==connection){
            last_disconnect_reason=hci_event_disconnection_complete_get_reason(packet);
            prepared.clear();read_id=0;
            connection=HCI_CON_HANDLE_INVALID;interval=0;encrypted=false;subscribed=false;raw_subscribed=false;send_pending=false;report_queue=0;feedback_valid=false;suspended=false;advertise();
        }
        break;
    case HCI_EVENT_HIDS_META:
        switch(hci_event_hids_meta_get_subevent_code(packet)){
        case HIDS_SUBEVENT_INPUT_REPORT_ENABLE:
            if(hids_subevent_input_report_enable_get_report_id(packet)==1)
                subscribed=hids_subevent_input_report_enable_get_enable(packet)!=0;
            if(hids_subevent_input_report_enable_get_report_id(packet)==26)
                raw_subscribed=hids_subevent_input_report_enable_get_enable(packet)!=0;
            if(!subscribed)report_queue&=~1;
            if(!raw_subscribed)report_queue&=~2;
            break;
        case HIDS_SUBEVENT_SET_REPORT:
            if(encrypted && hids_subevent_set_report_get_report_id(packet)==27 &&
               hids_subevent_set_report_get_report_type(packet)==HID_REPORT_TYPE_OUTPUT &&
               wireless::decode_feedback(hids_subevent_set_report_get_report_data(packet),hids_subevent_set_report_get_report_length(packet),remote_mode)){
                std::memcpy(feedback.data(),hids_subevent_set_report_get_report_data(packet),feedback.size());
                feedback_at=now_ms();feedback_valid=true;
            }
            break;
        case HIDS_SUBEVENT_SUSPEND:suspended=true;break;
        case HIDS_SUBEVENT_EXIT_SUSPEND:suspended=false;break;
        case HIDS_SUBEVENT_CAN_SEND_NOW:
            send_pending=false;
            if(active.enabled && encrypted && !suspended && hids_subevent_can_send_now_get_con_handle(packet)==connection){
                if((report_queue&2) && hids_device_send_input_report_for_id(connection,26,raw.data(),raw.size())==ERROR_CODE_SUCCESS)report_queue&=~2;
                else if((report_queue&1) && hids_device_send_input_report_for_id(connection,1,reinterpret_cast<const uint8_t*>(&latest),sizeof(latest))==ERROR_CODE_SUCCESS)report_queue&=~1;
            }
            break;
        default:break;
        }
        break;
    default:break;
    }
}
bool start(){
    if(initialized)return true;
    if(!radio::ensure_ready())return false;
    cyw43_thread_enter();
    l2cap_init();sm_init();
    sm_set_io_capabilities(IO_CAPABILITY_NO_INPUT_NO_OUTPUT);
    sm_set_authentication_requirements(SM_AUTHREQ_SECURE_CONNECTION | SM_AUTHREQ_BONDING);
    sm_set_encryption_key_size_range(16,16);
    att_server_init(profile_data,nullptr,nullptr);
    device_information_service_server_init();
    device_information_service_server_set_manufacturer_name("Vibetar DIY");
    device_information_service_server_set_model_number("Pico W Guitar");
    device_information_service_server_set_pnp_id(2,0xcafe,0x4005,0x020d);
    hids_device_init_with_storage(0,report_map,sizeof(report_map),28,report_storage);
    hids_device_register_packet_handler(packet_handler);
    hids_device_register_get_report_callback(get_report);
    hci_registration.callback=packet_handler;hci_add_event_handler(&hci_registration);
    sm_registration.callback=packet_handler;sm_add_event_handler(&sm_registration);
    initialized=true;
    hci_power_control(HCI_POWER_ON);
    cyw43_thread_exit();return true;
}
}
void configure(const BluetoothConfig& config,uint32_t now){
    // Saved BLE settings may come from a Pico W. Never touch a missing radio.
    if(!radio::available()){active={};return;}
    const bool was_enabled=active.enabled;
    // Init must precede the lock; CYW43 creates the context being locked.
    if(config.enabled && !start()){active=config;error=true;return;}
    if(!initialized){active=config;return;}
    cyw43_thread_enter();active=config;latest={};
    if(active.enabled && !was_enabled){pair_until=now+120000;clear_diagnostics();}
    if(active.enabled)stop_at=0;
    else {pair_until=0;if(connection!=HCI_CON_HANDLE_INVALID && !stop_at)stop_at=now+3000;}
    advertise();cyw43_thread_exit();
}
void task(uint32_t,const State& state,bool usb_mounted){
    if(!initialized)return;
    // Only the main loop touches settings/USB state. IRQ callbacks copy this cache.
    uint32_t now=now_ms();
    FeatureTransfer submission{};
    cyw43_thread_enter();submission=queued;queued.clear();cyw43_thread_exit();
    if(submission.id)submit_feature_report(submission.id,submission.data.data(),feature_size(submission.id));
    if(submission.id || uint32_t(now-last_cache)>=10){
        last_cache=now;
        // Main-loop-only staging: keep 780 bytes off the Pico's small IRQ stack.
        static decltype(feature_cache) snapshot{};
        for(uint8_t id=2;id<=24;++id)read_feature_report(id,snapshot[id].data(),feature_size(id));
        read_feature_report(33,snapshot[33].data(),24);
        snapshot[25]={'X','C','T','R',1,1,0,0}; // v1: transport 1 = BLE HID
        cyw43_thread_enter();feature_cache=snapshot;
        if(queued.id && queued.id!=4)feature_cache[queued.id][1]=1;
        cyw43_thread_exit();
    }
    cyw43_thread_enter();
    now=now_ms();
    if(stop_at && int32_t(now-stop_at)>=0){stop_at=0;if(connection!=HCI_CON_HANDLE_INVALID)gap_disconnect(connection);}
    if(deferred_command && int32_t(now-action_at)>=0){
        const auto command=deferred_command;deferred_command=0;
        if(connection!=HCI_CON_HANDLE_INVALID)gap_disconnect(connection);
        if(command==2){
            for(int i=0;i<le_device_db_max_count();++i){
                int address_type;bd_addr_t address;sm_key_t key;
                le_device_db_info(i,&address_type,address,key);
                if(address_type!=BD_ADDR_TYPE_UNKNOWN)gap_delete_bonding(static_cast<bd_addr_type_t>(address_type),address);
            }
        }
        if(command==1 || command==2){pair_until=now+120000;clear_diagnostics();}
        advertise();
    }
    // Never renegotiate connection timing during encryption/key exchange.
    if(encrypted && (subscribed || raw_subscribed) && !parameters_checked && elapsed_at_least(now,connection_at,2000)){
        parameters_checked=true;
        if(interval<6 || interval>12)gap_request_connection_parameter_update(connection,6,12,0,600);
    }
    paused=active.enabled && active.usb_priority && usb_mounted;
    latest=make_hid_report(paused?State{}:state);
    if(pending_command){
        const auto command=pending_command;pending_command=0;
        if(!active.enabled || !working)command_status=3;
        else {
            if(command==1 && connection==HCI_CON_HANDLE_INVALID){pair_until=now+120000;clear_diagnostics();}
            else {deferred_command=command;action_at=now+3000;}
            advertise();command_status=0;
        }
    }
    if(active.enabled && encrypted && (subscribed || raw_subscribed) && !suspended && connection!=HCI_CON_HANDLE_INVALID){
        if(!report_queue && uint32_t(now-last_send)>=8){
            last_send=now;report_queue=(subscribed?1:0)|(raw_subscribed?2:0);
            raw=wireless::encode(paused?State{}:state,mode_counter,wireless_sequence++);
        }
        if(report_queue && !send_pending){
            send_pending=true;
            if(hids_device_request_can_send_now_event(connection)!=ERROR_CODE_SUCCESS)send_pending=false;
        }
    }
    cyw43_thread_exit();
}
std::array<uint8_t,24> live_report(){
    if(initialized)cyw43_thread_enter();
    const uint16_t flags=(active.enabled?1:0)|(working?2:0)|(advertising?4:0)|
        (connection!=HCI_CON_HANDLE_INVALID?8:0)|(encrypted?16:0)|(subscribed?32:0)|
        (paused?64:0)|(pairing_open()?128:0)|(error?256:0);
    std::array<uint8_t,24> p{'X','C','B','L',2,static_cast<uint8_t>(flags),static_cast<uint8_t>(flags>>8),
        static_cast<uint8_t>(initialized?le_device_db_count():0),command_status,last_command};
    p[10]=interval;p[11]=interval>>8;
    const uint16_t seconds=pairing_open()?static_cast<uint16_t>((pair_until-now_ms()+999)/1000):0;
    p[12]=seconds;p[13]=seconds>>8;
    if(working){bd_addr_t address;gap_local_bd_addr(address);std::memcpy(p.data()+14,address,6);}
    p[20]=last_connection_status;p[21]=last_pairing_status;p[22]=last_pairing_reason;p[23]=last_disconnect_reason;
    if(initialized)cyw43_thread_exit();
    return p;
}
void mode_press(){if(initialized)cyw43_thread_enter();++mode_counter;if(initialized)cyw43_thread_exit();}
UsbMode output_mode(UsbMode local,uint32_t){
    if(initialized)cyw43_thread_enter();
    const auto result=feedback_valid && encrypted && raw_subscribed && !paused && uint32_t(now_ms()-feedback_at)<3000?remote_mode:local;
    if(initialized)cyw43_thread_exit();
    return result;
}
std::array<uint8_t,24> link_report(){
    if(!initialized){std::array<uint8_t,24> p{'X','C','L','P',1,0,255};p[20]=255;p[21]=255;p[22]=VIBETAR_RECEIVER;return p;}
    cyw43_thread_enter();
    const auto report=make_link_report(connection,(connection!=HCI_CON_HANDLE_INVALID?1:0)|(encrypted?2:0)|((subscribed || raw_subscribed)?4:0),0);
    cyw43_thread_exit();return report;
}
Phase connection_phase(){
    if(initialized)cyw43_thread_enter();
    const auto result=phase(active.enabled,working,advertising,false,connection!=HCI_CON_HANDLE_INVALID,encrypted && (subscribed || raw_subscribed));
    if(initialized)cyw43_thread_exit();
    return result;
}
BluetoothPayload command_report(){
    if(initialized)cyw43_thread_enter();
    BluetoothPayload p{1,command_status,last_command};
    if(initialized)cyw43_thread_exit();
    return p;
}
void submit_command(const uint8_t* p,uint16_t size){
    if(!radio::available()){command_status=3;return;}
    if(initialized)cyw43_thread_enter();
    if(command_status!=1 && !deferred_command){
        bool valid=size==8 && p[0]==1 && p[1]==0 && p[2]>=1 && p[2]<=3;
        if(valid)for(unsigned i=3;i<8;++i)if(p[i])valid=false;
        if(!valid)command_status=2;
        else if(!active.enabled || !initialized)command_status=3;
        else {pending_command=p[2];last_command=p[2];command_status=1;}
    }
    if(initialized)cyw43_thread_exit();
}
}

extern "C" uint16_t vibetar_hid_feature_read(uint8_t id,uint16_t offset,uint8_t* buffer,uint16_t size){
    // Invoked with CYW43 context locked. ATT helper handles length queries/blobs.
    if(!guitar::bluetooth::feature_size(id))return 0;
    using namespace guitar::bluetooth;
    if(buffer && (!offset || read_id!=id)){read_snapshot=feature_cache[id];read_id=id;}
    return att_read_callback_handle_blob(read_snapshot.data(),feature_size(id),offset,buffer,size);
}
extern "C" int vibetar_hid_feature_write(uint8_t id,uint16_t transaction,uint16_t offset,const uint8_t* buffer,uint16_t size){
    using namespace guitar::bluetooth;
    if(transaction==ATT_TRANSACTION_MODE_CANCEL){prepared.clear();return 0;}
    if(transaction==ATT_TRANSACTION_MODE_VALIDATE){
        if(!prepared.id)return 0;
        if(writable_feature(prepared.id) && queued.id)return ATT_ERROR_INSUFFICIENT_RESOURCES;
        return prepared.complete()?0:ATT_ERROR_INVALID_ATTRIBUTE_VALUE_LENGTH;
    }
    if(transaction==ATT_TRANSACTION_MODE_EXECUTE){
        if(!prepared.id)return 0;
        if(!prepared.complete())return ATT_ERROR_INVALID_ATTRIBUTE_VALUE_LENGTH;
        if(!writable_feature(prepared.id)){prepared.clear();return 0;}
        if(queued.id)return ATT_ERROR_INSUFFICIENT_RESOURCES;
        queued=prepared;prepared.clear();
        if(queued.id!=4)feature_cache[queued.id][1]=1;
        return 0;
    }
    if(!feature_size(id))return ATT_ERROR_WRITE_NOT_PERMITTED;
    // HIDS mandates READ + WRITE for every Feature characteristic (Table 2.4).
    // Snapshot/default/identity reports accept SET as a no-op, like USB.
    if(id==24 && deferred_command)return ATT_ERROR_INSUFFICIENT_RESOURCES;
    if(offset+size>feature_size(id))return ATT_ERROR_INVALID_OFFSET;
    if(transaction==ATT_TRANSACTION_MODE_ACTIVE){
        return prepared.append(id,offset,buffer,size,true)?0:ATT_ERROR_INVALID_ATTRIBUTE_VALUE_LENGTH;
    }
    if(transaction!=ATT_TRANSACTION_MODE_NONE)return ATT_ERROR_REQUEST_NOT_SUPPORTED;
    if(offset || size!=feature_size(id))return ATT_ERROR_INVALID_ATTRIBUTE_VALUE_LENGTH;
    if(!writable_feature(id))return 0;
    if(queued.id || prepared.id)return ATT_ERROR_INSUFFICIENT_RESOURCES;
    queued.append(id,0,buffer,size);
    if(id!=4)feature_cache[id][1]=1;
    return 0;
}
