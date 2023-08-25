#include "steam_datagram_peer.hpp"

#include <godot_cpp/core/class_db.hpp>

#include <godot_cpp/classes/global_constants.hpp>
#include <godot_cpp/classes/multiplayer_peer.hpp>
#include <godot_cpp/variant/packed_byte_array.hpp>

using namespace godot;

void SteamDatagramRelayPeer::_bind_methods()
{
    ClassDB::bind_method(D_METHOD("host_game_p2p"), &SteamDatagramRelayPeer::host_game_p2p);
    ClassDB::bind_method(D_METHOD("join_game_p2p", "steam_identity"), &SteamDatagramRelayPeer::join_game_p2p);
}

const int MAX_PACKETS_READ_PER_POLL = 16;

void SteamDatagramRelayPeer::OnRelayStatusUpdate(SteamRelayNetworkStatus_t *pCallback)
{
    // log status updates
    switch (pCallback->m_eAvail)
    {
    case k_ESteamNetworkingAvailability_CannotTry:
        ERR_PRINT("SteamDatagramRelayPeer: Cannot try to use relay network");
        break;
    case k_ESteamNetworkingAvailability_Failed:
        ERR_PRINT("SteamDatagramRelayPeer: Failed to initialize relay network");
        break;
    case k_ESteamNetworkingAvailability_Previously:
        UtilityFunctions::print("SteamDatagramRelayPeer: Previously used relay network\n");
        break;
    case k_ESteamNetworkingAvailability_Retrying:
        UtilityFunctions::print("SteamDatagramRelayPeer: Retrying to initialize relay network\n");
        break;
    case k_ESteamNetworkingAvailability_Current:
        UtilityFunctions::print("SteamDatagramRelayPeer: Relay network initialized successfully\n");
        break;
    }
}

// add peer
void SteamDatagramRelayPeer::add_peer(uint64 steam_id)
{
    int peer_id = TARGET_PEER_SERVER;
    // check if we're the server
    if (this->m_hListenSock == k_HSteamListenSocket_Invalid)
        peer_id = steam_id % 2147483647;
    // add to dictionary
    this->_steam_id_to_peer_id[steam_id] = peer_id;
    this->_peer_id_to_steam_id[peer_id] = steam_id;
}

// connection status changed callback SteamNetConnectionStatusChangedCallback_t
void SteamDatagramRelayPeer::OnConnectionStatusChanged(SteamNetConnectionStatusChangedCallback_t *pCallback)
{
    // print connection status using GetDetailedConnectionStatus
    char *buffer = new char[1024];
    SteamNetworkingSockets()->GetDetailedConnectionStatus(pCallback->m_hConn, buffer, 1024);
    UtilityFunctions::print("SteamDatagramRelayPeer: Connection status:" + String(buffer));

    // check if we received a connection
    if (pCallback->m_info.m_eState == k_ESteamNetworkingConnectionState_Connecting)
    {
        _connection_status = MultiplayerPeer::CONNECTION_CONNECTING;
        // convert steam id to peer id, peer ids must be between 1 and 2147483647
        // and store in dictionary
        uint64 steam_id_64 = pCallback->m_info.m_identityRemote.GetSteamID64();
        this->add_peer(steam_id_64);

        // accept connection
        // check if we're the server first
        if (this->m_hListenSock == k_HSteamListenSocket_Invalid)
            return; // don't accept connection if we're not the server

        // check if we're refusing new connections
        if (this->_refuse_new_connections)
            return; // don't accept connection if we're refusing new connections

        SteamNetworkingSockets()->AcceptConnection(pCallback->m_hConn);
    }

    // check if we lost a connection
    if (pCallback->m_info.m_eState == k_ESteamNetworkingConnectionState_ClosedByPeer)
    {
        _connection_status = MultiplayerPeer::CONNECTION_DISCONNECTED;
        // convert steam id to peer id, peer ids must be between 1 and 2147483647
        // and remove from dictionary
        uint64 steam_id_64 = pCallback->m_info.m_identityRemote.GetSteamID64();
        int peer_id = this->_steam_id_to_peer_id[steam_id_64];
        this->_steam_id_to_peer_id.erase(steam_id_64);
        this->_peer_id_to_steam_id.erase(peer_id);

        UtilityFunctions::print("SteamDatagramRelayPeer: Lost connection to peer");
    }

    if (pCallback->m_info.m_eState == k_ESteamNetworkingConnectionState_FindingRoute)
    {
        _connection_status = MultiplayerPeer::CONNECTION_CONNECTING;
        UtilityFunctions::print("SteamDatagramRelayPeer: Finding route...\n");
    }

    if (pCallback->m_info.m_eState == k_ESteamNetworkingConnectionState_Connected)
    {
        _connection_status = MultiplayerPeer::CONNECTION_CONNECTED;
        UtilityFunctions::print("SteamDatagramRelayPeer: Connected!\n");

        connections.push_back(pCallback->m_hConn);
    }

    if (pCallback->m_info.m_eState == k_ESteamNetworkingConnectionState_None)
    {
        _connection_status = MultiplayerPeer::CONNECTION_DISCONNECTED;

        // check if connections contains pCallback->m_hConn
        if (connections.has(pCallback->m_hConn))
        {
            connections.erase(pCallback->m_hConn);
        }

        UtilityFunctions::print("SteamDatagramRelayPeer: Connection state is none\n");
    }

    if (pCallback->m_info.m_eState == k_ESteamNetworkingConnectionState_ProblemDetectedLocally)
    {
        _connection_status = MultiplayerPeer::CONNECTION_DISCONNECTED;

        // check if connections contains pCallback->m_hConn
        if (connections.has(pCallback->m_hConn))
        {
            connections.erase(pCallback->m_hConn);
        }

        UtilityFunctions::print("SteamDatagramRelayPeer: Problem detected locally\n");
    }
}

