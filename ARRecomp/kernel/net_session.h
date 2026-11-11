#pragma once

// Experimental multiplayer between copies of the game (AR_ONLINE), with no online service: see imports/net.cpp and net_session.cpp.
namespace net
{
    bool Online();

    static constexpr uint16_t BASE_PORT = 36000, PORT_BLOCK = 16, DISCOVERY_OFFSET = 15, MAX_INSTANCES = 8;

    // This copy's first port, and its XNADDR (36 bytes).
    uint16_t LocalBase();
    void FillLocalAddress(uint8_t* address);

    // The profile's name, which a game's host shows to others (the title checks it when they join).
    void SetHostName(const char* name);

    // Answers other copies looking for a game, on the port already bound to this socket.
    void StartDiscovery(uint64_t socket);

    // Keeps a UDP socket working when a packet it sent finds nobody listening: Windows would otherwise fail its next receive (WSAECONNRESET), which a console never does.
    void IgnoreUnreachable(uint64_t socket);

    // The broadcast address of every network this PC is on, so a search reaches them all (255.255.255.255 leaves through one adapter only).
    std::vector<uint32_t> BroadcastAddresses();

    // An IPv4 address or a host name, in network byte order. False if it can't be found.
    bool ResolveAddress(const std::string& text, uint32_t& address);

    // Whether the title has a session open, hosted or joined: from the lobby to leaving the match.
    bool InSession();

    // AR_LOG_MOTION, once a frame: players' animations that changed outside the functions it wraps (kernel/motion_log.cpp).
    void PollMotionLog(uint8_t* base);

    // The title's session messages to the XGI service. Returns a Win32 style result; true if the message was handled here.
    bool HandleSessionMessage(uint32_t message, uint32_t buffer, uint32_t bufferLength, uint32_t& result);
}
