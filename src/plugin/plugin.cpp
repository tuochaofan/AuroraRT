#include "aurorart/plugin/plugin.h"
#include "aurorart/utils/logger.h"
#include "aurorart/utils/config.h"
#include <filesystem>

#ifdef _WIN32
#include <windows.h>
#elif defined(__linux__)
#include <dlfcn.h>
#endif

namespace aurorart {
namespace plugin {

// DynamicPluginLoader implementation

std::shared_ptr<PluginInterface> DynamicPluginLoader::load(const std::string& path) {
    void* handle = nullptr;
    
#ifdef _WIN32
    handle = LoadLibraryA(path.c_str());
    if (!handle) {
        AURORA_LOG_ERROR("Failed to load plugin: {}. Error: {}", path, GetLastError());
        return nullptr;
    }
#elif defined(__linux__)
    handle = dlopen(path.c_str(), RTLD_LAZY);
    if (!handle) {
        AURORA_LOG_ERROR("Failed to load plugin: {}. Error: {}", path, dlerror());
        return nullptr;
    }
#endif
    
    // 定义插件创建和销毁函数类型
    typedef PluginInterface* (*CreatePluginFunc)();
    typedef void (*DestroyPluginFunc)(PluginInterface*);
    
    CreatePluginFunc createFunc = nullptr;
    DestroyPluginFunc destroyFunc = nullptr;
    
#ifdef _WIN32
    createFunc = (CreatePluginFunc)GetProcAddress(handle, "createPlugin");
    destroyFunc = (DestroyPluginFunc)GetProcAddress(handle, "destroyPlugin");
#elif defined(__linux__)
    createFunc = (CreatePluginFunc)dlsym(handle, "createPlugin");
    destroyFunc = (DestroyPluginFunc)dlsym(handle, "destroyPlugin");
#endif
    
    if (!createFunc || !destroyFunc) {
        AURORA_LOG_ERROR("Failed to find plugin entry points in {}", path);
#ifdef _WIN32
        FreeLibrary((HMODULE)handle);
#elif defined(__linux__)
        dlclose(handle);
#endif
        return nullptr;
    }
    
    // 创建插件实例
    PluginInterface* plugin = createFunc();
    if (!plugin) {
        AURORA_LOG_ERROR("Failed to create plugin instance from {}", path);
#ifdef _WIN32
        FreeLibrary((HMODULE)handle);
#elif defined(__linux__)
        dlclose(handle);
#endif
        return nullptr;
    }
    
    // 初始化插件
    if (!plugin->initialize()) {
        AURORA_LOG_ERROR("Failed to initialize plugin from {}", path);
        destroyFunc(plugin);
#ifdef _WIN32
        FreeLibrary((HMODULE)handle);
#elif defined(__linux__)
        dlclose(handle);
#endif
        return nullptr;
    }
    
    // 存储插件句柄
    pluginHandles_[plugin->getID()] = handle;
    
    AURORA_LOG_INFO("Loaded plugin: {} (v{})", plugin->getName(), plugin->getVersion());
    
    // 使用自定义删除器创建智能指针
    return std::shared_ptr<PluginInterface>(plugin, [this, destroyFunc, pluginId = plugin->getID()](PluginInterface* p) {
        destroyFunc(p);
        auto it = pluginHandles_.find(pluginId);
        if (it != pluginHandles_.end()) {
#ifdef _WIN32
            FreeLibrary((HMODULE)it->second);
#elif defined(__linux__)
            dlclose(it->second);
#endif
            pluginHandles_.erase(it);
        }
    });
}

bool DynamicPluginLoader::unload(const std::string& pluginId) {
    auto it = pluginHandles_.find(pluginId);
    if (it == pluginHandles_.end()) {
        AURORA_LOG_WARN("Plugin not found: {}", pluginId);
        return false;
    }
    
#ifdef _WIN32
    if (!FreeLibrary((HMODULE)it->second)) {
        AURORA_LOG_ERROR("Failed to unload plugin: {}. Error: {}", pluginId, GetLastError());
        return false;
    }
#elif defined(__linux__)
    if (dlclose(it->second) != 0) {
        AURORA_LOG_ERROR("Failed to unload plugin: {}. Error: {}", pluginId, dlerror());
        return false;
    }
#endif
    
    pluginHandles_.erase(it);
    AURORA_LOG_INFO("Unloaded plugin: {}", pluginId);
    return true;
}

std::vector<std::string> DynamicPluginLoader::listAvailablePlugins(const std::string& directory) {
    std::vector<std::string> plugins;
    
    try {
        for (const auto& entry : std::filesystem::directory_iterator(directory)) {
            if (entry.is_regular_file()) {
#ifdef _WIN32
                if (entry.path().extension() == ".dll") {
                    plugins.push_back(entry.path().string());
                }
#elif defined(__linux__)
                if (entry.path().extension() == ".so") {
                    plugins.push_back(entry.path().string());
                }
#endif
            }
        }
    } catch (const std::filesystem::filesystem_error& e) {
        AURORA_LOG_ERROR("Error listing plugins in {}: {}", directory, e.what());
    }
    
    return plugins;
}

// PluginRegistry implementation

void PluginRegistry::registerPlugin(std::shared_ptr<PluginInterface> plugin) {
    std::lock_guard<std::mutex> lock(mutex_);
    plugins_[plugin->getID()] = plugin;
    AURORA_LOG_INFO("Registered plugin: {} ({})", plugin->getName(), plugin->getID());
}

void PluginRegistry::unregisterPlugin(const std::string& pluginId) {
    std::lock_guard<std::mutex> lock(mutex_);
    auto it = plugins_.find(pluginId);
    if (it != plugins_.end()) {
        AURORA_LOG_INFO("Unregistered plugin: {} ({})", it->second->getName(), pluginId);
        plugins_.erase(it);
    } else {
        AURORA_LOG_WARN("Plugin not found for unregistration: {}", pluginId);
    }
}

std::shared_ptr<PluginInterface> PluginRegistry::getPlugin(const std::string& pluginId) const {
    std::lock_guard<std::mutex> lock(mutex_);
    auto it = plugins_.find(pluginId);
    if (it != plugins_.end()) {
        return it->second;
    }
    return nullptr;
}

std::vector<std::shared_ptr<PluginInterface>> PluginRegistry::listPlugins() const {
    std::lock_guard<std::mutex> lock(mutex_);
    std::vector<std::shared_ptr<PluginInterface>> pluginList;
    for (const auto& [id, plugin] : plugins_) {
        pluginList.push_back(plugin);
    }
    return pluginList;
}

// PluginManager implementation

PluginManager::PluginManager() {
}

PluginManager& PluginManager::instance() {
    static PluginManager instance;
    return instance;
}

void PluginManager::init() {
    loader_ = std::make_unique<DynamicPluginLoader>();
    registry_ = std::make_unique<PluginRegistry>();
    
    // 添加默认插件目录
    addPluginDirectory("./plugins");
    
    AURORA_LOG_INFO("Plugin manager initialized");
}

void PluginManager::start() {
    if (running_) {
        AURORA_LOG_WARN("Plugin manager is already running");
        return;
    }
    
    // 启动所有已加载的插件
    auto plugins = registry_->listPlugins();
    for (auto& plugin : plugins) {
        if (plugin->start()) {
            AURORA_LOG_INFO("Started plugin: {}", plugin->getName());
        } else {
            AURORA_LOG_ERROR("Failed to start plugin: {}", plugin->getName());
        }
    }
    
    running_ = true;
    AURORA_LOG_INFO("Plugin manager started");
}

void PluginManager::stop() {
    if (!running_) {
        AURORA_LOG_WARN("Plugin manager is not running");
        return;
    }
    
    // 停止所有已加载的插件
    auto plugins = registry_->listPlugins();
    for (auto& plugin : plugins) {
        if (plugin->stop()) {
            AURORA_LOG_INFO("Stopped plugin: {}", plugin->getName());
        } else {
            AURORA_LOG_ERROR("Failed to stop plugin: {}", plugin->getName());
        }
    }
    
    running_ = false;
    AURORA_LOG_INFO("Plugin manager stopped");
}

std::shared_ptr<PluginInterface> PluginManager::loadPlugin(const std::string& path) {
    std::lock_guard<std::mutex> lock(mutex_);
    
    auto plugin = loader_->load(path);
    if (plugin) {
        registry_->registerPlugin(plugin);
        
        // 如果插件管理器正在运行，启动插件
        if (running_ && !plugin->start()) {
            AURORA_LOG_ERROR("Failed to start plugin: {}", plugin->getName());
        }
    }
    
    return plugin;
}

bool PluginManager::unloadPlugin(const std::string& pluginId) {
    std::lock_guard<std::mutex> lock(mutex_);
    
    auto plugin = registry_->getPlugin(pluginId);
    if (!plugin) {
        AURORA_LOG_WARN("Plugin not found: {}", pluginId);
        return false;
    }
    
    // 停止插件
    if (running_ && !plugin->stop()) {
        AURORA_LOG_ERROR("Failed to stop plugin: {}", plugin->getName());
    }
    
    // 从注册表中移除
    registry_->unregisterPlugin(pluginId);
    
    // 卸载插件
    return loader_->unload(pluginId);
}

std::shared_ptr<PluginInterface> PluginManager::getPlugin(const std::string& pluginId) const {
    return registry_->getPlugin(pluginId);
}

std::vector<std::shared_ptr<PluginInterface>> PluginManager::listPlugins() const {
    return registry_->listPlugins();
}

void PluginManager::addPluginDirectory(const std::string& directory) {
    std::lock_guard<std::mutex> lock(mutex_);
    pluginDirectories_.push_back(directory);
    AURORA_LOG_INFO("Added plugin directory: {}", directory);
}

std::vector<std::string> PluginManager::listAvailablePlugins() const {
    std::lock_guard<std::mutex> lock(mutex_);
    std::vector<std::string> allPlugins;
    
    for (const auto& directory : pluginDirectories_) {
        auto plugins = loader_->listAvailablePlugins(directory);
        allPlugins.insert(allPlugins.end(), plugins.begin(), plugins.end());
    }
    
    return allPlugins;
}

} // namespace plugin
} // namespace aurorart
