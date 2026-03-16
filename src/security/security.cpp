#include "aurorart/security/security.h"
#include "aurorart/utils/logger.h"
#include "aurorart/utils/config.h"
#include <openssl/aes.h>
#include <openssl/rsa.h>
#include <openssl/evp.h>
#include <openssl/rand.h>
#include <openssl/pem.h>
#include <openssl/x509.h>
#include <vector>
#include <set>
#include <unordered_map>

namespace aurorart {
namespace security {

// SecurityFramework implementation

class DefaultSecurityFramework : public SecurityFramework {
public:
    bool init() override {
        AURORA_LOG_INFO("Security framework initialized");
        return true;
    }
    
    bool shutdown() override {
        AURORA_LOG_INFO("Security framework shutdown");
        return true;
    }
    
    SecurityLevel getSecurityLevel() const override {
        return securityLevel_;
    }
    
    void setSecurityLevel(SecurityLevel level) override {
        securityLevel_ = level;
        AURORA_LOG_INFO("Security level set to: {}", static_cast<int>(level));
    }
    
private:
    SecurityLevel securityLevel_ = SecurityLevel::MEDIUM;
};

// JWT算法枚举
enum class JWTAlgorithm {
    HS256, // HMAC-SHA256
    HS384, // HMAC-SHA384
    HS512, // HMAC-SHA512
    RS256, // RSA-SHA256
    RS384, // RSA-SHA384
    RS512  // RSA-SHA512
};

// JWT实现，支持多种算法
class JWTUtil {
public:
    static std::string generateToken(const std::string& identity, const std::string& secret, int expiresInSeconds, JWTAlgorithm algorithm = JWTAlgorithm::HS256) {
        try {
            // 创建JWT头部
            std::string header = R"({"alg":"HS256","typ":"JWT"})";
            // 创建JWT载荷
            time_t now = time(nullptr);
            time_t exp = now + expiresInSeconds;
            std::string payload = R"({"sub":")" + identity + R"(","iat":)" + std::to_string(now) + R"(","exp":)" + std::to_string(exp) + R"(","jti":")" + generateJTI() + R"(})";
            
            // Base64编码（URL安全）
            std::string encodedHeader = base64URLEncode(header);
            std::string encodedPayload = base64URLEncode(payload);
            
            // 创建签名
            std::string signatureInput = encodedHeader + "." + encodedPayload;
            std::string signature = generateSignature(signatureInput, secret, algorithm);
            std::string encodedSignature = base64URLEncode(signature);
            
            // 组合JWT
            return encodedHeader + "." + encodedPayload + "." + encodedSignature;
        } catch (const std::exception& e) {
            AURORA_LOG_ERROR("Failed to generate JWT token: {}", e.what());
            return "";
        }
    }
    
    static bool validateToken(const std::string& token, const std::string& secret, std::string& identity) {
        try {
            // 分割JWT
            size_t firstDot = token.find(".");
            size_t secondDot = token.find(".", firstDot + 1);
            if (firstDot == std::string::npos || secondDot == std::string::npos) {
                return false;
            }
            
            std::string encodedHeader = token.substr(0, firstDot);
            std::string encodedPayload = token.substr(firstDot + 1, secondDot - firstDot - 1);
            std::string encodedSignature = token.substr(secondDot + 1);
            
            // 验证签名
            std::string signatureInput = encodedHeader + "." + encodedPayload;
            std::string expectedSignature = generateSignature(signatureInput, secret, JWTAlgorithm::HS256);
            std::string expectedEncodedSignature = base64URLEncode(expectedSignature);
            
            if (encodedSignature != expectedEncodedSignature) {
                return false;
            }
            
            // 解码载荷
            std::string payload = base64URLDecode(encodedPayload);
            
            // 解析载荷
            size_t subPos = payload.find("\"sub\":\"");
            size_t expPos = payload.find("\"exp\":");
            if (subPos == std::string::npos || expPos == std::string::npos) {
                return false;
            }
            
            subPos += 7; // 跳过 "sub":"  
            size_t subEnd = payload.find("\"", subPos);
            if (subEnd == std::string::npos) {
                return false;
            }
            
            identity = payload.substr(subPos, subEnd - subPos);
            
            expPos += 6; // 跳过 "exp":
            size_t expEnd = payload.find(",", expPos);
            if (expEnd == std::string::npos) {
                expEnd = payload.find("}", expPos);
            }
            if (expEnd == std::string::npos) {
                return false;
            }
            
            std::string expStr = payload.substr(expPos, expEnd - expPos);
            time_t exp = std::stoll(expStr);
            
            // 检查过期时间
            return time(nullptr) <= exp;
        } catch (...) {
            return false;
        }
    }
    
private:
    static std::string generateJTI() {
        // 生成唯一标识符
        unsigned char buffer[16];
        if (RAND_bytes(buffer, sizeof(buffer)) == 1) {
            std::string jti;
            for (size_t i = 0; i < sizeof(buffer); i++) {
                char hex[3];
                sprintf(hex, "%02x", buffer[i]);
                jti += hex;
            }
            return jti;
        }
        return std::to_string(time(nullptr)) + std::to_string(rand());
    }
    
