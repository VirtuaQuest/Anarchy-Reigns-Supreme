#pragma once
#include <Xinput.h>

// Controllers of every kind through SDL: Xbox, PlayStation, Switch, Steam and generic ones, each read as an Xbox 360
// controller. Controller N is the Nth one connected (an Xbox controller keeps its own number), and it stays that one
// until it's unplugged. AR_XINPUT (--xinput) turns it off, and only XInput controllers are read, as before; so does
// SDL failing to start.
namespace pads
{
    // Whether controllers are read through SDL. The first call starts it.
    bool Enabled();

    // The same as XInputGetState, XInputGetCapabilities and XInputSetState for that controller.
    DWORD GetState(DWORD index, XINPUT_STATE* state);
    DWORD GetCapabilities(DWORD index, XINPUT_CAPABILITIES* capabilities);
    DWORD SetState(DWORD index, XINPUT_VIBRATION* vibration);
}
