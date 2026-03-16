#include "aurorart/serialization/serializer.h"
#include "aurorart/utils/logger.h"
#include <cstring>
#include <sstream>
#include <iomanip>

namespace aurorart {
namespace serialization {

// Base64 encoding/decoding utilities

std::string base64Encode(const void* data, size_t size) {
    const char* base64Chars = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
    std::string result;
    result.reserve((size + 2) / 3 * 4);
    
    const uint8_t* bytes = static_cast<const uint8_t*>(data);
    for (size_t i = 0; i < size; i += 3) {
        uint32_t val = 0;
        for (size_t j = 0; j < 3 && i + j < size; ++j) {
            val |= static_cast<uint32_t>(bytes[i + j]) << (8 * (2 - j));
        }
        
        for (size_t j = 0; j < 4; ++j) {
            if (i + j * 3 / 4 < size) {
                result += base64Chars[(val >> (6 * (3 - j))) & 0x3F];
            } else {
                result += '=';
            }
        }
    }
    return result;
}

std::vector<uint8_t> base64Decode(const std::string& encoded) {
    const char* base64Chars = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
    std::vector<uint8_t> result;
    result.reserve(encoded.size() * 3 / 4);
    
    std::vector<int> lookup(256, -1);
    for (int i = 0; i < 64; ++i) {
        lookup[static_cast<unsigned char>(base64Chars[i])] = i;
    }
    
    size_t i = 0;
    while (i < encoded.size()) {
        uint32_t val = 0;
        int count = 0;
        for (int j = 0; j < 4 && i < encoded.size(); ++j) {
            char c = encoded[i++];
            if (c == '=') break;
            if (lookup[static_cast<unsigned char>(c)] == -1) continue;
            val |= static_cast<uint32_t>(lookup[static_cast<unsigned char>(c)]) << (6 * (3 - j));
            count++;
        }
        
        for (int j = 0; j < count - 1; ++j) {
            result.push_back(static_cast<uint8_t>((val >> (8 * (2 - j))) & 0xFF));
        }
    }
    return result;
}

// CDRSerializer implementation

void CDRSerializer::alignBuffer(size_t alignment) {
    size_t currentSize = buffer_.size();
    size_t padding = (alignment - (currentSize % alignment)) % alignment;
    buffer_.insert(buffer_.end(), padding, 0);
}

std::vector<uint8_t> CDRSerializer::serialize(const void* data, size_t size) {
    if (!data || size == 0) {
        AURORA_LOG_ERROR("CDRSerializer: Invalid data or size");
        return {};
    }
    
    auto start_time = std::chrono::high_resolution_clock::now();
    
    try {
        buffer_.clear();
        // 预分配缓冲区，避免多次内存分配
        size_t alignedSize = ((size + 3) / 4) * 4; // 4字节对齐
        buffer_.reserve(alignedSize);
        
        alignBuffer(4);
        buffer_.resize(buffer_.size() + size);
        memcpy(buffer_.data() + buffer_.size() - size, data, size);
        
        // 更新统计信息
        total_serialized_bytes_ += size;
        serialization_count_++;
        auto end_time = std::chrono::high_resolution_clock::now();
        double duration = std::chrono::duration<double, std::micro>(end_time - start_time).count();
        total_serialization_time_ += duration;
        
        AURORA_LOG_DEBUG("CDRSerializer: Serialized {} bytes", size);
        return buffer_;
    } catch (const std::exception& e) {
        AURORA_LOG_ERROR("CDRSerializer: Serialization error: {}", e.what());
        return {};
    }
}

std::vector<std::vector<uint8_t>> CDRSerializer::serializeBatch(const std::vector<std::pair<const void*, size_t>>& data_items) {
    std::vector<std::vector<uint8_t>> results;
    results.reserve(data_items.size());
    
    auto start_time = std::chrono::high_resolution_clock::now();
    
    for (const auto& item : data_items) {
        results.push_back(serialize(item.first, item.second));
    }
    
    auto end_time = std::chrono::high_resolution_clock::now();
    double duration = std::chrono::duration<double, std::micro>(end_time - start_time).count();
    AURORA_LOG_DEBUG("CDRSerializer: Serialized batch of {} items in {:.2f}us", data_items.size(), duration);
    
    return results;
}

std::vector<bool> CDRSerializer::deserializeBatch(const std::vector<std::vector<uint8_t>>& buffers, 
                                               const std::vector<std::pair<void*, size_t>>& data_items) {
    std::vector<bool> results;
    results.reserve(buffers.size());
    
    auto start_time = std::chrono::high_resolution_clock::now();
    
    for (size_t i = 0; i < std::min(buffers.size(), data_items.size()); ++i) {
        deserialize(buffers[i], data_items[i].first, data_items[i].second);
        results.push_back(true);
    }
    
    auto end_time = std::chrono::high_resolution_clock::now();
    double duration = std::chrono::duration<double, std::micro>(end_time - start_time).count();
    AURORA_LOG_DEBUG("CDRSerializer: Deserialized batch of {} items in {:.2f}us", results.size(), duration);
    
    return results;
}

void CDRSerializer::resetStats() {
    total_serialized_bytes_ = 0;
    total_deserialized_bytes_ = 0;
    serialization_count_ = 0;
    deserialization_count_ = 0;
    total_serialization_time_ = 0.0;
    total_deserialization_time_ = 0.0;
}

size_t CDRSerializer::getTotalSerializedBytes() const {
    return total_serialized_bytes_;
}

size_t CDRSerializer::getTotalDeserializedBytes() const {
    return total_deserialized_bytes_;
}

double CDRSerializer::getAverageSerializationTime() const {
    size_t count = serialization_count_;
    return count > 0 ? total_serialization_time_ / count : 0.0;
}

double CDRSerializer::getAverageDeserializationTime() const {
    size_t count = deserialization_count_;
    return count > 0 ? total_deserialization_time_ / count : 0.0;
}

void CDRSerializer::deserialize(const std::vector<uint8_t>& buffer, void* data, size_t size) {
    if (!data || size == 0 || buffer.empty()) {
        AURORA_LOG_ERROR("CDRSerializer: Invalid parameters");
        return;
    }
    
    try {
        if (buffer.size() >= size) {
            size_t offset = 0;
            size_t alignment = 4;
            offset = (alignment - (offset % alignment)) % alignment;
            if (offset + size <= buffer.size()) {
                memcpy(data, buffer.data() + offset, size);
                AURORA_LOG_DEBUG("CDRSerializer: Deserialized {} bytes", size);
            } else {
                AURORA_LOG_ERROR("CDRSerializer: Buffer size insufficient: {} < {}", buffer.size(), offset + size);
            }
        } else {
            AURORA_LOG_ERROR("CDRSerializer: Buffer size insufficient: {} < {}", buffer.size(), size);
        }
    } catch (const std::exception& e) {
        AURORA_LOG_ERROR("CDRSerializer: Deserialization error: {}", e.what());
    }
}

// ProtobufSerializer implementation

void ProtobufSerializer::encodeVarint(uint64_t value, std::vector<uint8_t>& buffer) {
    while (value > 0x7F) {
        buffer.push_back(static_cast<uint8_t>((value & 0x7F) | 0x80));
        value >>= 7;
    }
    buffer.push_back(static_cast<uint8_t>(value));
}

uint64_t ProtobufSerializer::decodeVarint(const uint8_t* data, size_t& offset) {
    uint64_t value = 0;
    int shift = 0;
    while (true) {
        uint8_t byte = data[offset++];
        value |= static_cast<uint64_t>(byte & 0x7F) << shift;
        if (!(byte & 0x80)) break;
        shift += 7;
    }
    return value;
}

std::vector<uint8_t> ProtobufSerializer::serialize(const void* data, size_t size) {
    if (!data || size == 0) {
        AURORA_LOG_ERROR("ProtobufSerializer: Invalid data or size");
        return {};
    }
    
    auto start_time = std::chrono::high_resolution_clock::now();
    
    try {
        // 预分配缓冲区，避免多次内存分配
        // 预估varint编码的大小（最多10字节）
        std::vector<uint8_t> buffer;
        buffer.reserve(10 + 10 + size); // tag + size + data
        
        uint32_t tag = 1;
        encodeVarint(tag << 3 | 2, buffer);
        encodeVarint(size, buffer);
        buffer.resize(buffer.size() + size);
        memcpy(buffer.data() + buffer.size() - size, data, size);
        
        // 更新统计信息
        total_serialized_bytes_ += size;
        serialization_count_++;
        auto end_time = std::chrono::high_resolution_clock::now();
        double duration = std::chrono::duration<double, std::micro>(end_time - start_time).count();
        total_serialization_time_ += duration;
        
        AURORA_LOG_DEBUG("ProtobufSerializer: Serialized {} bytes", size);
        return buffer;
    } catch (const std::exception& e) {
        AURORA_LOG_ERROR("ProtobufSerializer: Serialization error: {}", e.what());
        return {};
    }
}

std::vector<std::vector<uint8_t>> ProtobufSerializer::serializeBatch(const std::vector<std::pair<const void*, size_t>>& data_items) {
    std::vector<std::vector<uint8_t>> results;
    results.reserve(data_items.size());
    
    auto start_time = std::chrono::high_resolution_clock::now();
    
    for (const auto& item : data_items) {
        results.push_back(serialize(item.first, item.second));
    }
    
    auto end_time = std::chrono::high_resolution_clock::now();
    double duration = std::chrono::duration<double, std::micro>(end_time - start_time).count();
    AURORA_LOG_DEBUG("ProtobufSerializer: Serialized batch of {} items in {:.2f}us", data_items.size(), duration);
    
    return results;
}

std::vector<bool> ProtobufSerializer::deserializeBatch(const std::vector<std::vector<uint8_t>>& buffers, 
                                                   const std::vector<std::pair<void*, size_t>>& data_items) {
    std::vector<bool> results;
    results.reserve(buffers.size());
    
    auto start_time = std::chrono::high_resolution_clock::now();
    
    for (size_t i = 0; i < std::min(buffers.size(), data_items.size()); ++i) {
        deserialize(buffers[i], data_items[i].first, data_items[i].second);
        results.push_back(true);
    }
    
    auto end_time = std::chrono::high_resolution_clock::now();
    double duration = std::chrono::duration<double, std::micro>(end_time - start_time).count();
    AURORA_LOG_DEBUG("ProtobufSerializer: Deserialized batch of {} items in {:.2f}us", results.size(), duration);
    
    return results;
}

void ProtobufSerializer::resetStats() {
    total_serialized_bytes_ = 0;
    total_deserialized_bytes_ = 0;
    serialization_count_ = 0;
    deserialization_count_ = 0;
    total_serialization_time_ = 0.0;
    total_deserialization_time_ = 0.0;
}

size_t ProtobufSerializer::getTotalSerializedBytes() const {
    return total_serialized_bytes_;
}

size_t ProtobufSerializer::getTotalDeserializedBytes() const {
    return total_deserialized_bytes_;
}

double ProtobufSerializer::getAverageSerializationTime() const {
    size_t count = serialization_count_;
    return count > 0 ? total_serialization_time_ / count : 0.0;
}

double ProtobufSerializer::getAverageDeserializationTime() const {
    size_t count = deserialization_count_;
    return count > 0 ? total_deserialization_time_ / count : 0.0;
}

void ProtobufSerializer::deserialize(const std::vector<uint8_t>& buffer, void* data, size_t size) {
    if (!data || size == 0 || buffer.empty()) {
        AURORA_LOG_ERROR("ProtobufSerializer: Invalid parameters");
        return;
    }
    
    auto start_time = std::chrono::high_resolution_clock::now();
    
    try {
        if (buffer.size() > 0) {
            size_t offset = 0;
            decodeVarint(buffer.data(), offset);
            uint64_t length = decodeVarint(buffer.data(), offset);
            if (offset + size <= buffer.size()) {
                memcpy(data, buffer.data() + offset, size);
                AURORA_LOG_DEBUG("ProtobufSerializer: Deserialized {} bytes", size);
            } else {
                AURORA_LOG_ERROR("ProtobufSerializer: Buffer size insufficient: {} < {}", buffer.size(), offset + size);
            }
        } else {
            AURORA_LOG_ERROR("ProtobufSerializer: Empty buffer");
        }
        
        // 更新统计信息
        total_deserialized_bytes_ += size;
        deserialization_count_++;
        auto end_time = std::chrono::high_resolution_clock::now();
        double duration = std::chrono::duration<double, std::micro>(end_time - start_time).count();
        total_deserialization_time_ += duration;
    } catch (const std::exception& e) {
        AURORA_LOG_ERROR("ProtobufSerializer: Deserialization error: {}", e.what());
    }
}

// FlatBuffersSerializer implementation

void FlatBuffersSerializer::prependSizePrefix(std::vector<uint8_t>& buffer) {
    uint32_t size = static_cast<uint32_t>(buffer.size());
    std::vector<uint8_t> prefix(sizeof(uint32_t));
    memcpy(prefix.data(), &size, sizeof(uint32_t));
    prefix.insert(prefix.end(), buffer.begin(), buffer.end());
    buffer.swap(prefix);
}

size_t FlatBuffersSerializer::getSizePrefix(const std::vector<uint8_t>& buffer) {
    if (buffer.size() >= sizeof(uint32_t)) {
        uint32_t size;
        memcpy(&size, buffer.data(), sizeof(uint32_t));
        return size;
    }
    return 0;
}

std::vector<uint8_t> FlatBuffersSerializer::serialize(const void* data, size_t size) {
    if (!data || size == 0) {
        AURORA_LOG_ERROR("FlatBuffersSerializer: Invalid data or size");
        return {};
    }
    
    auto start_time = std::chrono::high_resolution_clock::now();
    
    try {
        // 预分配缓冲区，包含大小前缀
        std::vector<uint8_t> buffer(size + sizeof(uint32_t));
        // 先复制数据
        memcpy(buffer.data() + sizeof(uint32_t), data, size);
        // 再添加大小前缀
        uint32_t sizeValue = static_cast<uint32_t>(size);
        memcpy(buffer.data(), &sizeValue, sizeof(uint32_t));
        
        // 更新统计信息
        total_serialized_bytes_ += size;
        serialization_count_++;
        auto end_time = std::chrono::high_resolution_clock::now();
        double duration = std::chrono::duration<double, std::micro>(end_time - start_time).count();
        total_serialization_time_ += duration;
        
        AURORA_LOG_DEBUG("FlatBuffersSerializer: Serialized {} bytes", size);
        return buffer;
    } catch (const std::exception& e) {
        AURORA_LOG_ERROR("FlatBuffersSerializer: Serialization error: {}", e.what());
        return {};
    }
}

std::vector<std::vector<uint8_t>> FlatBuffersSerializer::serializeBatch(const std::vector<std::pair<const void*, size_t>>& data_items) {
    std::vector<std::vector<uint8_t>> results;
    results.reserve(data_items.size());
    
    auto start_time = std::chrono::high_resolution_clock::now();
    
    for (const auto& item : data_items) {
        results.push_back(serialize(item.first, item.second));
    }
    
    auto end_time = std::chrono::high_resolution_clock::now();
    double duration = std::chrono::duration<double, std::micro>(end_time - start_time).count();
    AURORA_LOG_DEBUG("FlatBuffersSerializer: Serialized batch of {} items in {:.2f}us", data_items.size(), duration);
    
    return results;
}

std::vector<bool> FlatBuffersSerializer::deserializeBatch(const std::vector<std::vector<uint8_t>>& buffers, 
                                                      const std::vector<std::pair<void*, size_t>>& data_items) {
    std::vector<bool> results;
    results.reserve(buffers.size());
    
    auto start_time = std::chrono::high_resolution_clock::now();
    
    for (size_t i = 0; i < std::min(buffers.size(), data_items.size()); ++i) {
        deserialize(buffers[i], data_items[i].first, data_items[i].second);
        results.push_back(true);
    }
    
    auto end_time = std::chrono::high_resolution_clock::now();
    double duration = std::chrono::duration<double, std::micro>(end_time - start_time).count();
    AURORA_LOG_DEBUG("FlatBuffersSerializer: Deserialized batch of {} items in {:.2f}us", results.size(), duration);
    
    return results;
}

void FlatBuffersSerializer::resetStats() {
    total_serialized_bytes_ = 0;
    total_deserialized_bytes_ = 0;
    serialization_count_ = 0;
    deserialization_count_ = 0;
    total_serialization_time_ = 0.0;
    total_deserialization_time_ = 0.0;
}

size_t FlatBuffersSerializer::getTotalSerializedBytes() const {
    return total_serialized_bytes_;
}

size_t FlatBuffersSerializer::getTotalDeserializedBytes() const {
    return total_deserialized_bytes_;
}

double FlatBuffersSerializer::getAverageSerializationTime() const {
    size_t count = serialization_count_;
    return count > 0 ? total_serialization_time_ / count : 0.0;
}

double FlatBuffersSerializer::getAverageDeserializationTime() const {
    size_t count = deserialization_count_;
    return count > 0 ? total_deserialization_time_ / count : 0.0;
}

void FlatBuffersSerializer::deserialize(const std::vector<uint8_t>& buffer, void* data, size_t size) {
    if (!data || size == 0 || buffer.empty()) {
        AURORA_LOG_ERROR("FlatBuffersSerializer: Invalid parameters");
        return;
    }
    
    auto start_time = std::chrono::high_resolution_clock::now();
    
    try {
        if (buffer.size() >= sizeof(uint32_t)) {
            size_t dataSize = getSizePrefix(buffer);
            if (dataSize >= size) {
                memcpy(data, buffer.data() + sizeof(uint32_t), size);
                AURORA_LOG_DEBUG("FlatBuffersSerializer: Deserialized {} bytes", size);
            } else {
                AURORA_LOG_ERROR("FlatBuffersSerializer: Data size insufficient: {} < {}", dataSize, size);
            }
        } else {
            AURORA_LOG_ERROR("FlatBuffersSerializer: Buffer size insufficient: {} < {}", buffer.size(), sizeof(uint32_t));
        }
        
        // 更新统计信息
        total_deserialized_bytes_ += size;
        deserialization_count_++;
        auto end_time = std::chrono::high_resolution_clock::now();
        double duration = std::chrono::duration<double, std::micro>(end_time - start_time).count();
        total_deserialization_time_ += duration;
    } catch (const std::exception& e) {
        AURORA_LOG_ERROR("FlatBuffersSerializer: Deserialization error: {}", e.what());
    }
}

// JSONSerializer implementation

std::string JSONSerializer::escapeString(const std::string& str) {
    std::string result;
    for (char c : str) {
        switch (c) {
        case '"': result += '\\"'; break;
        case '\\': result += '\\\\'; break;
        case '\b': result += '\\b'; break;
        case '\f': result += '\\f'; break;
        case '\n': result += '\\n'; break;
        case '\r': result += '\\r'; break;
        case '\t': result += '\\t'; break;
        default:
            if (static_cast<unsigned char>(c) < 32 || static_cast<unsigned char>(c) > 126) {
                std::stringstream ss;
                ss << "\\u" << std::setw(4) << std::setfill('0') << std::hex << static_cast<int>(static_cast<unsigned char>(c));
                result += ss.str();
            } else {
                result += c;
            }
            break;
        }
    }
    return result;
}

std::string JSONSerializer::unescapeString(const std::string& str) {
    std::string result;
    size_t i = 0;
    while (i < str.size()) {
        if (str[i] == '\\' && i + 1 < str.size()) {
            switch (str[i + 1]) {
            case '"': result += '"'; i += 2; break;
            case '\\': result += '\\'; i += 2; break;
            case 'b': result += '\b'; i += 2; break;
            case 'f': result += '\f'; i += 2; break;
            case 'n': result += '\n'; i += 2; break;
            case 'r': result += '\r'; i += 2; break;
            case 't': result += '\t'; i += 2; break;
            case 'u':
                if (i + 5 < str.size()) {
                    std::string hex = str.substr(i + 2, 4);
                    try {
                        int code = std::stoi(hex, nullptr, 16);
                        result += static_cast<char>(code);
                        i += 6;
                    } catch (...) {
                        result += str[i];
                        i++;
                    }
                } else {
                    result += str[i];
                    i++;
                }
                break;
            default:
                result += str[i];
                i++;
                break;
            }
        } else {
            result += str[i];
            i++;
        }
    }
    return result;
}

std::vector<uint8_t> JSONSerializer::serialize(const void* data, size_t size) {
    if (!data || size == 0) {
        AURORA_LOG_ERROR("JSONSerializer: Invalid data or size");
        return {};
    }
    
    auto start_time = std::chrono::high_resolution_clock::now();
    
    try {
        std::string base64Data = base64Encode(data, size);
        std::string escapedData = escapeString(base64Data);
        std::string sizeStr = std::to_string(size);
        
        // 预分配缓冲区，避免多次内存分配
        size_t jsonSize = 8 + escapedData.size() + 8 + sizeStr.size() + 2; // {"data":""size":""}
        std::string json;
        json.reserve(jsonSize);
        
        json += "{\"data\":\"" + escapedData + "\",\"size\":\"" + sizeStr + "\"}";
        
        std::vector<uint8_t> buffer(json.begin(), json.end());
        
        // 更新统计信息
        total_serialized_bytes_ += size;
        serialization_count_++;
        auto end_time = std::chrono::high_resolution_clock::now();
        double duration = std::chrono::duration<double, std::micro>(end_time - start_time).count();
        total_serialization_time_ += duration;
        
        AURORA_LOG_DEBUG("JSONSerializer: Serialized {} bytes", size);
        return buffer;
    } catch (const std::exception& e) {
        AURORA_LOG_ERROR("JSONSerializer: Serialization error: {}", e.what());
        return {};
    }
}

std::vector<std::vector<uint8_t>> JSONSerializer::serializeBatch(const std::vector<std::pair<const void*, size_t>>& data_items) {
    std::vector<std::vector<uint8_t>> results;
    results.reserve(data_items.size());
    
    auto start_time = std::chrono::high_resolution_clock::now();
    
    for (const auto& item : data_items) {
        results.push_back(serialize(item.first, item.second));
    }
    
    auto end_time = std::chrono::high_resolution_clock::now();
    double duration = std::chrono::duration<double, std::micro>(end_time - start_time).count();
    AURORA_LOG_DEBUG("JSONSerializer: Serialized batch of {} items in {:.2f}us", data_items.size(), duration);
    
    return results;
}

std::vector<bool> JSONSerializer::deserializeBatch(const std::vector<std::vector<uint8_t>>& buffers, 
                                               const std::vector<std::pair<void*, size_t>>& data_items) {
    std::vector<bool> results;
    results.reserve(buffers.size());
    
    auto start_time = std::chrono::high_resolution_clock::now();
    
    for (size_t i = 0; i < std::min(buffers.size(), data_items.size()); ++i) {
        deserialize(buffers[i], data_items[i].first, data_items[i].second);
        results.push_back(true);
    }
    
    auto end_time = std::chrono::high_resolution_clock::now();
    double duration = std::chrono::duration<double, std::micro>(end_time - start_time).count();
    AURORA_LOG_DEBUG("JSONSerializer: Deserialized batch of {} items in {:.2f}us", results.size(), duration);
    
    return results;
}

void JSONSerializer::resetStats() {
    total_serialized_bytes_ = 0;
    total_deserialized_bytes_ = 0;
    serialization_count_ = 0;
    deserialization_count_ = 0;
    total_serialization_time_ = 0.0;
    total_deserialization_time_ = 0.0;
}

size_t JSONSerializer::getTotalSerializedBytes() const {
    return total_serialized_bytes_;
}

size_t JSONSerializer::getTotalDeserializedBytes() const {
    return total_deserialized_bytes_;
}

double JSONSerializer::getAverageSerializationTime() const {
    size_t count = serialization_count_;
    return count > 0 ? total_serialization_time_ / count : 0.0;
}

double JSONSerializer::getAverageDeserializationTime() const {
    size_t count = deserialization_count_;
    return count > 0 ? total_deserialization_time_ / count : 0.0;
}

void JSONSerializer::deserialize(const std::vector<uint8_t>& buffer, void* data, size_t size) {
    if (!data || size == 0 || buffer.empty()) {
        AURORA_LOG_ERROR("JSONSerializer: Invalid parameters");
        return;
    }
    
    auto start_time = std::chrono::high_resolution_clock::now();
    
    try {
        std::string json(buffer.begin(), buffer.end());
        // 简化的JSON解析，实际应该使用JSON库
        size_t dataStart = json.find("\"data\":\"");
        size_t dataEnd = json.find("\"", dataStart + 8);
        if (dataStart != std::string::npos && dataEnd != std::string::npos) {
            std::string base64Data = json.substr(dataStart + 8, dataEnd - (dataStart + 8));
            base64Data = unescapeString(base64Data);
            std::vector<uint8_t> decodedData = base64Decode(base64Data);
            if (decodedData.size() >= size) {
                memcpy(data, decodedData.data(), size);
                AURORA_LOG_DEBUG("JSONSerializer: Deserialized {} bytes", size);
            } else {
                AURORA_LOG_ERROR("JSONSerializer: Decoded data size insufficient: {} < {}", decodedData.size(), size);
            }
        } else {
            AURORA_LOG_ERROR("JSONSerializer: Invalid JSON format");
        }
        
        // 更新统计信息
        total_deserialized_bytes_ += size;
        deserialization_count_++;
        auto end_time = std::chrono::high_resolution_clock::now();
        double duration = std::chrono::duration<double, std::micro>(end_time - start_time).count();
        total_deserialization_time_ += duration;
    } catch (const std::exception& e) {
        AURORA_LOG_ERROR("JSONSerializer: Deserialization error: {}", e.what());
    }
}

// SerializerManager implementation

SerializerManager& SerializerManager::instance() {
    static SerializerManager instance;
    return instance;
}

void SerializerManager::init() {
    serializers_[SerializerType::CDR] = std::make_shared<CDRSerializer>();
    serializers_[SerializerType::PROTOBUF] = std::make_shared<ProtobufSerializer>();
    serializers_[SerializerType::FLATBUFFERS] = std::make_shared<FlatBuffersSerializer>();
    serializers_[SerializerType::JSON] = std::make_shared<JSONSerializer>();
    AURORA_LOG_INFO("SerializerManager initialized with {} serializer types", serializers_.size());
}

std::shared_ptr<Serializer> SerializerManager::getSerializer(SerializerType type) {
    auto it = serializers_.find(type);
    if (it != serializers_.end()) {
        return it->second;
    }
    AURORA_LOG_WARN("Serializer type not found: {}", static_cast<int>(type));
    return nullptr;
}

std::shared_ptr<Serializer> SerializerManager::getOptimalSerializer(size_t dataSize, bool realTime) {
    if (realTime) {
        auto serializer = getSerializer(SerializerType::CDR);
        AURORA_LOG_DEBUG("Selected CDR serializer for real-time data");
        return serializer;
    } else if (dataSize < 1024) {
        auto serializer = getSerializer(SerializerType::FLATBUFFERS);
        AURORA_LOG_DEBUG("Selected FlatBuffers serializer for small data: {} bytes", dataSize);
        return serializer;
    } else {
        auto serializer = getSerializer(SerializerType::PROTOBUF);
        AURORA_LOG_DEBUG("Selected Protobuf serializer for large data: {} bytes", dataSize);
        return serializer;
    }
}

// SerializerFactory implementation

std::shared_ptr<Serializer> SerializerFactory::createSerializer(SerializerType type) {
    switch (type) {
    case SerializerType::CDR:
        return std::make_shared<CDRSerializer>();
    case SerializerType::PROTOBUF:
        return std::make_shared<ProtobufSerializer>();
    case SerializerType::FLATBUFFERS:
        return std::make_shared<FlatBuffersSerializer>();
    case SerializerType::JSON:
        return std::make_shared<JSONSerializer>();
    default:
        AURORA_LOG_ERROR("Unknown serializer type: {}", static_cast<int>(type));
        return nullptr;
    }
}

std::shared_ptr<Serializer> SerializerFactory::getOptimalSerializer(const void* data, size_t size, bool realTime) {
    if (realTime) {
        AURORA_LOG_DEBUG("Creating CDR serializer for real-time data");
        return std::make_shared<CDRSerializer>();
    } else if (size < 1024) {
        AURORA_LOG_DEBUG("Creating FlatBuffers serializer for small data: {} bytes", size);
        return std::make_shared<FlatBuffersSerializer>();
    } else {
        AURORA_LOG_DEBUG("Creating Protobuf serializer for large data: {} bytes", size);
        return std::make_shared<ProtobufSerializer>();
    }
}

} // namespace serialization
} // namespace aurorart