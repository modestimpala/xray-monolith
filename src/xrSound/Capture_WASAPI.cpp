#include "stdafx.h"
#pragma hdrstop

#include "Capture_WASAPI.h"

#include <mmdeviceapi.h>
#include <audioclient.h>
#include <functiondiscoverykeys_devpkey.h>

// stdafx.h defines NONEWRIFF before <mmreg.h>, which can exclude
// WAVEFORMATEXTENSIBLE and some tags. Provide robust fallbacks and avoid
// relying on the WAVEFORMATEXTENSIBLE struct being declared.
#ifndef WAVE_FORMAT_IEEE_FLOAT
#define WAVE_FORMAT_IEEE_FLOAT 0x0003
#endif
#ifndef WAVE_FORMAT_EXTENSIBLE
#define WAVE_FORMAT_EXTENSIBLE 0xFFFE
#endif

// KSDATAFORMAT_SUBTYPE_IEEE_FLOAT, defined locally to avoid ksmedia.h.
static const GUID kSUBTYPE_IEEE_FLOAT =
{ 0x00000003, 0x0000, 0x0010, { 0x80, 0x00, 0x00, 0xaa, 0x00, 0x38, 0x9b, 0x71 } };

// PKEY_Device_FriendlyName, declared here to avoid a propsys.lib dependency.
static const PROPERTYKEY kPKEY_Device_FriendlyName =
{
	{ 0xa45c254e, 0xdf1c, 0x4efd, { 0x80, 0x20, 0x67, 0xd1, 0x46, 0xa8, 0x50, 0xe0 } }, 14
};

static const REFERENCE_TIME REFTIMES_PER_MS = 10000;
#define SAFE_RELEASE(p) do { if (p) { (p)->Release(); (p) = nullptr; } } while (0)

static void wide_to_narrow(LPCWSTR w, char* dst, int dst_sz)
{
	if (!w) { dst[0] = 0; return; }
	int n = WideCharToMultiByte(CP_UTF8, 0, w, -1, dst, dst_sz, nullptr, nullptr);
	if (n <= 0) dst[0] = 0;
}

CSoundRender_Capture_WASAPI::CSoundRender_Capture_WASAPI()
	: m_sink(nullptr), m_running(false), m_loopback(false),
	  m_device(nullptr), m_client(nullptr), m_capture(nullptr), m_mixfmt(nullptr),
	  m_src_rate(0), m_src_channels(0), m_src_float(false), m_src_bits(0),
	  m_dst_rate(0), m_dst_channels(0), m_res_t(0.0), m_have_last(false)
{
	m_endpoint_id[0] = 0;
	m_last[0] = m_last[1] = 0.f;
}

CSoundRender_Capture_WASAPI::~CSoundRender_Capture_WASAPI()
{
	stop();
}

void CSoundRender_Capture_WASAPI::enumerate(xr_vector<SSoundCaptureDevice>& dst)
{
	dst.clear();

	IMMDeviceEnumerator* en = nullptr;
	HRESULT hr = CoCreateInstance(__uuidof(MMDeviceEnumerator), nullptr, CLSCTX_ALL,
	                              __uuidof(IMMDeviceEnumerator), (void**)&en);
	if (FAILED(hr) || !en) return;

	const EDataFlow flows[2] = { eRender, eCapture };
	for (int fi = 0; fi < 2; ++fi)
	{
		IMMDeviceCollection* col = nullptr;
		if (FAILED(en->EnumAudioEndpoints(flows[fi], DEVICE_STATE_ACTIVE, &col)) || !col)
			continue;

		UINT cnt = 0;
		col->GetCount(&cnt);
		for (UINT i = 0; i < cnt; ++i)
		{
			IMMDevice* dev = nullptr;
			if (FAILED(col->Item(i, &dev)) || !dev) continue;

			SSoundCaptureDevice rec;
			rec.is_render = (flows[fi] == eRender);

			LPWSTR wid = nullptr;
			if (SUCCEEDED(dev->GetId(&wid)) && wid)
			{
				string512 nid;
				wide_to_narrow(wid, nid, sizeof(nid));
				rec.id = nid;
				CoTaskMemFree(wid);
			}

			IPropertyStore* props = nullptr;
			if (SUCCEEDED(dev->OpenPropertyStore(STGM_READ, &props)) && props)
			{
				PROPVARIANT pv;
				PropVariantInit(&pv);
				if (SUCCEEDED(props->GetValue(kPKEY_Device_FriendlyName, &pv)) && pv.vt == VT_LPWSTR)
				{
					string512 nname;
					wide_to_narrow(pv.pwszVal, nname, sizeof(nname));
					rec.name = nname;
				}
				PropVariantClear(&pv);
				props->Release();
			}

			dst.push_back(rec);
			dev->Release();
		}
		col->Release();
	}
	en->Release();
}

