#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>
#include <signal.h>
#include "nvoiceai_sdk_c.h"
#include "../utilities/config_loader.h"

#ifdef SAVE_AUDIO_FILES
#include <sndfile.h>
#endif

/* Demo duration in seconds (can be modified to change how long the demo runs) */
#define DEMO_DURATION_SECONDS 60

/* Interval in milliseconds for checking demo duration */
#define DEMO_CHECK_INTERVAL_MS 500

/* Interval in seconds for printing frame statistics */
#define FRAME_PRINT_INTERVAL_SECONDS 5

/* Global variables for audio file handling */
#ifdef SAVE_AUDIO_FILES
static SNDFILE* mic_file = NULL;
static SNDFILE* near_file = NULL;
static SNDFILE* far_file = NULL;
static SNDFILE* mix_file = NULL;
static SF_INFO sf_info;
static time_t last_print_time;
#endif

static time_t callback_start_time;
static size_t frame_count = 0;
static volatile int demo_running = 1;

/* Signal handler for graceful shutdown */
static void signal_handler(int sig) {
    (void)sig;  /* Unused parameter */
    printf("\nReceived interrupt signal. Stopping demo...\n");
    demo_running = 0;
}

#ifdef SAVE_AUDIO_FILES
/* Initialize WAV file writers */
static int init_wav_files(const char* output_dir, int sample_rate) {
    /* Create output directory if it doesn't exist */
    char dir_cmd[512];
    snprintf(dir_cmd, sizeof(dir_cmd), "mkdir -p %s", output_dir);
    if (system(dir_cmd) != 0) {
        fprintf(stderr, "Error: Failed to create output directory: %s\n", output_dir);
        return 0;
    }
    printf("Created output directory: %s\n", output_dir);
    
    sf_info.samplerate = sample_rate;
    sf_info.channels = 1;
    sf_info.format = SF_FORMAT_WAV | SF_FORMAT_PCM_16;

    char filename[512];

    /* Create mic.wav */
    snprintf(filename, sizeof(filename), "%s/mic.wav", output_dir);
    mic_file = sf_open(filename, SFM_WRITE, &sf_info);
    if (!mic_file) {
        fprintf(stderr, "Error: Failed to create %s: %s\n", filename, sf_strerror(NULL));
        return 0;
    }
    printf("Created: %s\n", filename);

    /* Create near.wav */
    snprintf(filename, sizeof(filename), "%s/near.wav", output_dir);
    near_file = sf_open(filename, SFM_WRITE, &sf_info);
    if (!near_file) {
        fprintf(stderr, "Error: Failed to create %s: %s\n", filename, sf_strerror(NULL));
        return 0;
    }
    printf("Created: %s\n", filename);

    /* Create far.wav */
    snprintf(filename, sizeof(filename), "%s/far.wav", output_dir);
    far_file = sf_open(filename, SFM_WRITE, &sf_info);
    if (!far_file) {
        fprintf(stderr, "Error: Failed to create %s: %s\n", filename, sf_strerror(NULL));
        return 0;
    }
    printf("Created: %s\n", filename);

    /* Create mix.wav */
    snprintf(filename, sizeof(filename), "%s/mix.wav", output_dir);
    mix_file = sf_open(filename, SFM_WRITE, &sf_info);
    if (!mix_file) {
        fprintf(stderr, "Error: Failed to create %s: %s\n", filename, sf_strerror(NULL));
        return 0;
    }
    printf("Created: %s\n", filename);

    last_print_time = time(NULL);
    return 1;
}

/* Close all WAV files */
static void close_wav_files(void) {
    if (mic_file) {
        sf_close(mic_file);
        mic_file = NULL;
    }
    if (near_file) {
        sf_close(near_file);
        near_file = NULL;
    }
    if (far_file) {
        sf_close(far_file);
        far_file = NULL;
    }
    if (mix_file) {
        sf_close(mix_file);
        mix_file = NULL;
    }
}
#endif

/* Simple callback for processing audio frames */
static void capture_postprocess_callback(
    const float* mic,      /* Microphone input */
    const float* near,     /* Echo-cancelled near-end output */
    const float* far,      /* Far-end (system audio) output */
    const float* mix,      /* Mix of both near and far-end outputs */
    size_t frameSize,      /* Number of samples in one frame */
    void* user_data)       /* Custom user data pointer */
{
    (void)user_data;  /* Unused parameter */
    
    frame_count++;

#ifdef SAVE_AUDIO_FILES
    /* Write audio data to WAV files */
    if (mic_file && mic) {
        sf_write_float(mic_file, mic, frameSize);
    }
    if (near_file && near) {
        sf_write_float(near_file, near, frameSize);
    }
    if (far_file && far) {
        sf_write_float(far_file, far, frameSize);
    }
    if (mix_file && mix) {
        sf_write_float(mix_file, mix, frameSize);
    }

    /* Print frame size every N seconds */
    time_t now = time(NULL);
    time_t elapsed = now - last_print_time;
    
    if (elapsed >= FRAME_PRINT_INTERVAL_SECONDS) {
        printf("[%ld s] Frame Size: %zu samples, Total Frames: %zu\n",
               elapsed, frameSize, frame_count);
        last_print_time = now;
    }
#else
    time_t now = time(NULL);
    time_t elapsed = now - callback_start_time;
    
    static time_t last_log_time = 0;
    if (last_log_time == 0) {
        last_log_time = callback_start_time;
    }
    
    time_t log_elapsed = now - last_log_time;
    
    if (log_elapsed >= FRAME_PRINT_INTERVAL_SECONDS) {
        printf("[%ld s] Frame Size: %zu samples, Total Frames: %zu\n",
               elapsed, frameSize, frame_count);
        last_log_time = now;
    }
#endif
}