    static std::string base64URLEncode(const std::string& input) {
        static const std::string base64_chars = 
            "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789-_";
        
        std::string result;
        int i = 0;
        unsigned char char_array_3[3];
        unsigned char char_array_4[4];
        
        for (size_t n = 0; n < input.length(); n++) {
            char_array_3[i++] = input[n];
            if (i == 3) {
                char_array_4[0] = (char_array_3[0] & 0xfc) >> 2;
                char_array_4[1] = ((char_array_3[0] & 0x03) << 4) + ((char_array_3[1] & 0xf0) >> 4);
                char_array_4[2] = ((char_array_3[1] & 0x0f) << 2) + ((char_array_3[2] & 0xc0) >> 6);
                char_array_4[3] = char_array_3[2] & 0x3f;
                
                for (i = 0; i < 4; i++)
                    result += base64_chars[char_array_4[i]];
                i = 0;
            }
        }
        
        if (i > 0) {
            for (int j = i; j < 3; j++)
                char_array_3[j] = '\0';
            
            char_array_4[0] = (char_array_3[0] & 0xfc) >> 2;
            char_array_4[1] = ((char_array_3[0] & 0x03) << 4) + ((char_array_3[1] & 0xf0) >> 4);
            char_array_4[2] = ((char_array_3[1] & 0x0f) << 2) + ((char_array_3[2] & 0xc0) >> 6);
            char_array_4[3] = char_array_3[2] & 0x3f;
            
            for (int j = 0; j < i + 1; j++)
                result += base64_chars[char_array_4[j]];
        }
        
        return result;
    }
    
    static std::string base64URLDecode(const std::string& input) {
        static const std::string base64_chars = 
            "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789-_";
        
        std::string result;
        int i = 0;
        unsigned char char_array_4[4];
        unsigned char char_array_3[3];
        
        for (size_t n = 0; n < input.length(); n++) {
            if (base64_chars.find(input[n]) == std::string::npos) continue;
            
            char_array_4[i++] = input[n];
            if (i == 4) {
                for (i = 0; i < 4; i++)
                    char_array_4[i] = base64_chars.find(char_array_4[i]);
                
                char_array_3[0] = (char_array_4[0] << 2) + ((char_array_4[1] & 0x30) >> 4);
                char_array_3[1] = ((char_array_4[1] & 0xf) << 4) + ((char_array_4[2] & 0x3c) >> 2);
                char_array_3[2] = ((char_array_4[2] & 0x3) << 6) + char_array_4[3];
                
                for (i = 0; i < 3; i++)
                    result += char_array_3[i];
                i = 0;
            }
        }
        
        if (i > 0) {
            for (int j = i; j < 4; j++)
                char_array_4[j] = 0;
            
            for (int j = 0; j < 4; j++)
                char_array_4[j] = base64_chars.find(char_array_4[j]);
            
            char_array_3[0] = (char_array_4[0] << 2) + ((char_array_4[1] & 0x30) >> 4);
            char_array_3[1] = ((char_array_4[1] & 0xf) << 4) + ((char_array_4[2] & 0x3c) >> 2);
            char_array_3[2] = ((char_array_4[2] & 0x3) << 6) + char_array_4[3];
            
            for (int j = 0; j < i - 1; j++)
                result += char_array_3[j];
        }
        
        return result;
    }
    
    static std::string generateSignature(const std::string& message, const std::string& secret, JWTAlgorithm algorithm) {
        unsigned char digest[EVP_MAX_MD_SIZE];
        unsigned int digestLen;
        
        const EVP_MD* md = nullptr;
        switch (algorithm) {
            case JWTAlgorithm::HS256: md = EVP_sha256(); break;
            case JWTAlgorithm::HS384: md = EVP_sha384(); break;
            case JWTAlgorithm::HS512: md = EVP_sha512(); break;
            case JWTAlgorithm::RS256: md = EVP_sha256(); break;
            case JWTAlgorithm::RS384: md = EVP_sha384(); break;
            case JWTAlgorithm::RS512: md = EVP_sha512(); break;
            default: md = EVP_sha256(); break;
        }
        
        HMAC(md, secret.c_str(), secret.length(), 
             reinterpret_cast<const unsigned char*>(message.c_str()), message.length(), 
             digest, &digestLen);
        
        return std::string(reinterpret_cast<const char*>(digest), digestLen);
    }
};

// Authentication implementation

class DefaultAuthentication : public Authentication {
public:
    DefaultAuthentication() {
        // 生成随机密钥
        generateSecret();
    }
    
    AuthenticationStatus authenticate(const std::string& identity, const std::string& credentials, 
                                   AuthenticationMethod method) override {
        std::lock_guard<std::mutex> lock(mutex_);
        
        switch (method) {
            case AuthenticationMethod::PASSWORD:
                return authenticatePassword(identity, credentials);
            case AuthenticationMethod::TOKEN:
                return authenticateToken(identity, credentials);
            case AuthenticationMethod::X509_CERTIFICATE:
                return authenticateX509(identity, credentials);
            case AuthenticationMethod::OAUTH2:
                return authenticateOAuth2(identity, credentials);
            case AuthenticationMethod::UNIX_CREDENTIALS:
                return authenticateUnixCredentials(identity, credentials);
            default:
                AURORA_LOG_WARN("Unknown authentication method: {}", static_cast<int>(method));
                return AuthenticationStatus::FAILED;
        }
    }
    
    bool registerIdentity(const std::string& identity, const std::string& credentials, 
                       AuthenticationMethod method) override {
        std::lock_guard<std::mutex> lock(mutex_);
        
        if (identities_.find(identity) != identities_.end()) {
            AURORA_LOG_WARN("Identity already registered: {}", identity);
            return false;
        }
        
        switch (method) {
            case AuthenticationMethod::PASSWORD:
                return registerPasswordIdentity(identity, credentials);
            case AuthenticationMethod::X509_CERTIFICATE:
                return registerX509Identity(identity, credentials);
            case AuthenticationMethod::UNIX_CREDENTIALS:
                return registerUnixIdentity(identity, credentials);
            default:
                AURORA_LOG_WARN("Unsupported registration method: {}", static_cast<int>(method));
                return false;
        }
    }
    
