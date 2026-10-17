/* C demo: NvoiceAI SDK real-time audio processing + Cactus "needle" speech
 * recognition for BOTH directions of the conversation (full duplex).
 *
 * Pipeline:
 *   mic      -> NvoiceAI SDK (AEC/NS/Gain) -> near-end audio -> ASR worker A
 *   playback -> NvoiceAI SDK               -> far-end audio  -> ASR worker B
 *
 * The audio callback never blocks: it only copies samples into per-channel
 * fill buffers and hands a full chunk over to a dedicated worker thread.
 *
 * IMPORTANT - needle threading model:
 *   needle is documented as "one process-global, non-thread-safe model".
 *   There is therefore a single engine and a single streaming state for the
 *   whole process. To stay within that contract this demo:
 *     - serializes every needle call with a global mutex, and
 *     - uses the streaming API (needle_stream_transcribe_process) only for
 *       the near-end channel, while the far-end channel is transcribed with
 *       the stateless window API (needle_transcribe) on larger chunks.
 *   That keeps the live, incremental behaviour on the near end (what you say)
 *   without two threads fighting over the single global stream state.
 *
 * GUI:
 *   A deliberately minimal ANSI-terminal "GUI": two blackboards drawn side by
 *   side, near-end transcript on the left, far-end transcript on the right.
 *   No external toolkit, no dependencies - just escape codes.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <math.h>
#include <time.h>
#include <unistd.h>
#include <signal.h>
#include <pthread.h>
#include <sys/ioctl.h>

#include "nvoiceai_sdk_c.h"
#include "config_loader.h"
#include "needle.h"

/* Demo duration in seconds */
#define DEMO_DURATION_SECONDS 60

/* Interval in milliseconds for checking demo duration */
#define DEMO_CHECK_INTERVAL_MS 500

/* Maximum transcript length kept per blackboard */
#define TRANSCRIPT_CAPACITY 16384

/* The far-end channel is transcribed in bigger, stateless windows. */
#define FAR_CHUNK_MULTIPLIER 3

/* Voice activity gate: chunks whose RMS and peak stay below these thresholds
 * are treated as silence and never reach the model. Whisper-style models
 * hallucinate filler phrases ("Thank you.", "Thanks for watching!", ...) out
 * of silence or steady noise, so the cheapest fix is to not ask them. */
#define VAD_RMS_THRESHOLD   0.006f
#define VAD_PEAK_THRESHOLD  0.030f

/* Blackboard rendering limits */
#define MAX_BOARD_ROWS 80
#define MAX_BOARD_COLS 240
#define MAX_WRAP_LINES 512

static volatile int demo_running = 1;

/* ------------------------------------------------------------------ */
/* Runtime parameters taken from the "cactus_needle" config section.    */
/* ------------------------------------------------------------------ */
static int needle_sample_rate = 16000;
static int needle_chunk_samples = 16000;   /* near-end samples per streaming pass */
static int far_chunk_samples = 48000;      /* far-end samples per stateless pass */
static int needle_out_capacity = 8192;
static const char* needle_language = NULL; /* NULL = auto-detect */
static const char* needle_keywords = NULL; /* NULL = no keyword biasing */

/* needle is not thread safe: every call goes through this mutex. */
static pthread_mutex_t needle_mutex = PTHREAD_MUTEX_INITIALIZER;

/* ------------------------------------------------------------------ */
/* Blackboards                                                          */
/* ------------------------------------------------------------------ */
typedef struct {
    const char* title;
    char committed[TRANSCRIPT_CAPACITY];
    size_t committed_len;
    char pending[1024];
} Board;

static Board board_near = { "NEAR END  (microphone / you)", {0}, 0, {0} };
static Board board_far  = { "FAR END  (playback / them)",  {0}, 0, {0} };
static pthread_mutex_t ui_mutex = PTHREAD_MUTEX_INITIALIZER;

static void signal_handler(int sig) {
    (void)sig;
    demo_running = 0;
}

/* ------------------------------------------------------------------ */
/* Tiny JSON helper (needle returns flat JSON objects)                  */
/* ------------------------------------------------------------------ */
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

