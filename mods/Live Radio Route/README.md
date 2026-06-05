# Live Radio Route

Routes a live Windows audio device into the game as an in-game 2D "radio".
It can capture either:

- a render endpoint in loopback mode (whatever your PC is playing), or
- a capture endpoint (microphone, line-in, or a virtual audio cable).

The captured PCM is played through the engine's streaming pipeline as a
looping sound, so it behaves like any other radio.

It cooperates with the in-game PDA radio (Anomaly Radio Revamp / vanilla):
it needs a charged PDA to run, drains the PDA battery while playing, never
plays at the same time as the PDA radio or music player, and remembers its
on/off state across saves (resuming on load).

## Requirements

An engine build that exposes the `radio_channel` Lua class (the xrSound
changes shipped alongside this folder). Without it the scripts no-op safely.

## Install

Copy `gamedata/` into your game (or enable this folder as a mod in MO2).
Standalone: it does not depend on any other radio addon.

## Use

- Default hotkey: Home (toggle on/off). Rebind in MCM.
- MCM (Live Radio): pick the audio device, volume, latency, PDA battery use,
  and in-world reverb.
- "Default output (loopback)" captures the system default playback device.
- Reopen the options menu to refresh the device list after plugging or
  unplugging devices.

## In-world reverb

The reverb option plays the feed as a mono source routed through the level's
EAX reverb, so it echoes your surroundings (caves, bunkers, indoors) without
any distance volume falloff. The "reverb amount" slider controls how wet it
is. This needs EFX sound enabled in the engine; with EFX off it just plays
mono and dry. Note that reverb mode is mono, so it trades stereo separation
for the spatial echo.

## PDA battery

When "use PDA battery" is on (default) the live radio behaves like the
in-game radio: it only starts with a charged PDA in your slot and drains the
battery while playing, switching off if the PDA is removed or runs flat. Turn
it off in MCM if you want the live radio to run regardless.

## Notes

- Expect a short startup delay equal to the latency setting while the ring
  buffer fills (default 1500 ms). Lower it for less delay, raise it to avoid
  dropouts.
- The audio itself is never saved, but the on/off state is: if the live radio
  was playing when you saved, it reopens the device and resumes on load.
