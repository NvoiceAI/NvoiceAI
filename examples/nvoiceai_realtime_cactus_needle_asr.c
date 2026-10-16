/* C demo: NvoiceAI SDK real-time audio processing + Cactus "needle" speech
 * recognition, using needle's streaming transcription API so that speech is
 * transcribed live (incrementally) as audio comes in from the microphone.
 *
 * Pipeline:
 *   mic -> NvoiceAI SDK (AEC/NS/Gain) -> near-end (echo-cancelled) audio
 *       -> buffered into ~1s windows -> needle_stream_transcribe_process()
 *       -> live transcript (committed words + pending tail) printed on a
 *          single, continuously updated line
 *
 * Needle only needs 16 kHz mono float PCM in [-1, 1], which matches the
 * NvoiceAI SDK's native near-end output format when config.json's audio
 * section is configured for 16000 Hz / mono (the default).
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>
#include <signal.h>
#include <sys/ioctl.h>

#include "nvoiceai_sdk_c.h"
#include "config_loader.h"
#include "needle.h"

/* Demo duration in seconds (can be modified to change how long the demo runs) */
#define DEMO_DURATION_SECONDS 60

/* Interval in milliseconds for checking demo duration */
#define DEMO_CHECK_INTERVAL_MS 500

/* Maximum length of the committed transcript we keep for display */
#define TRANSCRIPT_CAPACITY 8192

/* Maximum length of the complete transcript kept for the final summary */
#define FULL_TRANSCRIPT_CAPACITY 65536

static volatile int demo_running = 1;

/* Runtime parameters taken from the "cactus_needle" config section. */
static int needle_chunk_samples = 16000;   /* samples buffered per streaming pass */
static int needle_out_capacity = 8192;     /* byte size of the JSON result buffer */
static int sentences_per_line = 4;         /* new output line after N sentences */
static const char* needle_language = NULL; /* NULL = auto-detect */
static const char* needle_keywords = NULL; /* NULL = no keyword biasing */

/* Streaming audio buffer (near-end / echo-cancelled mic audio) */
static float* needle_buffer = NULL;
static size_t needle_buffer_count = 0;

/* Samples still to be discarded from the start of the capture stream. */
static size_t skip_samples_remaining = 0;

/* JSON result buffer returned by needle (allocated from out_capacity). */
static char* needle_out = NULL;

/* Accumulated committed transcript for the current display line, plus the
 * last line we printed so we can erase it cleanly and skip no-op repaints. */
static char committed_text[TRANSCRIPT_CAPACITY];
static size_t committed_len = 0;
static char last_display_text[TRANSCRIPT_CAPACITY * 2];
static size_t last_display_len = 0;

/* Complete transcript across all display lines, for the final summary. */
static char full_transcript[FULL_TRANSCRIPT_CAPACITY];
static size_t full_len = 0;

/* Signal handler for graceful shutdown */
static void signal_handler(int sig) {
    (void)sig;
    printf("\nReceived interrupt signal. Stopping demo...\n");
    demo_running = 0;
}

/* Extract a JSON string value for `key` out of a flat JSON object, handling
 * the escape sequences needle emits. Returns 1 on success, 0 if not found. */
static int json_get_string(const char* json, const char* key, char* out, size_t out_size) {
    if (!json || !key || !out || out_size == 0) {
        return 0;
    }

    char search_key[64];
    snprintf(search_key, sizeof(search_key), "\"%s\"", key);

    const char* pos = strstr(json, search_key);
    if (!pos) {
        return 0;
    }

    pos = strchr(pos, ':');
    if (!pos) {
        return 0;
    }
    pos++;

    while (*pos == ' ' || *pos == '\t' || *pos == '\n' || *pos == '\r') {
        pos++;
    }
    if (*pos != '"') {
        return 0;
    }
    pos++;

    size_t i = 0;
    while (*pos && *pos != '"' && i < out_size - 1) {
        if (*pos == '\\' && pos[1]) {
            pos++;
            switch (*pos) {
                case 'n': out[i++] = '\n'; break;
                case 't': out[i++] = '\t'; break;
                case 'r': out[i++] = '\r'; break;
                case '"': out[i++] = '"';  break;
                case '\\': out[i++] = '\\'; break;
                case '/': out[i++] = '/';  break;
                default:  out[i++] = *pos; break;
            }
            pos++;
        } else {
            out[i++] = *pos++;
        }
    }
    out[i] = '\0';
    return 1;
}

