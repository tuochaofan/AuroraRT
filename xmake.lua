-- AuroraRT 项目的 xmake 配置文件

set_project("AuroraRT")
set_version("1.0.0")

-- 设置 C++ 标准
set_languages("c++17")

-- 构建选项
option("debug")
    set_default(false)
    set_showmenu(true)
    set_description("Enable debug mode")

option("optimize")
    set_default(true)
    set_showmenu(true)
    set_description("Enable optimization")

option("profile")
    set_default(false)
    set_showmenu(true)
    set_description("Enable profiling")

option("coverage")
    set_default(false)
    set_showmenu(true)
    set_description("Enable code coverage")

option("tsn")
    set_default(true)
    set_showmenu(true)
    set_description("Enable TSN support")

option("shared")
    set_default(false)
    set_showmenu(true)
    set_description("Build shared library")

-- 依赖库
add_requires("boost 1.78.0")
add_requires("fmt 8.0.0")
add_requires("spdlog 1.9.0")
add_requires("nlohmann_json 3.10.0")
add_requires("protobuf 3.19.0")
add_requires("flatbuffers 2.0.0")
add_requires("asio 1.22.0")
add_requires("openssl 1.1.1")
add_requires("gtest 1.11.0")
add_requires("benchmark 1.6.0")

-- 平台配置
local plat = get_plat()
local arch = get_arch()

-- 编译选项
if plat == "linux" or plat == "macosx" or plat == "qnx" or plat == "vxworks" or plat == "android" then
    add_cxflags("-Wall", "-Wextra", "-Wpedantic")
    if has_config("debug") then
        add_cxflags("-g", "-O0")
    elseif has_config("optimize") then
        add_cxflags("-O3")
        -- 添加硬件特性支持
        if arch == "x86_64" then
            add_cxflags("-march=native", "-msse2", "-mavx")
        elseif arch == "arm64" then
            add_cxflags("-march=armv8-a", "-mfpu=neon")
        end
    end
    if has_config("profile") then
        add_cxflags("-pg")
        add_ldflags("-pg")
    end
    if has_config("coverage") then
        add_cxflags("--coverage")
        add_ldflags("--coverage")
    end
    add_ldflags("-pthread")
    if plat == "linux" or plat == "macosx" then
        add_ldflags("-lrt")
    elseif plat == "qnx" then
        add_ldflags("-lsocket")
    elseif plat == "android" then
        add_ldflags("-llog")
    end
    -- 添加TSN支持
    if has_config("tsn") then
        add_defines("AURORART_HAS_TSN_SUPPORT")
    end
elif plat == "windows" then
    add_cxflags("/W4")
    if has_config("debug") then
        add_cxflags("/DEBUG", "/Od")
    elseif has_config("optimize") then
        add_cxflags("/O2")
        -- 添加硬件特性支持
        if arch == "x86_64" then
            add_cxflags("/arch:SSE2", "/arch:AVX")
        end
    end
    add_ldflags("ws2_32.lib", "mswsock.lib")
    -- 添加TSN支持
    if has_config("tsn") then
        add_defines("AURORART_HAS_TSN_SUPPORT")
    end
end

-- 平台特定定义
if plat == "linux" then
    add_defines("__LINUX__")
elif plat == "windows" then
    add_defines("_WIN32", "_WINDOWS")
elif plat == "macosx" then
    add_defines("__APPLE__", "__MACH__")
elif plat == "qnx" then
    add_defines("__QNX__", "__QNXNTO__")
elif plat == "vxworks" then
    add_defines("__VXWORKS__")
elif plat == "android" then
    add_defines("__ANDROID__")
end

-- 调试模式定义
if has_config("debug") then
    add_defines("DEBUG")
end

-- 源文件列表
local sources = {
    "src/platform/platform_abstraction.cpp",
    "src/platform/platform_features.cpp",
    "src/memory/memory_manager.cpp",
    "src/transport/transport.cpp",
    "src/communication/communication_pattern.cpp",
    "src/serialization/serializer.cpp",
    "src/service_discovery/service_discovery.cpp",
    "src/qos/qos_policy.cpp",
    "src/scheduler/scheduler.cpp",
    "src/pipeline/pipeline.cpp",
    "src/self_test/self_test.cpp",
    "src/node/node.cpp",
    "src/security/security.cpp",
    "src/monitoring/monitoring.cpp",
    "src/diagnostics/diagnostics.cpp",
    "src/domain/domain_manager.cpp",
    "src/plugin/plugin.cpp",
    "src/utils/config.cpp",
    "src/utils/logger.cpp",
    "src/utils/performance.cpp",
    "src/main.cpp"
}

