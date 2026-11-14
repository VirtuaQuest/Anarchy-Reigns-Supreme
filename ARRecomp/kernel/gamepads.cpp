// Controllers of every kind through SDL, read as Xbox 360 controllers (see gamepads.h). SDL runs on a thread of its
// own, which keeps what each controller is doing for the game to read.
#include <stdafx.h>
#include <kernel/gamepads.h>
#include <SDL3/SDL.h>
#include <future>

namespace pads
{
    namespace
    {
        constexpr int SLOTS = XUSER_MAX_COUNT;

        // What the game reads, written by SDL's thread.
        struct Slot
        {
            bool connected = false;
            bool rumble = false;
            XINPUT_GAMEPAD gamepad{};
            DWORD packet = 0;
        };

        std::mutex g_mutex;
        Slot g_slots[SLOTS];
        std::atomic<uint32_t> g_vibration[SLOTS]; // Asked for by the game: left motor << 16 | right motor.

        // Face buttons by where they are, so the bottom one is A on every controller, as the game's prompts show.
        constexpr std::pair<SDL_GamepadButton, WORD> BUTTONS[] =
        {
            { SDL_GAMEPAD_BUTTON_SOUTH, XINPUT_GAMEPAD_A }, { SDL_GAMEPAD_BUTTON_EAST, XINPUT_GAMEPAD_B },
            { SDL_GAMEPAD_BUTTON_WEST, XINPUT_GAMEPAD_X }, { SDL_GAMEPAD_BUTTON_NORTH, XINPUT_GAMEPAD_Y },
            { SDL_GAMEPAD_BUTTON_BACK, XINPUT_GAMEPAD_BACK }, { SDL_GAMEPAD_BUTTON_START, XINPUT_GAMEPAD_START },
            { SDL_GAMEPAD_BUTTON_LEFT_STICK, XINPUT_GAMEPAD_LEFT_THUMB }, { SDL_GAMEPAD_BUTTON_RIGHT_STICK, XINPUT_GAMEPAD_RIGHT_THUMB },
            { SDL_GAMEPAD_BUTTON_LEFT_SHOULDER, XINPUT_GAMEPAD_LEFT_SHOULDER }, { SDL_GAMEPAD_BUTTON_RIGHT_SHOULDER, XINPUT_GAMEPAD_RIGHT_SHOULDER },
            { SDL_GAMEPAD_BUTTON_DPAD_UP, XINPUT_GAMEPAD_DPAD_UP }, { SDL_GAMEPAD_BUTTON_DPAD_DOWN, XINPUT_GAMEPAD_DPAD_DOWN },
            { SDL_GAMEPAD_BUTTON_DPAD_LEFT, XINPUT_GAMEPAD_DPAD_LEFT }, { SDL_GAMEPAD_BUTTON_DPAD_RIGHT, XINPUT_GAMEPAD_DPAD_RIGHT },
        };

        XINPUT_GAMEPAD ReadGamepad(SDL_Gamepad* pad)
        {
            XINPUT_GAMEPAD gamepad{};
            for (auto [button, bit] : BUTTONS)
            {
                if (SDL_GetGamepadButton(pad, button))
                    gamepad.wButtons |= bit;
            }

            // Triggers go from 0 to 32767; sticks are the same as XInput's, apart from up being negative.
            auto trigger = [&](SDL_GamepadAxis axis) { return BYTE((std::max<int>(SDL_GetGamepadAxis(pad, axis), 0) * 255 + 16383) / 32767); };
            auto up = [&](SDL_GamepadAxis axis) { return SHORT(std::clamp(-int(SDL_GetGamepadAxis(pad, axis)), -32768, 32767)); };
            gamepad.bLeftTrigger = trigger(SDL_GAMEPAD_AXIS_LEFT_TRIGGER);
            gamepad.bRightTrigger = trigger(SDL_GAMEPAD_AXIS_RIGHT_TRIGGER);
            gamepad.sThumbLX = SDL_GetGamepadAxis(pad, SDL_GAMEPAD_AXIS_LEFTX);
            gamepad.sThumbLY = up(SDL_GAMEPAD_AXIS_LEFTY);
            gamepad.sThumbRX = SDL_GetGamepadAxis(pad, SDL_GAMEPAD_AXIS_RIGHTX);
            gamepad.sThumbRY = up(SDL_GAMEPAD_AXIS_RIGHTY);
            return gamepad;
        }

