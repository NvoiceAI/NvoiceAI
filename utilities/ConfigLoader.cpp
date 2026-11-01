#include "ConfigLoader.h"
#include <iostream>
#include <fstream>
#include <cstring>

// Simple JSON parsing (using raw string parsing for no external dependencies)
// For production, consider using a proper JSON library like nlohmann/json

// Static member initialization
ConfigLoader::OutputConfig ConfigLoader::output_config_;
ConfigLoader::SherpaOnnxConfig ConfigLoader::sherpa_onnx_config_;
bool ConfigLoader::loaded_ = false;

/**
 * Extract a JSON section (object) from the full JSON
 * For example: extractJsonSection(json, "aec") returns the content of {"aec": {...}}
 */
static std::string extractJsonSection(const std::string& json, const std::string& section_name) {
    std::string search_key = "\"" + section_name + "\"";
    size_t pos = json.find(search_key);
    if (pos == std::string::npos) {
        return "";
    }
    
    // Find the opening brace
    pos = json.find("{", pos);
    if (pos == std::string::npos) {
        return "";
    }
    
    // Find the matching closing brace
    int brace_count = 0;
    size_t end_pos = pos;
    for (size_t i = pos; i < json.length(); i++) {
        if (json[i] == '{') brace_count++;
        else if (json[i] == '}') {
            brace_count--;
            if (brace_count == 0) {
                end_pos = i + 1;
                break;
            }
        }
    }
    
    return json.substr(pos, end_pos - pos);
}

/**
 * Simple JSON value extractor
 * Extracts string or numeric values from JSON
 */
static std::string extractJsonValue(const std::string& json, const std::string& key) {
    std::string search_key = "\"" + key + "\"";
    size_t pos = json.find(search_key);
    if (pos == std::string::npos) {
        return "";
    }
    
    // Find the colon after the key
    pos = json.find(":", pos);
    if (pos == std::string::npos) {
        return "";
    }
    
    // Skip whitespace and quotes
    pos++;
    while (pos < json.length() && (json[pos] == ' ' || json[pos] == '\t' || json[pos] == '\n' || json[pos] == '\r')) {
        pos++;
    }
    
    if (pos >= json.length()) {
        return "";
    }
    
    // Check if value is a string (quoted) or number/bool
    if (json[pos] == '"') {
        // String value
        pos++;
        size_t end_pos = json.find('"', pos);
        if (end_pos == std::string::npos) {
            return "";
        }
        return json.substr(pos, end_pos - pos);
    } else {
        // Number or boolean
        size_t end_pos = pos;
        while (end_pos < json.length() && json[end_pos] != ',' && json[end_pos] != '}' && json[end_pos] != ']') {
            end_pos++;
        }
        std::string value = json.substr(pos, end_pos - pos);
        
        // Trim whitespace
        while (!value.empty() && (value.back() == ' ' || value.back() == '\n' || value.back() == '\r' || value.back() == '\t')) {
            value.pop_back();
        }
        return value;
    }
}

static bool extractJsonBool(const std::string& json, const std::string& key, bool default_val = false) {
    std::string value = extractJsonValue(json, key);
    if (value == "true") return true;
    if (value == "false") return false;
    return default_val;
}

static int extractJsonInt(const std::string& json, const std::string& key, int default_val = 0) {
    std::string value = extractJsonValue(json, key);
    if (value.empty()) return default_val;
    try {
        return std::stoi(value);
    } catch (...) {
        return default_val;
    }
}

static float extractJsonFloat(const std::string& json, const std::string& key, float default_val = 0.0f) {
    std::string value = extractJsonValue(json, key);
    if (value.empty()) return default_val;
    try {
        return std::stof(value);
    } catch (...) {
        return default_val;
    }
}