    AuthenticationStatus authenticatePassword(const std::string& identity, const std::string& password) {
        auto it = identities_.find(identity);
        if (it != identities_.end() && verifyPassword(password, it->second)) {
            AURORA_LOG_INFO("Password authentication successful for identity: {}", identity);
            return AuthenticationStatus::SUCCESS;
        }
        AURORA_LOG_WARN("Password authentication failed for identity: {}", identity);
        return AuthenticationStatus::FAILED;
    }
    
    AuthenticationStatus authenticateToken(const std::string& identity, const std::string& token) {
        std::string tokenIdentity;
        if (JWTUtil::validateToken(token, secret_, tokenIdentity) && tokenIdentity == identity) {
            AURORA_LOG_INFO("Token authentication successful for identity: {}", identity);
            return AuthenticationStatus::SUCCESS;
        }
        AURORA_LOG_WARN("Token authentication failed for identity: {}", identity);
        return AuthenticationStatus::FAILED;
    }
    
    AuthenticationStatus authenticateX509(const std::string& identity, const std::string& certificate) {
        // 简单的X509证书验证（仅用于演示）
        AURORA_LOG_INFO("X509 certificate authentication for identity: {}", identity);
        // 实际实现中，应该解析和验证X509证书
        return AuthenticationStatus::SUCCESS;
    }
    
    AuthenticationStatus authenticateOAuth2(const std::string& identity, const std::string& token) {
        // 简单的OAuth2 token验证（仅用于演示）
        AURORA_LOG_INFO("OAuth2 authentication for identity: {}", identity);
        // 实际实现中，应该验证OAuth2 token的有效性
        return AuthenticationStatus::SUCCESS;
    }
    
    AuthenticationStatus authenticateUnixCredentials(const std::string& identity, const std::string& credentials) {
        // 简单的UNIX凭证验证（仅用于演示）
        AURORA_LOG_INFO("UNIX credentials authentication for identity: {}", identity);
        // 实际实现中，应该验证UNIX UID/GID
        return AuthenticationStatus::SUCCESS;
    }
    
    bool registerPasswordIdentity(const std::string& identity, const std::string& password) {
        std::string hashedPassword = hashPassword(password);
        identities_[identity] = hashedPassword;
        AURORA_LOG_INFO("Registered password identity: {}", identity);
        return true;
    }
    
    bool registerX509Identity(const std::string& identity, const std::string& certificate) {
        identities_[identity] = certificate; // 存储证书（实际实现中应该存储证书的指纹或公钥）
        AURORA_LOG_INFO("Registered X509 identity: {}", identity);
        return true;
    }
    
    bool registerUnixIdentity(const std::string& identity, const std::string& credentials) {
        identities_[identity] = credentials; // 存储UNIX凭证信息
        AURORA_LOG_INFO("Registered UNIX identity: {}", identity);
        return true;
    }
    
    bool unregisterIdentity(const std::string& identity) override {
        std::lock_guard<std::mutex> lock(mutex_);
        
        auto it = identities_.find(identity);
        if (it == identities_.end()) {
            AURORA_LOG_WARN("Identity not found: {}", identity);
            return false;
        }
        
        identities_.erase(it);
        AURORA_LOG_INFO("Unregistered identity: {}", identity);
        return true;
    }
    
    bool isIdentityRegistered(const std::string& identity) const override {
        std::lock_guard<std::mutex> lock(mutex_);
        return identities_.find(identity) != identities_.end();
    }
    
    std::string generateToken(const std::string& identity, int expiresInSeconds = 3600) override {
        std::lock_guard<std::mutex> lock(mutex_);
        
        if (identities_.find(identity) == identities_.end()) {
            AURORA_LOG_WARN("Identity not registered: {}", identity);
            return "";
        }
        
        std::string token = JWTUtil::generateToken(identity, secret_, expiresInSeconds);
        AURORA_LOG_INFO("Generated token for identity: {}", identity);
        return token;
    }
    
private:
    void generateSecret() {
        // 生成随机密钥
        unsigned char buffer[32];
        if (RAND_bytes(buffer, sizeof(buffer)) == 1) {
            secret_ = std::string(reinterpret_cast<const char*>(buffer), sizeof(buffer));
        } else {
            // 使用默认密钥（仅用于演示）
            secret_ = "aurorart_secret_key_2026";
            AURORA_LOG_WARN("Failed to generate random secret, using default");
        }
    }
    
    std::string hashPassword(const std::string& password) {
        // 使用SHA-256哈希密码
        unsigned char digest[EVP_MAX_MD_SIZE];
        unsigned int digestLen;
        
        EVP_MD_CTX* ctx = EVP_MD_CTX_new();
        if (ctx) {
            EVP_DigestInit_ex(ctx, EVP_sha256(), nullptr);
            EVP_DigestUpdate(ctx, password.c_str(), password.length());
            EVP_DigestFinal_ex(ctx, digest, &digestLen);
            EVP_MD_CTX_free(ctx);
            
            // 转换为十六进制字符串
            std::string result;
            for (unsigned int i = 0; i < digestLen; i++) {
                char hex[3];
                sprintf(hex, "%02x", digest[i]);
                result += hex;
            }
            return result;
        }
        return password; // 失败时返回原密码（仅用于演示）
    }
    
    bool verifyPassword(const std::string& password, const std::string& hashedPassword) {
        std::string hash = hashPassword(password);
        return hash == hashedPassword;
    }
    
    std::map<std::string, std::string> identities_;
    std::string secret_;
    mutable std::mutex mutex_;
};

// Encryption implementation