        void Run(std::promise<bool>& started)
        {
            // The Steam Controller from 2015 isn't read by default (the one from 2026 is). Controllers are read with the
            // window in the back too, as XInput does, for two copies of the game on one PC. The game sets the timer's
            // resolution itself, and handles Ctrl+C.
            SDL_SetHint(SDL_HINT_JOYSTICK_HIDAPI_STEAM, "1");
            SDL_SetHint(SDL_HINT_JOYSTICK_ALLOW_BACKGROUND_EVENTS, "1");
            SDL_SetHint(SDL_HINT_JOYSTICK_THREAD, "1");
            SDL_SetHint(SDL_HINT_TIMER_RESOLUTION, "0");
            SDL_SetHint(SDL_HINT_NO_SIGNAL_HANDLERS, "1");
            if (!SDL_Init(SDL_INIT_GAMEPAD))
            {
                LOGF_WARNING("SDL didn't start ({}): only XInput controllers work", SDL_GetError());
                started.set_value(false);
                return;
            }

            // Only connecting and disconnecting go through events; what controllers do is read below.
            for (uint32_t type : { SDL_EVENT_JOYSTICK_AXIS_MOTION, SDL_EVENT_JOYSTICK_BALL_MOTION, SDL_EVENT_JOYSTICK_HAT_MOTION,
                     SDL_EVENT_JOYSTICK_BUTTON_DOWN, SDL_EVENT_JOYSTICK_BUTTON_UP, SDL_EVENT_JOYSTICK_BATTERY_UPDATED,
                     SDL_EVENT_GAMEPAD_AXIS_MOTION, SDL_EVENT_GAMEPAD_BUTTON_DOWN, SDL_EVENT_GAMEPAD_BUTTON_UP,
                     SDL_EVENT_GAMEPAD_TOUCHPAD_DOWN, SDL_EVENT_GAMEPAD_TOUCHPAD_MOTION, SDL_EVENT_GAMEPAD_TOUCHPAD_UP,
                     SDL_EVENT_GAMEPAD_SENSOR_UPDATE })
                SDL_SetEventEnabled(type, false);

            SDL_Gamepad* open[SLOTS] = {};
            bool rumble[SLOTS] = {};
            std::vector<SDL_Gamepad*> waiting; // More than four: they get the first place to come free.
            uint32_t vibrating[SLOTS] = {};
            uint64_t vibrationTime[SLOTS] = {};

            auto place = [&](SDL_Gamepad* pad)
                {
                    // An Xbox controller keeps its number; others get the first free place.
                    int slot = SDL_GetGamepadPlayerIndex(pad);
                    if (slot < 0 || slot >= SLOTS || open[slot])
                        slot = int(std::find(open, open + SLOTS, nullptr) - open);
                    if (slot >= SLOTS)
                    {
                        waiting.push_back(pad);
                        return;
                    }
                    open[slot] = pad;
                    rumble[slot] = SDL_GetBooleanProperty(SDL_GetGamepadProperties(pad), SDL_PROP_GAMEPAD_CAP_RUMBLE_BOOLEAN, false);
                    vibrating[slot] = 0;
                    SDL_SetGamepadPlayerIndex(pad, slot);
                    const char* name = SDL_GetGamepadName(pad);
                    LOGF_INFO("controller {}: {} ({}{})", slot + 1, name ? name : "unnamed", SDL_GetGamepadStringForType(SDL_GetGamepadType(pad)),
                        rumble[slot] ? ", rumble" : "");
                };

            auto events = [&]
                {
                    SDL_Event event;
                    while (SDL_PollEvent(&event))
                    {
                        if (event.type == SDL_EVENT_GAMEPAD_ADDED)
                        {
                            if (SDL_Gamepad* pad = SDL_OpenGamepad(event.gdevice.which))
                                place(pad);
                            else
                                LOGF_WARNING("a controller couldn't be opened: {}", SDL_GetError());
                        }
                        else if (event.type == SDL_EVENT_GAMEPAD_REMOVED)
                        {
                            for (int slot = 0; slot < SLOTS; slot++)
                            {
                                if (open[slot] && SDL_GetGamepadID(open[slot]) == event.gdevice.which)
                                {
                                    LOGF_INFO("controller {} disconnected", slot + 1);
                                    SDL_CloseGamepad(open[slot]);
                                    open[slot] = nullptr;
                                }
                            }
                            std::erase_if(waiting, [&](SDL_Gamepad* pad)
                                {
                                    if (SDL_GetGamepadID(pad) != event.gdevice.which)
                                        return false;
                                    SDL_CloseGamepad(pad);
                                    return true;
                                });
                            while (!waiting.empty() && std::find(open, open + SLOTS, nullptr) != open + SLOTS)
                            {
                                SDL_Gamepad* pad = waiting.front();
                                waiting.erase(waiting.begin());
                                place(pad);
                            }
                        }
                    }
                };

            auto publish = [&]
                {
                    std::lock_guard lock(g_mutex);
                    for (int slot = 0; slot < SLOTS; slot++)
                    {
                        Slot& out = g_slots[slot];
                        if (!open[slot])
                        {
                            out = {};
                            continue;
                        }
                        XINPUT_GAMEPAD gamepad = ReadGamepad(open[slot]);
                        if (!out.connected || memcmp(&gamepad, &out.gamepad, sizeof(gamepad)) != 0)
                            out.packet++;
                        out.connected = true;
                        out.rumble = rumble[slot];
                        out.gamepad = gamepad;
                    }
                };

            events();
            publish();
            started.set_value(true);

            // Every 2 ms, well within a frame at 120 fps. SDL takes about 2% of one core, most of it in its own threads.
            while (true)
            {
                SDL_Delay(2);
                events();
                publish();

                // Rumble lasts as long as it's asked for: SDL's stops by itself, so it's asked for again every second.
                uint64_t now = SDL_GetTicks();
                for (int slot = 0; slot < SLOTS; slot++)
                {
                    uint32_t wanted = g_vibration[slot].load();
                    if (!open[slot] || !rumble[slot] || (wanted == vibrating[slot] && (wanted == 0 || now - vibrationTime[slot] < 1000)))
                        continue;
                    SDL_RumbleGamepad(open[slot], Uint16(wanted >> 16), Uint16(wanted), 2000);
                    vibrating[slot] = wanted;
                    vibrationTime[slot] = now;
                }
            }
        }
    }

