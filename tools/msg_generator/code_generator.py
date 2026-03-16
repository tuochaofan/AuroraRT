#!/usr/bin/env python3

"""
AuroraRT 消息代码生成器

根据消息定义生成 C++ 代码，支持多种序列化格式
"""

import os
import subprocess
import argparse

class CodeGenerator:
    """代码生成器"""
    
    def __init__(self):
        self.cpp_types = {
            'bool': 'bool',
            'int8': 'int8_t',
            'int16': 'int16_t',
            'int32': 'int32_t',
            'int64': 'int64_t',
            'uint8': 'uint8_t',
            'uint16': 'uint16_t',
            'uint32': 'uint32_t',
            'uint64': 'uint64_t',
            'float32': 'float',
            'float64': 'double',
            'string': 'std::string'
        }
    
    def _get_cpp_type(self, field_type):
        """获取 C++ 类型"""
        return self.cpp_types.get(field_type, field_type)
    
    def generate_cpp_header(self, msg_def, include_dir):
        """生成 C++ 头文件"""
        header_path = os.path.join(include_dir, f"{msg_def.name}.hpp")
        
        with open(header_path, 'w', encoding='utf-8') as f:
            f.write("#pragma once\n")
            f.write("#include <vector>\n")
            f.write("#include <string>\n")
            f.write("#include <nlohmann/json.hpp>\n")
            f.write("#include \"aurorart/serialization/serializer.h\"\n")
            
            # 添加Protobuf支持
            if 'protobuf' in msg_def.serialization_formats:
                f.write("#include \"{}.pb.h\"\n".format(msg_def.name))
            
            # 添加FlatBuffers支持
            if 'flatbuffers' in msg_def.serialization_formats:
                f.write("#include \"{}_generated.h\"\n".format(msg_def.name))
            
            f.write("\nnamespace aurorart {\n")
            f.write("namespace msg {\n\n")
            
            # 生成类定义
            f.write("class {} {{\n".format(msg_def.name))
            f.write("public:\n")
            f.write("    {}();\n".format(msg_def.name))
            f.write("    ~{}() = default;\n\n".format(msg_def.name))
            
            # 生成字段
            for field in msg_def.fields:
                cpp_type = self._get_cpp_type(field.field_type)
                if field.is_array:
                    if field.array_size:
                        f.write("    {} {}[{}];\n".format(cpp_type, field.field_name, field.array_size))
                    else:
                        f.write("    std::vector<{}> {};\n".format(cpp_type, field.field_name))
                else:
                    f.write("    {} {};\n".format(cpp_type, field.field_name))
            
            f.write("\n")
            
            # 生成序列化方法
            f.write("    // 序列化方法\n")
            f.write("    std::vector<uint8_t> serialize(serialization::SerializerType type) const;\n")
            f.write("    bool deserialize(const std::vector<uint8_t>& data, serialization::SerializerType type);\n\n")
            
            # 生成Protobuf序列化方法
            if 'protobuf' in msg_def.serialization_formats:
                f.write("    // Protobuf序列化方法\n")
                f.write("    bool serializeToProtobuf(std::vector<uint8_t>& data) const;\n")
                f.write("    bool deserializeFromProtobuf(const std::vector<uint8_t>& data);\n\n")
            
            # 生成FlatBuffers序列化方法
            if 'flatbuffers' in msg_def.serialization_formats:
                f.write("    // FlatBuffers序列化方法\n")
                f.write("    bool serializeToFlatBuffers(std::vector<uint8_t>& data) const;\n")
                f.write("    bool deserializeFromFlatBuffers(const std::vector<uint8_t>& data);\n\n")
            
            f.write("};")
            f.write("\n\n")
            
            # 生成辅助函数
            f.write("// 辅助函数\n")
            f.write("bool saveMessage(const {}& msg, const std::string& filename, serialization::SerializerType type);\n".format(msg_def.name))
            f.write("bool loadMessage({}& msg, const std::string& filename, serialization::SerializerType type);\n".format(msg_def.name))
            f.write("bool saveMessageAsJSON(const {}& msg, const std::string& filename);\n".format(msg_def.name))
            f.write("bool loadMessageFromJSON({}& msg, const std::string& filename);\n".format(msg_def.name))
            
            f.write("} // namespace msg\n")
            f.write("} // namespace aurorart\n")
    
    def generate_cpp_source(self, msg_def, src_dir):
        """生成 C++ 源文件"""
        source_path = os.path.join(src_dir, f"{msg_def.name}.cpp")
        
        with open(source_path, 'w', encoding='utf-8') as f:
            f.write(f"#include \"aurorart/msg/{msg_def.name}.hpp\"\n")
            f.write("#include <fstream>\n")
            f.write("#include <nlohmann/json.hpp>\n")
            
            # 添加Protobuf支持
            if 'protobuf' in msg_def.serialization_formats:
                f.write(f"#include \"{msg_def.name}.pb.h\"\n")
            
            # 添加FlatBuffers支持
            if 'flatbuffers' in msg_def.serialization_formats:
                f.write(f"#include \"{msg_def.name}_generated.h\"\n")
            
            f.write("\nnamespace aurorart {\n")
            f.write("namespace msg {\n\n")
            
            # 生成构造函数
            f.write("{}::{}() {{".format(msg_def.name, msg_def.name))
            f.write("\n")
            f.write("    // 初始化字段\n")
            f.write("}\n\n")
            
            # 生成序列化方法
            f.write("std::vector<uint8_t> {}::serialize(serialization::SerializerType type) const {{".format(msg_def.name))
            f.write("\n")
            f.write("    auto serializer = serialization::SerializerFactory::createSerializer(type);\n")
            f.write("    std::vector<uint8_t> data;\n")
            
            # 序列化每个字段
            for field in msg_def.fields:
                if field.is_array:
                    if field.array_size:
                        f.write("    // 序列化 {}\n".format(field.field_name))
                        f.write("    for (int i = 0; i < {}; ++i) {{".format(field.array_size))
                        f.write("\n")
                        f.write("        auto field_data = serializer->serialize(&{}[i], sizeof({}[i]));\n".format(field.field_name, field.field_name))
                        f.write("        data.insert(data.end(), field_data.begin(), field_data.end());\n")
                        f.write("    }\n")
                    else:
                        f.write("    // 序列化 {}\n".format(field.field_name))
                        f.write("    size_t size = {}.size();\n".format(field.field_name))
                        f.write("    auto size_data = serializer->serialize(&size, sizeof(size));\n")
                        f.write("    data.insert(data.end(), size_data.begin(), size_data.end());\n")
                        f.write("    for (const auto& item : {}) {{".format(field.field_name))
                        f.write("\n")
                        f.write("        auto field_data = serializer->serialize(&item, sizeof(item));\n")
                        f.write("        data.insert(data.end(), field_data.begin(), field_data.end());\n")
                        f.write("    }\n")
                else:
                    f.write("    // 序列化 {}\n".format(field.field_name))
                    f.write("    auto field_data = serializer->serialize(&{}, sizeof({}));\n".format(field.field_name, field.field_name))
                    f.write("    data.insert(data.end(), field_data.begin(), field_data.end());\n")
            
            f.write("    return data;\n")
            f.write("}\n\n")
            
            # 生成反序列化方法
            f.write("bool {}::deserialize(const std::vector<uint8_t>& data, serialization::SerializerType type) {{".format(msg_def.name))
            f.write("\n")
            f.write("    auto serializer = serialization::SerializerFactory::createSerializer(type);\n")
            f.write("    size_t offset = 0;\n")
            
            # 反序列化每个字段
            for field in msg_def.fields:
                if field.is_array:
                    if field.array_size:
                        f.write("    // 反序列化 {}\n".format(field.field_name))
                        f.write("    for (int i = 0; i < {}; ++i) {{".format(field.array_size))
                        f.write("\n")
                        f.write("        if (!serializer->deserialize(data, offset, &{}[i], sizeof({}[i]))) {{".format(field.field_name, field.field_name))
                        f.write("\n")
                        f.write("            return false;\n")
                        f.write("        }\n")
                        f.write("    }\n")
                    else:
                        f.write("    // 反序列化 {}\n".format(field.field_name))
                        f.write("    size_t size;\n")
                        f.write("    if (!serializer->deserialize(data, offset, &size, sizeof(size))) {{")
                        f.write("\n")
                        f.write("        return false;\n")
                        f.write("    }\n")
                        f.write("    {}.resize(size);\n".format(field.field_name))
                        f.write("    for (size_t i = 0; i < size; ++i) {{")
                        f.write("\n")
                        f.write("        if (!serializer->deserialize(data, offset, &{}[i], sizeof({}[i]))) {{".format(field.field_name, field.field_name))
                        f.write("\n")
                        f.write("            return false;\n")
                        f.write("        }\n")
                        f.write("    }\n")
                else:
                    f.write("    // 反序列化 {}\n".format(field.field_name))
                    f.write("    if (!serializer->deserialize(data, offset, &{}, sizeof({}))) {{".format(field.field_name, field.field_name))
                    f.write("\n")
                    f.write("        return false;\n")
                    f.write("    }\n")
            
            f.write("    return true;\n")
            f.write("}\n\n")
            
            # 生成Protobuf序列化方法
            if 'protobuf' in msg_def.serialization_formats:
                f.write("bool {}::serializeToProtobuf(std::vector<uint8_t>& data) const {{".format(msg_def.name))
                f.write("\n")
                f.write("    {}_proto pb_msg;\n".format(msg_def.name))
                
                # 序列化每个字段
                for field in msg_def.fields:
                    if field.is_array:
                        if field.array_size:
                            for i in range(field.array_size):
                                f.write("    pb_msg.add_{}({}[{}]);\n".format(field.field_name, field.field_name, i))
                        else:
                            f.write("    for (const auto& item : {}) {{".format(field.field_name))
                            f.write("\n")
                            f.write("        pb_msg.add_{}(item);\n".format(field.field_name))
                            f.write("    }\n")
                    else:
                        f.write("    pb_msg.set_{}({});\n".format(field.field_name, field.field_name))
                
                f.write("    data.resize(pb_msg.ByteSizeLong());\n")
                f.write("    return pb_msg.SerializeToArray(data.data(), data.size());\n")
                f.write("}\n\n")
                
                f.write("bool {}::deserializeFromProtobuf(const std::vector<uint8_t>& data) {{".format(msg_def.name))
                f.write("\n")
                f.write("    {}_proto pb_msg;\n".format(msg_def.name))
                f.write("    if (!pb_msg.ParseFromArray(data.data(), data.size())) {{")
                f.write("\n")
                f.write("        return false;\n")
                f.write("    }\n")
                
                # 反序列化每个字段
                for field in msg_def.fields:
                    if field.is_array:
                        if field.array_size:
                            f.write("    for (int i = 0; i < std::min({}, pb_msg.{}_size()); ++i) {{".format(field.array_size, field.field_name))
                            f.write("\n")
                            f.write("        {}[i] = pb_msg.{}_size(i);\n".format(field.field_name, field.field_name))
                            f.write("    }\n")
                        else:
                            f.write("    {}.clear();\n".format(field.field_name))
                            f.write("    for (int i = 0; i < pb_msg.{}_size(); ++i) {{".format(field.field_name))
                            f.write("\n")
                            f.write("        {}.push_back(pb_msg.{}_size(i));\n".format(field.field_name, field.field_name))
                            f.write("    }\n")
                    else:
                        f.write("    if (pb_msg.has_{}()) {{".format(field.field_name))
                        f.write("\n")
                        f.write("        {} = pb_msg.{}();\n".format(field.field_name, field.field_name))
                        f.write("    }\n")
                
                f.write("    return true;\n")
                f.write("}\n\n")
            
            # 生成FlatBuffers序列化方法
            if 'flatbuffers' in msg_def.serialization_formats:
                f.write("bool {}::serializeToFlatBuffers(std::vector<uint8_t>& data) const {{".format(msg_def.name))
                f.write("\n")
                f.write("    flatbuffers::FlatBufferBuilder builder;\n")
                
                # 序列化每个字段
                field_offsets = []
                for field in msg_def.fields:
                    if field.is_array:
                        if field.array_size:
                            f.write("    std::vector<flatbuffers::Offset<{}>> {}_offsets;\n".format(self._get_cpp_type(field.field_type), field.field_name))
                            f.write("    for (int i = 0; i < {}; ++i) {{".format(field.array_size))
                            f.write("\n")
                            f.write("        {}_offsets.push_back(builder.CreateString({}[i]));\n".format(field.field_name, field.field_name))
                            f.write("    }\n")
                            f.write("    auto {}_vector = builder.CreateVector({}_offsets);\n".format(field.field_name, field.field_name))
                        else:
                            f.write("    std::vector<flatbuffers::Offset<{}>> {}_offsets;\n".format(self._get_cpp_type(field.field_type), field.field_name))
                            f.write("    for (const auto& item : {}) {{".format(field.field_name))
                            f.write("\n")
                            f.write("        {}_offsets.push_back(builder.CreateString(item));\n".format(field.field_name))
                            f.write("    }\n")
                            f.write("    auto {}_vector = builder.CreateVector({}_offsets);\n".format(field.field_name, field.field_name))
                    else:
                        if field.field_type == 'string':
                            f.write("    auto {}_offset = builder.CreateString({});\n".format(field.field_name, field.field_name))
                        else:
                            f.write("    auto {}_offset = {};\n".format(field.field_name, field.field_name))
                    field_offsets.append("{}_offset".format(field.field_name))
                
                # 创建消息
                f.write("    auto msg_offset = Create{}(builder, {});\n".format(msg_def.name, ', '.join(field_offsets)))
                f.write("    builder.Finish(msg_offset);\n")
                f.write("    data = std::vector<uint8_t>(builder.GetBufferPointer(), builder.GetBufferPointer() + builder.GetSize());\n")
                f.write("    return true;\n")
                f.write("}\n\n")
                
                f.write("bool {}::deserializeFromFlatBuffers(const std::vector<uint8_t>& data) {{".format(msg_def.name))
                f.write("\n")
                f.write("    auto msg = Get{}(data.data());\n".format(msg_def.name))
                
                # 反序列化每个字段
                for field in msg_def.fields:
                    if field.is_array:
                        if field.array_size:
                            f.write("    for (int i = 0; i < std::min({}, msg->{}()->size()); ++i) {{".format(field.array_size, field.field_name))
                            f.write("\n")
                            f.write("        {}[i] = msg->{}()->Get(i);\n".format(field.field_name, field.field_name))
                            f.write("    }\n")
                        else:
                            f.write("    {}.clear();\n".format(field.field_name))
                            f.write("    for (int i = 0; i < msg->{}()->size(); ++i) {{".format(field.field_name))
                            f.write("\n")
                            f.write("        {}.push_back(msg->{}()->Get(i));\n".format(field.field_name, field.field_name))
                            f.write("    }\n")
                    else:
                        if field.field_type == 'string':
                            f.write("    if (msg->{}) {{".format(field.field_name))
                            f.write("\n")
                            f.write("        {} = msg->{}()->c_str();\n".format(field.field_name, field.field_name))
                            f.write("    }\n")
                        else:
                            f.write("    {} = msg->{}();\n".format(field.field_name, field.field_name))
                
                f.write("    return true;\n")
                f.write("}\n\n")
            
            # 生成辅助函数
            f.write("bool saveMessage(const {}& msg, const std::string& filename, serialization::SerializerType type) {{".format(msg_def.name))
            f.write("\n")
            f.write("    auto data = msg.serialize(type);\n")
            f.write("    std::ofstream file(filename, std::ios::binary);\n")
            f.write("    if (!file) return false;\n")
            f.write("    file.write(reinterpret_cast<const char*>(data.data()), data.size());\n")
            f.write("    return file.good();\n")
            f.write("}\n\n")
            
            f.write("bool loadMessage({}& msg, const std::string& filename, serialization::SerializerType type) {{".format(msg_def.name))
            f.write("\n")
            f.write("    std::ifstream file(filename, std::ios::binary | std::ios::ate);\n")
            f.write("    if (!file) return false;\n")
            f.write("    std::streamsize size = file.tellg();\n")
            f.write("    file.seekg(0, std::ios::beg);\n")
            f.write("    std::vector<uint8_t> data(size);\n")
            f.write("    if (!file.read(reinterpret_cast<char*>(data.data()), size)) return false;\n")
            f.write("    return msg.deserialize(data, type);\n")
            f.write("}\n\n")
            
            f.write("bool saveMessageAsJSON(const {}& msg, const std::string& filename) {{".format(msg_def.name))
            f.write("\n")
            f.write("    nlohmann::json j;\n")
            for field in msg_def.fields:
                if field.is_array:
                    f.write("    j['{}'] = msg.{};\n".format(field.field_name, field.field_name))
                else:
                    f.write("    j['{}'] = msg.{};\n".format(field.field_name, field.field_name))
            f.write("    std::ofstream file(filename);\n")
            f.write("    if (!file) return false;\n")
            f.write("    file << j.dump(4);\n")
            f.write("    return file.good();\n")
            f.write("}\n\n")
            
            f.write("bool loadMessageFromJSON({}& msg, const std::string& filename) {{".format(msg_def.name))
            f.write("\n")
            f.write("    std::ifstream file(filename);\n")
            f.write("    if (!file) return false;\n")
            f.write("    nlohmann::json j;\n")
            f.write("    try {\n")
            f.write("        file >> j;\n")
            for field in msg_def.fields:
                if field.is_array:
                    f.write("        msg.{} = j['{}'].get<std::vector<{}>>();\n".format(field.field_name, field.field_name, self._get_cpp_type(field.field_type)))
                else:
                    f.write("        msg.{} = j['{}'].get<{}>();\n".format(field.field_name, field.field_name, self._get_cpp_type(field.field_type)))
            f.write("    } catch (...) {\n")
            f.write("        return false;\n")
            f.write("    }\n")
            f.write("    return true;\n")
            f.write("}\n\n")
            
            f.write("} // namespace msg\n")
            f.write("} // namespace aurorart\n")
    
    def generate_protobuf_schema(self, msg_def, output_dir):
        """生成 Protobuf  schema 文件"""
        schema_path = os.path.join(output_dir, f"{msg_def.name}.proto")
        
        with open(schema_path, 'w', encoding='utf-8') as f:
            f.write("syntax = \"proto3\";\n\n")
            f.write("package aurorart.msg;\n\n")
            f.write("message {}_proto {{\n".format(msg_def.name))
            
            # 生成字段
            for i, field in enumerate(msg_def.fields, 1):
                proto_type = {
                    'bool': 'bool',
                    'int8': 'int32',
                    'int16': 'int32',
                    'int32': 'int32',
                    'int64': 'int64',
                    'uint8': 'uint32',
                    'uint16': 'uint32',
                    'uint32': 'uint32',
                    'uint64': 'uint64',
                    'float32': 'float',
                    'float64': 'double',
                    'string': 'string'
                }.get(field.field_type, field.field_type)
                
                if field.is_array:
                    f.write("    repeated {} {} = {};\n".format(proto_type, field.field_name, i))
                else:
                    f.write("    {} {} = {};\n".format(proto_type, field.field_name, i))
            
            f.write("}\n")
    
    def generate_flatbuffers_schema(self, msg_def, output_dir):
        """生成 FlatBuffers schema 文件"""
        schema_path = os.path.join(output_dir, f"{msg_def.name}.fbs")
        
        with open(schema_path, 'w', encoding='utf-8') as f:
            f.write("namespace aurorart.msg;\n\n")
            f.write("table {} {{\n".format(msg_def.name))
            
            # 生成字段
            for i, field in enumerate(msg_def.fields, 1):
                flatbuffers_type = {
                    'bool': 'bool',
                    'int8': 'int8',
                    'int16': 'int16',
                    'int32': 'int32',
                    'int64': 'int64',
                    'uint8': 'uint8',
                    'uint16': 'uint16',
                    'uint32': 'uint32',
                    'uint64': 'uint64',
                    'float32': 'float',
                    'float64': 'double',
                    'string': 'string'
                }.get(field.field_type, field.field_type)
                
                if field.is_array:
                    f.write("    {}: [{}];\n".format(field.field_name, flatbuffers_type))
                else:
                    f.write("    {}: {};\n".format(field.field_name, flatbuffers_type))
            
            f.write("}\n")
            f.write("root_type {};\n".format(msg_def.name))
    
    def generate_code(self, msg_def, output_dir):
        """生成所有代码"""
        # 创建输出目录
        include_dir = os.path.join(output_dir, "include", "aurorart", "msg")
        src_dir = os.path.join(output_dir, "src", "msg")
        schema_dir = os.path.join(output_dir, "schemas")
        
        os.makedirs(include_dir, exist_ok=True)
        os.makedirs(src_dir, exist_ok=True)
        os.makedirs(schema_dir, exist_ok=True)
        
        # 生成 C++ 代码
        self.generate_cpp_header(msg_def, include_dir)
        self.generate_cpp_source(msg_def, src_dir)
        
        # 生成 Protobuf schema
        if 'protobuf' in msg_def.serialization_formats:
            self.generate_protobuf_schema(msg_def, schema_dir)
            # 编译 Protobuf schema
            proto_file = os.path.join(schema_dir, f"{msg_def.name}.proto")
            try:
                subprocess.run([
                    "protoc",
                    "--cpp_out=.",
                    "--proto_path=",
                    proto_file
                ], cwd=output_dir, check=True)
            except (FileNotFoundError, subprocess.CalledProcessError):
                # 如果 protoc 命令不存在或执行失败，跳过编译步骤
                print(f"Warning: protoc command not found, skipping Protobuf compilation for {msg_def.name}")
        
        # 生成 FlatBuffers schema
        if 'flatbuffers' in msg_def.serialization_formats:
            self.generate_flatbuffers_schema(msg_def, schema_dir)
            # 编译 FlatBuffers schema
            fbs_file = os.path.join(schema_dir, f"{msg_def.name}.fbs")
            try:
                subprocess.run([
                    "flatc",
                    "--cpp",
                    fbs_file
                ], cwd=output_dir, check=True)
            except (FileNotFoundError, subprocess.CalledProcessError):
                # 如果 flatc 命令不存在或执行失败，跳过编译步骤
                print(f"Warning: flatc command not found, skipping FlatBuffers compilation for {msg_def.name}")