class DefaultEncryption : public Encryption {
public:
    DefaultEncryption() : algorithm_(EncryptionAlgorithm::AES_256_GCM) {
        // 初始化OpenSSL
        OpenSSL_add_all_algorithms();
        // 生成随机密钥
        generateKey();
    }
    
    bool encrypt(const void* data, size_t size, std::vector<uint8_t>& encryptedData) override {
        try {
            // 获取对应的加密算法
            const EVP_CIPHER* cipher = getCipher();
            if (!cipher) {
                AURORA_LOG_ERROR("Invalid encryption algorithm");
                return false;
            }
            
            EVP_CIPHER_CTX* ctx = EVP_CIPHER_CTX_new();
            if (!ctx) {
                AURORA_LOG_ERROR("Failed to create cipher context");
                return false;
            }
            
            // 生成随机IV
            int ivLen = EVP_CIPHER_iv_length(cipher);
            uint8_t* iv = new uint8_t[ivLen];
            if (RAND_bytes(iv, ivLen) != 1) {
                AURORA_LOG_ERROR("Failed to generate IV");
                EVP_CIPHER_CTX_free(ctx);
                delete[] iv;
                return false;
            }
            
            // 初始化加密
            if (EVP_EncryptInit_ex(ctx, cipher, nullptr, key_, iv) != 1) {
                AURORA_LOG_ERROR("Failed to initialize encryption");
                EVP_CIPHER_CTX_free(ctx);
                delete[] iv;
                return false;
            }
            
            // 计算加密后的数据大小
            int blockSize = EVP_CIPHER_block_size(cipher);
            int out_len = size + blockSize;
            encryptedData.resize(ivLen + out_len);
            
            // 复制IV到输出数据
            memcpy(encryptedData.data(), iv, ivLen);
            delete[] iv;
            
            // 执行加密
            int len;
            if (EVP_EncryptUpdate(ctx, encryptedData.data() + ivLen, &len, 
                                 static_cast<const uint8_t*>(data), size) != 1) {
                AURORA_LOG_ERROR("Failed to encrypt data");
                EVP_CIPHER_CTX_free(ctx);
                return false;
            }
            
            // 完成加密
            int final_len;
            if (EVP_EncryptFinal_ex(ctx, encryptedData.data() + ivLen + len, &final_len) != 1) {
                AURORA_LOG_ERROR("Failed to finalize encryption");
                EVP_CIPHER_CTX_free(ctx);
                return false;
            }
            
            // 调整输出大小
            encryptedData.resize(ivLen + len + final_len);
            
            EVP_CIPHER_CTX_free(ctx);
            AURORA_LOG_DEBUG("Encrypted {} bytes to {} bytes using {}", size, encryptedData.size(), getAlgorithmName());
            return true;
        } catch (const std::exception& e) {
            AURORA_LOG_ERROR("Encryption failed: {}", e.what());
            return false;
        }
    }
    
    bool decrypt(const void* data, size_t size, std::vector<uint8_t>& decryptedData) override {
        try {
            // 获取对应的加密算法
            const EVP_CIPHER* cipher = getCipher();
            if (!cipher) {
                AURORA_LOG_ERROR("Invalid encryption algorithm");
                return false;
            }
            
            int ivLen = EVP_CIPHER_iv_length(cipher);
            if (size < ivLen) {
                AURORA_LOG_ERROR("Invalid encrypted data size");
                return false;
            }
            
            EVP_CIPHER_CTX* ctx = EVP_CIPHER_CTX_new();
            if (!ctx) {
                AURORA_LOG_ERROR("Failed to create cipher context");
                return false;
            }
            
            // 提取IV
            uint8_t* iv = new uint8_t[ivLen];
            memcpy(iv, data, ivLen);
            
            // 初始化解密
            if (EVP_DecryptInit_ex(ctx, cipher, nullptr, key_, iv) != 1) {
                AURORA_LOG_ERROR("Failed to initialize decryption");
                EVP_CIPHER_CTX_free(ctx);
                delete[] iv;
                return false;
            }
            delete[] iv;
            
            // 计算解密后的数据大小
            int out_len = size - ivLen;
            decryptedData.resize(out_len);
            
            // 执行解密
            int len;
            if (EVP_DecryptUpdate(ctx, decryptedData.data(), &len, 
                                 static_cast<const uint8_t*>(data) + ivLen, out_len) != 1) {
                AURORA_LOG_ERROR("Failed to decrypt data");
                EVP_CIPHER_CTX_free(ctx);
                return false;
            }
            
            // 完成解密
            int final_len;
            if (EVP_DecryptFinal_ex(ctx, decryptedData.data() + len, &final_len) != 1) {
                AURORA_LOG_ERROR("Failed to finalize decryption");
                EVP_CIPHER_CTX_free(ctx);
                return false;
            }
            
            // 调整输出大小
            decryptedData.resize(len + final_len);
            
            EVP_CIPHER_CTX_free(ctx);
            AURORA_LOG_DEBUG("Decrypted {} bytes to {} bytes using {}", size, decryptedData.size(), getAlgorithmName());
            return true;
        } catch (const std::exception& e) {
            AURORA_LOG_ERROR("Decryption failed: {}", e.what());
            return false;
        }
    }
    
    void setAlgorithm(EncryptionAlgorithm algorithm) override {
        algorithm_ = algorithm;
        // 根据算法重新生成密钥
        generateKey();
        AURORA_LOG_INFO("Encryption algorithm set to: {}", getAlgorithmName());
    }
    
    EncryptionAlgorithm getAlgorithm() const override {
        return algorithm_;
    }
    
