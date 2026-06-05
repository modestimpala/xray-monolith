#ifndef Capture_WASAPIH
#define Capture_WASAPIH
#pragma once

#include "Sound.h"
#include <thread>

struct IMMDevice;
struct IAudioClient;
struct IAudioCaptureClient;
typedef struct tWAVEFORMATEX WAVEFORMATEX;

// WASAPI capture producer. Strictly a producer: it never touches the renderer or
// OpenAL state, it only converts captured PCM to the sink's format and calls
// sink->push_pcm(). One capture thread per instance.
//
// Conversion pipeline per packet:
//   src (float32 or s16, src_channels, src_rate)
//     -> downmix to dst_channels float
//     -> linear resample src_rate -> dst_rate
//     -> s16 interleaved
//     -> sink->push_pcm()
class CSoundRender_Capture_WASAPI
{
public:
	CSoundRender_Capture_WASAPI();
	~CSoundRender_Capture_WASAPI();

	// endpoint_id==null => default endpoint. loopback=true captures whatever is
	// playing on a render endpoint; false captures a mic/line-in capture endpoint.
	bool start(LPCSTR endpoint_id, bool loopback, CSound_live_channel* sink);
	void stop();
	bool is_active() const { return m_running; }

	// Enumerate render (loopback-able) + capture endpoints for a device picker.
	static void enumerate(xr_vector<SSoundCaptureDevice>& dst);

private:
	void thread_proc();
	bool init_device();
	void shutdown_device();
	void process(const u8* data, u32 frames);
	void emit_resampled(); // flush m_mixbuf -> resample -> s16 -> sink

	CSound_live_channel* m_sink;
	volatile bool        m_running;
	std::thread          m_thread;

	// request params
	string512 m_endpoint_id; // narrow utf-8/ansi copy ("" => default)
	bool      m_loopback;

	// COM device objects (owned by capture thread)
	IMMDevice*           m_device;
	IAudioClient*        m_client;
	IAudioCaptureClient* m_capture;
	WAVEFORMATEX*        m_mixfmt;

	// source format
	u32  m_src_rate;
	u16  m_src_channels;
	bool m_src_float;
	u16  m_src_bits;

	// destination (sink) format
	u32  m_dst_rate;
	u16  m_dst_channels;

	// resampler state
	double m_res_t;          // fractional input-sample phase
	float  m_last[2];        // last downmixed source frame from previous packet
	bool   m_have_last;

	xr_vector<float> m_mixbuf; // interleaved dst-channel float (downmixed source)
	xr_vector<s16>   m_outbuf; // interleaved s16 result
};
#endif
