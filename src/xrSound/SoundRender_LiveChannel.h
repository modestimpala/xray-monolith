#ifndef SoundRender_LiveChannelH
#define SoundRender_LiveChannelH
#pragma once

#include "Sound.h"

class CSoundRender_Source_Live;
class CSoundRender_Capture_WASAPI;

// Concrete live channel: bundles a ring-buffer source, a ref_sound played as a 2D
// looped "radio", and an optional WASAPI capture producer. Owned by the caller via
// CSoundRender_Core::destroy_live_channel().
class CLiveRadioChannel : public CSound_live_channel
{
	CSoundRender_Source_Live*    m_source;
	ref_sound                    m_sound;
	CSoundRender_Capture_WASAPI* m_capture;
	esound_type                  m_type;
	float                        m_volume;

public:
	CLiveRadioChannel(CSoundRender_Source_Live* src, esound_type t);
	virtual ~CLiveRadioChannel();

	CSoundRender_Source_Live* source() const { return m_source; }

	virtual void push_pcm(const void* data, u32 bytes) override;
	virtual void play(CObject* O, float volume) override;
	virtual void stop() override;
	virtual bool is_playing() override;
	virtual void set_volume(float v) override;
	virtual void clear() override;

	virtual bool start_capture(LPCSTR endpoint_id, bool loopback) override;
	virtual void stop_capture() override;

	virtual u16 channels() const override;
	virtual u32 sample_rate() const override;
};
#endif
