#include "aurorart/serialization/serializer.h"

namespace aurorart {
namespace serialization {

// 序列化器工厂实现
std::unique_ptr<Serializer> SerializerFactory::createSerializer(SerializerType type) {
    switch (type) {
        case SerializerType::CDR:
            return std::make_unique<CDRSerializer>();
        case SerializerType::Protobuf:
            return std::make_unique<ProtobufSerializer>();
        case SerializerType::FlatBuffers:
            return std::make_unique<FlatBuffersSerializer>();
        default:
            return std::make_unique<CDRSerializer>();
    }
}

// CDR 序列化器实现
std::vector<uint8_t> CDRSerializer::serialize(const void* data, size_t size) const {
    std::vector<uint8_t> result(size);
    memcpy(result.data(), data, size);
    return result;
}

bool CDRSerializer::deserialize(const std::vector<uint8_t>& data, size_t& offset, void* dest, size_t size) const {
    if (offset + size > data.size()) {
        return false;
    }
    memcpy(dest, data.data() + offset, size);
    offset += size;
    return true;
}

// Protobuf 序列化器实现
std::vector<uint8_t> ProtobufSerializer::serialize(const void* data, size_t size) const {
    // Protobuf序列化实现
    // 注意：实际使用时，应该根据具体的Protobuf消息类型进行序列化
    // 这里提供一个通用的实现，适用于简单类型
    std::vector<uint8_t> result(size);
    memcpy(result.data(), data, size);
    return result;
}

bool ProtobufSerializer::deserialize(const std::vector<uint8_t>& data, size_t& offset, void* dest, size_t size) const {
    // Protobuf反序列化实现
    // 注意：实际使用时，应该根据具体的Protobuf消息类型进行反序列化
    if (offset + size > data.size()) {
        return false;
    }
    memcpy(dest, data.data() + offset, size);
    offset += size;
    return true;
}

// FlatBuffers 序列化器实现
std::vector<uint8_t> FlatBuffersSerializer::serialize(const void* data, size_t size) const {
    // FlatBuffers序列化实现
    // 注意：实际使用时，应该根据具体的FlatBuffers消息类型进行序列化
    // 这里提供一个通用的实现，适用于简单类型
    std::vector<uint8_t> result(size);
    memcpy(result.data(), data, size);
    return result;
}

bool FlatBuffersSerializer::deserialize(const std::vector<uint8_t>& data, size_t& offset, void* dest, size_t size) const {
    // FlatBuffers反序列化实现
    // 注意：实际使用时，应该根据具体的FlatBuffers消息类型进行反序列化
    if (offset + size > data.size()) {
        return false;
    }
    memcpy(dest, data.data() + offset, size);
    offset += size;
    return true;
}

} // namespace serialization
} // namespace aurorart
