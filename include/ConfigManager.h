#pragma once

#include <string>
#include <memory>

/**
 * Configuration Manager
 * 
 * Loads and manages configuration from JSON file for easy tuning
 * without recompiling code.
 */
class ConfigManager {
public:
    struct AudioConfig {
        int sample_rate = 16000;
        int channels = 1;
        int frame_size_ms = 10;
        int frameSize() const { return sample_rate / (1000 / frame_size_ms); }
    };
    
    struct AecConfig {
        bool enabled = true;
        bool mobile_mode = false;
        bool high_pass_filter_enabled = true;
    };
    
    struct GainControlConfig {
        bool agc1_enabled = true;
        std::string agc1_mode = "adaptive_analog";  // adaptive_analog, adaptive_digital, fixed_digital
        bool agc2_enabled = true;
    };

    struct SysOutGainConfig {
        float pre_gain = 1.0f;          // Gain applied before processing (input gain 0.0 - 2.0)
        float post_gain_ratio = 1.0f;   // Ratio of processed output to original input (0.0-1.0 or 0% - 100%)
    };
    
    struct OutputConfig {
        bool wav_dump_enabled = false;
        std::string wav_dump_directory = "./output";
    };
    
    /**
     * Load configuration from JSON file
     * @param config_file Path to config.json file
     * @return true if loaded successfully
     */
    static bool load(const std::string& config_file);
    
    /**
     * Get audio configuration
     */
    static const AudioConfig& getAudioConfig();
    
    /**
     * Get AEC configuration
     */
    static const AecConfig& getAecConfig();
    
    /**
     * Get gain control configuration
     */
    static const GainControlConfig& getGainControlConfig();

    /**
     * Get system output gain configuration
     */
    static const SysOutGainConfig& getSysOutGainConfig();

    /**
     * Get output configuration
     */
    static const OutputConfig& getOutputConfig();
    
    /**
     * Print current configuration (for debugging)
     */
    static void printConfig();

private:
    ConfigManager() = default;
    
    static AudioConfig audio_config_;
    static AecConfig aec_config_;
    static GainControlConfig gain_control_config_;
    static SysOutGainConfig sys_out_gain_config_;
    static OutputConfig output_config_;
    static bool loaded_;
};
