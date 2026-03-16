#ifndef AURORART_CONFIG_H
#define AURORART_CONFIG_H

#include <string>
#include <map>
#include <nlohmann/json.hpp>

namespace aurorart {
namespace utils {

class Config {
public:
    static Config& instance();
    
    bool load(const std::string& configFile);
    bool save(const std::string& configFile) const;
    
    bool loadFromString(const std::string& content);
    std::string saveToString() const;
    
    bool contains(const std::string& key) const;
    void clear();
    
    template<typename T>
    T get(const std::string& key, const T& defaultValue = T()) const;
    
    template<typename T>
    void set(const std::string& key, const T& value);
    
    const nlohmann::json& getRoot() const { return root_; }
    
private:
    Config() = default;
    nlohmann::json root_;
};

// 模板实现
template<typename T>
T Config::get(const std::string& key, const T& defaultValue) const {
    try {
        // 支持点号分隔的路径，如 "network.port"
        nlohmann::json current = root_;
        size_t pos = 0;
        std::string keyPart;
        
        while ((pos = key.find('.')) != std::string::npos) {
            keyPart = key.substr(0, pos);
            if (!current.contains(keyPart)) {
                return defaultValue;
            }
            current = current[keyPart];
            key = key.substr(pos + 1);
        }
        
        if (current.contains(key)) {
            return current[key].get<T>();
        }
    } catch (...) {
        // 解析错误，返回默认值
    }
    
    return defaultValue;
}

template<typename T>
void Config::set(const std::string& key, const T& value) {
    // 支持点号分隔的路径，如 "network.port"
    nlohmann::json* current = &root_;
    size_t pos = 0;
    std::string keyPart;
    
    while ((pos = key.find('.')) != std::string::npos) {
        keyPart = key.substr(0, pos);
        if (!current->contains(keyPart)) {
            (*current)[keyPart] = nlohmann::json::object();
        }
        current = &(*current)[keyPart];
        key = key.substr(pos + 1);
    }
    
    (*current)[key] = value;
}

} // namespace utils
} // namespace aurorart

#endif // AURORART_CONFIG_H