/* ------------------------------------------------------------------ */
/* Minimal ANSI "GUI": two blackboards side by side                     */
/* ------------------------------------------------------------------ */
static void terminal_size(int* rows, int* cols) {
    struct winsize ws;
    if (ioctl(STDOUT_FILENO, TIOCGWINSZ, &ws) == 0 && ws.ws_col > 20 && ws.ws_row > 10) {
        *rows = (int)ws.ws_row;
        *cols = (int)ws.ws_col;
        return;
    }
    *rows = 24;
    *cols = 100;
}

/* Word-wrap `text` into `lines` (each at most width chars). Returns the number
 * of produced lines; only the last `max_lines` are kept (scrolling board). */
static int wrap_text(const char* text, int width, char lines[][MAX_BOARD_COLS], int max_lines) {
    static char all[MAX_WRAP_LINES][MAX_BOARD_COLS];
    int count = 0;
    int col = 0;

    all[0][0] = '\0';

    while (*text && count < MAX_WRAP_LINES) {
        /* Grab the next word */
        while (*text == ' ') {
            text++;
        }
        const char* word = text;
        while (*text && *text != ' ') {
            text++;
        }
        int wlen = (int)(text - word);
        if (wlen <= 0) {
            break;
        }
        if (wlen > width) {
            wlen = width; /* never happens for normal speech, just be safe */
        }

        if (col > 0 && col + 1 + wlen > width) {
            count++;
            if (count >= MAX_WRAP_LINES) {
                break;
            }
            all[count][0] = '\0';
            col = 0;
        }
        if (col > 0) {
            all[count][col++] = ' ';
        }
        memcpy(all[count] + col, word, (size_t)wlen);
        col += wlen;
        all[count][col] = '\0';
    }
    count++; /* number of used lines */

    int start = (count > max_lines) ? (count - max_lines) : 0;
    int n = count - start;
    for (int i = 0; i < n; ++i) {
        snprintf(lines[i], MAX_BOARD_COLS, "%s", all[start + i]);
    }
    return n;
}

/* Build "committed + pending" view for a board. */
static void board_text(const Board* b, char* out, size_t out_size) {
    if (b->pending[0]) {
        snprintf(out, out_size, "%s%s%s",
                 b->committed,
                 (b->committed_len > 0) ? " " : "",
                 b->pending);
    } else {
        snprintf(out, out_size, "%s", b->committed);
    }
}

static void draw_ui(void) {
    static char left[MAX_BOARD_ROWS][MAX_BOARD_COLS];
    static char right[MAX_BOARD_ROWS][MAX_BOARD_COLS];
    static char text[TRANSCRIPT_CAPACITY + 1024];

    int rows, cols;
    terminal_size(&rows, &cols);

    int bw = (cols - 5) / 2;              /* inner width of each board */
    if (bw > MAX_BOARD_COLS - 1) bw = MAX_BOARD_COLS - 1;
    if (bw < 20) bw = 20;

    int bh = rows - 7;                    /* inner height of each board */
    if (bh > MAX_BOARD_ROWS) bh = MAX_BOARD_ROWS;
    if (bh < 4) bh = 4;

    pthread_mutex_lock(&ui_mutex);

    board_text(&board_near, text, sizeof(text));
    int nl = wrap_text(text, bw, left, bh);

    board_text(&board_far, text, sizeof(text));
    int nr = wrap_text(text, bw, right, bh);

    const char* near_title = board_near.title;
    const char* far_title = board_far.title;

    /* Home cursor + clear screen */
    printf("\033[H\033[2J");

    printf(" NvoiceAI SDK - Full-duplex Cactus Needle ASR   (Ctrl+C to stop)\n");

    /* Titles */
    printf(" %-*.*s   %-*.*s\n", bw, bw, near_title, bw, bw, far_title);

    /* Top border */
    printf(" +");
    for (int i = 0; i < bw; ++i) putchar('-');
    printf("+ +");
    for (int i = 0; i < bw; ++i) putchar('-');
    printf("+\n");

    /* Board rows (text is bottom-aligned so the newest line is at the end) */
    for (int r = 0; r < bh; ++r) {
        int li = r - (bh - nl);
        int ri = r - (bh - nr);
        const char* ls = (li >= 0 && li < nl) ? left[li] : "";
        const char* rs = (ri >= 0 && ri < nr) ? right[ri] : "";
        printf(" |%-*.*s| |%-*.*s|\n", bw, bw, ls, bw, bw, rs);
    }

    /* Bottom border */
    printf(" +");
    for (int i = 0; i < bw; ++i) putchar('-');
    printf("+ +");
    for (int i = 0; i < bw; ++i) putchar('-');
    printf("+\n");

    fflush(stdout);

    pthread_mutex_unlock(&ui_mutex);
}

