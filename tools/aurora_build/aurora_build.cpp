#include <iostream>
#include <string>
#include <vector>
#include <unordered_map>
#include <functional>
#include <memory>
#include <fstream>
#include <sstream>
#include <filesystem>

namespace aurorart {
namespace build {

namespace fs = std::filesystem;

class ConfigParser {
public:
    ConfigParser(const std::string& configFile) : configFile_(configFile) {
        parse();
    }
    
    std::string getName() const {
        return getValue("name");
    }
    
    std::string getType() const {
        return getValue("type");
    }
    
    std::string getVersion() const {
        return getValue("version");
    }
    
    std::vector<std::string> getDependencies() const {
        return getList("dependencies");
    }
    
    std::vector<std::string> getSources() const {
        return getList("sources");
    }
    
    std::string getValue(const std::string& key) const {
        auto it = config_.find(key);
        if (it != config_.end()) {
            return it->second;
        }
        return "";
    }
    
    std::vector<std::string> getList(const std::string& key) const {
        std::vector<std::string> result;
        auto it = configLists_.find(key);
        if (it != configLists_.end()) {
            result = it->second;
        }
        return result;
    }
    
private:
    void parse() {
        std::ifstream file(configFile_);
        if (!file.is_open()) {
            std::cerr << "Failed to open config file: " << configFile_ << std::endl;
            return;
        }
        
        std::string line;
        std::string currentSection;
        
        while (std::getline(file, line)) {
            // Remove comments
            size_t commentPos = line.find('#');
            if (commentPos != std::string::npos) {
                line = line.substr(0, commentPos);
            }
            
            // Trim whitespace
            line = trim(line);
            
            if (line.empty()) {
                continue;
            }
            
            // Check for section
            if (line.front() == '[' && line.back() == ']') {
                currentSection = line.substr(1, line.size() - 2);
                continue;
            }
            
            // Check for key-value pair
            size_t colonPos = line.find(':');
            if (colonPos != std::string::npos) {
                std::string key = trim(line.substr(0, colonPos));
                std::string value = trim(line.substr(colonPos + 1));
                
                if (!currentSection.empty()) {
                    key = currentSection + "." + key;
                }
                
                config_[key] = value;
            }
        }
    }
    
    std::string trim(const std::string& str) {
        size_t start = str.find_first_not_of(" \t");
        if (start == std::string::npos) return "";
        size_t end = str.find_last_not_of(" \t");
        return str.substr(start, end - start + 1);
    }
    
    std::string configFile_;
    std::unordered_map<std::string, std::string> config_;
    std::unordered_map<std::string, std::vector<std::string>> configLists_;
};

class DependencyManager {
public:
    void addDependency(const std::string& dep) {
        dependencies_.push_back(dep);
    }
    
    void resolveDependencies() {
        std::cout << "Resolving dependencies..." << std::endl;
        for (const auto& dep : dependencies_) {
            std::cout << "  - " << dep << std::endl;
        }
    }
    
    const std::vector<std::string>& getDependencies() const {
        return dependencies_;
    }
    
private:
    std::vector<std::string> dependencies_;
};

class PlatformAdapter {
public:
    virtual std::string getPlatformName() const = 0;
    virtual std::string getCompilerCommand() const = 0;
    virtual std::string getLinkerCommand() const = 0;
    virtual std::vector<std::string> getCompilerFlags() const = 0;
    virtual std::vector<std::string> getLinkerFlags() const = 0;
    virtual std::string getBuildDirectory() const = 0;
    virtual std::string getOutputExtension() const = 0;
    virtual bool supportsParallelBuild() const = 0;
    virtual int getMaxParallelJobs() const = 0;
};

class LinuxPlatformAdapter : public PlatformAdapter {
public:
    std::string getPlatformName() const override {
        return "Linux";
    }
    
    std::string getCompilerCommand() const override {
        return "g++";
    }
    
    std::string getLinkerCommand() const override {
        return "g++";
    }
    
    std::vector<std::string> getCompilerFlags() const override {
        return {"-std=c++17", "-O2", "-Wall", "-Wextra"};
    }
    
    std::vector<std::string> getLinkerFlags() const override {
        return {};
    }
    
    std::string getBuildDirectory() const override {
        return "build/linux";
    }
    
    std::string getOutputExtension() const override {
        return "";
    }
    
