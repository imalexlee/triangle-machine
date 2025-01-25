#pragma once

#include <filesystem>
#include <fmod_common.h>

struct ChannelCtx {
    FMOD_CHANNEL* channel = nullptr;
    bool          paused  = false;
};

struct AudioContext {
    FMOD_SYSTEM*            system = nullptr;
    std::vector<ChannelCtx> channels;
};

void audio_ctx_init(AudioContext* audio_ctx);

size_t audio_ctx_play_sound(AudioContext* audio_ctx, const std::filesystem::path& path, bool loop = false);

void audio_ctx_set_volume(AudioContext* audio_ctx, size_t channel_idx, float volume);

void audio_ctx_toggle_sound(AudioContext* audio_ctx, size_t channel_idx);
