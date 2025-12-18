#include "config.h"
#include <fstream>
#include <sstream>
#include <algorithm>

namespace DmitriCompat {

Config& Config::GetInstance() {
    static Config instance;
    return instance;
}

bool Config::Load(const std::string& configPath) {
    std::ifstream file(configPath);
    if (!file.is_open()) {
        return false;
    }

    std::string currentSection;
    std::string line;

    while (std::getline(file, line)) {
        // 移除前后空白
        line.erase(0, line.find_first_not_of(" \t\r\n"));
        line.erase(line.find_last_not_of(" \t\r\n") + 1);

        // 跳过空行和注释
        if (line.empty() || line[0] == ';' || line[0] == '#') {
            continue;
        }

        // 解析节
        if (line[0] == '[' && line.back() == ']') {
            currentSection = line.substr(1, line.length() - 2);
            continue;
        }

        // 解析键值对
        size_t pos = line.find('=');
        if (pos != std::string::npos) {
            std::string key = line.substr(0, pos);
            std::string value = line.substr(pos + 1);

            // 移除键和值的空白
            key.erase(0, key.find_first_not_of(" \t"));
            key.erase(key.find_last_not_of(" \t") + 1);
            value.erase(0, value.find_first_not_of(" \t"));
            value.erase(value.find_last_not_of(" \t") + 1);

            // 存储
            values_[MakeKey(currentSection, key)] = value;
        }
    }

    loaded_ = true;
    return true;
}

bool Config::IsTextureFormatConversionEnabled() const {
    return GetBool("Fixes", "EnableTextureFormatConversion", true);
}

bool Config::IsColorSpaceCorrectionEnabled() const {
    return GetBool("Fixes", "EnableColorSpaceCorrection", false);
}

bool Config::IsGPUSyncEnabled() const {
    return GetBool("Fixes", "EnableGPUSync", false);
}

bool Config::IsShaderRegisterRemapEnabled() const {
    return GetBool("Fixes", "EnableShaderRegisterRemap", false);
}

int Config::GetLogLevel() const {
    return GetInt("Debug", "LogLevel", 2); // 默认 Info 级别
}

bool Config::IsDumpTexturesEnabled() const {
    return GetBool("Debug", "DumpTextures", false);
}

bool Config::IsDumpShadersEnabled() const {
    return GetBool("Debug", "DumpShaders", false);
}

int Config::GetInt(const std::string& section, const std::string& key, int defaultValue) const {
    std::string fullKey = MakeKey(section, key);
    auto it = values_.find(fullKey);

    if (it != values_.end()) {
        try {
            return std::stoi(it->second);
        } catch (...) {
            return defaultValue;
        }
    }

    return defaultValue;
}

bool Config::GetBool(const std::string& section, const std::string& key, bool defaultValue) const {
    std::string fullKey = MakeKey(section, key);
    auto it = values_.find(fullKey);

    if (it != values_.end()) {
        std::string value = it->second;
        // 转小写
        std::transform(value.begin(), value.end(), value.begin(), ::tolower);

        if (value == "true" || value == "1" || value == "yes" || value == "on") {
            return true;
        } else if (value == "false" || value == "0" || value == "no" || value == "off") {
            return false;
        }
    }

    return defaultValue;
}

std::string Config::GetString(const std::string& section, const std::string& key, const std::string& defaultValue) const {
    std::string fullKey = MakeKey(section, key);
    auto it = values_.find(fullKey);

    if (it != values_.end()) {
        return it->second;
    }

    return defaultValue;
}

std::string Config::MakeKey(const std::string& section, const std::string& key) const {
    return section + "." + key;
}

} // namespace DmitriCompat
