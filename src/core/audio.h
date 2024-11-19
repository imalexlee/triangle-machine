#pragma once

#include <filesystem>
#include <fmod_common.h>

struct AudioContext {
    FMOD_SYSTEM* system = nullptr;
};

void audio_ctx_init(AudioContext* audio_ctx);

void audio_ctx_play_sound(const AudioContext* audio_ctx, const std::filesystem::path& path);