/* Append newly committed text to the running transcript, inserting a space
 * between segments when needed. The text is appended both to the current
 * display line and to the full transcript kept for the final summary. */
static void append_committed(const char* text) {
    if (!text || text[0] == '\0') {
        return;
    }

    /* Skip leading whitespace coming from needle */
    while (*text == ' ') {
        text++;
    }
    if (*text == '\0') {
        return;
    }

    if (committed_len > 0 && committed_len < TRANSCRIPT_CAPACITY - 1 &&
        committed_text[committed_len - 1] != ' ') {
        committed_text[committed_len++] = ' ';
    }

    size_t remaining = TRANSCRIPT_CAPACITY - 1 - committed_len;
    size_t len = strlen(text);
    if (len > remaining) {
        len = remaining;
    }
    memcpy(committed_text + committed_len, text, len);
    committed_len += len;
    committed_text[committed_len] = '\0';

    /* Mirror into the full transcript used for the final summary. */
    if (full_len > 0 && full_len < FULL_TRANSCRIPT_CAPACITY - 1 &&
        full_transcript[full_len - 1] != ' ') {
        full_transcript[full_len++] = ' ';
    }
    size_t full_remaining = FULL_TRANSCRIPT_CAPACITY - 1 - full_len;
    size_t full_copy = strlen(text);
    if (full_copy > full_remaining) {
        full_copy = full_remaining;
    }
    memcpy(full_transcript + full_len, text, full_copy);
    full_len += full_copy;
    full_transcript[full_len] = '\0';
}

/* Count completed sentences (., ? or !) in a string. */
static int count_sentences(const char* s) {
    int n = 0;
    for (; *s; ++s) {
        if (*s == '.' || *s == '?' || *s == '!') {
            n++;
        }
    }
    return n;
}

/* Width of the terminal, used to keep the live transcript on exactly one
 * row. Wrapping would break the "\r" overwrite and duplicate lines. */
static size_t terminal_width(void) {
    struct winsize ws;
    if (ioctl(STDOUT_FILENO, TIOCGWINSZ, &ws) == 0 && ws.ws_col > 1) {
        return (size_t)ws.ws_col - 1; /* leave a column so we never wrap */
    }
    return 100; /* sensible fallback for non-tty / unknown size */
}

/* Render the live transcript on a single line: committed words followed by
 * the pending (not yet confirmed) tail, overwriting the previous render.
 *
 * The text is clamped to the terminal width (showing the most recent tail)
 * so the line never wraps, and identical renders are skipped so that silence
 * does not repaint - both of which would otherwise duplicate output. */
static void render_transcript(const char* pending) {
    char line[TRANSCRIPT_CAPACITY * 2];
    int written;

    if (pending && pending[0] != '\0') {
        written = snprintf(line, sizeof(line), "%s%s%s",
                           committed_text,
                           (committed_len > 0 && pending[0] != ' ') ? " " : "",
                           pending);
    } else {
        written = snprintf(line, sizeof(line), "%s", committed_text);
    }

    if (written < 0) {
        return;
    }

    size_t line_len = (size_t)written;
    if (line_len >= sizeof(line)) {
        line_len = sizeof(line) - 1; /* snprintf truncated */
    }

    /* Keep only the trailing portion that fits on one terminal row. */
    const char* view = line;
    size_t max_cols = terminal_width();
    if (line_len > max_cols) {
        view = line + (line_len - max_cols);
        line_len = max_cols;
    }

    /* Skip the repaint entirely if nothing changed (e.g. during silence). */
    if (line_len == last_display_len && strcmp(view, last_display_text) == 0) {
        return;
    }

    /* Overwrite the current row: carriage return, text, then pad with spaces
     * to erase any leftovers from a previously longer render. */
    printf("\r%s", view);
    for (size_t i = line_len; i < last_display_len; ++i) {
        putchar(' ');
    }
    if (line_len < last_display_len) {
        printf("\r%s", view);
    }
    fflush(stdout);

    snprintf(last_display_text, sizeof(last_display_text), "%s", view);
    last_display_len = line_len;
}

