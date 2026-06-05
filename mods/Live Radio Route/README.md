# Live Radio Route

Routes a live Windows audio device into the game as an in-game 2D "radio".
It can capture either:

- a render endpoint in loopback mode (whatever your PC is playing), or
- a capture endpoint (microphone, line-in, or a virtual audio cable).

The captured PCM is played through the engine's streaming pipeline as a
looping, non-positional sound, so it behaves like any other radio.

## Requirements

An engine build that exposes the `radio_channel` Lua class (the xrSound
changes shipped alongside this folder). Without it the scripts no-op safely.

## Install

Copy `gamedata/` into your game (or enable this folder as a mod in MO2).
Standalone: it does not depend on any other radio addon.

## Use

- Default hotkey: Home (toggle on/off). Rebind in MCM.
- MCM (Live Radio): pick the audio device, volume, and latency.
- "Default output (loopback)" captures the system default playback device.
- Reopen the options menu to refresh the device list after plugging or
  unplugging devices.

## Notes

- Expect a short startup delay equal to the latency setting while the ring
  buffer fills (default 1500 ms). Lower it for less delay, raise it to avoid
  dropouts.
- The live feed is never saved; it stops on actor unload.
