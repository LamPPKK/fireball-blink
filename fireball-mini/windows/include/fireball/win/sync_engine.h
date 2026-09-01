#pragma once

#include <string>
#include <vector>

namespace fireball::win {

class SyncEngine {
public:
    SyncEngine();

    // Generates a 24-word recovery/sync phrase
    std::string Generate24WordPhrase() const;

    // Validates a 24-word phrase
    bool ValidatePhrase(const std::string& phrase) const;

    // Generates a 6-word ephemeral pairing code (e.g. for desktop-to-mobile sync)
    std::string GeneratePairingCode() const;

    // Derives sync encryption key from phrase
    std::string DeriveSyncKey(const std::string& phrase) const;

private:
    std::vector<std::string> wordlist_;
};

} // namespace fireball::win