    // 设置自定义密钥
    bool setKey(const void* key, size_t keySize) {
        if (keySize != sizeof(key_)) {
            AURORA_LOG_ERROR("Invalid key size: expected {}, got {}", sizeof(key_), keySize);
            return false;
        }
        memcpy(key_, key, keySize);
        AURORA_LOG_INFO("Custom encryption key set");
        return true;
    }
    
    // 获取密钥大小
    size_t getKeySize() const {
        return sizeof(key_);
    }
    
private:
    const EVP_CIPHER* getCipher() const {
        switch (algorithm_) {
            case EncryptionAlgorithm::AES_128_CBC:
                return EVP_aes_128_cbc();
            case EncryptionAlgorithm::AES_192_CBC:
                return EVP_aes_192_cbc();
            case EncryptionAlgorithm::AES_256_CBC:
                return EVP_aes_256_cbc();
            case EncryptionAlgorithm::AES_128_GCM:
                return EVP_aes_128_gcm();
            case EncryptionAlgorithm::AES_256_GCM:
                return EVP_aes_256_gcm();
            default:
                return EVP_aes_256_gcm();
        }
    }
    
    std::string getAlgorithmName() const {
        switch (algorithm_) {
            case EncryptionAlgorithm::AES_128_CBC:
                return "AES-128-CBC";
            case EncryptionAlgorithm::AES_192_CBC:
                return "AES-192-CBC";
            case EncryptionAlgorithm::AES_256_CBC:
                return "AES-256-CBC";
            case EncryptionAlgorithm::AES_128_GCM:
                return "AES-128-GCM";
            case EncryptionAlgorithm::AES_256_GCM:
                return "AES-256-GCM";
            case EncryptionAlgorithm::RSA_2048:
                return "RSA-2048";
            case EncryptionAlgorithm::RSA_4096:
                return "RSA-4096";
            case EncryptionAlgorithm::ECC_P256:
                return "ECC-P256";
            case EncryptionAlgorithm::ECC_P384:
                return "ECC-P384";
            default:
                return "Unknown";
        }
    }
    
    void generateKey() {
        // 根据算法生成合适大小的密钥
        size_t keySize = sizeof(key_);
        switch (algorithm_) {
            case EncryptionAlgorithm::AES_128_CBC:
            case EncryptionAlgorithm::AES_128_GCM:
                keySize = 16;
                break;
            case EncryptionAlgorithm::AES_192_CBC:
                keySize = 24;
                break;
            case EncryptionAlgorithm::AES_256_CBC:
            case EncryptionAlgorithm::AES_256_GCM:
                keySize = 32;
                break;
            default:
                keySize = 32;
                break;
        }
        
        // 生成随机密钥
        if (RAND_bytes(key_, keySize) != 1) {
            AURORA_LOG_ERROR("Failed to generate encryption key");
            // 使用默认密钥（仅用于演示）
            memset(key_, 0x55, sizeof(key_));
        }
    }
    
    EncryptionAlgorithm algorithm_;
    uint8_t key_[32]; // 最大密钥大小（AES-256）
};

// E2EProtection implementation

class DefaultE2EProtection : public E2EProtection {
public:
    DefaultE2EProtection() : securityLevel_(SecurityLevel::MEDIUM) {
        // 初始化OpenSSL
        OpenSSL_add_all_algorithms();
        // 生成默认密钥
        generateKeys();
    }
    
    bool protect(const void* data, size_t size, std::vector<uint8_t>& protectedData, 
                const std::string& senderId, const std::string& receiverId) override {
        try {
            // 获取发送者和接收者的密钥
            std::vector<uint8_t> key = getKeyForPair(senderId, receiverId);
            if (key.empty()) {
                AURORA_LOG_ERROR("Failed to get key for sender {} and receiver {}", senderId, receiverId);
                return false;
            }
            
            // 使用AES-256-GCM进行加密和认证
            const EVP_CIPHER* cipher = EVP_aes_256_gcm();
            EVP_CIPHER_CTX* ctx = EVP_CIPHER_CTX_new();
            if (!ctx) {
                AURORA_LOG_ERROR("Failed to create cipher context");
                return false;
            }
            
            // 生成随机IV
            int ivLen = EVP_CIPHER_iv_length(cipher);
            uint8_t* iv = new uint8_t[ivLen];
            if (RAND_bytes(iv, ivLen) != 1) {
                AURORA_LOG_ERROR("Failed to generate IV");
                EVP_CIPHER_CTX_free(ctx);
                delete[] iv;
                return false;
            }
            
            // 初始化加密
            if (EVP_EncryptInit_ex(ctx, cipher, nullptr, key.data(), iv) != 1) {
                AURORA_LOG_ERROR("Failed to initialize encryption");
                EVP_CIPHER_CTX_free(ctx);
                delete[] iv;
                return false;
            }
            
            // 添加附加认证数据（AAD）
            std::string aad = senderId + ":" + receiverId;
            if (EVP_EncryptUpdate(ctx, nullptr, nullptr, 
                                 reinterpret_cast<const uint8_t*>(aad.c_str()), aad.length()) != 1) {
                AURORA_LOG_ERROR("Failed to add AAD");
                EVP_CIPHER_CTX_free(ctx);
                delete[] iv;
                return false;
            }
            
            // 计算加密后的数据大小
            int blockSize = EVP_CIPHER_block_size(cipher);
            int out_len = size + blockSize;
            protectedData.resize(ivLen + out_len + EVP_GCM_TLS_TAG_LENGTH);
            
            // 复制IV到输出数据
            memcpy(protectedData.data(), iv, ivLen);
            delete[] iv;
            
            // 执行加密
            int len;
            if (EVP_EncryptUpdate(ctx, protectedData.data() + ivLen, &len, 
                                 static_cast<const uint8_t*>(data), size) != 1) {
                AURORA_LOG_ERROR("Failed to encrypt data");
                EVP_CIPHER_CTX_free(ctx);
                return false;
            }
            
            // 完成加密
            int final_len;
            if (EVP_EncryptFinal_ex(ctx, protectedData.data() + ivLen + len, &final_len) != 1) {
                AURORA_LOG_ERROR("Failed to finalize encryption");
                EVP_CIPHER_CTX_free(ctx);
                return false;
            }
            
            // 获取认证标签
            int tagLen = EVP_GCM_TLS_TAG_LENGTH;
            if (EVP_CIPHER_CTX_ctrl(ctx, EVP_CTRL_GCM_GET_TAG, tagLen, 
                                  protectedData.data() + ivLen + len + final_len) != 1) {
                AURORA_LOG_ERROR("Failed to get authentication tag");
                EVP_CIPHER_CTX_free(ctx);
                return false;
            }
            
            // 调整输出大小
            protectedData.resize(ivLen + len + final_len + tagLen);
            
            EVP_CIPHER_CTX_free(ctx);
            AURORA_LOG_DEBUG("Protected {} bytes to {} bytes for {} -> {}", 
                           size, protectedData.size(), senderId, receiverId);
            return true;
        } catch (const std::exception& e) {
            AURORA_LOG_ERROR("E2E protection failed: {}", e.what());
            return false;
        }
    }
    
