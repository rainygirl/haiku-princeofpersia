/*
 * Sound output for the Prince of Persia engine on Haiku, through BSoundPlayer.
 *
 * The engine mixes everything itself (digitised effects, the PC-speaker
 * fallback, and the OPL2 music synthesiser) in one callback that fills a
 * buffer of interleaved 16-bit stereo samples. That callback is plugged
 * straight into a BSoundPlayer; the media kit's mixer does the rest.
 */
#include <SDL2/SDL.h>

#include <Locker.h>
#include <MediaDefs.h>
#include <SoundPlayer.h>

namespace {

BSoundPlayer* g_player = NULL;
BLocker g_audio_lock("pop audio");
SDL_AudioSpec g_spec;
bool g_started = false;
bool g_has_data = false;
Uint8* g_convert_buffer = NULL;
size_t g_convert_size = 0;

// The engine writes its native format (U8 or S16 stereo). If the player ended
// up with a different sample format, render into a scratch buffer and convert.
// POP_AUDIO_DEBUG=1 in the environment prints one line per second about the
// buffers the media kit pulls, useful to check that sound is actually flowing.
void debug_report(const void* buffer, size_t size, const media_raw_audio_format& format) {
	static bool checked = false, enabled = false;
	static int calls = 0;
	static bigtime_t last = 0;
	static float peak = 0;
	if (!checked) { checked = true; enabled = getenv("POP_AUDIO_DEBUG") != NULL; }
	if (!enabled) return;
	++calls;
	if (format.format == media_raw_audio_format::B_AUDIO_SHORT) {
		const int16* s = (const int16*)buffer;
		for (size_t i = 0; i < size / 2; ++i) { float v = s[i] / 32768.0f; if (v < 0) v = -v; if (v > peak) peak = v; }
	} else if (format.format == media_raw_audio_format::B_AUDIO_FLOAT) {
		const float* s = (const float*)buffer;
		for (size_t i = 0; i < size / 4; ++i) { float v = s[i] < 0 ? -s[i] : s[i]; if (v > peak) peak = v; }
	}
	bigtime_t now = system_time();
	if (now - last >= 1000000) {
		fprintf(stderr, "audio: %d buffers/s, %u bytes each, format 0x%x, %.0f Hz, %u ch, peak %.3f\n",
			calls, (unsigned)size, (unsigned)format.format, format.frame_rate, (unsigned)format.channel_count, peak);
		calls = 0; peak = 0; last = now;
	}
}

void buffer_proc(void* cookie, void* buffer, size_t size, const media_raw_audio_format& format) {
	(void)cookie;
	if (g_spec.callback == NULL) {
		memset(buffer, 0, size);
		return;
	}
	const uint32 out_format = format.format;
	const bool want_u8 = g_spec.format == AUDIO_U8;
	const uint32 native = want_u8 ? media_raw_audio_format::B_AUDIO_UCHAR : media_raw_audio_format::B_AUDIO_SHORT;

	if (out_format == native) {
		g_audio_lock.Lock();
		g_spec.callback(g_spec.userdata, (Uint8*)buffer, (int)size);
		g_audio_lock.Unlock();
		debug_report(buffer, size, format);
		return;
	}

	size_t out_sample_bytes = out_format & 0xf;
	if (out_sample_bytes == 0) out_sample_bytes = 2;
	size_t samples = size / out_sample_bytes;
	size_t native_bytes = samples * (want_u8 ? 1 : 2);
	if (g_convert_size < native_bytes) {
		free(g_convert_buffer);
		g_convert_buffer = (Uint8*)malloc(native_bytes);
		g_convert_size = native_bytes;
	}
	g_audio_lock.Lock();
	g_spec.callback(g_spec.userdata, g_convert_buffer, (int)native_bytes);
	g_audio_lock.Unlock();

	for (size_t i = 0; i < samples; ++i) {
		float v = want_u8 ? ((int)g_convert_buffer[i] - 128) / 128.0f
		                  : ((const int16*)g_convert_buffer)[i] / 32768.0f;
		switch (out_format) {
			case media_raw_audio_format::B_AUDIO_FLOAT: ((float*)buffer)[i] = v; break;
			case media_raw_audio_format::B_AUDIO_INT: ((int32*)buffer)[i] = (int32)(v * 2147483647.0f); break;
			case media_raw_audio_format::B_AUDIO_SHORT: ((int16*)buffer)[i] = (int16)(v * 32767.0f); break;
			case media_raw_audio_format::B_AUDIO_UCHAR: ((uint8*)buffer)[i] = (uint8)(v * 127.0f + 128.0f); break;
			case media_raw_audio_format::B_AUDIO_CHAR: ((int8*)buffer)[i] = (int8)(v * 127.0f); break;
			default: break;
		}
	}
	debug_report(buffer, size, format);
}

} // namespace

extern "C" {

int SDL_OpenAudio(SDL_AudioSpec* desired, SDL_AudioSpec* obtained) {
	if (desired == NULL || desired->callback == NULL) return -1;
	if (g_player != NULL) return -1;

	media_raw_audio_format format = media_raw_audio_format::wildcard;
	format.frame_rate = (float)desired->freq;
	format.channel_count = desired->channels;
	format.format = (desired->format == AUDIO_U8) ? media_raw_audio_format::B_AUDIO_UCHAR
	                                              : media_raw_audio_format::B_AUDIO_SHORT;
	format.byte_order = B_MEDIA_LITTLE_ENDIAN;
	int bytes_per_sample = (desired->format == AUDIO_U8) ? 1 : 2;
	int samples = desired->samples > 0 ? desired->samples : 1024;
	format.buffer_size = (size_t)samples * desired->channels * bytes_per_sample;

	g_spec = *desired;
	g_spec.silence = (desired->format == AUDIO_U8) ? 0x80 : 0;
	g_spec.size = (Uint32)format.buffer_size;

	g_player = new BSoundPlayer(&format, "Prince of Persia", buffer_proc, NULL, NULL);
	if (g_player->InitCheck() != B_OK) {
		delete g_player;
		g_player = NULL;
		memset(&g_spec, 0, sizeof(g_spec));
		return -1;
	}
	g_player->SetVolume(1.0f);
	g_started = false;
	g_has_data = false;

	desired->silence = g_spec.silence;
	desired->size = g_spec.size;
	if (obtained) *obtained = g_spec;
	return 0;
}

void SDL_PauseAudio(int pause_on) {
	if (g_player == NULL) return;
	if (pause_on) {
		if (g_has_data) { g_player->SetHasData(false); g_has_data = false; }
	} else {
		if (!g_started) { g_player->Start(); g_started = true; }
		if (!g_has_data) { g_player->SetHasData(true); g_has_data = true; }
	}
}

void SDL_LockAudio(void) { g_audio_lock.Lock(); }
void SDL_UnlockAudio(void) { g_audio_lock.Unlock(); }

SDL_AudioStatus SDL_GetAudioStatus(void) {
	if (g_player == NULL) return SDL_AUDIO_STOPPED;
	return g_has_data ? SDL_AUDIO_PLAYING : SDL_AUDIO_PAUSED;
}

void SDL_CloseAudio(void) {
	if (g_player == NULL) return;
	BSoundPlayer* p = g_player;
	g_audio_lock.Lock();
	g_spec.callback = NULL;
	g_audio_lock.Unlock();
	if (g_started) p->Stop();
	g_player = NULL;
	delete p;
	g_started = false;
	g_has_data = false;
}

} // extern "C"