/* Append newly recognized text to a board (caller must not hold ui_mutex). */
static void board_append(Board* b, const char* text) {
    if (!text || !text[0]) {
        return;
    }
    while (*text == ' ') {
        text++;
    }
    if (!*text) {
        return;
    }

    pthread_mutex_lock(&ui_mutex);

    /* Keep the board bounded: drop the oldest half when it fills up. */
    if (b->committed_len > TRANSCRIPT_CAPACITY - 1024) {
        size_t half = b->committed_len / 2;
        memmove(b->committed, b->committed + half, b->committed_len - half + 1);
        b->committed_len -= half;
    }

    if (b->committed_len > 0 && b->committed[b->committed_len - 1] != ' ') {
        b->committed[b->committed_len++] = ' ';
    }
    size_t remaining = TRANSCRIPT_CAPACITY - 1 - b->committed_len;
    size_t len = strlen(text);
    if (len > remaining) {
        len = remaining;
    }
    memcpy(b->committed + b->committed_len, text, len);
    b->committed_len += len;
    b->committed[b->committed_len] = '\0';

    pthread_mutex_unlock(&ui_mutex);
}

static void board_set_pending(Board* b, const char* text) {
    pthread_mutex_lock(&ui_mutex);
    snprintf(b->pending, sizeof(b->pending), "%s", text ? text : "");
    pthread_mutex_unlock(&ui_mutex);
}

/* ------------------------------------------------------------------ */
/* Per-channel ASR worker                                               */
/* ------------------------------------------------------------------ */
typedef struct {
    const char* name;
    Board* board;
    int streaming;          /* 1 = streaming API, 0 = stateless windows */
    int chunk_samples;

    float* fill;            /* filled by the audio callback */
    size_t fill_count;

    float* work;            /* handed to the worker thread */
    size_t work_count;
    int work_ready;

    char* out;              /* JSON result buffer (per worker) */

    pthread_t thread;
    pthread_mutex_t lock;
    pthread_cond_t cond;
    int started;
} Channel;

static Channel chan_near;
static Channel chan_far;

/* Cheap energy-based voice activity detector. Returns 0 for silence or steady
 * low-level noise, which the model would otherwise hallucinate into text. */
static int chunk_has_voice(const float* pcm, int samples) {
    if (!pcm || samples <= 0) {
        return 0;
    }

    double sum_sq = 0.0;
    float peak = 0.0f;
    for (int i = 0; i < samples; ++i) {
        float v = pcm[i];
        sum_sq += (double)v * (double)v;
        float a = fabsf(v);
        if (a > peak) {
            peak = a;
        }
    }

    float rms = (float)sqrt(sum_sq / (double)samples);
    return (rms >= VAD_RMS_THRESHOLD) && (peak >= VAD_PEAK_THRESHOLD);
}

/* Second line of defence: drop the well-known Whisper-style hallucinations
 * that still slip through when a chunk contains only noise or breathing. */
