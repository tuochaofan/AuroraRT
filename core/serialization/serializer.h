#pragma once

#include <vector>
#include <memory>

namespace aurorart {
namespace serialization {

// 序列化器类型
enum class SerializerType {
    CDR,     // Common Data Representation
    Protobuf, // Google Protocol Buffers
    FlatBuffers // Google FlatBuffers
};

// 序列化器基类
class Serializer {
public:
    virtual ~Serializer() = default;
    
    // 序列化方法
    virtual std::vector<uint8_t> serialize(const void* data, size_t size) const = 0;
    
    // 反序列化方法
    virtual bool deserialize(const std::vector<uint8_t>& data, size_t& offset, void* dest, size_t size) const = 0;
};

// 序列化器工厂类
class SerializerFactory {
public:
    // 创建序列化器
    static std::unique_ptr<Serializer> createSerializer(SerializerType type);
};

// CDR 序列化器
class CDRSerializer : public Serializer {
public:
    std::vector<uint8_t> serialize(const void* data, size_t size) const override;
    bool deserialize(const std::vector<uint8_t>& data, size_t& offset, void* dest, size_t size) const override;
};

// Protobuf 序列化器
class ProtobufSerializer : public Serializer {
public:
    std::vector<uint8_t> serialize(const void* data, size_t size) const override;
    bool deserialize(const std::vector<uint8_t>& data, size_t& offset, void* dest, size_t size) const override;
};

// FlatBuffers 序列化器
class FlatBuffersSerializer : public Serializer {
public:
    std::vector<uint8_t> serialize(const void* data, size_t size) const override;
    bool deserialize(const std::vector<uint8_t>& data, size_t& offset, void* dest, size_t size) const override;
};

} // namespace serialization
} // namespace aurorart
