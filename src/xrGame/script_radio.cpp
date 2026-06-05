////////////////////////////////////////////////////////////////////////////
//	Module 		: script_radio.cpp
//	Description : Live external-PCM "radio" channel - implementation.
////////////////////////////////////////////////////////////////////////////

#include "stdafx.h"
#include "script_radio.h"
#include "script_game_object.h"
#include "gameobject.h"
#include "../xrSound/Sound.h"

// Shared device snapshot used by the script device picker (MCM dropdown).
static xr_vector<SSoundCaptureDevice> g_radio_devices;

CScriptRadio::CScriptRadio()
	: m_channel(nullptr), m_volume(1.f), m_ring_ms(1500), m_spatial(false), m_reverb_wet(1.f)
{
}

CScriptRadio::~CScriptRadio()
{
	Close();
}

// Spatial reverb needs a mono source; flat 2D radio stays stereo.
static inline u16 radio_channels(bool spatial) { return spatial ? (u16)1 : (u16)2; }

void CScriptRadio::SetReverb(float wet)
{
	clamp(wet, 0.f, 1.f);
	m_reverb_wet = wet;
	if (m_channel) m_channel->set_spatial(m_spatial, m_reverb_wet);
}

bool CScriptRadio::OpenLoopback(LPCSTR device_id)
{
	if (!::Sound) return false;
	Close();
	LPCSTR id = (device_id && xr_strlen(device_id)) ? device_id : nullptr;
	m_channel = ::Sound->radio_open(id, true, m_ring_ms, radio_channels(m_spatial));
	if (m_channel) m_channel->set_spatial(m_spatial, m_reverb_wet);
	return m_channel != nullptr;
}

bool CScriptRadio::OpenLoopbackDefault()
{
	return OpenLoopback(nullptr);
}

bool CScriptRadio::OpenCapture(LPCSTR device_id)
{
	if (!::Sound) return false;
	Close();
	LPCSTR id = (device_id && xr_strlen(device_id)) ? device_id : nullptr;
	m_channel = ::Sound->radio_open(id, false, m_ring_ms, radio_channels(m_spatial));
	if (m_channel) m_channel->set_spatial(m_spatial, m_reverb_wet);
	return m_channel != nullptr;
}

void CScriptRadio::Close()
{
	if (m_channel && ::Sound)
	{
		::Sound->destroy_live_channel(m_channel);
		m_channel = nullptr;
	}
}

void CScriptRadio::Play(CScriptGameObject* obj)
{
	if (!m_channel) return;
	m_channel->play(obj ? &obj->object() : nullptr, m_volume);
}

void CScriptRadio::Play2D()
{
	if (!m_channel) return;
	m_channel->play(nullptr, m_volume);
}

void CScriptRadio::Stop()
{
	if (m_channel) m_channel->stop();
}

bool CScriptRadio::IsPlaying()
{
	return m_channel ? m_channel->is_playing() : false;
}

void CScriptRadio::SetVolume(float v)
{
	clamp(v, 0.f, 1.f);
	m_volume = v;
	if (m_channel) m_channel->set_volume(v);
}

void CScriptRadio::Clear()
{
	if (m_channel) m_channel->clear();
}

//-----------------------------------------------------------------------------
// device picker
//-----------------------------------------------------------------------------

int CScriptRadio::RefreshDevices()
{
	g_radio_devices.clear();
	if (::Sound) ::Sound->enumerate_capture_devices(g_radio_devices);
	return (int)g_radio_devices.size();
}

int CScriptRadio::DeviceCount()
{
	return (int)g_radio_devices.size();
}

LPCSTR CScriptRadio::DeviceId(int i)
{
	if (i < 0 || i >= (int)g_radio_devices.size()) return "";
	return g_radio_devices[i].id.c_str();
}

LPCSTR CScriptRadio::DeviceName(int i)
{
	if (i < 0 || i >= (int)g_radio_devices.size()) return "";
	return g_radio_devices[i].name.c_str();
}

bool CScriptRadio::DeviceIsRender(int i)
{
	if (i < 0 || i >= (int)g_radio_devices.size()) return false;
	return g_radio_devices[i].is_render;
}