    bool supportsParallelBuild() const override {
        return true;
    }
    
    int getMaxParallelJobs() const override {
        return std::thread::hardware_concurrency();
    }
};

class WindowsPlatformAdapter : public PlatformAdapter {
public:
    std::string getPlatformName() const override {
        return "Windows";
    }
    
    std::string getCompilerCommand() const override {
        return "cl";
    }
    
    std::string getLinkerCommand() const override {
        return "link";
    }
    
    std::vector<std::string> getCompilerFlags() const override {
        return {"/std:c++17", "/O2", "/W4"};
    }
    
    std::vector<std::string> getLinkerFlags() const override {
        return {};
    }
    
    std::string getBuildDirectory() const override {
        return "build/windows";
    }
    
    std::string getOutputExtension() const override {
        return ".exe";
    }
    
    bool supportsParallelBuild() const override {
        return true;
    }
    
    int getMaxParallelJobs() const override {
        return std::thread::hardware_concurrency();
    }
};

class MacOSPlatformAdapter : public PlatformAdapter {
public:
    std::string getPlatformName() const override {
        return "macOS";
    }
    
    std::string getCompilerCommand() const override {
        return "clang++";
    }
    
    std::string getLinkerCommand() const override {
        return "clang++";
    }
    
    std::vector<std::string> getCompilerFlags() const override {
        return {"-std=c++17", "-O2", "-Wall", "-Wextra"};
    }
    
    std::vector<std::string> getLinkerFlags() const override {
        return {};
    }
    
    std::string getBuildDirectory() const override {
        return "build/macos";
    }
    
    std::string getOutputExtension() const override {
        return "";
    }
    
    bool supportsParallelBuild() const override {
        return true;
    }
    
    int getMaxParallelJobs() const override {
        return std::thread::hardware_concurrency();
    }
};

class QNXPlatformAdapter : public PlatformAdapter {
public:
    std::string getPlatformName() const override {
        return "QNX";
    }
    
    std::string getCompilerCommand() const override {
        return "q++";
    }
    
    std::string getLinkerCommand() const override {
        return "q++";
    }
    
    std::vector<std::string> getCompilerFlags() const override {
        return {"-std=c++17", "-O2", "-Wall"};
    }
    
    std::vector<std::string> getLinkerFlags() const override {
        return {};
    }
    
    std::string getBuildDirectory() const override {
        return "build/qnx";
    }
    
    std::string getOutputExtension() const override {
        return "";
    }
    
    bool supportsParallelBuild() const override {
        return true;
    }
    
    int getMaxParallelJobs() const override {
        return std::thread::hardware_concurrency();
    }
};

class VxWorksPlatformAdapter : public PlatformAdapter {
public:
    std::string getPlatformName() const override {
        return "VxWorks";
    }
    
    std::string getCompilerCommand() const override {
        return "vx++";
    }
    
    std::string getLinkerCommand() const override {
        return "vx++";
    }
    
    std::vector<std::string> getCompilerFlags() const override {
        return {"-std=c++17", "-O2", "-Wall"};
    }
    
    std::vector<std::string> getLinkerFlags() const override {
        return {};
    }
    
    std::string getBuildDirectory() const override {
        return "build/vxworks";
    }
    
    std::string getOutputExtension() const override {
        return "";
    }
    
    bool supportsParallelBuild() const override {
        return true;
    }
    
    int getMaxParallelJobs() const override {
        return std::thread::hardware_concurrency();
    }
};

class PlatformManager {
public:
    static PlatformManager& instance() {
        static PlatformManager instance;
        return instance;
    }
    
    PlatformAdapter* getPlatform() {
        if (!platform_) {
            detectPlatform();
        }
        return platform_.get();
    }
    
private:
    PlatformManager() = default;
    
    void detectPlatform() {
        #ifdef __linux__
        platform_ = std::make_unique<LinuxPlatformAdapter>();
        #elif defined(_WIN32)
        platform_ = std::make_unique<WindowsPlatformAdapter>();
        #elif defined(__APPLE__)
        platform_ = std::make_unique<MacOSPlatformAdapter>();
        #elif defined(__QNX__) || defined(__QNXNTO__)
        platform_ = std::make_unique<QNXPlatformAdapter>();
        #elif defined(__VXWORKS__)
        platform_ = std::make_unique<VxWorksPlatformAdapter>();
        #else
        std::cerr << "Unsupported platform" << std::endl;
        exit(1);
        #endif
    }
    
