#ifndef SoundRender_TargetAH
#define SoundRender_TargetAH
#pragma once

#include "soundrender_Target.h"
#include "soundrender_CoreA.h"

class CSoundRender_TargetA : public CSoundRender_Target
{
	typedef CSoundRender_Target inherited;

public:
	// OpenAL
	ALuint pSource;
	ALuint pBuffers[sdef_target_count];
	float cache_gain;
	float cache_pitch;
	ALuint Slot;

	ALuint buf_block;
private:
	ALuint pFilter;   // per-source wet-send filter for live "radio" reverb (0 = none)
	float  cache_wet; // last applied reverb send level
	void fill_block(ALuint BufferID);
	// (re)bind this source's reverb aux-send, honoring a live source's wet level
	void set_reverb_send();
public:
	CSoundRender_TargetA();
	virtual ~CSoundRender_TargetA();

	void SetSlot(ALuint NewSlot);
	virtual BOOL _initialize();
	virtual void _destroy();
	virtual void _restart();

	virtual void start(CSoundRender_Emitter* E);
	virtual void render();
	virtual void rewind();
	virtual void stop();
	virtual void update();
	virtual void fill_parameters();
	void source_changed();
};
#endif
