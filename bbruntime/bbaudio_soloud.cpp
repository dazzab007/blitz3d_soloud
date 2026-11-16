#include "std.h"
#include "bbaudio_soloud.h"
#include "bb_handle_types.h"

#include "soloud.h"
#include "soloud_wav.h"
#include "soloud_wavstream.h"

SoLoud::Soloud *soloud;

namespace {
	std::set<Sound *> sounds;

	inline void debugSound(Sound *sound) {
		if (::debug && sounds.find(sound) == sounds.end()) RTEX("Sound does not exist"); // NOLINT
	}

	inline void debugChannel(uint32_t channel) {
		if (::debug && !soloud) RTEX("Soloud has not been initialized"); // NOLINT
	}

	SoLoud::WavStream musicStream;
	uint32_t musicChannel;

	float rolloffFactor = 1.0f;
	float dopplerFactor = 1.0f;
	float distanceFactor = 1.0f;
}

struct Sound {
	SoLoud::Wav wav;

	float volume{1};
	float pan{0};
	int pitch{0};

	explicit Sound() {
		if (debug) sounds.insert(this);
	}

	~Sound() {
#ifndef _WIN32
		g_handle_table.forget(this);
#endif
		if (debug) sounds.erase(this);
	}
};

BB_HANDLE(Sound) bbLoadSound(BBStr *path) {
	if (!soloud) {
		delete path;
		return BB_HANDLE(Sound)(0);
	}
	auto sound = new Sound();
	auto r = sound->wav.load(path->c_str());
	delete path;
	if (r == SoLoud::SO_NO_ERROR) return BB_HANDLE(Sound)(sound);
	delete sound;
	return BB_HANDLE(Sound)(0);
}

void bbFreeSound(BB_HANDLE(Sound) sound_h) {
	Sound *sound = sound_h;
	if (!sound) return;
	debugSound(sound);
	delete sound;
}

void bbLoopSound(BB_HANDLE(Sound) sound_h) {
	Sound *sound = sound_h;
	if (!sound) return;
	debugSound(sound);
	sound->wav.setLooping(true);
}

void bbSoundVolume(BB_HANDLE(Sound) sound_h, float volume) {
	Sound *sound = sound_h;
	if (!sound) return;
	debugSound(sound);
	sound->volume = volume;
}

void bbSoundPan(BB_HANDLE(Sound) sound_h, float pan) {
	Sound *sound = sound_h;
	if (!sound) return;
	debugSound(sound);
	sound->pan = pan;
}

void bbSoundPitch(BB_HANDLE(Sound) sound_h, int pitch) {
	Sound *sound = sound_h;
	if (!sound) return;
	debugSound(sound);
	sound->pitch = pitch;
}

uint32_t bbPlaySound(BB_HANDLE(Sound) sound_h) {
	Sound *sound = sound_h;
	if (!sound) return 0;
	debugSound(sound);
	uint32_t chan;
	if (sound->pitch) {
		chan = soloud->play(sound->wav, sound->volume, sound->pan, true, 0);
		soloud->setSamplerate(chan, (float) sound->pitch);
		soloud->setPause(chan, false);
		return chan;
	}
	return soloud->play(sound->wav, sound->volume, sound->pan, false, 0);
}

uint32_t bbPlay3dSound(Sound *sound, float x, float y, float z, float vx, float vy, float vz) {
	if (!sound) return 0;
	debugSound(sound);
	sound->wav.set3dAttenuation(SoLoud::AudioSource::INVERSE_DISTANCE, rolloffFactor);
	sound->wav.set3dDopplerFactor(dopplerFactor);
	if (sound->pitch) {
		auto chan = soloud->play3d(sound->wav, x, y, z, vx, vy, vz, sound->volume, true, 0);
		soloud->setSamplerate(chan, (float) sound->pitch);
		soloud->setPause(chan, false);
		return chan;
	}
	return soloud->play3d(sound->wav, x, y, z, vx, vy, vz, sound->volume, false, 0);
}

uint32_t bbPlayMusic(BBStr *path) {
	if (!soloud) {
		delete path;
		return 0;
	}
	if (musicChannel) {
		soloud->stop(musicChannel);
		musicChannel = 0;
	}
	auto r = musicStream.load(path->c_str());
	delete path;
	if (r != SoLoud::SO_NO_ERROR) return 0;
	return musicChannel = soloud->play(musicStream);
}

