#ifndef AURORART_PLUGIN_H
#define AURORART_PLUGIN_H

#include <string>
#include <memory>
#include <vector>
#include <map>
#include <mutex>

namespace aurorart {
namespace plugin {

class PluginInterface {
public:
    virtual ~PluginInterface() = default;
    
    virtual bool initialize() = 0;
    virtual bool start() = 0;
    virtual bool stop() = 0;
    virtual std::string getID() const = 0;
    virtual std::string getName() const = 0;
    virtual std::string getVersion() const = 0;
    virtual std::string getDescription() const = 0;
};

class PluginLoader {
public:
    virtual ~PluginLoader() = default;
    
    virtual std::shared_ptr<PluginInterface> load(const std::string& path) = 0;
    virtual bool unload(const std::string& pluginId) = 0;
    virtual std::vector<std::string> listAvailablePlugins(const std::string& directory) = 0;
};

class DynamicPluginLoader : public PluginLoader {
public:
    std::shared_ptr<PluginInterface> load(const std::string& path) override;
    bool unload(const std::string& pluginId) override;
    std::vector<std::string> listAvailablePlugins(const std::string& directory) override;
    
private:
    std::map<std::string, void*> pluginHandles_;
};

class PluginRegistry {
public:
    void registerPlugin(std::shared_ptr<PluginInterface> plugin);
    void unregisterPlugin(const std::string& pluginId);
    std::shared_ptr<PluginInterface> getPlugin(const std::string& pluginId) const;
    std::vector<std::shared_ptr<PluginInterface>> listPlugins() const;
    
private:
    std::map<std::string, std::shared_ptr<PluginInterface>> plugins_;
    mutable std::mutex mutex_;
};

class PluginManager {
public:
    static PluginManager& instance();
    
    void init();
    void start();
    void stop();
    
    std::shared_ptr<PluginInterface> loadPlugin(const std::string& path);
    bool unloadPlugin(const std::string& pluginId);
    std::shared_ptr<PluginInterface> getPlugin(const std::string& pluginId) const;
    std::vector<std::shared_ptr<PluginInterface>> listPlugins() const;
    
    void addPluginDirectory(const std::string& directory);
    std::vector<std::string> listAvailablePlugins() const;
    
private:
    PluginManager();
    
    std::unique_ptr<PluginLoader> loader_;
    std::unique_ptr<PluginRegistry> registry_;
    std::vector<std::string> pluginDirectories_;
    mutable std::mutex mutex_;
    bool running_ = false;
};

// 插件接口宏定义
#define AURORART_PLUGIN_INTERFACE(PluginClass) \
extern "C" { \
    PluginClass* createPlugin() { \
        return new PluginClass(); \
    } \
    void destroyPlugin(PluginClass* plugin) { \
        delete plugin; \
    } \
}

} // namespace plugin
} // namespace aurorart

#endif // AURORART_PLUGIN_H