bool ConfigLoader::load(const std::string& config_file) {
    std::ifstream file(config_file);
    if (!file.is_open()) {
        std::cerr << "ConfigLoader: Cannot open config file: " << config_file << "\n";
        return false;
    }
    
    // Read entire file
    std::string json((std::istreambuf_iterator<char>(file)), std::istreambuf_iterator<char>());
    file.close();

    // Parse output section
    std::string output_section = extractJsonSection(json, "output");
    output_config_.save_audio_files_enabled = extractJsonBool(output_section, "save_audio_files_enabled", false);
    output_config_.save_audio_files_directory = extractJsonValue(output_section, "save_audio_files_directory");
    if (output_config_.save_audio_files_directory.empty()) {
        output_config_.save_audio_files_directory = "./output";
    }
    
    // Parse Sherpa ONNX section
    std::string sherpa_onnx_section = extractJsonSection(json, "sherpa_onnx");
    sherpa_onnx_config_.enabled = extractJsonBool(sherpa_onnx_section, "enabled", false);
    sherpa_onnx_config_.model_dir = extractJsonValue(sherpa_onnx_section, "model_dir");
    sherpa_onnx_config_.sample_rate = extractJsonInt(sherpa_onnx_section, "sample_rate", 16000);
    sherpa_onnx_config_.num_threads = extractJsonInt(sherpa_onnx_section, "num_threads", 1);
    sherpa_onnx_config_.enable_endpoint = extractJsonBool(sherpa_onnx_section, "enable_endpoint", true);
    sherpa_onnx_config_.encoder = extractJsonValue(sherpa_onnx_section, "encoder");
    sherpa_onnx_config_.decoder = extractJsonValue(sherpa_onnx_section, "decoder");
    sherpa_onnx_config_.joiner = extractJsonValue(sherpa_onnx_section, "joiner");
    sherpa_onnx_config_.tokens = extractJsonValue(sherpa_onnx_section, "tokens");
    sherpa_onnx_config_.rule1_min_trailing_silence = extractJsonFloat(sherpa_onnx_section, "rule1_min_trailing_silence", 2.4f);
    sherpa_onnx_config_.rule2_min_trailing_silence = extractJsonFloat(sherpa_onnx_section, "rule2_min_trailing_silence", 1.2f);
    sherpa_onnx_config_.rule3_min_utterance_length = extractJsonInt(sherpa_onnx_section, "rule3_min_utterance_length", 300);
    
    std::string decoding_method = extractJsonValue(sherpa_onnx_section, "decoding_method");
    sherpa_onnx_config_.decoding_method = decoding_method.empty() ? "greedy_search" : decoding_method;
    
    sherpa_onnx_config_.feature_dim = extractJsonInt(sherpa_onnx_section, "feature_dim", 80);
    
    std::string provider = extractJsonValue(sherpa_onnx_section, "provider");
    sherpa_onnx_config_.provider = provider.empty() ? "cpu" : provider;
    
    // Apply defaults if not specified
    if (sherpa_onnx_config_.encoder.empty()) sherpa_onnx_config_.encoder = "encoder-epoch-99-avg-1.int8.onnx";
    if (sherpa_onnx_config_.decoder.empty()) sherpa_onnx_config_.decoder = "decoder-epoch-99-avg-1.onnx";
    if (sherpa_onnx_config_.joiner.empty()) sherpa_onnx_config_.joiner = "joiner-epoch-99-avg-1.int8.onnx";
    if (sherpa_onnx_config_.tokens.empty()) sherpa_onnx_config_.tokens = "tokens.txt";
    
    loaded_ = true;

    std::cout << "ConfigLoader: Configuration loaded from " << config_file << "\n";

    return true;
}

const ConfigLoader::OutputConfig& ConfigLoader::getOutputConfig() {
    return output_config_;
}

const ConfigLoader::SherpaOnnxConfig& ConfigLoader::getSherpaOnnxConfig() {
    return sherpa_onnx_config_;
}

void ConfigLoader::printConfig() {
    std::cout << "\n=== Current Configuration ===\n";
    
    std::cout << "Output:\n";
    std::cout << "  Audio File Saving Enabled: " << (output_config_.save_audio_files_enabled ? "true" : "false") << "\n";
    std::cout << "  Audio File Saving Directory: " << output_config_.save_audio_files_directory << "\n";
    
    std::cout << "Sherpa ONNX:\n";
    std::cout << "  Enabled: " << (sherpa_onnx_config_.enabled ? "true" : "false") << "\n";
    if (!sherpa_onnx_config_.model_dir.empty()) {
        std::cout << "  Model Dir: " << sherpa_onnx_config_.model_dir << "\n";
    }
    std::cout << "  Num Threads: " << sherpa_onnx_config_.num_threads << "\n";
    std::cout << "  Enable Endpoint: " << (sherpa_onnx_config_.enable_endpoint ? "true" : "false") << "\n";
    std::cout << "  Encoder: " << sherpa_onnx_config_.encoder << "\n";
    std::cout << "  Decoder: " << sherpa_onnx_config_.decoder << "\n";
    std::cout << "  Joiner: " << sherpa_onnx_config_.joiner << "\n";
    std::cout << "  Tokens: " << sherpa_onnx_config_.tokens << "\n";
    std::cout << "==============================\n\n";
}
