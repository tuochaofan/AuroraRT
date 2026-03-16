#ifndef AURORART_SECURITY_H
#define AURORART_SECURITY_H

#include <string>
#include <memory>
#include <vector>
#include <map>
#include <mutex>
#include <unordered_map>

namespace aurorart {
namespace security {

enum class SecurityLevel {
    LOW,
    MEDIUM,
    HIGH,
    CRITICAL
};

enum class AuthenticationStatus {
    SUCCESS,
    FAILED,
    PENDING
};

enum class EncryptionAlgorithm {
    AES_128_CBC,
    AES_192_CBC,
    AES_256_CBC,
    AES_128_GCM,
    AES_256_GCM,
    RSA_2048,
    RSA_4096,
    ECC_P256,
    ECC_P384
};

enum class AuthenticationMethod {
    PASSWORD,
    TOKEN,
    X509_CERTIFICATE,
    OAUTH2,
    UNIX_CREDENTIALS
};

class SecurityFramework {
public:
    virtual ~SecurityFramework() = default;
    
    virtual bool init() = 0;
    virtual bool shutdown() = 0;
    
    virtual SecurityLevel getSecurityLevel() const = 0;
    virtual void setSecurityLevel(SecurityLevel level) = 0;
};

class Authentication {
public:
    virtual ~Authentication() = default;
    
    virtual AuthenticationStatus authenticate(const std::string& identity, const std::string& credentials, 
                                          AuthenticationMethod method = AuthenticationMethod::PASSWORD) = 0;
    virtual bool registerIdentity(const std::string& identity, const std::string& credentials, 
                               AuthenticationMethod method = AuthenticationMethod::PASSWORD) = 0;
    virtual bool unregisterIdentity(const std::string& identity) = 0;
    virtual bool isIdentityRegistered(const std::string& identity) const = 0;
    virtual std::string generateToken(const std::string& identity, int expiresInSeconds = 3600) = 0;
};

class Encryption {
public:
    virtual ~Encryption() = default;
    
    virtual bool encrypt(const void* data, size_t size, std::vector<uint8_t>& encryptedData) = 0;
    virtual bool decrypt(const void* data, size_t size, std::vector<uint8_t>& decryptedData) = 0;
    virtual void setAlgorithm(EncryptionAlgorithm algorithm) = 0;
    virtual EncryptionAlgorithm getAlgorithm() const = 0;
    virtual bool setKey(const void* key, size_t keySize) = 0;
    virtual size_t getKeySize() const = 0;
};

class E2EProtection {
public:
    virtual ~E2EProtection() = default;
    
    virtual bool protect(const void* data, size_t size, std::vector<uint8_t>& protectedData, 
                       const std::string& senderId, const std::string& receiverId) = 0;
    virtual bool unprotect(const void* data, size_t size, std::vector<uint8_t>& unprotectedData, 
                         const std::string& senderId, const std::string& receiverId) = 0;
    virtual void setSecurityLevel(SecurityLevel level) = 0;
    virtual SecurityLevel getSecurityLevel() const = 0;
};

class AccessControl {
public:
    virtual ~AccessControl() = default;
    
    virtual bool checkAccess(const std::string& identity, const std::string& resource) = 0;
    virtual bool grantAccess(const std::string& identity, const std::string& resource) = 0;
    virtual bool revokeAccess(const std::string& identity, const std::string& resource) = 0;
    virtual std::vector<std::string> getResourcesForIdentity(const std::string& identity) const = 0;
    virtual bool grantAccessBatch(const std::string& identity, const std::vector<std::string>& resources) = 0;
    virtual bool revokeAccessBatch(const std::string& identity, const std::vector<std::string>& resources) = 0;
    virtual void clearAccess(const std::string& identity) = 0;
    virtual std::vector<std::string> getIdentities() const = 0;
};

class SecurityManager {
public:
    static SecurityManager& instance();
    
    bool init();
    bool shutdown();
    
    SecurityFramework* getSecurityFramework();
    Authentication* getAuthentication();
    Encryption* getEncryption();
    E2EProtection* getE2EProtection();
    AccessControl* getAccessControl();
    
    bool authenticateNode(const std::string& nodeId, const std::string& credentials, 
                        AuthenticationMethod method = AuthenticationMethod::PASSWORD);
    std::string generateNodeToken(const std::string& nodeId, int expiresInSeconds = 3600);
    bool encryptData(const void* data, size_t size, std::vector<uint8_t>& encryptedData);
    bool decryptData(const void* data, size_t size, std::vector<uint8_t>& decryptedData);
    bool protectData(const void* data, size_t size, std::vector<uint8_t>& protectedData, 
                    const std::string& senderId, const std::string& receiverId);
    bool unprotectData(const void* data, size_t size, std::vector<uint8_t>& unprotectedData, 
                      const std::string& senderId, const std::string& receiverId);
    bool checkNodeAccess(const std::string& nodeId, const std::string& resource);
    bool grantNodeAccess(const std::string& nodeId, const std::string& resource);
    bool revokeNodeAccess(const std::string& nodeId, const std::string& resource);
    bool grantNodeAccessBatch(const std::string& nodeId, const std::vector<std::string>& resources);
    bool revokeNodeAccessBatch(const std::string& nodeId, const std::vector<std::string>& resources);
    void clearNodeAccess(const std::string& nodeId);
    std::vector<std::string> getNodeResources(const std::string& nodeId);
    std::vector<std::string> getAllNodes();
    
    bool setEncryptionKey(const void* key, size_t keySize);
    void setSecurityLevel(SecurityLevel level);
    SecurityLevel getSecurityLevel() const;
    
private:
    SecurityManager();
    
    std::unique_ptr<SecurityFramework> securityFramework_;
    std::unique_ptr<Authentication> authentication_;
    std::unique_ptr<Encryption> encryption_;
    std::unique_ptr<E2EProtection> e2eProtection_;
    std::unique_ptr<AccessControl> accessControl_;
    
    std::mutex mutex_;
    bool initialized_;
};

} // namespace security
} // namespace aurorart

#endif // AURORART_SECURITY_H