    std::unique_ptr<PlatformAdapter> platform_;
};

class LanguageAdapter {
public:
    virtual std::string getLanguageName() const = 0;
    virtual bool compile(const std::string& sourceFile, const std::string& outputFile) = 0;
    virtual bool link(const std::vector<std::string>& objectFiles, const std::string& outputFile) = 0;
};

class CppLanguageAdapter : public LanguageAdapter {
public:
    std::string getLanguageName() const override {
        return "C++";
    }
    
    bool compile(const std::string& sourceFile, const std::string& outputFile) override {
        auto platform = PlatformManager::instance().getPlatform();
        std::stringstream command;
        command << platform->getCompilerCommand() << " ";
        
        for (const auto& flag : platform->getCompilerFlags()) {
            command << flag << " ";
        }
        
        command << "-c " << sourceFile << " -o " << outputFile;
        
        std::cout << "Compiling: " << sourceFile << std::endl;
        int result = system(command.str().c_str());
        return result == 0;
    }
    
    bool link(const std::vector<std::string>& objectFiles, const std::string& outputFile) override {
        auto platform = PlatformManager::instance().getPlatform();
        std::stringstream command;
        command << platform->getLinkerCommand() << " ";
        
        for (const auto& flag : platform->getLinkerFlags()) {
            command << flag << " ";
        }
        
        for (const auto& objFile : objectFiles) {
            command << objFile << " ";
        }
        
        command << "-o " << outputFile;
        
        std::cout << "Linking: " << outputFile << std::endl;
        int result = system(command.str().c_str());
        return result == 0;
    }
};

class BuildEngine {
public:
    BuildEngine(const std::string& configFile) : config_(configFile) {
        platform_ = PlatformManager::instance().getPlatform();
        languageAdapter_ = std::make_unique<CppLanguageAdapter>();
        
        // Add dependencies from config
        for (const auto& dep : config_.getDependencies()) {
            dependencyManager_.addDependency(dep);
        }
    }
    
    bool build() {
        std::cout << "Building project: " << config_.getName() << std::endl;
        std::cout << "Platform: " << platform_->getPlatformName() << std::endl;
        
        // Resolve dependencies
        dependencyManager_.resolveDependencies();
        
        // Collect source files
        std::vector<std::string> sourceFiles;
        for (const auto& sourceDir : config_.getSources()) {
            collectSourceFiles(sourceDir, sourceFiles);
        }
        
        // Create platform-specific build directory
        std::string buildDir = platform_->getBuildDirectory();
        fs::create_directories(buildDir);
        
        // Compile source files
        std::vector<std::string> objectFiles;
        for (const auto& sourceFile : sourceFiles) {
            std::string objFile = buildDir + "/" + fs::path(sourceFile).stem().string() + ".o";
            if (!languageAdapter_->compile(sourceFile, objFile)) {
                std::cerr << "Compilation failed for: " << sourceFile << std::endl;
                return false;
            }
            objectFiles.push_back(objFile);
        }
        
        // Link
        std::string outputFile = buildDir + "/" + config_.getName() + platform_->getOutputExtension();
        if (!languageAdapter_->link(objectFiles, outputFile)) {
            std::cerr << "Linking failed" << std::endl;
            return false;
        }
        
        std::cout << "Build completed successfully!" << std::endl;
        std::cout << "Output: " << outputFile << std::endl;
        return true;
    }
    
    bool clean() {
        std::cout << "Cleaning build directory..." << std::endl;
        std::string buildDir = platform_->getBuildDirectory();
        if (fs::exists(buildDir)) {
            fs::remove_all(buildDir);
        } else if (fs::exists("build")) {
            // 清理旧的构建目录结构
            fs::remove_all("build");
        }
        std::cout << "Clean completed!" << std::endl;
        return true;
    }
    