    bool unprotect(const void* data, size_t size, std::vector<uint8_t>& unprotectedData, 
                  const std::string& senderId, const std::string& receiverId) override {
        try {
            // 获取发送者和接收者的密钥
            std::vector<uint8_t> key = getKeyForPair(senderId, receiverId);
            if (key.empty()) {
                AURORA_LOG_ERROR("Failed to get key for sender {} and receiver {}", senderId, receiverId);
                return false;
            }
            
            // 使用AES-256-GCM进行解密和认证
            const EVP_CIPHER* cipher = EVP_aes_256_gcm();
            EVP_CIPHER_CTX* ctx = EVP_CIPHER_CTX_new();
            if (!ctx) {
                AURORA_LOG_ERROR("Failed to create cipher context");
                return false;
            }
            
            int ivLen = EVP_CIPHER_iv_length(cipher);
            int tagLen = EVP_GCM_TLS_TAG_LENGTH;
            if (size < ivLen + tagLen) {
                AURORA_LOG_ERROR("Invalid protected data size");
                EVP_CIPHER_CTX_free(ctx);
                return false;
            }
            
            // 提取IV和标签
            uint8_t* iv = const_cast<uint8_t*>(static_cast<const uint8_t*>(data));
            uint8_t* tag = const_cast<uint8_t*>(static_cast<const uint8_t*>(data)) + size - tagLen;
            
            // 初始化解密
            if (EVP_DecryptInit_ex(ctx, cipher, nullptr, key.data(), iv) != 1) {
                AURORA_LOG_ERROR("Failed to initialize decryption");
                EVP_CIPHER_CTX_free(ctx);
                return false;
            }
            
            // 添加附加认证数据（AAD）
            std::string aad = senderId + ":" + receiverId;
            if (EVP_DecryptUpdate(ctx, nullptr, nullptr, 
                                 reinterpret_cast<const uint8_t*>(aad.c_str()), aad.length()) != 1) {
                AURORA_LOG_ERROR("Failed to add AAD");
                EVP_CIPHER_CTX_free(ctx);
                return false;
            }
            
            // 计算解密后的数据大小
            int out_len = size - ivLen - tagLen;
            unprotectedData.resize(out_len);
            
            // 执行解密
            int len;
            if (EVP_DecryptUpdate(ctx, unprotectedData.data(), &len, 
                                 static_cast<const uint8_t*>(data) + ivLen, out_len) != 1) {
                AURORA_LOG_ERROR("Failed to decrypt data");
                EVP_CIPHER_CTX_free(ctx);
                return false;
            }
            
            // 设置认证标签
            if (EVP_CIPHER_CTX_ctrl(ctx, EVP_CTRL_GCM_SET_TAG, tagLen, tag) != 1) {
                AURORA_LOG_ERROR("Failed to set authentication tag");
                EVP_CIPHER_CTX_free(ctx);
                return false;
            }
            
            // 完成解密（验证标签）
            int final_len;
            if (EVP_DecryptFinal_ex(ctx, unprotectedData.data() + len, &final_len) != 1) {
                AURORA_LOG_ERROR("Failed to finalize decryption (tag verification failed)");
                EVP_CIPHER_CTX_free(ctx);
                return false;
            }
            
            // 调整输出大小
            unprotectedData.resize(len + final_len);
            
            EVP_CIPHER_CTX_free(ctx);
            AURORA_LOG_DEBUG("Unprotected {} bytes to {} bytes for {} -> {}", 
                           size, unprotectedData.size(), senderId, receiverId);
            return true;
        } catch (const std::exception& e) {
            AURORA_LOG_ERROR("E2E unprotection failed: {}", e.what());
            return false;
        }
    }
    
    void setSecurityLevel(SecurityLevel level) override {
        securityLevel_ = level;
        AURORA_LOG_INFO("E2E protection security level set to: {}", static_cast<int>(level));
        // 根据安全级别重新生成密钥
        if (level >= SecurityLevel::HIGH) {
            generateKeys();
        }
    }
    
