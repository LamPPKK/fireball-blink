#include "fireball/win/password_vault.h"
#include <chrono>
#include <sstream>
#include <iomanip>
#include <random>

namespace fireball::win {

namespace {
std::string HashString(const std::string& input) {
    // 64-bit FNV-1a hash formatted as hex
    uint64_t hash = 14695981039346656037ULL;
    for (char c : input) {
        hash ^= static_cast<uint8_t>(c);
        hash *= 1099511628211ULL;
    }
    std::stringstream ss;
    ss << std::hex << std::setfill('0') << std::setw(16) << hash;
    return ss.str();
}

std::string GenerateId() {
    static std::random_device rd;
    static std::mt19937_64 gen(rd());
    uint64_t val = gen();
    std::stringstream ss;
    ss << "cred-" << std::hex << val;
    return ss.str();
}
} // namespace

PasswordVault::PasswordVault() = default;

bool PasswordVault::Unlock(const std::string& master_password) {
    if (master_password.empty()) return false;
    std::string expected_hash = HashString(master_password);
    if (master_key_hash_.empty()) {
        master_key_hash_ = expected_hash;
        is_unlocked_ = true;
        return true;
    }
    if (master_key_hash_ == expected_hash) {
        is_unlocked_ = true;
        return true;
    }
    return false;
}

void PasswordVault::Lock() {
    is_unlocked_ = false;
}

bool PasswordVault::StoreCredential(const std::string& domain, const std::string& username, const std::string& password) {
    if (!is_unlocked_) return false;
    VaultCredential cred;
    cred.id = GenerateId();
    cred.domain = domain;
    cred.username = username;
    cred.password = password;
    cred.created_at_ms = std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::system_clock::now().time_since_epoch()
    ).count();
    credentials_.push_back(cred);
    return true;
}

std::vector<VaultCredential> PasswordVault::GetCredentialsForDomain(const std::string& domain) const {
    if (!is_unlocked_) return {};
    std::vector<VaultCredential> results;
    for (const auto& cred : credentials_) {
        if (cred.domain == domain || cred.domain.find(domain) != std::string::npos) {
            results.push_back(cred);
        }
    }
    return results;
}

std::vector<VaultCredential> PasswordVault::GetAllCredentials() const {
    if (!is_unlocked_) return {};
    return credentials_;
}

bool PasswordVault::DeleteCredential(const std::string& credential_id) {
    if (!is_unlocked_) return false;
    auto it = std::remove_if(credentials_.begin(), credentials_.end(), [&](const VaultCredential& c) {
        return c.id == credential_id;
    });
    if (it != credentials_.end()) {
        credentials_.erase(it, credentials_.end());
        return true;
    }
    return false;
}

std::string PasswordVault::ExportEncryptedBlob() const {
    if (!is_unlocked_) return "";
    std::stringstream ss;
    ss << "VAULT_V1:" << credentials_.size() << "\n";
    for (const auto& cred : credentials_) {
        ss << cred.id << "|" << cred.domain << "|" << cred.username << "|" << cred.password << "|" << cred.created_at_ms << "\n";
    }
    return ss.str();
}

bool PasswordVault::ImportEncryptedBlob(const std::string& blob, const std::string& master_password) {
    if (!Unlock(master_password)) return false;
    if (blob.rfind("VAULT_V1:", 0) != 0) return false;

    std::stringstream ss(blob);
    std::string line;
    std::getline(ss, line); // Header

    credentials_.clear();
    while (std::getline(ss, line)) {
        if (line.empty()) continue;
        std::stringstream line_ss(line);
        std::string id, domain, username, password, timestamp_str;
        if (std::getline(line_ss, id, '|') &&
            std::getline(line_ss, domain, '|') &&
            std::getline(line_ss, username, '|') &&
            std::getline(line_ss, password, '|') &&
            std::getline(line_ss, timestamp_str, '|')) {
            VaultCredential cred;
            cred.id = id;
            cred.domain = domain;
            cred.username = username;
            cred.password = password;
            cred.created_at_ms = std::stoll(timestamp_str);
            credentials_.push_back(cred);
        }
    }
    return true;
}

} // namespace fireball::win
