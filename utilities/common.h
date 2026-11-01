#pragma once

// Demo duration in seconds (can be modified to change how long the demo runs)
#define DEMO_DURATION_SECONDS 60

// Interval in milliseconds for checking demo duration
#define DEMO_CHECK_INTERVAL_MS 500

// Interval in seconds for printing frame statistics
#define FRAME_PRINT_INTERVAL_SECONDS 5

struct AudioConfig {
    int sampleRate;
    int channels;
    int frameSize;
};