    SecurityLevel getSecurityLevel() const override {
        return securityLevel_;
    }
    
private:
    std::vector<uint8_t> getKeyForPair(const std::string& senderId, const std::string& receiverId) {
        std::lock_guard<std::mutex> lock(mutex_);
        
        std::string keyId = senderId + "-" + receiverId;
        auto it = keys_.find(keyId);
        if (it != keys_.end()) {
            return it->second;
        }
        
        // 如果没有找到密钥，生成一个新的
        std::vector<uint8_t> key(32); // AES-256密钥
        if (RAND_bytes(key.data(), key.size()) == 1) {
            keys_[keyId] = key;
            return key;
        }
        
        // 使用默认密钥（仅用于演示）
        return defaultKey_;
    }
    
    void generateKeys() {
        // 生成默认密钥
        defaultKey_.resize(32);
        if (RAND_bytes(defaultKey_.data(), defaultKey_.size()) != 1) {
            AURORA_LOG_ERROR("Failed to generate default E2E key");
            // 使用固定默认值（仅用于演示）
            memset(defaultKey_.data(), 0x55, defaultKey_.size());
        }
        
        // 清空现有密钥
        keys_.clear();
        AURORA_LOG_INFO("E2E protection keys regenerated");
    }
    
    SecurityLevel securityLevel_;
    std::vector<uint8_t> defaultKey_;
    std::unordered_map<std::string, std::vector<uint8_t>> keys_; // 存储发送者-接收者对的密钥
    mutable std::mutex mutex_;
};

// AccessControl implementation

class DefaultAccessControl : public AccessControl {
public:
    bool checkAccess(const std::string& identity, const std::string& resource) override {
        std::lock_guard<std::mutex> lock(mutex_);
        
        // 检查是否有通配符权限
        if (hasWildcardAccess(identity)) {
            AURORA_LOG_DEBUG("Identity {} has wildcard access to resource {}", identity, resource);
            return true;
        }
        
        // 检查具体资源权限
        auto it = accessMap_.find(identity);
        if (it != accessMap_.end()) {
            if (it->second.find(resource) != it->second.end()) {
                AURORA_LOG_DEBUG("Identity {} has access to resource {}", identity, resource);
                return true;
            }
        }
        
        AURORA_LOG_DEBUG("Identity {} denied access to resource {}", identity, resource);
        return false;
    }
    
    bool grantAccess(const std::string& identity, const std::string& resource) override {
        std::lock_guard<std::mutex> lock(mutex_);
        
        accessMap_[identity].insert(resource);
        AURORA_LOG_INFO("Granted access to resource {} for identity {}", resource, identity);
        
        // 记录审计日志
        logAccessChange("GRANT", identity, resource);
        return true;
    }
    
    bool revokeAccess(const std::string& identity, const std::string& resource) override {
        std::lock_guard<std::mutex> lock(mutex_);
        
        auto it = accessMap_.find(identity);
        if (it != accessMap_.end()) {
            it->second.erase(resource);
            AURORA_LOG_INFO("Revoked access to resource {} for identity {}", resource, identity);
            
            // 记录审计日志
            logAccessChange("REVOKE", identity, resource);
            return true;
        }
        
        return false;
    }
    
    std::vector<std::string> getResourcesForIdentity(const std::string& identity) const override {
        std::lock_guard<std::mutex> lock(mutex_);
        
        std::vector<std::string> resources;
        auto it = accessMap_.find(identity);
        if (it != accessMap_.end()) {
            resources.assign(it->second.begin(), it->second.end());
        }
        return resources;
    }
    
    // 批量授予权限
    bool grantAccessBatch(const std::string& identity, const std::vector<std::string>& resources) {
        std::lock_guard<std::mutex> lock(mutex_);
        
        for (const auto& resource : resources) {
            accessMap_[identity].insert(resource);
            logAccessChange("GRANT", identity, resource);
        }
        
        AURORA_LOG_INFO("Granted access to {} resources for identity {}", resources.size(), identity);
        return true;
    }
    
    // 批量撤销权限
    bool revokeAccessBatch(const std::string& identity, const std::vector<std::string>& resources) {
        std::lock_guard<std::mutex> lock(mutex_);
        
        auto it = accessMap_.find(identity);
        if (it != accessMap_.end()) {
            for (const auto& resource : resources) {
                it->second.erase(resource);
                logAccessChange("REVOKE", identity, resource);
            }
            
            AURORA_LOG_INFO("Revoked access to {} resources for identity {}", resources.size(), identity);
            return true;
        }
        
        return false;
    }
    
    // 检查是否有通配符权限
    bool hasWildcardAccess(const std::string& identity) const {
        auto it = accessMap_.find(identity);
        if (it != accessMap_.end()) {
            return it->second.find("*") != it->second.end() || it->second.find("all") != it->second.end();
        }
        return false;
    }
    
    // 清空所有权限
    void clearAccess(const std::string& identity) {
        std::lock_guard<std::mutex> lock(mutex_);
        
        auto it = accessMap_.find(identity);
        if (it != accessMap_.end()) {
            accessMap_.erase(it);
            AURORA_LOG_INFO("Cleared all access for identity {}", identity);
            logAccessChange("CLEAR", identity, "all");
        }
    }
    
    // 获取所有身份
    std::vector<std::string> getIdentities() const {
        std::lock_guard<std::mutex> lock(mutex_);
        
        std::vector<std::string> identities;
        for (const auto& entry : accessMap_) {
            identities.push_back(entry.first);
        }
        return identities;
    }
    
private:
    // 记录访问变更日志
    void logAccessChange(const std::string& action, const std::string& identity, const std::string& resource) {
        // 这里可以扩展为将审计日志写入文件或数据库
        AURORA_LOG_DEBUG("Access {}: identity={}, resource={}", action, identity, resource);
    }
    
    std::map<std::string, std::set<std::string>> accessMap_;
    mutable std::mutex mutex_;
};

// SecurityManager implementation

SecurityManager::SecurityManager() : initialized_(false) {
}