    bool test() {
        std::cout << "Running tests..." << std::endl;
        
        // 实现测试功能
        try {
            // 收集测试文件
            std::vector<std::string> testFiles;
            collectTestFiles(config_.getTestDirectory(), testFiles);
            
            if (testFiles.empty()) {
                std::cout << "No test files found." << std::endl;
                return true;
            }
            
            std::cout << "Found " << testFiles.size() << " test files." << std::endl;
            
            // 编译测试文件
            std::string testExecutable = "aurora_test";
            if (!compileTests(testFiles, testExecutable)) {
                std::cerr << "Failed to compile tests." << std::endl;
                return false;
            }
            
            // 运行测试
            std::cout << "Executing tests..." << std::endl;
            if (!runTests(testExecutable)) {
                std::cerr << "Tests failed." << std::endl;
                return false;
            }
            
            std::cout << "All tests passed!" << std::endl;
        } catch (const std::exception& e) {
            std::cerr << "Error during testing: " << e.what() << std::endl;
            return false;
        }
        
        std::cout << "Tests completed!" << std::endl;
        return true;
    }
    
private:
    void collectTestFiles(const std::string& directory, std::vector<std::string>& testFiles) {
        if (!fs::exists(directory)) {
            std::cerr << "Test directory does not exist: " << directory << std::endl;
            return;
        }
        
        for (const auto& entry : fs::recursive_directory_iterator(directory)) {
            if (entry.is_regular_file()) {
                std::string extension = entry.path().extension().string();
                if (extension == ".cpp" || extension == ".cc" || extension == ".cxx") {
                    std::string filename = entry.path().filename().string();
                    if (filename.find("test_") == 0 || filename.find("_test.") != std::string::npos) {
                        testFiles.push_back(entry.path().string());
                    }
                }
            }
        }
    }
    
    bool compileTests(const std::vector<std::string>& testFiles, const std::string& executable) {
        std::cout << "Compiling tests..." << std::endl;
        
        // 构建编译命令
        std::string command = platform_->getCompilerCommand();
        command += " -std=c++17 -I" + config_.getIncludeDirectory();
        command += " -L" + config_.getLibraryDirectory() + " -laurorart";
        
        for (const auto& file : testFiles) {
            command += " " + file;
        }
        
        command += " -o " + executable;
        
        std::cout << "Compilation command: " << command << std::endl;
        
        // 执行编译命令
        int result = system(command.c_str());
        return result == 0;
    }
    
    bool runTests(const std::string& executable) {
        std::cout << "Running test executable: " << executable << std::endl;
        
        // 执行测试命令
        std::string command = "./" + executable;
        int result = system(command.c_str());
        return result == 0;
    }
    
    void collectSourceFiles(const std::string& directory, std::vector<std::string>& sourceFiles) {
        if (!fs::exists(directory)) {
            std::cerr << "Directory does not exist: " << directory << std::endl;
            return;
        }
        
        for (const auto& entry : fs::recursive_directory_iterator(directory)) {
            if (entry.is_regular_file()) {
                std::string extension = entry.path().extension().string();
                if (extension == ".cpp" || extension == ".cc" || extension == ".cxx") {
                    sourceFiles.push_back(entry.path().string());
                }
            }
        }
    }
    
    ConfigParser config_;
    DependencyManager dependencyManager_;
    PlatformAdapter* platform_;
    std::unique_ptr<LanguageAdapter> languageAdapter_;
};

class AuroraBuild {
public:
    void run(int argc, char* argv[]) {
        if (argc < 2) {
            printHelp();
            return;
        }
        
        std::string command = argv[1];
        
        if (command == "build") {
            build();
        } else if (command == "clean") {
            clean();
        } else if (command == "test") {
            test();
        } else if (command == "install") {
            install();
        } else if (command == "list") {
            list();
        } else if (command == "deps") {
            deps();
        } else if (command == "init") {
            if (argc < 3) {
                std::cout << "Usage: aurorabuild init <project_name>" << std::endl;
                return;
            }
            init(argv[2]);
        } else {
            std::cout << "Unknown command: " << command << std::endl;
            printHelp();
        }
    }
    
private:
    void build() {
        BuildEngine engine("aurora.yml");
        engine.build();
    }
    
    void clean() {
        BuildEngine engine("aurora.yml");
        engine.clean();
    }
    
    void test() {
        BuildEngine engine("aurora.yml");
        engine.test();
    }
    
