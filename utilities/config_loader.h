#pragma once

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Configuration structures */
typedef struct {
    int sample_rate;
    int channels;
    int frame_size_ms;
} AudioConfig;

typedef struct {
    int enabled;
    int mobile_mode;
    int high_pass_filter_enabled;
} AecConfig;

typedef struct {
    int agc1_enabled;
    char agc1_mode[64];
    int agc2_enabled;
} GainControlConfig;

typedef struct {
    float pre_gain;
    float post_gain_ratio;
} SysOutGainConfig;

typedef struct {
    int save_audio_files_enabled;
    char save_audio_files_directory[512];
} OutputConfig;

typedef struct {
    int enabled;
    char model_dir[512];
    int sample_rate;
    int num_threads;
    int enable_endpoint;
    char encoder[256];
    char decoder[256];
    char joiner[256];
    char tokens[256];
    float rule1_min_trailing_silence;
    float rule2_min_trailing_silence;
    int rule3_min_utterance_length;
    char decoding_method[64];
    int feature_dim;
    char provider[32];
} SherpaOnnxConfig;

typedef struct {
    AudioConfig audio;
    AecConfig aec;
    GainControlConfig gain_control;
    SysOutGainConfig sys_out_gain;
    OutputConfig output;
    SherpaOnnxConfig sherpa_onnx;
} Config;

/**
 * Load configuration from JSON file
 * @param config_file Path to config.json file
 * @param config Pointer to Config structure to be filled
 * @return 1 on success, 0 on failure
 */
int load_config(const char* config_file, Config* config);

/**
 * Print configuration for debugging
 * @param config Pointer to Config structure
 */
void print_config(const Config* config);

#ifdef __cplusplus
}
#endif