/// @brief Constructor
SteamDatagramRelayPeer::SteamDatagramRelayPeer()
{
}

/// @brief Destructor
SteamDatagramRelayPeer::~SteamDatagramRelayPeer()
{
    // close all connections
    this->_close();
}

// host game
String SteamDatagramRelayPeer::host_game_p2p()
{
    // initialize connection to Steam Relay Network
    // this enables P2P functionality
    UtilityFunctions::print("SteamDatagramRelayPeer: Initializing relay network...\n");
    SteamNetworkingUtils()->InitRelayNetworkAccess();

    // print
    UtilityFunctions::print("SteamDatagramRelayPeer: Hosting game...\n");
    this->m_hListenSock = SteamNetworkingSockets()->CreateListenSocketP2P(0, 0, nullptr);
    // return the identity of the host
    SteamNetworkingIdentity steam_net_identity;
    SteamNetworkingSockets()->GetIdentity(&steam_net_identity);

    add_peer(steam_net_identity.GetSteamID64());

    // allocate buffer with size SteamNetworkingIdentity::k_cchMaxString
    char *buffer = new char[SteamNetworkingIdentity::k_cchMaxString];
    // copy string from steam_net_identity to steam_identity
    steam_net_identity.ToString(buffer, SteamNetworkingIdentity::k_cchMaxString);
    // print
    String steam_identity = String(buffer);
    UtilityFunctions::print("SteamDatagramRelayPeer: Host identity: " + steam_identity);
    return steam_identity;
}

// join game
void SteamDatagramRelayPeer::join_game_p2p(const String &steam_identity)
{
    // initialize connection to Steam Relay Network
    // this enables P2P functionality
    UtilityFunctions::print("SteamDatagramRelayPeer: Initializing relay network...\n");
    SteamNetworkingUtils()->InitRelayNetworkAccess();

    SteamNetworkingIdentity steam_net_identity;
    steam_net_identity.ParseString(steam_identity.utf8().get_data());
    // print
    UtilityFunctions::print("SteamDatagramRelayPeer: Joining host with identity: " + steam_identity);

    // connect to host
    this->m_hConn = SteamNetworkingSockets()->ConnectP2P(steam_net_identity, 0, 0, nullptr);

    // print connection status using GetDetailedConnectionStatus
    char *buffer = new char[1024];
    SteamNetworkingSockets()->GetDetailedConnectionStatus(this->m_hConn, buffer, 1024);
    UtilityFunctions::print("SteamDatagramRelayPeer: Connection status: %s\n", buffer);
}

void SteamDatagramRelayPeer::_close()
{
    if (this->connections.size() > 0)
    {
        // close connection to Steam Relay Network
        if (this->m_hConn != k_HSteamNetConnection_Invalid)
            SteamNetworkingSockets()->CloseConnection(this->m_hConn, 0, nullptr, false);
        if (this->m_hListenSock != k_HSteamListenSocket_Invalid)
            SteamNetworkingSockets()->CloseListenSocket(this->m_hListenSock);
    }
}

void SteamDatagramRelayPeer::_disconnect_peer(int p_peer, bool p_force) {}