SecurityManager& SecurityManager::instance() {
    static SecurityManager instance;
    return instance;
}

bool SecurityManager::init() {
    std::lock_guard<std::mutex> lock(mutex_);
    
    if (initialized_) {
        AURORA_LOG_WARN("Security manager is already initialized");
        return false;
    }
    
    securityFramework_ = std::make_unique<DefaultSecurityFramework>();
    authentication_ = std::make_unique<DefaultAuthentication>();
    encryption_ = std::make_unique<DefaultEncryption>();
    e2eProtection_ = std::make_unique<DefaultE2EProtection>();
    accessControl_ = std::make_unique<DefaultAccessControl>();
    
    securityFramework_->init();
    
    // 注册默认身份
    authentication_->registerIdentity("default_node", "default_credential");
    
    // 授予默认访问权限
    accessControl_->grantAccess("default_node", "all");
    
    initialized_ = true;
    AURORA_LOG_INFO("Security manager initialized");
    return true;
}

bool SecurityManager::shutdown() {
    std::lock_guard<std::mutex> lock(mutex_);
    
    if (!initialized_) {
        AURORA_LOG_WARN("Security manager is not initialized");
        return false;
    }
    
    securityFramework_->shutdown();
    
    securityFramework_.reset();
    authentication_.reset();
    encryption_.reset();
    e2eProtection_.reset();
    accessControl_.reset();
    
    initialized_ = false;
    AURORA_LOG_INFO("Security manager shutdown");
    return true;
}

SecurityFramework* SecurityManager::getSecurityFramework() {
    return securityFramework_.get();
}

Authentication* SecurityManager::getAuthentication() {
    return authentication_.get();
}

Encryption* SecurityManager::getEncryption() {
    return encryption_.get();
}

E2EProtection* SecurityManager::getE2EProtection() {
    return e2eProtection_.get();
}

AccessControl* SecurityManager::getAccessControl() {
    return accessControl_.get();
}

bool SecurityManager::authenticateNode(const std::string& nodeId, const std::string& credentials, 
                                     AuthenticationMethod method) {
    return authentication_->authenticate(nodeId, credentials, method) == AuthenticationStatus::SUCCESS;
}

std::string SecurityManager::generateNodeToken(const std::string& nodeId, int expiresInSeconds) {
    DefaultAuthentication* auth = dynamic_cast<DefaultAuthentication*>(authentication_.get());
    if (auth) {
        return auth->generateToken(nodeId, expiresInSeconds);
    }
    return "";
}

bool SecurityManager::encryptData(const void* data, size_t size, std::vector<uint8_t>& encryptedData) {
    return encryption_->encrypt(data, size, encryptedData);
}

bool SecurityManager::decryptData(const void* data, size_t size, std::vector<uint8_t>& decryptedData) {
    return encryption_->decrypt(data, size, decryptedData);
}

bool SecurityManager::protectData(const void* data, size_t size, std::vector<uint8_t>& protectedData, 
                                 const std::string& senderId, const std::string& receiverId) {
    return e2eProtection_->protect(data, size, protectedData, senderId, receiverId);
}

bool SecurityManager::unprotectData(const void* data, size_t size, std::vector<uint8_t>& unprotectedData, 
                                   const std::string& senderId, const std::string& receiverId) {
    return e2eProtection_->unprotect(data, size, unprotectedData, senderId, receiverId);
}

bool SecurityManager::setEncryptionKey(const void* key, size_t keySize) {
    DefaultEncryption* enc = dynamic_cast<DefaultEncryption*>(encryption_.get());
    if (enc) {
        return enc->setKey(key, keySize);
    }
    return false;
}

bool SecurityManager::checkNodeAccess(const std::string& nodeId, const std::string& resource) {
    return accessControl_->checkAccess(nodeId, resource);
}

bool SecurityManager::grantNodeAccess(const std::string& nodeId, const std::string& resource) {
    return accessControl_->grantAccess(nodeId, resource);
}

bool SecurityManager::revokeNodeAccess(const std::string& nodeId, const std::string& resource) {
    return accessControl_->revokeAccess(nodeId, resource);
}

bool SecurityManager::grantNodeAccessBatch(const std::string& nodeId, const std::vector<std::string>& resources) {
    DefaultAccessControl* ac = dynamic_cast<DefaultAccessControl*>(accessControl_.get());
    if (ac) {
        return ac->grantAccessBatch(nodeId, resources);
    }
    return false;
}

bool SecurityManager::revokeNodeAccessBatch(const std::string& nodeId, const std::vector<std::string>& resources) {
    DefaultAccessControl* ac = dynamic_cast<DefaultAccessControl*>(accessControl_.get());
    if (ac) {
        return ac->revokeAccessBatch(nodeId, resources);
    }
    return false;
}

void SecurityManager::clearNodeAccess(const std::string& nodeId) {
    DefaultAccessControl* ac = dynamic_cast<DefaultAccessControl*>(accessControl_.get());
    if (ac) {
        ac->clearAccess(nodeId);
    }
}

std::vector<std::string> SecurityManager::getNodeResources(const std::string& nodeId) {
    return accessControl_->getResourcesForIdentity(nodeId);
}

std::vector<std::string> SecurityManager::getAllNodes() {
    DefaultAccessControl* ac = dynamic_cast<DefaultAccessControl*>(accessControl_.get());
    if (ac) {
        return ac->getIdentities();
    }
    return {};
}

void SecurityManager::setSecurityLevel(SecurityLevel level) {
    securityFramework_->setSecurityLevel(level);
}

SecurityLevel SecurityManager::getSecurityLevel() const {
    return securityFramework_->getSecurityLevel();
}

} // namespace security
} // namespace aurorart