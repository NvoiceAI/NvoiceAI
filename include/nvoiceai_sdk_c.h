#pragma once
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef void* nvoiceai_handle_t;
typedef void (*nvoiceai_frame_cb_t)(const float* mic, const float* near, const float* far, const float* mix, size_t frameSize, void* user_data);

nvoiceai_handle_t nvoiceai_sdk_create(int sampleRate, int channels, int frameSize);
void nvoiceai_sdk_destroy(nvoiceai_handle_t h);
int nvoiceai_sdk_init_audio_processing(nvoiceai_handle_t h);
int nvoiceai_sdk_start(nvoiceai_handle_t h, nvoiceai_frame_cb_t cb, void* user_data);
int nvoiceai_sdk_process_offline(nvoiceai_handle_t h, const float* mic, const float* playback, float* output, size_t frameSize);
void nvoiceai_sdk_stop(nvoiceai_handle_t h);
void nvoiceai_sdk_set_privacy(nvoiceai_handle_t h, int privacy);
// Runtime AEC control: Enable/disable echo cancellation during active call.
// This allows UI to toggle AEC on/off without restarting audio processing.
void nvoiceai_sdk_set_aec_enabled(nvoiceai_handle_t h, int enabled);
int nvoiceai_sdk_is_aec_enabled(nvoiceai_handle_t h);
// Control system playback gain from C API (1.0 = unity)
void nvoiceai_sdk_set_system_gain(nvoiceai_handle_t h, float gain);
float nvoiceai_sdk_get_system_gain(nvoiceai_handle_t h);
// Enable audio file saving for audio debugging (requires SAVE_AUDIO_FILES build flag)
// Specify the directory to store the output files (e.g., "./output")
int nvoiceai_sdk_enable_audio_file_saving(nvoiceai_handle_t h, const char* output_dir);

#ifdef __cplusplus
}
#endif