def main():
    """命令行入口函数"""
    parser = argparse.ArgumentParser(description='AuroraRT message generator')
    parser.add_argument('--cpp', action='store_true', help='Generate C++ code')
    parser.add_argument('--input', type=str, required=True, help='Input directory containing message files')
    parser.add_argument('--output', type=str, required=True, help='Output directory for generated code')
    parser.add_argument('--serialization', type=str, default='cdr', help='Serialization format (cdr, protobuf, flatbuffers)')
    
    args = parser.parse_args()
    
    from msg_parser import parse_directory
    
    # 解析消息文件
    defs = parse_directory(args.input)
    
    # 生成代码
    generator = CodeGenerator()
    
    # 处理消息
    for msg_name, msg_def in defs['msg'].items():
        # 设置序列化格式
        msg_def.serialization_formats = args.serialization.split(',')
        generator.generate_code(msg_def, args.output)
    
    # 处理服务
    for srv_name, srv_def in defs['srv'].items():
        # 设置序列化格式
        srv_def.request.serialization_formats = args.serialization.split(',')
        srv_def.response.serialization_formats = args.serialization.split(',')
        generator.generate_code(srv_def.request, args.output)
        generator.generate_code(srv_def.response, args.output)
    
    # 处理动作
    for action_name, action_def in defs['action'].items():
        # 设置序列化格式
        action_def.goal.serialization_formats = args.serialization.split(',')
        action_def.result.serialization_formats = args.serialization.split(',')
        action_def.feedback.serialization_formats = args.serialization.split(',')
        generator.generate_code(action_def.goal, args.output)
        generator.generate_code(action_def.result, args.output)
        generator.generate_code(action_def.feedback, args.output)
    
    print(f"Generated code for {len(defs['msg'])} messages, {len(defs['srv'])} services, {len(defs['action'])} actions")

if __name__ == '__main__':
    main()