bool CSoundRender_Capture_WASAPI::start(LPCSTR endpoint_id, bool loopback, CSound_live_channel* sink)
{
	if (m_running || !sink) return false;

	xr_strcpy(m_endpoint_id, endpoint_id ? endpoint_id : "");
	m_loopback = loopback;
	m_sink = sink;
	m_dst_rate = sink->sample_rate();
	m_dst_channels = sink->channels();

	m_res_t = 0.0;
	m_have_last = false;
	m_last[0] = m_last[1] = 0.f;

	m_running = true;
	m_thread = std::thread(&CSoundRender_Capture_WASAPI::thread_proc, this);
	return true;
}

void CSoundRender_Capture_WASAPI::stop()
{
	if (m_running)
	{
		m_running = false;
		if (m_thread.joinable())
			m_thread.join();
	}
	m_sink = nullptr;
}

bool CSoundRender_Capture_WASAPI::init_device()
{
	IMMDeviceEnumerator* en = nullptr;
	HRESULT hr = CoCreateInstance(__uuidof(MMDeviceEnumerator), nullptr, CLSCTX_ALL,
	                              __uuidof(IMMDeviceEnumerator), (void**)&en);
	if (FAILED(hr) || !en) return false;

	if (m_endpoint_id[0])
	{
		wchar_t wid[512];
		MultiByteToWideChar(CP_UTF8, 0, m_endpoint_id, -1, wid, 512);
		hr = en->GetDevice(wid, &m_device);
	}
	else
	{
		// default endpoint: render for loopback, capture for mic/line
		hr = en->GetDefaultAudioEndpoint(m_loopback ? eRender : eCapture, eConsole, &m_device);
	}
	en->Release();
	if (FAILED(hr) || !m_device) return false;

	hr = m_device->Activate(__uuidof(IAudioClient), CLSCTX_ALL, nullptr, (void**)&m_client);
	if (FAILED(hr) || !m_client) return false;

	hr = m_client->GetMixFormat(&m_mixfmt);
	if (FAILED(hr) || !m_mixfmt) return false;

	// decode source format
	m_src_rate = m_mixfmt->nSamplesPerSec;
	m_src_channels = m_mixfmt->nChannels;
	m_src_bits = m_mixfmt->wBitsPerSample;
	m_src_float = false;
	if (m_mixfmt->wFormatTag == WAVE_FORMAT_IEEE_FLOAT)
		m_src_float = true;
	else if (m_mixfmt->wFormatTag == WAVE_FORMAT_EXTENSIBLE && m_mixfmt->cbSize >= 22)
	{
		// WAVEFORMATEXTENSIBLE layout: WAVEFORMATEX(18) + Samples(2) +
		// dwChannelMask(4) + SubFormat(GUID). Read the SubFormat by offset so we
		// don't depend on WAVEFORMATEXTENSIBLE being declared (NONEWRIFF excludes it).
		const GUID* sub = (const GUID*)((const u8*)m_mixfmt + sizeof(WAVEFORMATEX) + 6);
		m_src_float = (IsEqualGUID(*sub, kSUBTYPE_IEEE_FLOAT) != 0);
	}

	DWORD flags = m_loopback ? AUDCLNT_STREAMFLAGS_LOOPBACK : 0;
	REFERENCE_TIME dur = 200 * REFTIMES_PER_MS;
	hr = m_client->Initialize(AUDCLNT_SHAREMODE_SHARED, flags, dur, 0, m_mixfmt, nullptr);
	if (FAILED(hr)) return false;

	hr = m_client->GetService(__uuidof(IAudioCaptureClient), (void**)&m_capture);
	if (FAILED(hr) || !m_capture) return false;

	return true;
}

void CSoundRender_Capture_WASAPI::shutdown_device()
{
	if (m_client) m_client->Stop();
	SAFE_RELEASE(m_capture);
	if (m_mixfmt) { CoTaskMemFree(m_mixfmt); m_mixfmt = nullptr; }
	SAFE_RELEASE(m_client);
	SAFE_RELEASE(m_device);
}

void CSoundRender_Capture_WASAPI::thread_proc()
{
	HRESULT hrCom = CoInitializeEx(nullptr, COINIT_MULTITHREADED);
	bool com_ok = SUCCEEDED(hrCom) || (hrCom == RPC_E_CHANGED_MODE);

	if (!init_device())
	{
		shutdown_device();
		if (com_ok && hrCom != RPC_E_CHANGED_MODE) CoUninitialize();
		m_running = false;
		return;
	}

	if (FAILED(m_client->Start()))
	{
		shutdown_device();
		if (com_ok && hrCom != RPC_E_CHANGED_MODE) CoUninitialize();
		m_running = false;
		return;
	}

	while (m_running)
	{
		UINT32 packet = 0;
		if (FAILED(m_capture->GetNextPacketSize(&packet)))
			break;

		while (packet)
		{
			BYTE* pdata = nullptr;
			UINT32 frames = 0;
			DWORD dwflags = 0;
			if (FAILED(m_capture->GetBuffer(&pdata, &frames, &dwflags, nullptr, nullptr)))
				break;

			if (frames)
			{
				if (dwflags & AUDCLNT_BUFFERFLAGS_SILENT)
					process(nullptr, frames); // silence
				else
					process(pdata, frames);
			}

			m_capture->ReleaseBuffer(frames);

			if (FAILED(m_capture->GetNextPacketSize(&packet)))
			{
				packet = 0;
				break;
			}
		}

		Sleep(8);
	}

	shutdown_device();
	if (com_ok && hrCom != RPC_E_CHANGED_MODE) CoUninitialize();
}