static int is_filler_text(const char* text) {
    static const char* fillers[] = {
        "thank you",
        "thanks for watching",
        "thank you for watching",
        "thank you very much",
        "thanks",
        "bye",
        "you",
        "so",
        "okay",
        "ok",
        "uh",
        "um",
        "hmm",
        "mm",
        "...",
        "subtitles by the amara.org community",
    };

    char norm[128];
    size_t n = 0;
    for (const char* p = text; *p && n < sizeof(norm) - 1; ++p) {
        if (isalpha((unsigned char)*p) || *p == ' ' || *p == '.') {
            norm[n++] = (char)tolower((unsigned char)*p);
        }
    }
    norm[n] = '\0';

    /* trim leading/trailing spaces and trailing punctuation */
    char* s = norm;
    while (*s == ' ') s++;
    size_t len = strlen(s);
    while (len > 0 && (s[len - 1] == ' ' || s[len - 1] == '.')) {
        s[--len] = '\0';
    }
    if (len == 0) {
        return 1;
    }

    for (size_t i = 0; i < sizeof(fillers) / sizeof(fillers[0]); ++i) {
        if (strcmp(s, fillers[i]) == 0) {
            return 1;
        }
    }
    return 0;
}

static void channel_process(Channel* ch, const float* pcm, int samples) {
    char text[4096];
    char pending[1024];
    int rc;

    /* Silence / no voice -> do not transcribe at all. */
    if (!chunk_has_voice(pcm, samples)) {
        if (ch->streaming) {
            board_set_pending(ch->board, "");
        }
        return;
    }

    pthread_mutex_lock(&needle_mutex);
    if (ch->streaming) {
        rc = needle_stream_transcribe_process(pcm, samples,
                                              needle_language, needle_keywords,
                                              ch->out, needle_out_capacity);
    } else {
        rc = needle_transcribe(pcm, samples,
                               needle_language, needle_keywords, 0,
                               ch->out, needle_out_capacity);
    }
    pthread_mutex_unlock(&needle_mutex);

    if (rc < 0) {
        return; /* keep the UI clean in this prototype */
    }

    if (json_get_string(ch->out, "text", text, sizeof(text)) && !is_filler_text(text)) {
        board_append(ch->board, text);
    }
    if (ch->streaming) {
        if (!json_get_string(ch->out, "pending", pending, sizeof(pending)) ||
            is_filler_text(pending)) {
            pending[0] = '\0';
        }
        board_set_pending(ch->board, pending);
    }

    draw_ui();
}

static void* channel_thread(void* arg) {
    Channel* ch = (Channel*)arg;

    for (;;) {
        pthread_mutex_lock(&ch->lock);
        while (!ch->work_ready && demo_running) {
            pthread_cond_wait(&ch->cond, &ch->lock);
        }
        if (!ch->work_ready && !demo_running) {
            pthread_mutex_unlock(&ch->lock);
            break;
        }
        size_t count = ch->work_count;
        ch->work_ready = 0;
        pthread_mutex_unlock(&ch->lock);

        if (count > 0) {
            channel_process(ch, ch->work, (int)count);
        }
    }
    return NULL;
}

static int channel_init(Channel* ch, const char* name, Board* board,
                        int streaming, int chunk_samples) {
    memset(ch, 0, sizeof(*ch));
    ch->name = name;
    ch->board = board;
    ch->streaming = streaming;
    ch->chunk_samples = chunk_samples;

    ch->fill = (float*)malloc((size_t)chunk_samples * sizeof(float));
    ch->work = (float*)malloc((size_t)chunk_samples * sizeof(float));
    ch->out = (char*)malloc((size_t)needle_out_capacity);
    if (!ch->fill || !ch->work || !ch->out) {
        return -1;
    }

    pthread_mutex_init(&ch->lock, NULL);
    pthread_cond_init(&ch->cond, NULL);
    if (pthread_create(&ch->thread, NULL, channel_thread, ch) != 0) {
        return -1;
    }
    ch->started = 1;
    return 0;
}

static void channel_destroy(Channel* ch) {
    if (ch->started) {
        pthread_mutex_lock(&ch->lock);
        pthread_cond_signal(&ch->cond);
        pthread_mutex_unlock(&ch->lock);
        pthread_join(ch->thread, NULL);
        pthread_mutex_destroy(&ch->lock);
        pthread_cond_destroy(&ch->cond);
        ch->started = 0;
    }
    free(ch->fill);
    free(ch->work);
    free(ch->out);
    ch->fill = ch->work = NULL;
    ch->out = NULL;
}