/* Once the current line holds enough complete sentences, terminate it and
 * start a fresh line so the transcript stays readable. */
static void maybe_wrap_line(void) {
    if (count_sentences(committed_text) < sentences_per_line) {
        return;
    }

    printf("\n");
    fflush(stdout);

    committed_text[0] = '\0';
    committed_len = 0;
    last_display_text[0] = '\0';
    last_display_len = 0;
}

/* Read entire file into a malloc'd buffer. Caller must free() the result. */
static unsigned char* read_whole_file(const char* path, unsigned long long* out_size) {
    FILE* f = fopen(path, "rb");
    if (!f) {
        return NULL;
    }
    fseek(f, 0, SEEK_END);
    long size = ftell(f);
    fseek(f, 0, SEEK_SET);
    if (size <= 0) {
        fclose(f);
        return NULL;
    }
    unsigned char* buf = (unsigned char*)malloc((size_t)size);
    if (!buf) {
        fclose(f);
        return NULL;
    }
    size_t read_bytes = fread(buf, 1, (size_t)size, f);
    fclose(f);
    if (read_bytes != (size_t)size) {
        free(buf);
        return NULL;
    }
    *out_size = (unsigned long long)size;
    return buf;
}

/* Process one ~1-second chunk of buffered audio through needle's streaming
 * transcription API and update the live transcript line with the newly
 * committed words plus the pending (unconfirmed) tail. */
static void flush_needle_chunk(void) {
    if (needle_buffer_count == 0) {
        return;
    }

    int committed_words = needle_stream_transcribe_process(
        needle_buffer,
        (int)needle_buffer_count,
        needle_language,   /* NULL = auto-detect */
        needle_keywords,   /* NULL = no keyword biasing */
        needle_out,
        needle_out_capacity);

    needle_buffer_count = 0;

    if (committed_words < 0) {
        fprintf(stderr, "\nneedle_stream_transcribe_process error: %s\n", needle_last_error());
        return;
    }

    /* needle_out is a JSON object:
     * {"text":"...","words":[...],"pending":"...","language":"en",
     *  "received":0.00,"pass_ms":0.0}
     * "text"    -> words committed by this call
     * "pending" -> unconfirmed tail that may still change */
    char text[TRANSCRIPT_CAPACITY];
    char pending[TRANSCRIPT_CAPACITY];

    if (json_get_string(needle_out, "text", text, sizeof(text))) {
        append_committed(text);
    }
    if (!json_get_string(needle_out, "pending", pending, sizeof(pending))) {
        pending[0] = '\0';
    }

    render_transcript(pending);
    maybe_wrap_line();
}

/* NvoiceAI SDK frame callback: receives mic/near/far/mix audio frames in
 * real time. We only need the near-end (echo-cancelled) output for ASR. */
static void capture_postprocess_callback(
    const float* mic,
    const float* near,
    const float* far,
    const float* mix,
    size_t frameSize,
    void* user_data) {
    (void)mic;
    (void)far;
    (void)mix;
    (void)user_data;

    if (!near) {
        return;
    }

    /* Discard the configured amount of startup audio: device startup and
     * AEC/AGC convergence produce noise that the model tends to hallucinate
     * into filler phrases such as "Thank you." */
    if (skip_samples_remaining > 0) {
        if (frameSize <= skip_samples_remaining) {
            skip_samples_remaining -= frameSize;
            return;
        }
        near += skip_samples_remaining;
        frameSize -= skip_samples_remaining;
        skip_samples_remaining = 0;
    }

    size_t offset = 0;
    while (offset < frameSize) {
        size_t space = (size_t)needle_chunk_samples - needle_buffer_count;
        size_t to_copy = (frameSize - offset < space) ? (frameSize - offset) : space;

        memcpy(needle_buffer + needle_buffer_count, near + offset, to_copy * sizeof(float));
        needle_buffer_count += to_copy;
        offset += to_copy;

        if (needle_buffer_count >= (size_t)needle_chunk_samples) {
            flush_needle_chunk();
        }
    }
}

