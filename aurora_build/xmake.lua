-- xmake.lua
set_project("aurorart_demo")
set_version("1.0.0")
set_languages("c++17")

-- 示例目标
target("demo_node")
    set_kind("binary")
    add_files("src/*.cpp")
    add_includedirs("include")
