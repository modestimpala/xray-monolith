#include "stdafx.h"
#pragma hdrstop

#include "SoundRender_Core.h"
#include "SoundRender_Source_Live.h"

CSoundRender_Source_Live::CSoundRender_Source_Live(LPCSTR name, u16 channels, u32 sample_rate, u32 ring_bytes)
{
	// clamp to what the OpenAL streaming target supports (MONO16 / STEREO16)
	if (channels < 1) channels = 1;
	if (channels > 2) channels = 2;

	pname = name;
	fname = name;

	ZeroMemory(&m_wformat, sizeof(WAVEFORMATEX));
	m_wformat.wFormatTag = WAVE_FORMAT_PCM;
	m_wformat.nChannels = channels;
	m_wformat.nSamplesPerSec = sample_rate;
	m_wformat.wBitsPerSample = 16;
	m_wformat.nBlockAlign = m_wformat.wBitsPerSample / 8 * m_wformat.nChannels;
	m_wformat.nAvgBytesPerSec = m_wformat.nSamplesPerSec * m_wformat.nBlockAlign;

	// align ring capacity to a whole sample frame
	if (m_wformat.nBlockAlign)
		ring_bytes -= ring_bytes % m_wformat.nBlockAlign;
	if (ring_bytes < m_wformat.nBlockAlign)
		ring_bytes = m_wformat.nBlockAlign;

	m_ring.resize(ring_bytes);
	ZeroMemory(m_ring.data(), ring_bytes);
	m_write = 0;
	m_read = 0;
	m_avail = 0;

	// dwBytesTotal governs the looped-emitter wrap; fTimeTotal is "endless"
	dwBytesTotal = ring_bytes;
	fTimeTotal = float(ring_bytes) / float(m_wformat.nAvgBytesPerSec);

	// 2D radio: distance attenuation is irrelevant, keep sane defaults from base
	m_fBaseVolume = 1.f;
	m_uGameType = 0;
}

CSoundRender_Source_Live::~CSoundRender_Source_Live()
{
	// base ~CSoundRender_Source calls unload() -> cache.cat_destroy(CAT);
	// CAT was never created (table==0) so cat_destroy is a safe no-op.
}

void CSoundRender_Source_Live::push_pcm(const void* data, u32 bytes)
{
	if (!data || !bytes) return;

	m_cs.Enter();

	const u32 size = (u32)m_ring.size();
	const u8* src = (const u8*)data;

	// if a single push is larger than the whole ring, keep only the tail
	if (bytes >= size)
	{
		src += (bytes - size);
		bytes = size;
		m_write = 0;
		m_read = 0;
		m_avail = size;
		CopyMemory(m_ring.data(), src, size);
		m_cs.Leave();
		return;
	}

	// copy with wraparound at the write cursor
	u32 first = _min(bytes, size - m_write);
	CopyMemory(m_ring.data() + m_write, src, first);
	if (bytes > first)
		CopyMemory(m_ring.data(), src + first, bytes - first);
	m_write = (m_write + bytes) % size;

	// account available; if we overwrote unread data, advance the read cursor
	m_avail += bytes;
	if (m_avail > size)
	{
		u32 lost = m_avail - size;
		m_read = (m_read + lost) % size;
		m_avail = size;
	}

	m_cs.Leave();
}

void CSoundRender_Source_Live::read_pcm(void* dst, u32 bytes)
{
	if (!dst || !bytes) return;

	m_cs.Enter();

	const u32 size = (u32)m_ring.size();
	u8* out = (u8*)dst;

	u32 can = _min(bytes, m_avail);
	if (can)
	{
		u32 first = _min(can, size - m_read);
		CopyMemory(out, m_ring.data() + m_read, first);
		if (can > first)
			CopyMemory(out + first, m_ring.data(), can - first);
		m_read = (m_read + can) % size;
		m_avail -= can;
	}

	// underrun: zero-fill the remainder so we never play stale/garbage PCM
	if (can < bytes)
		ZeroMemory(out + can, bytes - can);

	m_cs.Leave();
}

void CSoundRender_Source_Live::flush()
{
	m_cs.Enter();
	m_read = m_write;
	m_avail = 0;
	m_cs.Leave();
}