    void install() {
        std::cout << "Installing project..." << std::endl;
        
        try {
            BuildEngine engine("aurora.yml");
            
            // 获取安装目录
            std::string installDir = engine.getInstallDirectory();
            if (installDir.empty()) {
                installDir = "/usr/local";
                std::cout << "Using default install directory: " << installDir << std::endl;
            }
            
            // 创建安装目录结构
            createInstallDirectories(installDir);
            
            // 安装头文件
            std::cout << "Installing header files..." << std::endl;
            installHeaders(engine.getIncludeDirectory(), installDir + "/include/aurorart");
            
            // 安装库文件
            std::cout << "Installing library files..." << std::endl;
            installLibraries(engine.getLibraryDirectory(), installDir + "/lib");
            
            // 安装可执行文件
            std::cout << "Installing executable files..." << std::endl;
            installExecutables(engine.getBinDirectory(), installDir + "/bin");
            
            // 安装配置文件
            std::cout << "Installing configuration files..." << std::endl;
            installConfigFiles(engine.getConfigDirectory(), installDir + "/etc/aurorart");
            
            std::cout << "Installation completed successfully!" << std::endl;
        } catch (const std::exception& e) {
            std::cerr << "Error during installation: " << e.what() << std::endl;
        }
    }
    
private:
    void createInstallDirectories(const std::string& baseDir) {
        std::vector<std::string> dirs = {
            baseDir + "/include/aurorart",
            baseDir + "/lib",
            baseDir + "/bin",
            baseDir + "/etc/aurorart"
        };
        
        for (const auto& dir : dirs) {
            if (!fs::exists(dir)) {
                if (fs::create_directories(dir)) {
                    std::cout << "Created directory: " << dir << std::endl;
                } else {
                    std::cerr << "Failed to create directory: " << dir << std::endl;
                }
            }
        }
    }
    
    void installHeaders(const std::string& sourceDir, const std::string& targetDir) {
        if (!fs::exists(sourceDir)) {
            std::cerr << "Source header directory does not exist: " << sourceDir << std::endl;
            return;
        }
        
        for (const auto& entry : fs::recursive_directory_iterator(sourceDir)) {
            if (entry.is_regular_file()) {
                std::string extension = entry.path().extension().string();
                if (extension == ".h" || extension == ".hpp" || extension == ".hxx") {
                    std::string relativePath = fs::relative(entry.path(), sourceDir).string();
                    std::string targetPath = targetDir + "/" + relativePath;
                    
                    // 创建目标目录
                    fs::create_directories(fs::path(targetPath).parent_path());
                    
                    // 复制文件
                    if (fs::copy_file(entry.path(), targetPath, fs::copy_options::overwrite_existing)) {
                        std::cout << "Installed header: " << relativePath << std::endl;
                    } else {
                        std::cerr << "Failed to install header: " << relativePath << std::endl;
                    }
                }
            }
        }
    }
    
    void installLibraries(const std::string& sourceDir, const std::string& targetDir) {
        if (!fs::exists(sourceDir)) {
            std::cerr << "Source library directory does not exist: " << sourceDir << std::endl;
            return;
        }
        
        for (const auto& entry : fs::directory_iterator(sourceDir)) {
            if (entry.is_regular_file()) {
                std::string extension = entry.path().extension().string();
                if (extension == ".so" || extension == ".dll" || extension == ".lib" || extension == ".a") {
                    std::string filename = entry.path().filename().string();
                    std::string targetPath = targetDir + "/" + filename;
                    
                    if (fs::copy_file(entry.path(), targetPath, fs::copy_options::overwrite_existing)) {
                        std::cout << "Installed library: " << filename << std::endl;
                    } else {
                        std::cerr << "Failed to install library: " << filename << std::endl;
                    }
                }
            }
        }
    }
    
    void installExecutables(const std::string& sourceDir, const std::string& targetDir) {
        if (!fs::exists(sourceDir)) {
            std::cerr << "Source executable directory does not exist: " << sourceDir << std::endl;
            return;
        }
        
        for (const auto& entry : fs::directory_iterator(sourceDir)) {
            if (entry.is_regular_file()) {
                // 检查是否为可执行文件
                #if defined(_WIN32)
                std::string extension = entry.path().extension().string();
                if (extension == ".exe") {
                    std::string filename = entry.path().filename().string();
                    std::string targetPath = targetDir + "/" + filename;
                    
                    if (fs::copy_file(entry.path(), targetPath, fs::copy_options::overwrite_existing)) {
                        std::cout << "Installed executable: " << filename << std::endl;
                    } else {
                        std::cerr << "Failed to install executable: " << filename << std::endl;
                    }
                }
                #else
                // 在Unix系统上检查执行权限
                if (fs::status(entry.path()).permissions() & fs::perms::owner_exec) {
                    std::string filename = entry.path().filename().string();
                    std::string targetPath = targetDir + "/" + filename;
                    
                    if (fs::copy_file(entry.path(), targetPath, fs::copy_options::overwrite_existing)) {
                        // 设置执行权限
                        fs::permissions(targetPath, fs::perms::owner_exec | fs::perms::group_exec | fs::perms::others_exec | fs::perms::owner_read | fs::perms::group_read | fs::perms::others_read);
                        std::cout << "Installed executable: " << filename << std::endl;
                    } else {
                        std::cerr << "Failed to install executable: " << filename << std::endl;
                    }
                }
                #endif
            }
        }
    }
    