/* Called from the real-time audio callback: never blocks on the worker. */
static void channel_feed(Channel* ch, const float* pcm, size_t frames) {
    size_t offset = 0;
    while (offset < frames) {
        size_t space = (size_t)ch->chunk_samples - ch->fill_count;
        size_t to_copy = (frames - offset < space) ? (frames - offset) : space;

        memcpy(ch->fill + ch->fill_count, pcm + offset, to_copy * sizeof(float));
        ch->fill_count += to_copy;
        offset += to_copy;

        if (ch->fill_count < (size_t)ch->chunk_samples) {
            break;
        }

        /* Hand the full chunk to the worker, if it is idle. */
        pthread_mutex_lock(&ch->lock);
        if (!ch->work_ready) {
            float* tmp = ch->work;
            ch->work = ch->fill;
            ch->fill = tmp;
            ch->work_count = ch->fill_count;
            ch->work_ready = 1;
            pthread_cond_signal(&ch->cond);
        }
        /* else: worker is still busy - drop this chunk (prototype) */
        pthread_mutex_unlock(&ch->lock);

        ch->fill_count = 0;
    }
}

/* ------------------------------------------------------------------ */
/* SDK callback                                                         */
/* ------------------------------------------------------------------ */
static size_t skip_frames_remaining = 0;

static void capture_postprocess_callback(
    const float* mic,
    const float* near,
    const float* far,
    const float* mix,
    size_t frameSize,
    void* user_data) {
    (void)mic;
    (void)mix;
    (void)user_data;

    /* Discard startup audio (device start + AEC/AGC convergence), which the
     * model tends to hallucinate into filler phrases such as "Thank you." */
    if (skip_frames_remaining > 0) {
        if (frameSize <= skip_frames_remaining) {
            skip_frames_remaining -= frameSize;
            return;
        }
        if (near) near += skip_frames_remaining;
        if (far)  far  += skip_frames_remaining;
        frameSize -= skip_frames_remaining;
        skip_frames_remaining = 0;
    }

    if (near) {
        channel_feed(&chan_near, near, frameSize);
    }
    if (far) {
        channel_feed(&chan_far, far, frameSize);
    }
}

/* ------------------------------------------------------------------ */
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

