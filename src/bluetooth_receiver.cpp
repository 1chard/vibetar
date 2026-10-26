#include "bluetooth.hpp"
#include "bluetooth_link.hpp"
#include "bluetooth_status.hpp"
#include "bluetooth_discovery.hpp"
#include "receiver_health.hpp"
#include "receiver_protocol.hpp"
#include "usb_device.hpp"
#include "radio.hpp"
#include "pico/cyw43_arch.h"
#include "btstack.h"
#include "btstack_tlv.h"
#include "receiver_ble.h"
#include <cstring>
namespace guitar::bluetooth
{
    namespace
    {
        BluetoothConfig active{};
        bool initialized = false, working = false, scanning = false, connecting = false, encrypted = false, ready = false, error = false;
        bool peer_valid = false, peer_save_pending = false, writing = false;
        uint8_t address_type = 0, pending_command = 0, command_status = 0, last_command = 0;
        uint8_t connection_status = 255, pairing_status = 255, pairing_reason = 0, disconnect_reason = 0;
        uint16_t interval = 0, hids_cid = 0;
        uint32_t pair_until = 0, next_scan = 0, deadline = 0, last_feedback = 0;
        hci_con_handle_t connection = HCI_CON_HANDLE_INVALID;
        bd_addr_t address{};
        wireless::Link link{};
        wireless::Feedback feedback{};
        uint8_t hid_descriptor[512]{};
        btstack_packet_callback_registration_t hci_registration{}, sm_registration{};
        Discovery discovery{};
        ConnectPayload selection{'X', 'C', 'C', 'N', 1};
        bool select_pending = false, manual_selection = false, observed_any = false;
        uint8_t gatt_status = 255, att_status = 255, att_stage = 0, failure_reason = 0, last_report_id = 0, last_report_length = 0;
        uint8_t received_reports = 0, valid_reports = 0;
        SecurityGate security{};
        constexpr uint32_t peer_tag = 0x58435250; // XCRP, outside the SDK bonding tags
        uint32_t now_ms() { return to_ms_since_boot(get_absolute_time()); }
        bool pairing_open() { return pair_until && int32_t(pair_until - now_ms()) > 0; }
        void clear_diagnostics()
        {
            att_status = 255;
            att_stage = 0;
            failure_reason = 0;
            connection_status = 255;
            pairing_status = 255;
            pairing_reason = 0;
            disconnect_reason = 0;
            error = false;
        }
        void stop_scan()
        {
            if (scanning)
                gap_stop_scan();
            scanning = false;
        }
        void load_peer()
        {
            const btstack_tlv_t *impl;
            void *context;
            btstack_tlv_get_instance(&impl, &context);
            uint8_t p[8]{};
            if (impl && impl->get_tag(context, peer_tag, p, sizeof(p)) == sizeof(p) && p[0] == 1 && p[1] <= 1)
            {
                peer_valid = true;
                address_type = p[1];
                std::memcpy(address, p + 2, 6);
            }
        }
        void scan()
        {
            if (!active.enabled || !working || scanning || connecting || connection != HCI_CON_HANDLE_INVALID)
                return;
            if ((manual_selection || !peer_valid) && !pairing_open())
                return;
            gap_set_scan_parameters(0, 48, manual_selection ? 48 : 24);
            gap_set_scan_duplicate_filter(false);
            gap_start_scan();
            scanning = true;
        }
        void fail(uint8_t reason)
        {
            failure_reason = reason;
            error = true;
            ready = false;
            writing = false;
            deadline = now_ms() + 10000;
            link.reset();
            if (connection != HCI_CON_HANDLE_INVALID)
                gap_disconnect(connection);
        }
        void gatt_handler(uint8_t, uint16_t, uint8_t *packet, uint16_t)
        {
            if (hci_event_packet_get_type(packet) != HCI_EVENT_GATTSERVICE_META)
                return;
            switch (hci_event_gattservice_meta_get_subevent_code(packet))
            {
            case GATTSERVICE_SUBEVENT_HID_SERVICE_CONNECTED:
                gatt_status = gattservice_subevent_hid_service_connected_get_status(packet);
                if (gatt_status)
                {
                    fail(1);
                    break;
                }
                hids_cid = gattservice_subevent_hid_service_connected_get_hids_cid(packet);
                ready = true;
                deadline = now_ms() + 10000;
                writing = false;
                last_feedback = 0;
                break;
            case GATTSERVICE_SUBEVENT_HID_SERVICE_DISCONNECTED:
                ready = false;
                writing = false;
                hids_cid = 0;
                link.reset();
                break;
            case GATTSERVICE_SUBEVENT_HID_REPORT:
                last_report_id = gattservice_subevent_hid_report_get_report_id(packet);
                last_report_length = static_cast<uint8_t>(gattservice_subevent_hid_report_get_report_len(packet));
                if (received_reports < 255)
                    ++received_reports;
                if (ready && encrypted && gattservice_subevent_hid_report_get_report_id(packet) == wireless::state_report_id)
                {
                    auto *p = gattservice_subevent_hid_report_get_report(packet);
                    auto size = gattservice_subevent_hid_report_get_report_len(packet);
                    // BTstack HIDS Client prepends the report ID to GATT input values.
                    if (size == 17 && p[0] == wireless::state_report_id)
                    {
                        ++p;
                        --size;
                    }
                    if (link.accept(p, size, now_ms()))
                    {
                        if (valid_reports < 255)
                            ++valid_reports;
                        error = false;
                        if (!peer_valid)
                        {
                            peer_valid = true;
                            peer_save_pending = true;
                        }
                        manual_selection = false;
                        pair_until = 0;
                    }
                }
                break;
            case GATTSERVICE_SUBEVENT_HID_REPORT_WRITTEN:
                writing = false;
                break;
            default:
                break;
            }
        }
        void discover()
        {
            if (!security.ready() || ready || hids_cid || connection == HCI_CON_HANDLE_INVALID)
                return;
            const auto status = hids_client_connect(connection, gatt_handler, HID_PROTOCOL_MODE_REPORT, &hids_cid);
            if (status)
            {
                gatt_status = status;
                fail(1);
            }
        }
        void packet_handler(uint8_t type, uint16_t, uint8_t *p, uint16_t)
        {
            if (type != HCI_EVENT_PACKET)
                return;
            switch (hci_event_packet_get_type(p))
            {
            case BTSTACK_EVENT_STATE:
                working = btstack_event_state_get_state(p) == HCI_STATE_WORKING;
                if (working)
                {
                    load_peer();
                    scan();
                }
                break;
            case GAP_EVENT_ADVERTISING_REPORT:
                if (scanning)
                    observed_any = true;
                if (scanning && wireless::advertised(gap_event_advertising_report_get_data(p), gap_event_advertising_report_get_data_length(p)))
                {
                    bd_addr_t candidate;
                    gap_event_advertising_report_get_address(p, candidate);
                    const auto kind = gap_event_advertising_report_get_address_type(p);
                    discovery.observe(candidate, kind, static_cast<int8_t>(gap_event_advertising_report_get_rssi(p)), now_ms());
                    if (manual_selection)
                        break;
                    if (peer_valid && (kind != address_type || std::memcmp(candidate, address, 6)))
                        break;
                    if (!peer_valid && !pairing_open())
                        break;
                    stop_scan();
                    address_type = kind;
                    std::memcpy(address, candidate, 6);
                    connecting = true;
                    deadline = now_ms() + 10000;
                    if (gap_connect(address, static_cast<bd_addr_type_t>(address_type)) != ERROR_CODE_SUCCESS)
                    {
                        connecting = false;
                        error = true;
                        next_scan = now_ms() + 1000;
                    }
                }
                break;
            case HCI_EVENT_META_GAP:
                if (hci_event_gap_meta_get_subevent_code(p) == GAP_SUBEVENT_LE_CONNECTION_COMPLETE)
                {
                    connecting = false;
                    connection_status = gap_subevent_le_connection_complete_get_status(p);
                    if (connection_status)
                    {
                        error = true;
                        next_scan = now_ms() + 1000;
                        break;
                    }
                    connection = gap_subevent_le_connection_complete_get_connection_handle(p);
                    interval = gap_subevent_le_connection_complete_get_conn_interval(p);
                    encrypted = false;
                    security.reset();
                    ready = false;
                    hids_cid = 0;
                    gatt_status = 255;
                    att_status = 255;
                    att_stage = 0;
                    failure_reason = 0;
                    received_reports = 0;
                    valid_reports = 0;
                    last_report_id = 0;
                    last_report_length = 0;
                    error = false;
                    link.reset();
                    deadline = now_ms() + 20000;
                    if (!active.enabled)
                    {
                        gap_disconnect(connection);
                        break;
                    }
                    sm_request_pairing(connection);
                }
                break;
            case HCI_EVENT_LE_META:
                if (hci_event_le_meta_get_subevent_code(p) == HCI_SUBEVENT_LE_CONNECTION_UPDATE_COMPLETE &&
                    !hci_subevent_le_connection_update_complete_get_status(p))
                    interval = hci_subevent_le_connection_update_complete_get_conn_interval(p);
                break;
            case HCI_EVENT_ENCRYPTION_CHANGE:
                if (hci_event_encryption_change_get_connection_handle(p) == connection)
                {
                    encrypted = !hci_event_encryption_change_get_status(p) && hci_event_encryption_change_get_encryption_enabled(p);
                    security.encryption(encrypted);
                    if (!encrypted)
                        fail(7);
                }
                break;
            case HCI_EVENT_ENCRYPTION_CHANGE_V2:
                if (hci_event_encryption_change_v2_get_connection_handle(p) == connection)
                {
                    encrypted = !hci_event_encryption_change_v2_get_status(p) && hci_event_encryption_change_v2_get_encryption_enabled(p);
                    security.encryption(encrypted);
                    if (!encrypted)
                        fail(7);
                }
                break;
            case SM_EVENT_JUST_WORKS_REQUEST:
                if (active.enabled && pairing_open())
                    sm_just_works_confirm(sm_event_just_works_request_get_handle(p));
                else
                {
                    sm_bonding_decline(sm_event_just_works_request_get_handle(p));
                    fail(8);
                }
                break;
            case SM_EVENT_PAIRING_COMPLETE:
                pairing_status = sm_event_pairing_complete_get_status(p);
                pairing_reason = sm_event_pairing_complete_get_reason(p);
                security.finished(pairing_status == 0);
                if (pairing_status)
                    fail(8);
                else
                {
                    encrypted = true;
                    deadline = now_ms() + 30000;
                    discover();
                }
                break;
            case SM_EVENT_REENCRYPTION_COMPLETE:
                pairing_status = sm_event_reencryption_complete_get_status(p);
                pairing_reason = 0;
                security.finished(pairing_status == 0);
                if (pairing_status)
                    fail(8);
                else
                {
                    encrypted = true;
                    deadline = now_ms() + 30000;
                    discover();
                }
                break;
            case HCI_EVENT_DISCONNECTION_COMPLETE:
                if (hci_event_disconnection_complete_get_connection_handle(p) == connection)
                {
                    disconnect_reason = hci_event_disconnection_complete_get_reason(p);
                    connection = HCI_CON_HANDLE_INVALID;
                    interval = 0;
                    encrypted = false;
                    security.reset();
                    ready = false;
                    writing = false;
                    hids_cid = 0;
                    link.reset();
                    next_scan = now_ms() + 1000;
                }
                break;
            default:
                break;
            }
        }
        bool start()
        {
            if (initialized)
                return true;
            if (!radio::ensure_ready())
                return false;
            cyw43_thread_enter();
            l2cap_init();
            sm_init();
            gatt_client_init();
            sm_set_io_capabilities(IO_CAPABILITY_NO_INPUT_NO_OUTPUT);
            sm_set_authentication_requirements(SM_AUTHREQ_SECURE_CONNECTION | SM_AUTHREQ_BONDING);
            sm_set_encryption_key_size_range(16, 16);
            att_server_init(profile_data, nullptr, nullptr);
            hids_client_init(hid_descriptor, sizeof(hid_descriptor));
            hci_registration.callback = packet_handler;
            hci_add_event_handler(&hci_registration);
            sm_registration.callback = packet_handler;
            sm_add_event_handler(&sm_registration);
            gap_set_connection_parameters(48, 48, 6, 12, 0, 600, 0, 0);
            initialized = true;
            hci_power_control(HCI_POWER_ON);
            cyw43_thread_exit();
            return true;
        }
    }
    void configure(const BluetoothConfig &config, uint32_t now)
    {
        if (!radio::available())
        {
            active = {false, true};
            return;
        }
        const bool previous = active.enabled;
        if (config.enabled && !start())
        {
            active = config;
            error = true;
            return;
        }
        if (!initialized)
        {
            active = config;
            return;
        }
        cyw43_thread_enter();
        active = config;
        if (active.enabled && !previous)
        {
            pair_until = now + 120000;
            clear_diagnostics();
        }
        if (active.enabled && !pair_until && !peer_valid && !ready)
        {
            pair_until = now + 120000;
        }
        if (!active.enabled)
        {
            pair_until = 0;
            stop_scan();
            if (connecting)
                gap_connect_cancel();
            connecting = false;
            ready = false;
            encrypted = false;
            link.reset();
            if (connection != HCI_CON_HANDLE_INVALID)
                gap_disconnect(connection);
        }
        scan();
        cyw43_thread_exit();
    }
    void task(uint32_t, const State &, bool)
    {
        if (!initialized)
            return;
        cyw43_thread_enter();
        const uint32_t now = now_ms(); // Sample after locking out newer radio callbacks.
        if (peer_save_pending)
        {
            const btstack_tlv_t *impl;
            void *context;
            btstack_tlv_get_instance(&impl, &context);
            uint8_t record[8]{1, address_type};
            std::memcpy(record + 2, address, 6);
            if (!impl || impl->store_tag(context, peer_tag, record, sizeof(record)))
                error = true;
            peer_save_pending = false;
        }
        if (pending_command)
        {
            const auto command = pending_command;
            pending_command = 0;
            if (!active.enabled || !working)
                command_status = 3;
            else
            {
                if (command == 1)
                {
                    pair_until = now + 120000;
                    manual_selection = true;
                    discovery.clear();
                    observed_any = false;
                    gatt_status = 255;
                    clear_diagnostics();
                }
                else
                {
                    stop_scan();
                    if (connecting)
                        gap_connect_cancel();
                    connecting = false;
                    if (connection != HCI_CON_HANDLE_INVALID)
                        gap_disconnect(connection);
                    ready = false;
                    encrypted = false;
                    link.reset();
                    manual_selection = true;
                    pair_until = now + 120000;
                    if (command == 2)
                    {
                        const btstack_tlv_t *impl;
                        void *context;
                        btstack_tlv_get_instance(&impl, &context);
                        if (impl)
                            impl->delete_tag(context, peer_tag);
                        peer_valid = false;
                        peer_save_pending = false;
                        for (int i = 0; i < le_device_db_max_count(); ++i)
                        {
                            int kind;
                            bd_addr_t addr;
                            sm_key_t key;
                            le_device_db_info(i, &kind, addr, key);
                            if (kind != BD_ADDR_TYPE_UNKNOWN)
                                gap_delete_bonding(static_cast<bd_addr_type_t>(kind), addr);
                        }
                        pair_until = now + 120000;
                        clear_diagnostics();
                    }
                    next_scan = now + 1000;
                }
                command_status = 0;
            }
        }
        if (select_pending)
        {
            select_pending = false;
            if (!active.enabled || !working || connection != HCI_CON_HANDLE_INVALID || connecting || !pairing_open())
                selection[5] = 3;
            else if (!discovery.contains(selection.data() + 8, selection[6], now))
                selection[5] = 2;
            else
            {
                stop_scan();
                manual_selection = true;
                address_type = selection[6];
                std::memcpy(address, selection.data() + 8, 6);
                peer_valid = false;
                peer_save_pending = false;
                connecting = true;
                deadline = now + 10000;
                gatt_status = 255;
                clear_diagnostics();
                const auto result = gap_connect(address, static_cast<bd_addr_type_t>(address_type));
                connection_status = result;
                if (result)
                {
                    connecting = false;
                    error = true;
                    next_scan = now + 1000;
                    selection[5] = 3;
                }
                else
                    selection[5] = 0;
            }
        }
        if (manual_selection && !pairing_open() && !connecting && connection == HCI_CON_HANDLE_INVALID)
            manual_selection = false;
        if (scanning && !peer_valid && !pairing_open())
            stop_scan();
        if (connecting && int32_t(now - deadline) >= 0)
        {
            gap_connect_cancel();
            connecting = false;
            error = true;
            next_scan = now + 1000;
        }
        if (connection != HCI_CON_HANDLE_INVALID && int32_t(now - deadline) >= 0 && (!ready || !link.valid))
        {
            deadline = now + 10000;
            fail(!security.ready() ? 2 : !ready ? 3
                                                : 4);
        }
        if (ready && link.valid && elapsed_at_least(now, link.updated, 2000))
            fail(5);
        if (writing && uint32_t(now - last_feedback) >= 3000)
            fail(6);
        if (active.enabled && int32_t(now - next_scan) >= 0)
            scan();
        if (ready && encrypted && link.fresh(now) && !writing && uint32_t(now - last_feedback) >= 500)
        {
            feedback = wireless::feedback(usb_mode(), usb_mode_mask());
            if (hids_client_send_write_report(hids_cid, 27, HID_REPORT_TYPE_OUTPUT, feedback.data(), feedback.size()) == ERROR_CODE_SUCCESS)
            {
                writing = true;
                last_feedback = now;
            }
        }
        cyw43_thread_exit();
    }
    PreviewPayload received_preview(UsbMode mode)
    {
        if (!initialized)
            return encode_preview({}, mode, true, false, now_ms());
        cyw43_thread_enter();
        const auto now = now_ms();
        const auto p = encode_preview(link.state(now), mode, true, link.fresh(now), now);
        cyw43_thread_exit();
        return p;
    }
    State received_state(uint32_t)
    {
        if (!initialized)
            return {};
        cyw43_thread_enter();
        const auto state = link.state(now_ms());
        cyw43_thread_exit();
        return state;
    }
    bool take_mode_press(uint32_t)
    {
        if (!initialized)
            return false;
        cyw43_thread_enter();
        const bool press = link.take_mode(now_ms());
        cyw43_thread_exit();
        return press;
    }
    void mode_press() {}
    UsbMode output_mode(UsbMode local, uint32_t) { return local; }
    std::array<uint8_t, 24> live_report()
    {
        if (initialized)
            cyw43_thread_enter();
        const uint32_t now = now_ms();
        const uint16_t flags = (active.enabled ? 1 : 0) | (working ? 2 : 0) | (scanning ? 4 : 0) | (connection != HCI_CON_HANDLE_INVALID ? 8 : 0) |
                               (encrypted ? 16 : 0) | (link.fresh(now) ? 32 : 0) | (pairing_open() ? 128 : 0) | (error ? 256 : 0);
        std::array<uint8_t, 24> p{'X', 'C', 'B', 'L', 2, static_cast<uint8_t>(flags), static_cast<uint8_t>(flags >> 8),
                                  static_cast<uint8_t>(initialized ? le_device_db_count() : 0), command_status, last_command};
        p[10] = interval;
        p[11] = interval >> 8;
        const uint16_t seconds = pairing_open() ? static_cast<uint16_t>((pair_until - now + 999) / 1000) : 0;
        p[12] = seconds;
        p[13] = seconds >> 8;
        if (peer_valid || connecting || connection != HCI_CON_HANDLE_INVALID)
            std::memcpy(p.data() + 14, address, 6);
        p[20] = connection_status;
        p[21] = pairing_status;
        p[22] = pairing_reason;
        p[23] = disconnect_reason;
        if (initialized)
            cyw43_thread_exit();
        return p;
    }
    DiscoveryPayload discovery_report()
    {
        if (initialized)
            cyw43_thread_enter();
        const uint8_t flags = (manual_selection ? 1 : 0) | (scanning ? 2 : 0) | (connecting ? 4 : 0) | (connection != HCI_CON_HANDLE_INVALID ? 8 : 0) | (ready ? 16 : 0) | (error ? 32 : 0) | (observed_any ? 64 : 0);
        const auto p = discovery.report(now_ms(), flags, gatt_status);
        if (initialized)
            cyw43_thread_exit();
        return p;
    }
    ConnectPayload health_report()
    {
        if (initialized)
            cyw43_thread_enter();
        const ConnectPayload p{'X', 'C', 'R', 'D', 1, failure_reason, att_status, att_stage, static_cast<uint8_t>(security.ready()), static_cast<uint8_t>(ready), static_cast<uint8_t>(link.valid), last_report_id, last_report_length, received_reports, valid_reports};
        if (initialized)
            cyw43_thread_exit();
        return p;
    }
    ConnectPayload connect_report()
    {
        if (initialized)
            cyw43_thread_enter();
        const auto p = selection;
        if (initialized)
            cyw43_thread_exit();
        return p;
    }
    void submit_connect(const uint8_t *p, uint16_t size)
    {
        if (initialized)
            cyw43_thread_enter();
        if (!select_pending)
        {
            if (!valid_connect(p, size))
                selection[5] = 2;
            else if (!initialized || !active.enabled)
                selection[5] = 3;
            else
            {
                std::memcpy(selection.data(), p, 16);
                selection[5] = 1;
                select_pending = true;
            }
        }
        if (initialized)
            cyw43_thread_exit();
    }
    std::array<uint8_t, 24> link_report()
    {
        if (!initialized)
        {
            std::array<uint8_t, 24> p{'X', 'C', 'L', 'P', 1, 0, 255};
            p[20] = 255;
            p[21] = 255;
            p[22] = VIBETAR_RECEIVER;
            return p;
        }
        cyw43_thread_enter();
        const auto report = make_link_report(connection, (connection != HCI_CON_HANDLE_INVALID ? 1 : 0) | (encrypted ? 2 : 0) | (ready ? 4 : 0), 1);
        cyw43_thread_exit();
        return report;
    }
    Phase connection_phase()
    {
        if (initialized)
            cyw43_thread_enter();
        const auto result = phase(active.enabled, working, scanning, connecting, connection != HCI_CON_HANDLE_INVALID, encrypted && ready);
        if (initialized)
            cyw43_thread_exit();
        return result;
    }
    BluetoothPayload command_report()
    {
        if (initialized)
            cyw43_thread_enter();
        BluetoothPayload p{1, command_status, last_command};
        if (initialized)
            cyw43_thread_exit();
        return p;
    }
    void submit_command(const uint8_t *p, uint16_t size)
    {
        if (!radio::available())
        {
            command_status = 3;
            return;
        }
        if (initialized)
            cyw43_thread_enter();
        if (command_status != 1)
        {
            bool valid = size == 8 && p[0] == 1 && p[1] == 0 && p[2] >= 1 && p[2] <= 3;
            if (valid)
                for (unsigned i = 3; i < 8; ++i)
                    if (p[i])
                        valid = false;
            if (!valid)
                command_status = 2;
            else if (!active.enabled || !initialized)
                command_status = 3;
            else
            {
                pending_command = p[2];
                last_command = p[2];
                command_status = 1;
            }
        }
        if (initialized)
            cyw43_thread_exit();
    }
}

extern "C" void vibetar_receiver_att_diagnostic(uint8_t status, uint8_t stage)
{
    // Invoked by BTstack with the same CYW43 lock as the receiver callbacks.
    if (guitar::bluetooth::att_status == 255 || guitar::bluetooth::att_status == 0 || status)
    {
        guitar::bluetooth::att_status = status;
        guitar::bluetooth::att_stage = stage;
    }
}