int main(void) {
    printf("========================================\n");
    printf("NvoiceAI SDK Realtime Simple Demo (C API)\n");
    printf("========================================\n\n");
    
    /* Load configuration from config.json */
    printf("Loading configuration from config.json...\n");
    Config config;
    if (!load_config("config.json", &config)) {
        fprintf(stderr, "Error: Failed to load config.json\n");
        return -1;
    }
    printf("Configuration loaded successfully!\n\n");
    
    /* Print current configuration */
    print_config(&config);
    printf("\n");
    
    /* Get audio configuration values */
    const int SAMPLE_RATE = config.audio.sample_rate;
    const int CHANNELS = config.audio.channels;
    const int FRAME_SIZE = (SAMPLE_RATE / 1000) * config.audio.frame_size_ms;
    
    /* Get output configuration values */
    const char* output_dir = config.output.save_audio_files_directory;
    const int save_audio_files_enabled = config.output.save_audio_files_enabled;
    
    printf("Audio Configuration:\n");
    printf("  Sample Rate: %d Hz\n", SAMPLE_RATE);
    printf("  Channels: %d\n", CHANNELS);
    printf("  Frame Size: %d samples (%d ms)\n\n", FRAME_SIZE, config.audio.frame_size_ms);
    
#ifdef SAVE_AUDIO_FILES
    /* Initialize WAV file output if enabled */
    if (save_audio_files_enabled) {
        printf("Initializing audio file output...\n");
        if (!init_wav_files(output_dir, SAMPLE_RATE)) {
            fprintf(stderr, "Error: Failed to initialize WAV files\n");
            return -1;
        }
        printf("Audio files will be saved to: %s\n\n", output_dir);
    } else {
        printf("WAV file output disabled in configuration\n\n");
    }
#endif
    
    /* Create SDK instance */
    printf("Creating SDK instance...\n");
    nvoiceai_handle_t sdk = nvoiceai_sdk_create(SAMPLE_RATE, CHANNELS, FRAME_SIZE);
    if (!sdk) {
        fprintf(stderr, "Error: Failed to create SDK instance\n");
#ifdef SAVE_AUDIO_FILES
        if (save_audio_files_enabled) {
            close_wav_files();
        }
#endif
        return -1;
    }
    
    /* Initialize audio processing */
    printf("Initializing audio processing...\n");
    if (nvoiceai_sdk_init_audio_processing(sdk) != 0) {
        fprintf(stderr, "Error: Failed to initialize audio processing\n");
        nvoiceai_sdk_destroy(sdk);
#ifdef SAVE_AUDIO_FILES
        if (save_audio_files_enabled) {
            close_wav_files();
        }
#endif
        return -1;
    }
    printf("Audio processing initialized successfully!\n\n");
    
    /* Setup signal handler for graceful shutdown */
    signal(SIGINT, signal_handler);
    signal(SIGTERM, signal_handler);
    
    /* Start real-time audio processing */
    printf("Starting audio capture and processing...\n");
    printf("Please speak into your microphone.\n");
    printf("Demo will run for %d seconds.\n", DEMO_DURATION_SECONDS);
    printf("Press Ctrl+C to stop...\n\n");
    
    callback_start_time = time(NULL);
    
    if (nvoiceai_sdk_start(sdk, capture_postprocess_callback, NULL) != 0) {
        fprintf(stderr, "Error: Failed to start SDK\n");
        nvoiceai_sdk_destroy(sdk);
#ifdef SAVE_AUDIO_FILES
        if (save_audio_files_enabled) {
            close_wav_files();
        }
#endif
        return -1;
    }
    
    /* Run for the specified duration or until interrupted */
    int num_iterations = (DEMO_DURATION_SECONDS * 1000) / DEMO_CHECK_INTERVAL_MS;
    for (int i = 0; i < num_iterations && demo_running; ++i) {
        usleep(DEMO_CHECK_INTERVAL_MS * 1000);  /* Convert ms to microseconds */
    }
    
    /* Shutdown */
    printf("\nStopping audio processing...\n");
    nvoiceai_sdk_stop(sdk);
    
#ifdef SAVE_AUDIO_FILES
    if (save_audio_files_enabled) {
        printf("Closing audio files...\n");
        close_wav_files();
        printf("Audio files saved to: %s\n", output_dir);
        printf("  - mic.wav (microphone input)\n");
        printf("  - near.wav (echo-cancelled output)\n");
        printf("  - far.wav (far-end/speaker reference)\n");
        printf("  - mix.wav (mixed output)\n");
    }
#endif
    
    /* Cleanup */
    nvoiceai_sdk_destroy(sdk);
    
    printf("Demo completed.\n");
    
    return 0;
}
