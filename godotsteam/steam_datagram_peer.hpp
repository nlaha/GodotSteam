#pragma once

#include <godot_cpp/classes/object.hpp>
#include <godot_cpp/core/class_db.hpp>

#include <godot_cpp/classes/global_constants.hpp>
#include <godot_cpp/classes/multiplayer_peer_extension.hpp>
#include <godot_cpp/variant/packed_byte_array.hpp>
#include <godot_cpp/templates/vector.hpp>
#include <godot_cpp/variant/utility_functions.hpp>

// Include Steamworks API headers
#include "steam/steam_api.h"
#include "steam/isteamnetworkingutils.h"
#include "steam/isteamnetworking.h"
#include "steam/isteamnetworkingsockets.h"
#include "steam/steamnetworkingtypes.h"

using namespace godot;

struct Packet
{
    int sender_peer_id;
    PackedByteArray data;
};

class SteamDatagramRelayPeer : public MultiplayerPeerExtension
{
    GDCLASS(SteamDatagramRelayPeer, MultiplayerPeerExtension);

private:
    STEAM_CALLBACK(SteamDatagramRelayPeer, OnRelayStatusUpdate, SteamRelayNetworkStatus_t);
    STEAM_CALLBACK(SteamDatagramRelayPeer, OnConnectionStatusChanged, SteamNetConnectionStatusChangedCallback_t);

protected:
    static void _bind_methods();

    ISteamNetworkingSockets *m_pInterface;

    HSteamListenSocket m_hListenSock;
    HSteamNetConnection m_hConn;

    // dictionary of peers by steam id
    Dictionary _steam_id_to_peer_id;
    Dictionary _peer_id_to_steam_id;
    Vector<int> connections;

    ConnectionStatus _connection_status;

    Vector<Packet> _packets;

    bool _refuse_new_connections;

    TransferMode _transfer_mode;
    int _transfer_channel;

    int _target_peer;

public:
    SteamDatagramRelayPeer();
    ~SteamDatagramRelayPeer();

    // custom multiplayer methods

    // host game
    String host_game_p2p();
    // join game
    void join_game_p2p(const String &steam_identity);

    void add_peer(uint64 steam_id);

    // override methods from MultiplayerPeerExtension
    void _close() override;
    void _disconnect_peer(int p_peer, bool p_force) override;
    int _get_available_packet_count() const override;
    MultiplayerPeer::ConnectionStatus _get_connection_status() const override;
    int _get_max_packet_size() const override;

    Error _get_packet(const uint8_t **r_buffer, int32_t *r_buffer_size) override;

    int _get_packet_channel() const override;
    MultiplayerPeer::TransferMode _get_packet_mode() const override;
    int _get_packet_peer() const override;
    PackedByteArray _get_packet_script() override;
    int _get_transfer_channel() const override;
    MultiplayerPeer::TransferMode _get_transfer_mode() const override;
    int _get_unique_id() const override;
    bool _is_refusing_new_connections() const override;
    bool _is_server() const override;
    bool _is_server_relay_supported() const override;
    void _poll() override;

    Error _put_packet(const uint8_t *p_buffer, int p_buffer_size) override;

    void _set_refuse_new_connections(bool p_enable) override;
    void _set_target_peer(int p_peer) override;
    void _set_transfer_channel(int p_channel) override;
    void _set_transfer_mode(MultiplayerPeer::TransferMode p_mode) override;
};