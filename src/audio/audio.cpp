#include "audio.h"

#include "fmt/compile.h"
#include "libshaderc_util/counting_includer.h"

#include <assert.h>
#include <fmod.hpp>

void audio_ctx_init(AudioContext* audio_ctx) {
    FMOD_RESULT result = FMOD_System_Create(&audio_ctx->system, FMOD_VERSION);
    assert(result == FMOD_OK);

    result = FMOD_System_Init(audio_ctx->system, 512, FMOD_INIT_NORMAL, nullptr);
    assert(result == FMOD_OK);
}

// returns the index of channel created from this audio
size_t audio_ctx_play_sound(AudioContext* audio_ctx, const std::filesystem::path& path, bool loop) {

    FMOD_MODE mode = loop ? FMOD_LOOP_NORMAL : FMOD_DEFAULT;

    FMOD_SOUND* sound;
    FMOD_RESULT result = FMOD_System_CreateSound(audio_ctx->system, path.string().c_str(), mode, nullptr, &sound);
    assert(result == FMOD_OK);

    FMOD_CHANNEL* channel;
    result = FMOD_System_PlaySound(audio_ctx->system, sound, nullptr, false, &channel);
    assert(result == FMOD_OK);

    ChannelCtx channel_ctx{};
    channel_ctx.channel = channel;

    audio_ctx->channels.push_back(channel_ctx);

    return audio_ctx->channels.size() - 1;
}

void audio_ctx_set_volume(AudioContext* audio_ctx, size_t channel_idx, float volume) {
    assert(channel_idx < audio_ctx->channels.size());
    ChannelCtx* channel_ctx = &audio_ctx->channels[channel_idx];
    FMOD_Channel_SetVolume(channel_ctx->channel, volume);
}

void audio_ctx_toggle_sound(AudioContext* audio_ctx, size_t channel_idx) {
    assert(channel_idx < audio_ctx->channels.size());

    ChannelCtx* channel_ctx = &audio_ctx->channels[channel_idx];
    channel_ctx->paused     = !channel_ctx->paused;
    FMOD_Channel_SetPaused(channel_ctx->channel, channel_ctx->paused);
}