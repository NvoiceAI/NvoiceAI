#include "config_loader.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

/* Helper function to skip whitespace */
static const char* skip_whitespace(const char* str) {
    while (*str && isspace(*str)) {
        str++;
    }
    return str;
}

/* Helper function to extract a string value from JSON */
static int extract_json_string(const char* json, const char* key, char* out_value, size_t out_size) {
    char search_key[256];
    snprintf(search_key, sizeof(search_key), "\"%s\"", key);
    
    const char* pos = strstr(json, search_key);
    if (!pos) return 0;
    
    pos = strchr(pos, ':');
    if (!pos) return 0;
    
    pos = skip_whitespace(pos + 1);
    
    if (*pos != '"') return 0;
    pos++;
    
    size_t i = 0;
    while (*pos && *pos != '"' && i < out_size - 1) {
        out_value[i++] = *pos++;
    }
    out_value[i] = '\0';
    
    return 1;
}

/* Helper function to extract an integer value from JSON */
static int extract_json_int(const char* json, const char* key, int* out_value) {
    char search_key[256];
    snprintf(search_key, sizeof(search_key), "\"%s\"", key);
    
    const char* pos = strstr(json, search_key);
    if (!pos) return 0;
    
    pos = strchr(pos, ':');
    if (!pos) return 0;
    
    pos = skip_whitespace(pos + 1);
    *out_value = atoi(pos);
    
    return 1;
}

/* Helper function to extract a float value from JSON */
static int extract_json_float(const char* json, const char* key, float* out_value) {
    char search_key[256];
    snprintf(search_key, sizeof(search_key), "\"%s\"", key);
    
    const char* pos = strstr(json, search_key);
    if (!pos) return 0;
    
    pos = strchr(pos, ':');
    if (!pos) return 0;
    
    pos = skip_whitespace(pos + 1);
    *out_value = (float)atof(pos);
    
    return 1;
}

/* Helper function to extract a boolean value from JSON */
static int extract_json_bool(const char* json, const char* key, int* out_value) {
    char search_key[256];
    snprintf(search_key, sizeof(search_key), "\"%s\"", key);
    
    const char* pos = strstr(json, search_key);
    if (!pos) return 0;
    
    pos = strchr(pos, ':');
    if (!pos) return 0;
    
    pos = skip_whitespace(pos + 1);
    
    if (strncmp(pos, "true", 4) == 0) {
        *out_value = 1;
    } else if (strncmp(pos, "false", 5) == 0) {
        *out_value = 0;
    } else {
        return 0;
    }
    
    return 1;
}

int load_config(const char* config_file, Config* config) {
    FILE* file = fopen(config_file, "r");
    if (!file) {
        fprintf(stderr, "Error: Cannot open config file: %s\n", config_file);
        return 0;
    }
    
    /* Read entire file into memory */
    fseek(file, 0, SEEK_END);
    long file_size = ftell(file);
    fseek(file, 0, SEEK_SET);
    
    char* json_buffer = (char*)malloc(file_size + 1);
    if (!json_buffer) {
        fprintf(stderr, "Error: Memory allocation failed\n");
        fclose(file);
        return 0;
    }
    
    size_t read_size = fread(json_buffer, 1, file_size, file);
    json_buffer[read_size] = '\0';
    fclose(file);
    
    /* Set defaults */
    config->audio.sample_rate = 16000;
    config->audio.channels = 1;
    config->audio.frame_size_ms = 10;
    config->aec.enabled = 1;
    config->aec.mobile_mode = 0;
    config->aec.high_pass_filter_enabled = 1;
    config->gain_control.agc1_enabled = 1;
    strcpy(config->gain_control.agc1_mode, "adaptive_analog");
    config->gain_control.agc2_enabled = 1;
    config->sys_out_gain.pre_gain = 1.0f;
    config->sys_out_gain.post_gain_ratio = 1.0f;
    config->output.save_audio_files_enabled = 0;
    strcpy(config->output.save_audio_files_directory, "./output");
    
    /* Extract audio configuration */
    extract_json_int(json_buffer, "sample_rate", &config->audio.sample_rate);
    extract_json_int(json_buffer, "channels", &config->audio.channels);
    extract_json_int(json_buffer, "frame_size_ms", &config->audio.frame_size_ms);
    
    /* Extract AEC configuration */
    extract_json_bool(json_buffer, "enabled", &config->aec.enabled);
    extract_json_bool(json_buffer, "mobile_mode", &config->aec.mobile_mode);
    extract_json_bool(json_buffer, "high_pass_filter_enabled", &config->aec.high_pass_filter_enabled);
    
    /* Extract gain control configuration */
    extract_json_bool(json_buffer, "agc1_enabled", &config->gain_control.agc1_enabled);
    extract_json_string(json_buffer, "agc1_mode", config->gain_control.agc1_mode, sizeof(config->gain_control.agc1_mode));
    extract_json_bool(json_buffer, "agc2_enabled", &config->gain_control.agc2_enabled);
    
    /* Extract system output gain configuration */
    extract_json_float(json_buffer, "pre_gain", &config->sys_out_gain.pre_gain);
    extract_json_float(json_buffer, "post_gain_ratio", &config->sys_out_gain.post_gain_ratio);
    
    /* Extract output configuration */
    extract_json_bool(json_buffer, "save_audio_files_enabled", &config->output.save_audio_files_enabled);
    extract_json_string(json_buffer, "save_audio_files_directory", config->output.save_audio_files_directory, sizeof(config->output.save_audio_files_directory));
    
    free(json_buffer);
    return 1;
}

void print_config(const Config* config) {
    printf("\n=== Current Configuration ===\n");
    printf("Audio:\n");
    printf("  sample_rate: %d\n", config->audio.sample_rate);
    printf("  channels: %d\n", config->audio.channels);
    printf("  frame_size_ms: %d\n", config->audio.frame_size_ms);
    printf("\nAEC:\n");
    printf("  enabled: %s\n", config->aec.enabled ? "true" : "false");
    printf("  mobile_mode: %s\n", config->aec.mobile_mode ? "true" : "false");
    printf("  high_pass_filter_enabled: %s\n", config->aec.high_pass_filter_enabled ? "true" : "false");
    printf("\nGain Control:\n");
    printf("  agc1_enabled: %s\n", config->gain_control.agc1_enabled ? "true" : "false");
    printf("  agc1_mode: %s\n", config->gain_control.agc1_mode);
    printf("  agc2_enabled: %s\n", config->gain_control.agc2_enabled ? "true" : "false");
    printf("\nSystem Output Gain:\n");
    printf("  pre_gain: %.2f\n", config->sys_out_gain.pre_gain);
    printf("  post_gain_ratio: %.2f\n", config->sys_out_gain.post_gain_ratio);
    printf("\nOutput:\n");
    printf("  save_audio_files_enabled: %s\n", config->output.save_audio_files_enabled ? "true" : "false");
    printf("  save_audio_files_directory: %s\n", config->output.save_audio_files_directory);
    printf("========================================\n");
}