    bool Enabled()
    {
        static const bool enabled = []
            {
                if (AR_ENV("AR_XINPUT"))
                {
                    LOGF_INFO("controllers through XInput only (AR_XINPUT)");
                    return false;
                }

                // Starts SDL and waits for the controllers already connected, so the game sees them from the first look.
                auto started = std::make_shared<std::promise<bool>>();
                std::future<bool> result = started->get_future();
                std::thread([started]
                    {
                        SetThreadDescription(GetCurrentThread(), L"Controllers");
                        Run(*started);
                    }).detach();
                if (result.wait_for(std::chrono::seconds(5)) != std::future_status::ready)
                {
                    LOGF_WARNING("SDL took too long to start: only XInput controllers work");
                    return false;
                }
                return result.get();
            }();
        return enabled;
    }

    DWORD GetState(DWORD index, XINPUT_STATE* state)
    {
        if (!Enabled())
            return XInputGetState(index, state);
        if (index >= SLOTS)
            return ERROR_BAD_ARGUMENTS;

        std::lock_guard lock(g_mutex);
        const Slot& slot = g_slots[index];
        if (!slot.connected)
            return ERROR_DEVICE_NOT_CONNECTED;
        state->dwPacketNumber = slot.packet;
        state->Gamepad = slot.gamepad;
        return ERROR_SUCCESS;
    }

    DWORD GetCapabilities(DWORD index, XINPUT_CAPABILITIES* capabilities)
    {
        if (!Enabled())
            return XInputGetCapabilities(index, XINPUT_FLAG_GAMEPAD, capabilities);
        if (index >= SLOTS)
            return ERROR_BAD_ARGUMENTS;

        std::lock_guard lock(g_mutex);
        const Slot& slot = g_slots[index];
        if (!slot.connected)
            return ERROR_DEVICE_NOT_CONNECTED;

        // Everything an Xbox 360 controller has, as XInput reports it.
        *capabilities = {};
        capabilities->Type = XINPUT_DEVTYPE_GAMEPAD;
        capabilities->SubType = XINPUT_DEVSUBTYPE_GAMEPAD;
        capabilities->Gamepad.wButtons = 0xF3FF;
        capabilities->Gamepad.bLeftTrigger = 0xFF;
        capabilities->Gamepad.bRightTrigger = 0xFF;
        capabilities->Gamepad.sThumbLX = capabilities->Gamepad.sThumbLY = SHORT(0xFFC0);
        capabilities->Gamepad.sThumbRX = capabilities->Gamepad.sThumbRY = SHORT(0xFFC0);
        if (slot.rumble)
            capabilities->Vibration.wLeftMotorSpeed = capabilities->Vibration.wRightMotorSpeed = 0xFF;
        return ERROR_SUCCESS;
    }

    DWORD SetState(DWORD index, XINPUT_VIBRATION* vibration)
    {
        if (!Enabled())
            return XInputSetState(index, vibration);
        if (index >= SLOTS)
            return ERROR_BAD_ARGUMENTS;

        g_vibration[index] = uint32_t(vibration->wLeftMotorSpeed) << 16 | vibration->wRightMotorSpeed;
        std::lock_guard lock(g_mutex);
        return g_slots[index].connected ? ERROR_SUCCESS : ERROR_DEVICE_NOT_CONNECTED;
    }
}
