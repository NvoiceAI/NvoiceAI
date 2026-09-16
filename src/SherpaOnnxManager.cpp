#include "SherpaOnnxManager.h"
#include "ConfigManager.h"
#include <cstring>
#include <iostream>
#include "sherpa-onnx/c-api/c-api.h"

SherpaOnnxManager::SherpaOnnxManager() = default;

SherpaOnnxManager::SherpaOnnxManager(SherpaOnnxManager&& other) noexcept
    : recognizer_(other.recognizer_),
      stream_(other.stream_),
      display_(other.display_),
      sample_rate_(other.sample_rate_)
{
    other.recognizer_ = nullptr;
    other.stream_ = nullptr;
    other.display_ = nullptr;
}

SherpaOnnxManager::~SherpaOnnxManager() {
    if (display_) SherpaOnnxDestroyDisplay(display_);
    if (stream_)  SherpaOnnxDestroyOnlineStream(stream_);
    if (recognizer_) SherpaOnnxDestroyOnlineRecognizer(recognizer_);
    display_ = nullptr;
    stream_ = nullptr;
    recognizer_ = nullptr;
}

bool SherpaOnnxManager::Init(const ConfigManager::SherpaOnnxConfig& cfg) {
    sample_rate_ = cfg.sample_rate;
    
    SherpaOnnxOnlineRecognizerConfig c;
    std::memset(&c, 0, sizeof(c));

    c.model_config.debug = 0;
    c.model_config.num_threads = cfg.num_threads;
    c.model_config.provider = cfg.provider.c_str();
    c.decoding_method = cfg.decoding_method.c_str();
    c.feat_config.sample_rate = cfg.sample_rate;
    c.feat_config.feature_dim = cfg.feature_dim;
    c.enable_endpoint = cfg.enable_endpoint ? 1 : 0;
    c.rule1_min_trailing_silence = cfg.rule1_min_trailing_silence;
    c.rule2_min_trailing_silence = cfg.rule2_min_trailing_silence;
    c.rule3_min_utterance_length = cfg.rule3_min_utterance_length;

    // Build paths relative to model_dir
    std::string base = cfg.model_dir;
    if (!base.empty() && base.back() != '/') base.push_back('/');

    static std::string enc, dec, join, toks;
    enc = base + cfg.encoder;
    dec = base + cfg.decoder;
    join = base + cfg.joiner;
    toks = base + cfg.tokens;
    
    c.model_config.transducer.encoder = enc.c_str();
    c.model_config.transducer.decoder = dec.c_str();
    c.model_config.transducer.joiner  = join.c_str();
    c.model_config.tokens = toks.c_str();

    recognizer_ = SherpaOnnxCreateOnlineRecognizer(&c);
    if (!recognizer_) {
        std::cerr << "SherpaOnnxManager: failed to create recognizer\n";
        return false;
    }
    stream_ = SherpaOnnxCreateOnlineStream(recognizer_);
    display_ = SherpaOnnxCreateDisplay(50);
    return (recognizer_ && stream_ && display_) ? true : false;
}

void SherpaOnnxManager::AcceptWaveform(const float* samples, int32_t nframes) {
    if (!stream_ || !recognizer_) return;
    SherpaOnnxOnlineStreamAcceptWaveform(stream_, sample_rate_, samples, nframes);
}

void SherpaOnnxManager::DecodeUntilReady() {
    if (!stream_ || !recognizer_) return;
    while (SherpaOnnxIsOnlineStreamReady(recognizer_, stream_)) {
        SherpaOnnxDecodeOnlineStream(recognizer_, stream_);
    }
}

std::string SherpaOnnxManager::GetResultText() {
    if (!stream_ || !recognizer_) return std::string();
    const SherpaOnnxOnlineRecognizerResult* r = SherpaOnnxGetOnlineStreamResult(recognizer_, stream_);
    if (!r) return std::string();
    std::string text = r->text ? r->text : std::string();
    SherpaOnnxDestroyOnlineRecognizerResult(r);
    return text;
}

bool SherpaOnnxManager::IsEndpoint() const {
    if (!stream_ || !recognizer_) return false;
    return SherpaOnnxOnlineStreamIsEndpoint(recognizer_, stream_) ? true : false;
}

void SherpaOnnxManager::ResetStream() {
    if (!stream_) return;
    SherpaOnnxOnlineStreamReset(recognizer_, stream_);
}

void SherpaOnnxManager::PrintDisplay(int32_t segment_index, const char* text) {
    if (display_) SherpaOnnxPrint(display_, segment_index, text);
}