void CSoundRender_Capture_WASAPI::process(const u8* data, u32 frames)
{
	const u16 sc = m_src_channels;
	const u16 dc = m_dst_channels;
	const u32 src_stride = (m_src_bits / 8) * sc;

	m_mixbuf.clear();
	m_mixbuf.reserve(frames * dc);

	for (u32 f = 0; f < frames; ++f)
	{
		float s0 = 0.f, s1 = 0.f; // up to stereo out

		if (data)
		{
			const u8* frame = data + (size_t)f * src_stride;
			// read up to first two source channels + accumulate mono mix
			float mono = 0.f;
			float c0 = 0.f, c1 = 0.f;
			for (u16 c = 0; c < sc; ++c)
			{
				float v = 0.f;
				const u8* sp = frame + (size_t)c * (m_src_bits / 8);
				if (m_src_float)        v = *(const float*)sp;
				else if (m_src_bits == 16) v = (*(const s16*)sp) / 32768.f;
				else if (m_src_bits == 32) v = (*(const s32*)sp) / 2147483648.f;
				else if (m_src_bits == 8)  v = ((int)(*(const u8*)sp) - 128) / 128.f;
				if (c == 0) c0 = v;
				if (c == 1) c1 = v;
				mono += v;
			}
			mono /= (float)sc;

			if (dc == 1)
			{
				s0 = mono;
			}
			else
			{
				s0 = (sc >= 2) ? c0 : mono;
				s1 = (sc >= 2) ? c1 : mono;
			}
		}

		m_mixbuf.push_back(s0);
		if (dc >= 2) m_mixbuf.push_back(s1);
	}

	emit_resampled();
}

void CSoundRender_Capture_WASAPI::emit_resampled()
{
	const u16 dc = m_dst_channels;
	const u32 N = (dc ? (u32)m_mixbuf.size() / dc : 0); // source frames this packet
	if (!N) return;

	// Same rate: no resampling, straight float->s16.
	if (m_src_rate == m_dst_rate)
	{
		m_outbuf.resize((size_t)N * dc);
		for (u32 i = 0; i < N * dc; ++i)
		{
			float v = m_mixbuf[i] * 32767.f;
			clamp(v, -32768.f, 32767.f);
			m_outbuf[i] = (s16)v;
		}
		m_have_last = true;
		m_last[0] = m_mixbuf[(N - 1) * dc + 0];
		if (dc >= 2) m_last[1] = m_mixbuf[(N - 1) * dc + 1];
		m_sink->push_pcm(m_outbuf.data(), (u32)m_outbuf.size() * sizeof(s16));
		return;
	}

	const double step = (double)m_src_rate / (double)m_dst_rate; // input frames per output frame

	// c[idx][ch]: idx -1 == previous last frame; 0..N-1 == this packet
	auto getf = [&](int idx, int ch) -> float
	{
		if (idx < 0)
			return m_have_last ? m_last[ch] : m_mixbuf[0 * dc + ch];
		return m_mixbuf[(size_t)idx * dc + ch];
	};

	m_outbuf.clear();
	m_outbuf.reserve((size_t)((N / step) + 2) * dc);

	// t is the fractional input position measured from frame index -1 (prev last).
	// On the first packet (no history) start at 0 == first real frame.
	double t = m_have_last ? m_res_t : 0.0;
	const double tmax = (double)N; // generate up to (but not incl.) frame N-1 -> N

	while (t < tmax)
	{
		int i = (int)floor(t) - 1; // base frame index in [-1 .. N-2]
		double frac = t - floor(t);
		for (u16 ch = 0; ch < dc; ++ch)
		{
			float a = getf(i, ch);
			float b = getf(i + 1, ch);
			float v = (a + (b - a) * (float)frac) * 32767.f;
			clamp(v, -32768.f, 32767.f);
			m_outbuf.push_back((s16)v);
		}
		t += step;
	}

	// carry phase into next packet (origin shifts by N frames)
	m_res_t = t - tmax;
	m_have_last = true;
	m_last[0] = m_mixbuf[(size_t)(N - 1) * dc + 0];
	if (dc >= 2) m_last[1] = m_mixbuf[(size_t)(N - 1) * dc + 1];

	if (!m_outbuf.empty())
		m_sink->push_pcm(m_outbuf.data(), (u32)m_outbuf.size() * sizeof(s16));
}
