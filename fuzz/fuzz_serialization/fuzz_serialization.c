/*
 * AuroraRT 序列化模块模糊测试
 */

#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include "aurorart/serialization/serializer.h"

int LLVMFuzzerTestOneInput(
    const uint8_t *data,
    size_t size)
{
    if (size == 0) {
        return 0;
    }

    // 测试CDR序列化器
    aurorart::serialization::CDRSerializer cdr_serializer;
    
    // 测试序列化
    std::vector<uint8_t> serialized = cdr_serializer.serialize(data, size);
    
    // 测试反序列化
    size_t offset = 0;
    uint8_t buffer[size];
    bool result = cdr_serializer.deserialize(serialized, offset, buffer, size);
    
    // 测试Protobuf序列化器
    aurorart::serialization::ProtobufSerializer protobuf_serializer;
    serialized = protobuf_serializer.serialize(data, size);
    offset = 0;
    result = protobuf_serializer.deserialize(serialized, offset, buffer, size);
    
    // 测试FlatBuffers序列化器
    aurorart::serialization::FlatBuffersSerializer flatbuffers_serializer;
    serialized = flatbuffers_serializer.serialize(data, size);
    offset = 0;
    result = flatbuffers_serializer.deserialize(serialized, offset, buffer, size);
    
    // 测试序列化器工厂
    auto cdr_serializer_ptr = aurorart::serialization::SerializerFactory::createSerializer(aurorart::serialization::SerializerType::CDR);
    auto protobuf_serializer_ptr = aurorart::serialization::SerializerFactory::createSerializer(aurorart::serialization::SerializerType::Protobuf);
    auto flatbuffers_serializer_ptr = aurorart::serialization::SerializerFactory::createSerializer(aurorart::serialization::SerializerType::FlatBuffers);
    
    return 0;
}