-- 添加通信模式相关文件
table.insert(sources, "src/communication/event_pattern.cpp")
table.insert(sources, "src/communication/pub_sub_pattern.cpp")
table.insert(sources, "src/communication/push_pull_pattern.cpp")
table.insert(sources, "src/communication/req_resp_pattern.cpp")

-- 添加平台相关文件
if plat == "linux" or plat == "macosx" or plat == "qnx" then
    table.insert(sources, "src/platform/timer_asm.S")
    table.insert(sources, "src/platform/hardware_abstraction.cpp")
    table.insert(sources, "src/platform/high_res_timer.cpp")
elif plat == "windows" then
    table.insert(sources, "src/platform/hardware_abstraction.cpp")
    table.insert(sources, "src/platform/high_res_timer.cpp")
end

-- 创建库
target("aurorart")
    if has_config("shared") then
        set_kind("shared")
    else
        set_kind("static")
    end
    add_files(sources)
    add_includedirs("include")
    add_packages(
        "boost",
        "fmt",
        "spdlog",
        "nlohmann_json",
        "protobuf",
        "flatbuffers",
        "asio",
        "openssl"
    )
    if plat == "windows" then
        add_defines("AURORART_EXPORTS")
    end

-- 测试
target("aurorart_test")
    set_kind("binary")
    add_files("test/**.cpp")
    add_includedirs("include")
    add_packages(
        "boost",
        "fmt",
        "spdlog",
        "nlohmann_json",
        "protobuf",
        "flatbuffers",
        "asio",
        "openssl",
        "gtest"
    )
    add_deps("aurorart")
    add_tests("aurorart_test")

-- 基准测试
target("aurorart_benchmark")
    set_kind("binary")
    add_files("benchmark/**.cpp")
    add_includedirs("include")
    add_packages(
        "boost",
        "fmt",
        "spdlog",
        "nlohmann_json",
        "protobuf",
        "flatbuffers",
        "asio",
        "openssl",
        "benchmark"
    )
    add_deps("aurorart")

-- 工具
target("aurora_cli")
    set_kind("binary")
    add_files("tools/aurora_cli/**.cpp")
    add_includedirs("include")
    add_packages(
        "boost",
        "fmt",
        "spdlog",
        "nlohmann_json"
    )
    add_deps("aurorart")

target("aurora_build")
    set_kind("binary")
    add_files("tools/aurora_build/**.cpp")
    add_includedirs("include")
    add_packages(
        "boost",
        "fmt",
        "spdlog",
        "nlohmann_json"
    )
    add_deps("aurorart")

-- 安装配置
if has_config("shared") then
    if plat == "windows" then
        install_files("build/bin/aurorart.dll", {dstdir = "bin"})
        install_files("build/bin/aurorart.lib", {dstdir = "lib"})
    else
        install_files("build/lib/libaurorart.so", {dstdir = "lib"})
    end
else
    if plat == "windows" then
        install_files("build/lib/aurorart.lib", {dstdir = "lib"})
    else
        install_files("build/lib/libaurorart.a", {dstdir = "lib"})
    end
end

install_files("include/aurorart/**", {dstdir = "include/aurorart"})
install_files("config.json.example", {dstdir = "etc/aurorart", rename = "config.json"})

-- 安装工具
install_files("build/bin/aurora_cli", {dstdir = "bin"})
install_files("build/bin/aurora_build", {dstdir = "bin"})

-- 平台特定安装
if plat == "windows" then
    install_files("build/bin/aurora_cli.exe", {dstdir = "bin"})
    install_files("build/bin/aurora_build.exe", {dstdir = "bin"})
end

-- 构建选项配置
set_configvar("AURORART_VERSION", get_version())
set_configvar("AURORART_PLATFORM", plat)
set_configvar("AURORART_ARCH", arch)
