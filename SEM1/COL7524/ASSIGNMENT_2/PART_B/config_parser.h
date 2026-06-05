#ifndef CONFIG_PARSER_H
#define CONFIG_PARSER_H

#include <iostream>
#include <fstream>
#include <string>
#include <map>
#include <sstream>
#include <algorithm>

class ConfigParser {
private:
    std::map<std::string, std::string> config;

    std::string trim(const std::string& str) {
        if (str.empty()) return "";
        
        // Remove all quotes and whitespace
        std::string result;
        for (char c : str) {
            if (c != '"' && c != ' ' && c != '\t' && c != '\n' && c != '\r') {
                result += c;
            }
        }
        return result;
    }

public:
    bool load(const std::string& filename) {
        std::ifstream file(filename);
        if (!file.is_open()) {
            std::cerr << "Cannot open config file: " << filename << std::endl;
            return false;
        }

        std::string line;
        while (std::getline(file, line)) {
            // Remove comments
            size_t comment_pos = line.find("//");
            if (comment_pos != std::string::npos) {
                line = line.substr(0, comment_pos);
            }

            // Skip empty lines and braces
            if (line.empty() || line.find('{') != std::string::npos || line.find('}') != std::string::npos) {
                continue;
            }

            // Parse key:value
            size_t colon_pos = line.find(':');
            if (colon_pos != std::string::npos && colon_pos > 0 && colon_pos < line.length() - 1) {
                std::string key = line.substr(0, colon_pos);
                std::string value = line.substr(colon_pos + 1);
                
                key = trim(key);
                value = trim(value);
                
                // Remove trailing comma if present
                if (!value.empty() && value.back() == ',') {
                    value.pop_back();
                }
                
                if (!key.empty() && !value.empty()) {
                    config[key] = value;
                }
            }
        }

        file.close();
        return true;
    }

    std::string getString(const std::string& key, const std::string& defaultValue = "") {
        auto it = config.find(key);
        if (it != config.end()) {
            return it->second;
        }
        std::cerr << "Config key '" << key << "' not found, using default: " << defaultValue << std::endl;
        return defaultValue;
    }

    int getInt(const std::string& key, int defaultValue = 0) {
        auto it = config.find(key);
        if (it != config.end()) {
            try {
                return std::stoi(it->second);
            } catch (...) {
                std::cerr << "Config key '" << key << "' has invalid integer, using default: " << defaultValue << std::endl;
                return defaultValue;
            }
        }
        std::cerr << "Config key '" << key << "' not found, using default: " << defaultValue << std::endl;
        return defaultValue;
    }
};

#endif