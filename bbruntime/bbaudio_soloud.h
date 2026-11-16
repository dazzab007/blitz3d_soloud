#pragma once

#include "bbsys.h"
#include "bb_handle.h"
#include "soloud.h"

struct Sound;

// Exposed for testing: allows tests to init SoLoud with a specific backend (e.g. null driver).
extern SoLoud::Soloud *soloud;

BB_HANDLE(Sound) bbLoadSound(BBStr *path);
BB_HANDLE(Sound) bbLoad3DSound(BBStr *path);
void bbFreeSound(BB_HANDLE(Sound) sound_h);
void bbLoopSound(BB_HANDLE(Sound) sound_h);
void bbSoundVolume(BB_HANDLE(Sound) sound_h, float volume);
void bbSoundPan(BB_HANDLE(Sound) sound_h, float pan);
void bbSoundPitch(BB_HANDLE(Sound) sound_h, int pitch);
uint32_t bbPlaySound(BB_HANDLE(Sound) sound_h);
uint32_t bbPlayMusic(BBStr* path);
void bbStopChannel(uint32_t channel);
void bbPauseChannel(uint32_t channel);
void bbResumeChannel(uint32_t channel);
void bbChannelVolume(uint32_t channel, float volume);
void bbChannelPan(uint32_t channel, float pan);
void bbChannelPitch(uint32_t channel, int pitch);
int bbChannelPlaying(uint32_t channel);

// ***** INTERNAL *****
uint32_t bbPlay3dSound(Sound* sound,float x, float y, float z, float vx, float vy, float vz);
void bbSet3dChannel(uint32_t channel, float x, float y, float z, float vx, float vy, float vz);
void bbSet3dListenerConfig(float roll, float dopp, float dist);
void bbSet3dListener(float x,float y, float z, float kx, float ky, float kz, float jx, float jy, float jz, float vx, float vy, float vz);
