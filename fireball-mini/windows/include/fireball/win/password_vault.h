#pragma once

#include <string>
#include <vector>
#include <cstdint>
#include <optional>

namespace fireball::win {

struct VaultCredential {
    std::string id;
    std::string domain;
    std::string username;
    std::string password;
    int64_t created_at_ms = 0;
};

class PasswordVault {
public:
    PasswordVault();

    bool IsUnlocked() const { return is_unlocked_; }
    bool Unlock(const std::string& master_password);
    void Lock();

    bool StoreCredential(const std::string& domain, const std::string& username, const std::string& password);
    std::vector<VaultCredential> GetCredentialsForDomain(const std::string& domain) const;
    std::vector<VaultCredential> GetAllCredentials() const;
    bool DeleteCredential(const std::string& credential_id);

    // Exports and imports encrypted blobs
    std::string ExportEncryptedBlob() const;
    bool ImportEncryptedBlob(const std::string& blob, const std::string& master_password);

private:
    bool is_unlocked_ = false;
    std::string master_key_hash_;
    std::vector<VaultCredential> credentials_;
};

} // namespace fireball::win
