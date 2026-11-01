#pragma once

#include <string>
#include <memory>

/**
 * Configuration Manager
 * 
 * Loads and manages configuration from JSON file for easy tuning
 * without recompiling code.
 */
class ConfigLoader {
public:    
    struct OutputConfig {
        bool save_audio_files_enabled = false;
        std::string save_audio_files_directory = "./output";
    };

    struct SherpaOnnxConfig {
        bool enabled = false;
        std::string model_dir;
        int sample_rate = 16000;
        int num_threads = 1;
        bool enable_endpoint = true;
        std::string encoder = "encoder-epoch-99-avg-1.int8.onnx";
        std::string decoder = "decoder-epoch-99-avg-1.onnx";
        std::string joiner = "joiner-epoch-99-avg-1.int8.onnx";
        std::string tokens = "tokens.txt";
        float rule1_min_trailing_silence = 2.4f;
        float rule2_min_trailing_silence = 1.2f;
        int rule3_min_utterance_length = 300;
        std::string decoding_method = "greedy_search";
        int feature_dim = 80;
        std::string provider = "cpu";
    };
    
    /**
     * Load configuration from JSON file
     * @param config_file Path to config.json file
     * @return true if loaded successfully
     */
    static bool load(const std::string& config_file);

    /**
     * Get output configuration
     */
    static const OutputConfig& getOutputConfig();

    /**
     * Get Sherpa ONNX speech recognition configuration
     */
    static const SherpaOnnxConfig& getSherpaOnnxConfig();
    
    /**
     * Print current configuration (for debugging)
     */
    static void printConfig();

private:
    ConfigLoader() = default;
    static OutputConfig output_config_;
    static SherpaOnnxConfig sherpa_onnx_config_;
    static bool loaded_;
};