void bbStopChannel(uint32_t channel) {
	if (!channel) return;
	debugChannel(channel);
	soloud->stop(channel);
}

void bbPauseChannel(uint32_t channel) {
	if (!channel) return;
	debugChannel(channel);
	soloud->setPause(channel, true);
}

void bbResumeChannel(uint32_t channel) {
	if (!channel) return;
	debugChannel(channel);
	soloud->setPause(channel, false);
}

void bbChannelVolume(uint32_t channel, float volume) {
	if (!channel) return;
	debugChannel(channel);
	soloud->setVolume(channel, volume);
}

void bbChannelPan(uint32_t channel, float pan) {
	if (!channel) return;
	debugChannel(channel);
	soloud->setPan(channel, pan);
}

void bbChannelPitch(uint32_t channel, int pitch) {
	if (!channel) return;
	debugChannel(channel);
	soloud->setSamplerate(channel, (float) pitch);
}

int bbChannelPlaying(uint32_t channel) {
	if (!channel) return 0;
	debugChannel(channel);
	return soloud->isValidVoiceHandle(channel);
}

void bbSet3dChannel(uint32_t channel, float x, float y, float z, float vx, float vy, float vz) {
	if (!channel) return;
	debugChannel(channel);
	auto sc = distanceFactor;
	soloud->set3dSourceParameters(channel, x * sc, y * sc, z * sc, vx * sc, vy * sc, vz * sc);
}

void bbSet3dListenerConfig(float roll, float dopp, float dist) {
	if (!soloud) return;
	rolloffFactor = roll;
	dopplerFactor = dopp;
	distanceFactor = dist;
}

void bbSet3dListener(float x, float y, float z, float kx, float ky, float kz, float jx, float jy, float jz, float vx,
					 float vy, float vz) {
	if (!soloud) return;
	auto sc = distanceFactor;
	soloud->set3dListenerParameters(x * sc, y * sc, z * sc, kx, ky, kz, jy, jy, jz, vx * sc, vy * sc, vz * sc);
	soloud->update3dAudio();
}

bool audio_create() {
	soloud = new SoLoud::Soloud();
	if (soloud->init(SoLoud::Soloud::LEFT_HANDED_3D) != SoLoud::SO_NO_ERROR) {
		delete soloud;
		soloud = nullptr;
	}
	return true;
}

bool audio_destroy() {
	if (!soloud) return true;
	soloud->deinit();
	delete soloud;
	soloud = nullptr;
	return true;
}

void audio_link(void(*rtSym)(const char *, void *)) {
	rtSym("%LoadSound$filename", RTSYM_CAST(bbLoadSound));
	rtSym("%Load3DSound$filename", RTSYM_CAST(bbLoadSound));
	rtSym("FreeSound%sound", RTSYM_CAST(bbFreeSound));
	rtSym("LoopSound%sound", RTSYM_CAST(bbLoopSound));
	rtSym("SoundPitch%sound%pitch", RTSYM_CAST(bbSoundPitch));
	rtSym("SoundVolume%sound#volume", RTSYM_CAST(bbSoundVolume));
	rtSym("SoundPan%sound#pan", RTSYM_CAST(bbSoundPan));
	rtSym("%PlaySound%sound", RTSYM_CAST(bbPlaySound));
	rtSym("%PlayMusic$midifile", RTSYM_CAST(bbPlayMusic));
	rtSym("StopChannel%channel", RTSYM_CAST(bbStopChannel));
	rtSym("PauseChannel%channel", RTSYM_CAST(bbPauseChannel));
	rtSym("ResumeChannel%channel", RTSYM_CAST(bbResumeChannel));
	rtSym("ChannelPitch%channel%pitch", RTSYM_CAST(bbChannelPitch));
	rtSym("ChannelVolume%channel#volume", RTSYM_CAST(bbChannelVolume));
	rtSym("ChannelPan%channel#pan", RTSYM_CAST(bbChannelPan));
	rtSym("%ChannelPlaying%channel", RTSYM_CAST(bbChannelPlaying));
}
