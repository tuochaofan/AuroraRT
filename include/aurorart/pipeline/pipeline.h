#ifndef AURORART_PIPELINE_H
#define AURORART_PIPELINE_H

#include <vector>
#include <memory>
#include <functional>
#include <string>

namespace aurorart {
namespace pipeline {

class PipelineStage {
public:
    virtual ~PipelineStage() = default;
    
    virtual void process(void* data) = 0;
    virtual std::string getName() const = 0;
    virtual bool isEnabled() const = 0;
    virtual void setEnabled(bool enabled) = 0;
};

class GenericPipelineStage : public PipelineStage {
public:
    GenericPipelineStage(const std::string& name, std::function<void(void*)> processor);
    
    void process(void* data) override;
    std::string getName() const override { return name_; }
    bool isEnabled() const override { return enabled_; }
    void setEnabled(bool enabled) override { enabled_ = enabled; }
    
private:
    std::string name_;
    std::function<void(void*)> processor_;
    bool enabled_;
};

// 前向声明
template <typename T>
class DataStream;

class Pipeline {
public:
    void addStage(std::shared_ptr<PipelineStage> stage);
    void removeStage(const std::string& name);
    void clearStages();
    void process(void* data);
    void processAsync(void* data);
    void processParallel(void* data);
    void processBatch(const std::vector<void*>& dataBatch);
    void processBatchAsync(const std::vector<void*>& dataBatch);
    
    const std::vector<std::shared_ptr<PipelineStage>>& getStages() const { return stages_; }
    
private:
    std::vector<std::shared_ptr<PipelineStage>> stages_;
};

class PipelineManager {
public:
    static PipelineManager& instance();
    
    void registerPipeline(const std::string& name, std::shared_ptr<Pipeline> pipeline);
    std::shared_ptr<Pipeline> getPipeline(const std::string& name);
    void removePipeline(const std::string& name);
    
private:
    PipelineManager() = default;
    std::map<std::string, std::shared_ptr<Pipeline>> pipelines_;
};

class PipelineBuilder {
public:
    PipelineBuilder();
    
    PipelineBuilder& addStage(const std::string& name, std::function<void(void*)> processor);
    PipelineBuilder& addStage(std::shared_ptr<PipelineStage> stage);
    PipelineBuilder& addSerializationStage(serialization::SerializerType type);
    PipelineBuilder& addCompressionStage();
    PipelineBuilder& addEncryptionStage(const std::string& key);
    PipelineBuilder& addValidationStage(std::function<bool(void*)> validator);
    
    std::shared_ptr<Pipeline> build();
    
private:
    std::shared_ptr<Pipeline> pipeline_;
};

class DataProcessor {
public:
    virtual ~DataProcessor() = default;
    virtual void process(void* data) = 0;
    virtual std::string getName() const = 0;
};

class SerializationProcessor : public DataProcessor {
public:
    SerializationProcessor(serialization::SerializerType type);
    void process(void* data) override;
    std::string getName() const override { return "SerializationProcessor"; }
    
private:
    serialization::SerializerType type_;
    std::shared_ptr<serialization::Serializer> serializer_;
};

class CompressionProcessor : public DataProcessor {
public:
    void process(void* data) override;
    std::string getName() const override { return "CompressionProcessor"; }
};

class EncryptionProcessor : public DataProcessor {
public:
    EncryptionProcessor(const std::string& key);
    void process(void* data) override;
    std::string getName() const override { return "EncryptionProcessor"; }
    
private:
    std::string key_;
};

class ValidationProcessor : public DataProcessor {
public:
    ValidationProcessor(std::function<bool(void*)> validator);
    void process(void* data) override;
    std::string getName() const override { return "ValidationProcessor"; }
    
private:
    std::function<bool(void*)> validator_;
};

} // namespace pipeline
} // namespace aurorart

#endif // AURORART_PIPELINE_H