    void installConfigFiles(const std::string& sourceDir, const std::string& targetDir) {
        if (!fs::exists(sourceDir)) {
            std::cerr << "Source config directory does not exist: " << sourceDir << std::endl;
            return;
        }
        
        for (const auto& entry : fs::directory_iterator(sourceDir)) {
            if (entry.is_regular_file()) {
                std::string filename = entry.path().filename().string();
                std::string targetPath = targetDir + "/" + filename;
                
                if (fs::copy_file(entry.path(), targetPath, fs::copy_options::overwrite_existing)) {
                    std::cout << "Installed config: " << filename << std::endl;
                } else {
                    std::cerr << "Failed to install config: " << filename << std::endl;
                }
            }
        }
    }
    
    void list() {
        std::cout << "Listing project and dependencies..." << std::endl;
        ConfigParser config("aurora.yml");
        std::cout << "Project: " << config.getName() << " (" << config.getVersion() << ")" << std::endl;
        std::cout << "Type: " << config.getType() << std::endl;
        std::cout << "Dependencies:" << std::endl;
        for (const auto& dep : config.getDependencies()) {
            std::cout << "  - " << dep << std::endl;
        }
    }
    
    void deps() {
        std::cout << "Analyzing dependencies..." << std::endl;
        ConfigParser config("aurora.yml");
        std::cout << "Dependencies:" << std::endl;
        for (const auto& dep : config.getDependencies()) {
            std::cout << "  - " << dep << std::endl;
        }
    }
    
    void init(const std::string& projectName) {
        std::cout << "Initializing new project: " << projectName << std::endl;
        
        // Create project directory
        fs::create_directories(projectName);
        fs::current_path(projectName);
        
        // Create aurora.yml config file
        std::ofstream configFile("aurora.yml");
        if (configFile.is_open()) {
            configFile << "name: " << projectName << std::endl;
            configFile << "type: cpp" << std::endl;
            configFile << "version: 1.0.0" << std::endl;
            configFile << "dependencies:" << std::endl;
            configFile << "sources:" << std::endl;
            configFile << "  - src" << std::endl;
            configFile.close();
        }
        
        // Create src directory
        fs::create_directories("src");
        
        // Create main.cpp
        std::ofstream mainFile("src/main.cpp");
        if (mainFile.is_open()) {
            mainFile << "#include <iostream>\n";
            mainFile << "\n";
            mainFile << "int main() {\n";
            mainFile << "    std::cout << \"Hello, " << projectName << "!\" << std::endl;\n";
            mainFile << "    return 0;\n";
            mainFile << "}\n";
            mainFile.close();
        }
        
        std::cout << "Project initialized successfully!" << std::endl;
    }
    
    void printHelp() {
        std::cout << "AuroraBuild - AuroraRT Build System" << std::endl;
        std::cout << "Usage: aurorabuild <command> [options]" << std::endl;
        std::cout << std::endl;
        std::cout << "Commands:" << std::endl;
        std::cout << "  build     - Build the project" << std::endl;
        std::cout << "  clean     - Clean build artifacts" << std::endl;
        std::cout << "  test      - Run tests" << std::endl;
        std::cout << "  install   - Install the project" << std::endl;
        std::cout << "  list      - List project and dependencies" << std::endl;
        std::cout << "  deps      - Analyze dependencies" << std::endl;
        std::cout << "  init      - Initialize a new project" << std::endl;
    }
};

} // namespace build
} // namespace aurorart

int main(int argc, char* argv[]) {
    aurorart::build::AuroraBuild builder;
    builder.run(argc, argv);
    return 0;
}
