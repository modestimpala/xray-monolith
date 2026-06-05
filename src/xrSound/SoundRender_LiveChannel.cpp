#include "stdafx.h"
#pragma hdrstop

#include "SoundRender_Core.h"
#include "SoundRender_Source_Live.h"
#include "SoundRender_LiveChannel.h"
#include "Capture_WASAPI.h"

#include <algorithm>

//-----------------------------------------------------------------------------
// Core: live source factory + channel management
//-----------------------------------------------------------------------------

CSoundRender_Source_Live* CSoundRender_Core::i_create_live_source(LPCSTR name, u16 channels, u32 sample_rate,
                                                                  u32 ring_bytes)
{
	CSoundRender_Source_Live* S = xr_new<CSoundRender_Source_Live>(name, channels, sample_rate, ring_bytes);
	s_live_sources.push_back(S);
	return S;
}

void CSoundRender_Core::i_destroy_live_source(CSoundRender_Source_Live* S)
{
	if (!S) return;
	auto it = std::find(s_live_sources.begin(), s_live_sources.end(), S);
	if (it != s_live_sources.end())
	{
		s_live_sources.erase(it);
		xr_delete(S);
	}
}

void CSoundRender_Core::create_live(ref_sound& S, CSoundRender_Source_Live* src, esound_type sound_type, int game_type)
{
	S._p = xr_new<ref_sound_data>(); // default ctor: does NOT call _create_data
	S._p->handle = (CSound_source*)src;
	S._p->g_type = (game_type == sg_SourceType) ? src->game_type() : game_type;
	S._p->s_type = sound_type;
	S._p->feedback = 0;
	S._p->g_object = 0;
	S._p->dwBytesTotal = src->bytes_total();
	S._p->fTimeTotal = src->length_sec();
}

CSound_live_channel* CSoundRender_Core::create_live_channel(u16 channels, u32 sample_rate, u32 ring_ms)
{
	if (!bPresent) return nullptr;
	if (channels < 1) channels = 1;
	if (channels > 2) channels = 2;
	if (sample_rate == 0) sample_rate = 44100;
	if (ring_ms < 100) ring_ms = 100;

	const u32 block = (16 / 8) * channels;
	u32 ring_bytes = (u32)((u64)sample_rate * block * ring_ms / 1000);

	static u32 s_uid = 0;
	string64 name;
	xr_sprintf(name, "$live_radio_%d", ++s_uid);

	CSoundRender_Source_Live* src = i_create_live_source(name, channels, sample_rate, ring_bytes);
	return xr_new<CLiveRadioChannel>(src, st_Music);
}

CSound_live_channel* CSoundRender_Core::radio_open(LPCSTR endpoint_id, bool loopback, u32 ring_ms, u16 channels)
{
	if (!bPresent) return nullptr;

	// Engine-friendly channel format; the capture resamples/downmixes the device
	// to it. Mono (1) is required for the spatial reverb send to be audible.
	CSound_live_channel* ch = create_live_channel(channels, 44100, ring_ms);
	if (!ch) return nullptr;

	if (!ch->start_capture(endpoint_id, loopback))
	{
		destroy_live_channel(ch);
		return nullptr;
	}
	return ch;
}

void CSoundRender_Core::destroy_live_channel(CSound_live_channel* ch)
{
	if (!ch) return;
	xr_delete(ch); // channel dtor stops capture, stops sound, frees its source
}

void CSoundRender_Core::enumerate_capture_devices(xr_vector<SSoundCaptureDevice>& dst)
{
	CSoundRender_Capture_WASAPI::enumerate(dst);
}

//-----------------------------------------------------------------------------
// CLiveRadioChannel
//-----------------------------------------------------------------------------

CLiveRadioChannel::CLiveRadioChannel(CSoundRender_Source_Live* src, esound_type t)
	: m_source(src), m_capture(nullptr), m_type(t), m_volume(1.f)
{
}

CLiveRadioChannel::~CLiveRadioChannel()
{
	stop_capture();
	stop();
	if (m_source)
	{
		SoundRender->i_destroy_live_source(m_source);
		m_source = nullptr;
	}
}

void CLiveRadioChannel::push_pcm(const void* data, u32 bytes)
{
	if (m_source) m_source->push_pcm(data, bytes);
}

void CLiveRadioChannel::play(CObject* O, float volume)
{
	if (!m_source) return;
	m_volume = volume;

	if (m_sound._feedback())
	{
		set_volume(volume);
		return; // already playing
	}

	m_source->flush();
	SoundRender->create_live(m_sound, m_source, m_type, 0);
	m_sound.play(O, sm_2D | sm_Looped);
	m_sound.set_volume(volume);
}

void CLiveRadioChannel::stop()
{
	if (m_sound._feedback())
		m_sound.stop();
	if (m_sound._p)
		m_sound.destroy();
}

bool CLiveRadioChannel::is_playing()
{
	return m_sound._feedback() != nullptr;
}

void CLiveRadioChannel::set_volume(float v)
{
	m_volume = v;
	m_sound.set_volume(v);
}

void CLiveRadioChannel::clear()
{
	if (m_source) m_source->flush();
}

void CLiveRadioChannel::set_spatial(bool on, float wet)
{
	// The source must be mono for OpenAL to route it through the reverb send;
	// the caller picks the channel count at open time accordingly.
	if (m_source) m_source->set_reverb(on, wet);
}

bool CLiveRadioChannel::start_capture(LPCSTR endpoint_id, bool loopback)
{
	if (!m_source) return false;
	if (m_capture) stop_capture();

	m_capture = xr_new<CSoundRender_Capture_WASAPI>();
	if (!m_capture->start(endpoint_id, loopback, this))
	{
		xr_delete(m_capture);
		m_capture = nullptr;
		return false;
	}
	return true;
}

void CLiveRadioChannel::stop_capture()
{
	if (m_capture)
	{
		m_capture->stop();
		xr_delete(m_capture);
		m_capture = nullptr;
	}
}

u16 CLiveRadioChannel::channels() const
{
	return m_source ? m_source->channels_num() : 0;
}

u32 CLiveRadioChannel::sample_rate() const
{
	return m_source ? m_source->m_wformat.nSamplesPerSec : 0;
}
