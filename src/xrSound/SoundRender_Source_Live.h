#ifndef SoundRender_Source_LiveH
#define SoundRender_Source_LiveH
#pragma once

#include "SoundRender_Source.h"

// A "live" sound source: instead of decoding an OGG file it owns a lock-protected
// ring buffer that an external producer (e.g. a WASAPI capture thread) pushes raw
// PCM into. The streaming consumer (CSoundRender_Emitter::fill_data) reads straight
// from the ring, bypassing the OGG decompress cache entirely.
//
// Producer (push_pcm) and consumer (read_pcm) run on different threads, so both
// take m_cs. The ring keeps the most recent N bytes of audio; if the consumer
// outruns the producer it is zero-filled (brief silence), if the producer laps the
// consumer the oldest unread data is dropped (acceptable for live audio).
class CSoundRender_Source_Live : public CSoundRender_Source
{
	xrCriticalSection m_cs;
	xr_vector<u8>     m_ring;   // raw PCM, capacity = ring_bytes
	u32               m_write;  // write cursor inside the ring (producer)
	u32               m_read;   // read cursor inside the ring (consumer)
	u32               m_avail;  // bytes written but not yet read (<= ring size)

public:
	CSoundRender_Source_Live(LPCSTR name, u16 channels, u32 sample_rate, u32 ring_bytes);
	virtual ~CSoundRender_Source_Live();

	virtual bool is_live() const override { return true; }

	// producer API (any thread)
	void push_pcm(const void* data, u32 bytes);

	// consumer API (sound update) - fills 'bytes' of PCM, zero-filling underrun.
	// 'offset' (the emitter stream cursor) is ignored: a live feed is served from
	// an internal read pointer that tracks the producer.
	void read_pcm(void* dst, u32 bytes);

	// drop everything currently buffered (e.g. on resume / device switch)
	void flush();

	u32  ring_size() const { return (u32)m_ring.size(); }
	u32  available() const { return m_avail; }
};
#endif
