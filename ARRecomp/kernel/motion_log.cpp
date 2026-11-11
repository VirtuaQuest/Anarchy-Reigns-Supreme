// AR_LOG_MOTION: who changes the players' animation (+16124 in a player) in a match, to find why a hit player's animation bounces on the attacker's
// copy (docs/LESSONS.md, Multiplayer). Each function known to store it is wrapped: the players' animations before and after, with the caller. Once a frame,
// anything else that changed them is reported too. Off by default; costs nothing then.
#include <stdafx.h>
#include <kernel/function.h>
#include <kernel/net_session.h>
#include <kernel/memory.h>

namespace
{
    constexpr uint32_t PLAYERS = 0x832AE200; // Player objects by session slot.
    constexpr uint32_t LOCAL_PLAYER = 0x831C51C0 + 6208;
    constexpr uint32_t SLOTS = 16;
    constexpr uint32_t MOTION = 16124;

    bool Enabled()
    {
        static const bool enabled = AR_ENV("AR_LOG_MOTION") != nullptr;
        return enabled;
    }

    struct Snapshot
    {
        uint32_t object[SLOTS];
        uint16_t motion[SLOTS];
    };

    bool Plausible(uint32_t object)
    {
        return object >= 0x40000000 && object < 0xC0000000;
    }

    Snapshot Take(uint8_t* base)
    {
        Snapshot s{};
        for (uint32_t i = 0; i < SLOTS; i++)
        {
            s.object[i] = PPC_LOAD_U32(PLAYERS + 4 * i);
            s.motion[i] = Plausible(s.object[i]) ? PPC_LOAD_U16(s.object[i] + MOTION) : 0;
        }
        return s;
    }

    std::mutex g_mutex;
    Snapshot g_last{};
    uint64_t g_frame = 0;
    thread_local int t_depth = 0;

    void Report(const char* who, uint32_t caller, const Snapshot& before, uint8_t* base)
    {
        Snapshot after = Take(base);
        uint32_t local = PPC_LOAD_U32(LOCAL_PLAYER);
        std::lock_guard lock(g_mutex);
        for (uint32_t i = 0; i < SLOTS; i++)
        {
            if (after.object[i] == before.object[i] && after.motion[i] != before.motion[i])
            {
                LOGF_INFO("motion: frame {} slot {} ({}) {:X} -> {:X} by {} called from {:08X}, depth {}", g_frame, i, after.object[i] == local ? "this copy's" : "other",
                    before.motion[i], after.motion[i], who, caller, t_depth);
            }
            g_last.object[i] = after.object[i];
            g_last.motion[i] = after.motion[i];
        }
    }
}

namespace net
{
    // Once a frame: animations that changed without going through a wrapped function.
    void PollMotionLog(uint8_t* base)
    {
        if (!Enabled() || !InSession())
            return;
        Snapshot now = Take(base);
        uint32_t local = PPC_LOAD_U32(LOCAL_PLAYER);
        std::lock_guard lock(g_mutex);
        g_frame++;
        for (uint32_t i = 0; i < SLOTS; i++)
        {
            if (now.object[i] == g_last.object[i] && now.motion[i] != g_last.motion[i] && Plausible(now.object[i]))
                LOGF_INFO("motion: frame {} slot {} ({}) {:X} -> {:X} elsewhere", g_frame, i, now.object[i] == local ? "this copy's" : "other", g_last.motion[i], now.motion[i]);
        }
        g_last = now;
    }
}

#define MOTION_HOOK(name)                                         \
    PPC_FUNC_IMPL(__imp__##name);                                 \
    PPC_FUNC(name)                                                \
    {                                                             \
        if (!Enabled() || !net::InSession())                      \
        {                                                         \
            __imp__##name(ctx, base);                             \
            return;                                               \
        }                                                         \
        uint32_t caller = uint32_t(ctx.lr);                       \
        Snapshot before = Take(base);                             \
        t_depth++;                                                \
        __imp__##name(ctx, base);                                 \
        t_depth--;                                                \
        Report(#name, caller, before, base);                      \
    }

MOTION_HOOK(sub_828E6638)
MOTION_HOOK(sub_828E6740)
MOTION_HOOK(sub_829A8C40)
MOTION_HOOK(sub_829B4680)
MOTION_HOOK(sub_829F0B60)
MOTION_HOOK(sub_829F5720)
MOTION_HOOK(sub_82A30560)
MOTION_HOOK(sub_82A330A8)
MOTION_HOOK(sub_82A337D8)
MOTION_HOOK(sub_82A4FF28)
