#include "audio.h"

#include "fmt/compile.h"

#include <assert.h>
#include <fmod.hpp>

void audio_ctx_init(AudioContext* audio_ctx) {
    FMOD_RESULT result = FMOD_System_Create(&audio_ctx->system, FMOD_VERSION);
    assert(result == FMOD_OK);

    result = FMOD_System_Init(audio_ctx->system, 512, FMOD_INIT_NORMAL, nullptr);
    assert(result == FMOD_OK);
}

void audio_ctx_play_sound(const AudioContext* audio_ctx, const std::filesystem::path& path) {

    FMOD_SOUND* sound;
    FMOD_RESULT result = FMOD_System_CreateSound(audio_ctx->system, path.string().c_str(), FMOD_DEFAULT, nullptr, &sound);
    assert(result == FMOD_OK);

    result = FMOD_System_PlaySound(audio_ctx->system, sound, nullptr, false, nullptr);
    assert(result == FMOD_OK);
}