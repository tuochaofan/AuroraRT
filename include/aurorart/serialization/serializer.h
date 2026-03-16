#ifndef AURORART_SERIALIZER_H
#define AURORART_SERIALIZER_H

#include <vector>
#include <memory>
#include <string>
#include <map>

namespace aurorart {
namespace serialization {

enum class SerializerType {
    CDR,
    PROTOBUF,
    FLATBUFFERS,
    JSON
};

class Serializer {
public:
    virtual ~Serializer() = default;
    
    virtual std::vector<uint8_t> serialize(const void* data, size_t size) = 0;
    virtual void deserialize(const std::vector<uint8_t>& buffer, void* data, size_t size) = 0;
    virtual SerializerType getType() const = 0;
    
    // 批量操作方法
    virtual std::vector<std::vector<uint8_t>> serializeBatch(const std::vector<std::pair<const void*, size_t>>& data_items) = 0;
    virtual std::vector<bool> deserializeBatch(const std::vector<std::vector<uint8_t>>& buffers, 
                                           const std::vector<std::pair<void*, size_t>>& data_items) = 0;
    
    // 性能统计方法
    virtual void resetStats() = 0;
    virtual size_t getTotalSerializedBytes() const = 0;
    virtual size_t getTotalDeserializedBytes() const = 0;
    virtual double getAverageSerializationTime() const = 0;
    virtual double getAverageDeserializationTime() const = 0;
    
    // 辅助方法
    template<typename T>
    std::vector<uint8_t> serialize(const T& data);
    
    template<typename T>
    T deserialize(const std::vector<uint8_t>& buffer);
    
    template<typename T>
    std::vector<std::vector<uint8_t>> serializeBatch(const std::vector<T>& data_items);
    
    template<typename T>
    std::vector<bool> deserializeBatch(const std::vector<std::vector<uint8_t>>& buffers, std::vector<T>& data_items);
};

class CDRSerializer : public Serializer {
public:
    std::vector<uint8_t> serialize(const void* data, size_t size) override;
    void deserialize(const std::vector<uint8_t>& buffer, void* data, size_t size) override;
    std::vector<std::vector<uint8_t>> serializeBatch(const std::vector<std::pair<const void*, size_t>>& data_items) override;
    std::vector<bool> deserializeBatch(const std::vector<std::vector<uint8_t>>& buffers, 
                                   const std::vector<std::pair<void*, size_t>>& data_items) override;
    void resetStats() override;
    size_t getTotalSerializedBytes() const override;
    size_t getTotalDeserializedBytes() const override;
    double getAverageSerializationTime() const override;
    double getAverageDeserializationTime() const override;
    SerializerType getType() const override { return SerializerType::CDR; }
    
private:
    // CDR序列化辅助方法
    void alignBuffer(size_t alignment);
    std::vector<uint8_t> buffer_;
    
    // 性能统计
    std::atomic<size_t> total_serialized_bytes_;
    std::atomic<size_t> total_deserialized_bytes_;
    std::atomic<size_t> serialization_count_;
    std::atomic<size_t> deserialization_count_;
    std::atomic<double> total_serialization_time_;
    std::atomic<double> total_deserialization_time_;
};

class ProtobufSerializer : public Serializer {
public:
    std::vector<uint8_t> serialize(const void* data, size_t size) override;
    void deserialize(const std::vector<uint8_t>& buffer, void* data, size_t size) override;
    std::vector<std::vector<uint8_t>> serializeBatch(const std::vector<std::pair<const void*, size_t>>& data_items) override;
    std::vector<bool> deserializeBatch(const std::vector<std::vector<uint8_t>>& buffers, 
                                   const std::vector<std::pair<void*, size_t>>& data_items) override;
    void resetStats() override;
    size_t getTotalSerializedBytes() const override;
    size_t getTotalDeserializedBytes() const override;
    double getAverageSerializationTime() const override;
    double getAverageDeserializationTime() const override;
    SerializerType getType() const override { return SerializerType::PROTOBUF; }
    
private:
    // Protobuf序列化辅助方法
    void encodeVarint(uint64_t value, std::vector<uint8_t>& buffer);
    uint64_t decodeVarint(const uint8_t* data, size_t& offset);
    
