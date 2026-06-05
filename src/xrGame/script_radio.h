////////////////////////////////////////////////////////////////////////////
//	Module 		: script_radio.h
//	Description : Live external-PCM "radio" channel exposed to scripts.
//	              Captures an audio device (WASAPI loopback or mic/line) and
//	              plays it through the engine as a 2D looped sound.
////////////////////////////////////////////////////////////////////////////

#pragma once

#include "script_export_space.h"

class CScriptGameObject;
class CSound_live_channel;

class CScriptRadio
{
	CSound_live_channel* m_channel;
	float                m_volume;
	u32                  m_ring_ms;
	bool                 m_spatial;    // spatial/reverb mode (mono + env reverb send)
	float                m_reverb_wet; // reverb send level [0..1]

public:
	CScriptRadio();
	virtual ~CScriptRadio();

	// open a render endpoint in loopback mode (captures whatever it plays)
	bool OpenLoopback(LPCSTR device_id); //!< device_id may be "" / nil => default
	bool OpenLoopbackDefault();
	// open a capture endpoint (microphone / line-in)
	bool OpenCapture(LPCSTR device_id);

	bool IsOpen() const { return m_channel != nullptr; }
	void Close();

	void  SetRingMs(u32 ms) { m_ring_ms = ms; }
	u32   GetRingMs() const { return m_ring_ms; }

	// "in-world" reverb mode. Takes effect on the next open() (mono needs a
	// re-open); call SetReverb() alone to retune wet while already playing.
	void  SetSpatial(bool on) { m_spatial = on; }
	bool  IsSpatial() const { return m_spatial; }
	void  SetReverb(float wet);
	float GetReverb() const { return m_reverb_wet; }

	void  Play(CScriptGameObject* obj);
	void  Play2D(); //!< play with no game object (pure non-positional)
	void  Stop();
	bool  IsPlaying();
	void  SetVolume(float v);
	float GetVolume() const { return m_volume; }
	void  Clear();

	// device picker support (snapshot based, refresh then query by index)
	static int    RefreshDevices();
	static int    DeviceCount();
	static LPCSTR DeviceId(int i);
	static LPCSTR DeviceName(int i);
	static bool   DeviceIsRender(int i);

	DECLARE_SCRIPT_REGISTER_FUNCTION
};
