////////////////////////////////////////////////////////////////////////////
//	Module 		: script_radio_script.cpp
//	Description : Live "radio" channel - luabind export.
////////////////////////////////////////////////////////////////////////////

#include "pch_script.h"
#include "script_radio.h"
#include "script_game_object.h"

using namespace luabind;

#pragma optimize("s",on)
void CScriptRadio::script_register(lua_State* L)
{
	module(L)
	[
		class_<CScriptRadio>("radio_channel")
		.def(constructor<>())
		.property("volume", &CScriptRadio::GetVolume, &CScriptRadio::SetVolume)
		.def("open_loopback", &CScriptRadio::OpenLoopback)
		.def("open_loopback_default", &CScriptRadio::OpenLoopbackDefault)
		.def("open_capture", &CScriptRadio::OpenCapture)
		.def("is_open", &CScriptRadio::IsOpen)
		.def("close", &CScriptRadio::Close)
		.def("set_ring_ms", &CScriptRadio::SetRingMs)
		.def("get_ring_ms", &CScriptRadio::GetRingMs)
		.def("set_spatial", &CScriptRadio::SetSpatial)
		.def("is_spatial", &CScriptRadio::IsSpatial)
		.def("set_reverb", &CScriptRadio::SetReverb)
		.def("get_reverb", &CScriptRadio::GetReverb)
		.def("play", &CScriptRadio::Play)
		.def("play_2d", &CScriptRadio::Play2D)
		.def("stop", &CScriptRadio::Stop)
		.def("playing", &CScriptRadio::IsPlaying)
		.def("set_volume", &CScriptRadio::SetVolume)
		.def("get_volume", &CScriptRadio::GetVolume)
		.def("clear", &CScriptRadio::Clear),

		// device picker (global free functions): refresh first, then query by index
		def("radio_refresh_devices", &CScriptRadio::RefreshDevices),
		def("radio_device_count", &CScriptRadio::DeviceCount),
		def("radio_device_id", &CScriptRadio::DeviceId),
		def("radio_device_name", &CScriptRadio::DeviceName),
		def("radio_device_is_render", &CScriptRadio::DeviceIsRender)
	];
}