int main(int argc, char* argv[]) {
    printf("================================================================\n");
    printf("NvoiceAI SDK Realtime Demo + Cactus Needle Streaming ASR (C API)\n");
    printf("================================================================\n\n");

    if (argc > 2) {
        printf("Usage: %s <config_file_path>\n", argv[0]);
        return -1;
    }
    const char* config_file_path = (argc == 2) ? argv[1] : "configs/config_cactus_needle.json";

    /* --- Load NvoiceAI SDK configuration --- */
    printf("Loading configuration from: %s\n", config_file_path);
    Config config;
    if (!load_config(config_file_path, &config)) {
        fprintf(stderr, "Error: Failed to load %s\n", config_file_path);
        return -1;
    }
    print_config(&config);
    printf("\n");

    if (!config.cactus_needle.enabled) {
        fprintf(stderr, "Error: cactus_needle is disabled in %s\n", config_file_path);
        return -1;
    }

    /* --- Apply the cactus_needle runtime parameters --- */
    const int NEEDLE_SAMPLE_RATE = config.cactus_needle.sample_rate;

    needle_chunk_samples = (NEEDLE_SAMPLE_RATE / 1000) * config.cactus_needle.chunk_ms;
    needle_out_capacity = config.cactus_needle.out_capacity;
    sentences_per_line = config.cactus_needle.sentences_per_line;
    skip_samples_remaining =
        (size_t)(NEEDLE_SAMPLE_RATE / 1000) * (size_t)config.cactus_needle.skip_initial_ms;
    needle_language = config.cactus_needle.language[0] ? config.cactus_needle.language : NULL;
    needle_keywords = config.cactus_needle.keywords[0] ? config.cactus_needle.keywords : NULL;

    if (needle_chunk_samples <= 0 || needle_out_capacity <= 0 || sentences_per_line <= 0) {
        fprintf(stderr, "Error: Invalid cactus_needle parameters in %s\n", config_file_path);
        return -1;
    }

    needle_buffer = (float*)malloc((size_t)needle_chunk_samples * sizeof(float));
    needle_out = (char*)malloc((size_t)needle_out_capacity);
    if (!needle_buffer || !needle_out) {
        fprintf(stderr, "Error: Failed to allocate needle buffers\n");
        free(needle_buffer);
        free(needle_out);
        return -1;
    }

    /* --- Load the needle speech model --- */
    const char* model_path = config.cactus_needle.model_path;
    printf("Loading needle model: %s\n", model_path);
    unsigned long long model_size = 0;
    unsigned char* model_data = read_whole_file(model_path, &model_size);
    if (!model_data) {
        fprintf(stderr, "Error: Failed to read needle model file: %s\n", model_path);
        fprintf(stderr, "Check \"cactus_needle.model_path\" in %s\n", config_file_path);
        return -1;
    }

    if (needle_load(model_data, model_size) < 0) {
        fprintf(stderr, "Error: needle_load failed: %s\n", needle_last_error());
        free(model_data);
        return -1;
    }
    free(model_data); /* needle_load copies/consumes what it needs */

    int models = needle_models();
    if (!(models & NEEDLE_SPEECH)) {
        fprintf(stderr, "Error: Loaded model does not contain a speech model\n");
        return -1;
    }
    printf("Needle speech model loaded successfully!\n\n");

    /* Language/keywords from config, no word timestamps for the streaming
     * demo (word_timestamps only affects needle_complete's internal
     * transcription, not needle_stream_transcribe_process). */
    needle_set_audio(needle_language, needle_keywords, 0);

    const int SAMPLE_RATE = config.audio.sample_rate;
    const int CHANNELS = config.audio.channels;
    const int FRAME_SIZE = (SAMPLE_RATE / 1000) * config.audio.frame_size_ms;

    printf("Audio Configuration:\n");
    printf("  Sample Rate: %d Hz\n", SAMPLE_RATE);
    printf("  Channels: %d\n", CHANNELS);
    printf("  Frame Size: %d samples (%d ms)\n\n", FRAME_SIZE, config.audio.frame_size_ms);

    if (SAMPLE_RATE != NEEDLE_SAMPLE_RATE || CHANNELS != 1) {
        fprintf(stderr,
                "Warning: needle expects %d Hz mono audio, but the audio section specifies "
                "%d Hz / %d channel(s). Transcription quality/behavior may be degraded.\n\n",
                NEEDLE_SAMPLE_RATE, SAMPLE_RATE, CHANNELS);
    }

    /* --- Create and initialize the NvoiceAI SDK --- */
    printf("Creating SDK instance...\n");
    nvoiceai_handle_t sdk = nvoiceai_sdk_create_with_config(config_file_path);
    if (!sdk) {
        fprintf(stderr, "Error: Failed to create SDK instance\n");
        return -1;
    }

    printf("Initializing audio processing (AEC/NS/Gain) from %s...\n", config_file_path);
    if (nvoiceai_sdk_init_audio_processing_with_config(sdk, config_file_path) != 0) {
        fprintf(stderr, "Error: Failed to initialize audio processing\n");
        nvoiceai_sdk_destroy(sdk);
        return -1;
    }
    printf("Audio processing initialized successfully!\n\n");

    /* Apply AEC configuration explicitly (mirrors other demos) */
    nvoiceai_sdk_set_aec_enabled(sdk, config.aec.enabled);
    printf("AEC: %s\n", config.aec.enabled ? "enabled" : "disabled");
    printf("Mobile Mode: %s\n\n", config.aec.mobile_mode ? "enabled" : "disabled");

    signal(SIGINT, signal_handler);
    signal(SIGTERM, signal_handler);

    printf("Starting real-time audio processing...\n");
    if (nvoiceai_sdk_start(sdk, capture_postprocess_callback, NULL) != 0) {
        fprintf(stderr, "Error: Failed to start SDK\n");
        nvoiceai_sdk_destroy(sdk);
        return -1;
    }

    printf("SDK running in real-time mode with live (streaming) transcription.\n");
    printf("Please speak into your microphone.\n");
    printf("(Ignoring the first %d ms of audio while capture settles.)\n",
           config.cactus_needle.skip_initial_ms);
    printf("Demo will run for %d seconds. Press Ctrl+C to stop early.\n", DEMO_DURATION_SECONDS);
    printf("Recognized text will appear below and update live:\n");
    printf("================================================================\n\n");

    int num_iterations = (DEMO_DURATION_SECONDS * 1000) / DEMO_CHECK_INTERVAL_MS;
    for (int i = 0; i < num_iterations && demo_running; ++i) {
        usleep(DEMO_CHECK_INTERVAL_MS * 1000);
    }

    printf("\n\n================================================================\n");
    printf("Stopping audio processing...\n");
    nvoiceai_sdk_stop(sdk);

    /* Flush any remaining buffered audio through the streaming API, then
     * finalize the stream to commit the trailing (previously "pending")
     * words. */
    flush_needle_chunk();

    int final_words = needle_stream_transcribe_stop(needle_out, needle_out_capacity);
    if (final_words < 0) {
        fprintf(stderr, "needle_stream_transcribe_stop error: %s\n", needle_last_error());
    } else {
        char final_text[TRANSCRIPT_CAPACITY];
        if (json_get_string(needle_out, "text", final_text, sizeof(final_text))) {
            append_committed(final_text);
        }
        /* Final render with no pending tail, then end the line. */
        render_transcript(NULL);
        printf("\n");
    }

    printf("\n=== Processing Complete ===\n");
    if (full_len > 0) {
        printf("Final transcript:\n%s\n", full_transcript);
    } else {
        printf("No speech detected\n");
    }

    nvoiceai_sdk_destroy(sdk);

    free(needle_buffer);
    needle_buffer = NULL;
    free(needle_out);
    needle_out = NULL;

    printf("\nDemo completed.\n");
    return 0;
}