    // 性能统计
    std::atomic<size_t> total_serialized_bytes_;
    std::atomic<size_t> total_deserialized_bytes_;
    std::atomic<size_t> serialization_count_;
    std::atomic<size_t> deserialization_count_;
    std::atomic<double> total_serialization_time_;
    std::atomic<double> total_deserialization_time_;
};

class FlatBuffersSerializer : public Serializer {
public:
    std::vector<uint8_t> serialize(const void* data, size_t size) override;
    void deserialize(const std::vector<uint8_t>& buffer, void* data, size_t size) override;
    std::vector<std::vector<uint8_t>> serializeBatch(const std::vector<std::pair<const void*, size_t>>& data_items) override;
    std::vector<bool> deserializeBatch(const std::vector<std::vector<uint8_t>>& buffers, 
                                   const std::vector<std::pair<void*, size_t>>& data_items) override;
    void resetStats() override;
    size_t getTotalSerializedBytes() const override;
    size_t getTotalDeserializedBytes() const override;
    double getAverageSerializationTime() const override;
    double getAverageDeserializationTime() const override;
    SerializerType getType() const override { return SerializerType::FLATBUFFERS; }
    
private:
    // FlatBuffers序列化辅助方法
    void prependSizePrefix(std::vector<uint8_t>& buffer);
    size_t getSizePrefix(const std::vector<uint8_t>& buffer);
    
    // 性能统计
    std::atomic<size_t> total_serialized_bytes_;
    std::atomic<size_t> total_deserialized_bytes_;
    std::atomic<size_t> serialization_count_;
    std::atomic<size_t> deserialization_count_;
    std::atomic<double> total_serialization_time_;
    std::atomic<double> total_deserialization_time_;
};

class JSONSerializer : public Serializer {
public:
    std::vector<uint8_t> serialize(const void* data, size_t size) override;
    void deserialize(const std::vector<uint8_t>& buffer, void* data, size_t size) override;
    std::vector<std::vector<uint8_t>> serializeBatch(const std::vector<std::pair<const void*, size_t>>& data_items) override;
    std::vector<bool> deserializeBatch(const std::vector<std::vector<uint8_t>>& buffers, 
                                   const std::vector<std::pair<void*, size_t>>& data_items) override;
    void resetStats() override;
    size_t getTotalSerializedBytes() const override;
    size_t getTotalDeserializedBytes() const override;
    double getAverageSerializationTime() const override;
    double getAverageDeserializationTime() const override;
    SerializerType getType() const override { return SerializerType::JSON; }
    
private:
    // JSON序列化辅助方法
    std::string escapeString(const std::string& str);
    std::string unescapeString(const std::string& str);
    
    // 性能统计
    std::atomic<size_t> total_serialized_bytes_;
    std::atomic<size_t> total_deserialized_bytes_;
    std::atomic<size_t> serialization_count_;
    std::atomic<size_t> deserialization_count_;
    std::atomic<double> total_serialization_time_;
    std::atomic<double> total_deserialization_time_;
};

class SerializerManager {
public:
    static SerializerManager& instance();
    
    void init();
    std::shared_ptr<Serializer> getSerializer(SerializerType type);
    std::shared_ptr<Serializer> getOptimalSerializer(size_t dataSize, bool realTime);
    
private:
    SerializerManager() = default;
    std::map<SerializerType, std::shared_ptr<Serializer>> serializers_;
};

class SerializerFactory {
public:
    static std::shared_ptr<Serializer> createSerializer(SerializerType type);
    static std::shared_ptr<Serializer> getOptimalSerializer(const void* data, size_t size, bool realTime);
};

// 模板方法实现
template<typename T>
std::vector<uint8_t> Serializer::serialize(const T& data) {
    return serialize(&data, sizeof(T));
}

template<typename T>
T Serializer::deserialize(const std::vector<uint8_t>& buffer) {
    T data;
    deserialize(buffer, &data, sizeof(T));
    return data;
}

template<typename T>
std::vector<std::vector<uint8_t>> Serializer::serializeBatch(const std::vector<T>& data_items) {
    std::vector<std::pair<const void*, size_t>> items;
    items.reserve(data_items.size());
    for (const auto& item : data_items) {
        items.emplace_back(&item, sizeof(T));
    }
    return serializeBatch(items);
}

template<typename T>
std::vector<bool> Serializer::deserializeBatch(const std::vector<std::vector<uint8_t>>& buffers, std::vector<T>& data_items) {
    std::vector<std::pair<void*, size_t>> items;
    items.reserve(data_items.size());
    for (auto& item : data_items) {
        items.emplace_back(&item, sizeof(T));
    }
    return deserializeBatch(buffers, items);
}

} // namespace serialization
} // namespace aurorart

#endif // AURORART_SERIALIZER_H