int main(int argc, char* argv[]) {
    printf("======================================================================\n");
    printf("NvoiceAI SDK Realtime Demo + Cactus Needle Full-duplex ASR (C API)\n");
    printf("======================================================================\n\n");

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
    needle_sample_rate = config.cactus_needle.sample_rate;
    needle_chunk_samples = (needle_sample_rate / 1000) * config.cactus_needle.chunk_ms;
    far_chunk_samples = needle_chunk_samples * FAR_CHUNK_MULTIPLIER;
    needle_out_capacity = config.cactus_needle.out_capacity;
    needle_language = config.cactus_needle.language[0] ? config.cactus_needle.language : NULL;
    needle_keywords = config.cactus_needle.keywords[0] ? config.cactus_needle.keywords : NULL;
    skip_frames_remaining =
        (size_t)(needle_sample_rate / 1000) * (size_t)config.cactus_needle.skip_initial_ms;

    if (needle_chunk_samples <= 0 || needle_out_capacity <= 0) {
        fprintf(stderr, "Error: Invalid cactus_needle parameters in %s\n", config_file_path);
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
    free(model_data);

    if (!(needle_models() & NEEDLE_SPEECH)) {
        fprintf(stderr, "Error: Loaded model does not contain a speech model\n");
        return -1;
    }
    printf("Needle speech model loaded successfully!\n\n");

    needle_set_audio(needle_language, needle_keywords, 0);

    const int SAMPLE_RATE = config.audio.sample_rate;
    const int CHANNELS = config.audio.channels;
    const int FRAME_SIZE = (SAMPLE_RATE / 1000) * config.audio.frame_size_ms;

    printf("Audio Configuration:\n");
    printf("  Sample Rate: %d Hz\n", SAMPLE_RATE);
    printf("  Channels: %d\n", CHANNELS);
    printf("  Frame Size: %d samples (%d ms)\n\n", FRAME_SIZE, config.audio.frame_size_ms);

    if (SAMPLE_RATE != needle_sample_rate || CHANNELS != 1) {
        fprintf(stderr,
                "Warning: needle expects %d Hz mono audio, but the audio section specifies "
                "%d Hz / %d channel(s). Transcription quality may be degraded.\n\n",
                needle_sample_rate, SAMPLE_RATE, CHANNELS);
    }

    /* --- Start the two ASR workers --- */
    if (channel_init(&chan_near, "near", &board_near, 1, needle_chunk_samples) != 0 ||
        channel_init(&chan_far, "far", &board_far, 0, far_chunk_samples) != 0) {
        fprintf(stderr, "Error: Failed to start ASR worker threads\n");
        channel_destroy(&chan_near);
        channel_destroy(&chan_far);
        return -1;
    }

    /* --- Create and initialize the NvoiceAI SDK --- */
    printf("Creating SDK instance...\n");
    nvoiceai_handle_t sdk = nvoiceai_sdk_create_with_config(config_file_path);
    if (!sdk) {
        fprintf(stderr, "Error: Failed to create SDK instance\n");
        demo_running = 0;
        channel_destroy(&chan_near);
        channel_destroy(&chan_far);
        return -1;
    }

    printf("Initializing audio processing (AEC/NS/Gain) from %s...\n", config_file_path);
    if (nvoiceai_sdk_init_audio_processing_with_config(sdk, config_file_path) != 0) {
        fprintf(stderr, "Error: Failed to initialize audio processing\n");
        nvoiceai_sdk_destroy(sdk);
        demo_running = 0;
        channel_destroy(&chan_near);
        channel_destroy(&chan_far);
        return -1;
    }
    printf("Audio processing initialized successfully!\n\n");

    nvoiceai_sdk_set_aec_enabled(sdk, config.aec.enabled);

    signal(SIGINT, signal_handler);
    signal(SIGTERM, signal_handler);

    printf("Starting real-time full-duplex transcription...\n");
    if (nvoiceai_sdk_start(sdk, capture_postprocess_callback, NULL) != 0) {
        fprintf(stderr, "Error: Failed to start SDK\n");
        nvoiceai_sdk_destroy(sdk);
        demo_running = 0;
        channel_destroy(&chan_near);
        channel_destroy(&chan_far);
        return -1;
    }

    sleep(1);
    draw_ui();

    int num_iterations = (DEMO_DURATION_SECONDS * 1000) / DEMO_CHECK_INTERVAL_MS;
    for (int i = 0; i < num_iterations && demo_running; ++i) {
        usleep(DEMO_CHECK_INTERVAL_MS * 1000);
    }

    nvoiceai_sdk_stop(sdk);
    demo_running = 0;

    channel_destroy(&chan_near);
    channel_destroy(&chan_far);

    /* Commit the trailing words of the near-end stream. */
    char final_out[8192];
    pthread_mutex_lock(&needle_mutex);
    int final_words = needle_stream_transcribe_stop(final_out, (int)sizeof(final_out));
    pthread_mutex_unlock(&needle_mutex);
    if (final_words >= 0) {
        char final_text[4096];
        if (json_get_string(final_out, "text", final_text, sizeof(final_text)) &&
            !is_filler_text(final_text)) {
            board_append(&board_near, final_text);
        }
    }
    board_set_pending(&board_near, "");
    draw_ui();

    nvoiceai_sdk_destroy(sdk);

    printf("\n=== Processing Complete ===\n");
    printf("Near-end transcript:\n%s\n\n",
           board_near.committed_len ? board_near.committed : "(no speech detected)");
    printf("Far-end transcript:\n%s\n\n",
           board_far.committed_len ? board_far.committed : "(no speech detected)");
    printf("Demo completed.\n");
    return 0;
}