int SteamDatagramRelayPeer::_get_available_packet_count() const
{
    return this->_packets.size();
}

MultiplayerPeer::ConnectionStatus SteamDatagramRelayPeer::_get_connection_status() const
{
    return this->_connection_status;
}

int SteamDatagramRelayPeer::_get_max_packet_size() const { return 4096; }

Error SteamDatagramRelayPeer::_get_packet(const uint8_t **r_buffer, int32_t *r_buffer_size) { return OK; }

int SteamDatagramRelayPeer::_get_packet_channel() const { return 0; }

MultiplayerPeer::TransferMode SteamDatagramRelayPeer::_get_packet_mode() const { return this->_transfer_mode; }

int SteamDatagramRelayPeer::_get_packet_peer() const
{
    return this->_packets.begin()->sender_peer_id;
}

PackedByteArray SteamDatagramRelayPeer::_get_packet_script()
{
    // get packet
    PackedByteArray packet_data = this->_packets.begin()->data;
    // pop packet from front of queue
    this->_packets.remove_at(0);
    // return packet
    return packet_data;
}

int SteamDatagramRelayPeer::_get_transfer_channel() const
{
    return this->_transfer_channel;
}

MultiplayerPeer::TransferMode
SteamDatagramRelayPeer::_get_transfer_mode() const
{
    return this->_transfer_mode;
}

int SteamDatagramRelayPeer::_get_unique_id() const
{
    // return id of peer
    SteamNetworkingIdentity steam_net_identity;
    SteamNetworkingSockets()->GetIdentity(&steam_net_identity);
    return this->_steam_id_to_peer_id[steam_net_identity.GetSteamID64()];
}

bool SteamDatagramRelayPeer::_is_refusing_new_connections() const { return this->_refuse_new_connections; }

bool SteamDatagramRelayPeer::_is_server() const
{
    if (TARGET_PEER_SERVER == _get_unique_id())
        return true;
    return false;
}

bool SteamDatagramRelayPeer::_is_server_relay_supported() const { return false; }

void SteamDatagramRelayPeer::_poll()
{
    SteamNetworkingMessage_t *pIncomingMsg = new SteamNetworkingMessage_t[MAX_PACKETS_READ_PER_POLL];

    int num_messages = SteamNetworkingSockets()->ReceiveMessagesOnConnection(this->m_hConn, &pIncomingMsg, MAX_PACKETS_READ_PER_POLL);
    if (num_messages == 0)
        return;

    for (int i = 0; i < num_messages; i++)
    {
        // get packet data
        Packet packet;

        SteamNetworkingMessage_t *pMsg = &pIncomingMsg[i];
        packet.data.resize(pMsg->m_cbSize);
        memcpy(packet.data.ptrw(), pMsg->m_pData, pMsg->m_cbSize);

        // get sender peer id
        int sender_peer_id = this->_steam_id_to_peer_id[pMsg->m_identityPeer.GetSteamID64()];
        packet.sender_peer_id = sender_peer_id;
        // add to packets array
        this->_packets.push_back(packet);
    }
}

Error SteamDatagramRelayPeer::_put_packet(const uint8_t *p_buffer, int p_buffer_size)
{
    // steam mode
    int send_type = _transfer_mode == MultiplayerPeer::TRANSFER_MODE_UNRELIABLE ? k_nSteamNetworkingSend_Unreliable : k_nSteamNetworkingSend_Reliable;

    if (this->_target_peer == TARGET_PEER_BROADCAST)
    {
        // we're the server, send to all peers
        for (int i = 0; i < connections.size(); i++)
        {
            // send packet
            SteamNetworkingSockets()->SendMessageToConnection(connections[i], p_buffer, p_buffer_size, send_type, nullptr);
        }
    }
    else
    {
        // send packet
        SteamNetworkingSockets()->SendMessageToConnection(this->m_hConn, p_buffer, p_buffer_size, send_type, nullptr);
    }

    return OK;
}

void SteamDatagramRelayPeer::_set_refuse_new_connections(bool p_enable)
{
    this->_refuse_new_connections = p_enable;
}

void SteamDatagramRelayPeer::_set_target_peer(int p_peer)
{
    this->_target_peer = p_peer;
}

void SteamDatagramRelayPeer::_set_transfer_channel(int p_channel)
{
    this->_transfer_channel = p_channel;
}

void SteamDatagramRelayPeer::_set_transfer_mode(MultiplayerPeer::TransferMode p_mode)
{
    this->_transfer_mode = p_mode;
}