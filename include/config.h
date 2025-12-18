#pragma once

#include <string>
#include <map>

namespace DmitriCompat {

class Config {
public:
    static Config& GetInstance();

    bool Load(const std::string& configPath);

    // 修复开关
    bool IsTextureFormatConversionEnabled() const;
    bool IsColorSpaceCorrectionEnabled() const;
    bool IsGPUSyncEnabled() const;
    bool IsShaderRegisterRemapEnabled() const;

    // 调试选项
    int GetLogLevel() const;
    bool IsDumpTexturesEnabled() const;
    bool IsDumpShadersEnabled() const;

    // 通用获取函数
    int GetInt(const std::string& section, const std::string& key, int defaultValue) const;
    bool GetBool(const std::string& section, const std::string& key, bool defaultValue) const;
    std::string GetString(const std::string& section, const std::string& key, const std::string& defaultValue) const;

private:
    Config() = default;
    ~Config() = default;

    Config(const Config&) = delete;
    Config& operator=(const Config&) = delete;

    std::string MakeKey(const std::string& section, const std::string& key) const;

    std::map<std::string, std::string> values_;
    bool loaded_ = false;
};

} // namespace DmitriCompat
