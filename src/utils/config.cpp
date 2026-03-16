#include "aurorart/utils/config.h"
#include <fstream>
#include <sstream>

namespace aurorart {
namespace utils {

Config& Config::instance() {
    static Config instance;
    return instance;
}

bool Config::load(const std::string& configFile) {
    try {
        std::ifstream file(configFile);
        if (!file.is_open()) {
            return false;
        }
        
        std::stringstream buffer;
        buffer << file.rdbuf();
        std::string content = buffer.str();
        
        root_ = nlohmann::json::parse(content);
        return true;
    } catch (const nlohmann::json::parse_error& e) {
        std::cerr << "Config parse error: " << e.what() << std::endl;
        return false;
    } catch (...) {
        std::cerr << "Config load error: unknown exception" << std::endl;
        return false;
    }
}

bool Config::save(const std::string& configFile) const {
    try {
        std::ofstream file(configFile);
        if (!file.is_open()) {
            return false;
        }
        
        file << root_.dump(4);
        return true;
    } catch (...) {
        return false;
    }
}

bool Config::loadFromString(const std::string& content) {
    try {
        root_ = nlohmann::json::parse(content);
        return true;
    } catch (const nlohmann::json::parse_error& e) {
        std::cerr << "Config parse error: " << e.what() << std::endl;
        return false;
    }
}

std::string Config::saveToString() const {
    return root_.dump(4);
}

bool Config::contains(const std::string& key) const {
    try {
        nlohmann::json current = root_;
        size_t pos = 0;
        std::string keyPart;
        
        while ((pos = key.find('.')) != std::string::npos) {
            keyPart = key.substr(0, pos);
            if (!current.contains(keyPart)) {
                return false;
            }
            current = current[keyPart];
            key = key.substr(pos + 1);
        }
        
        return current.contains(key);
    } catch (...) {
        return false;
    }
}

void Config::clear() {
    root_ = nlohmann::json::object();
}

} // namespace utils
} // namespace aurorart