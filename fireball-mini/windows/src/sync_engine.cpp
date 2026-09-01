#include "fireball/win/sync_engine.h"
#include <random>
#include <sstream>
#include <iomanip>
#include <algorithm>

namespace fireball::win {

SyncEngine::SyncEngine() {
    wordlist_ = {
        "abandon", "ability", "able", "about", "above", "absent", "absorb", "abstract",
        "absurd", "abuse", "access", "accident", "account", "accuse", "achieve", "acid",
        "acoustic", "acquire", "across", "act", "action", "actor", "actress", "actual",
        "adapt", "add", "addict", "address", "adjust", "admit", "adult", "advance",
        "advice", "aerobic", "affair", "afford", "afraid", "again", "age", "agent",
        "agree", "ahead", "aim", "air", "airport", "aisle", "alarm", "album",
        "alcohol", "alert", "alien", "all", "alley", "allow", "almost", "alone",
        "alpha", "already", "also", "alter", "always", "amateur", "amazing", "among",
        "amount", "amused", "analyst", "anchor", "ancient", "anger", "angle", "angry",
        "animal", "ankle", "announce", "annual", "another", "answer", "antenna", "antique",
        "anxiety", "any", "apart", "apology", "appear", "apple", "approve", "april",
        "arch", "arctic", "area", "arena", "argue", "arm", "armed", "armor",
        "army", "around", "arrange", "arrest", "arrive", "arrow", "art", "artefact",
        "artist", "artwork", "ask", "aspect", "assault", "asset", "assist", "assume",
        "asthma", "athlete", "atom", "attack", "attend", "attitude", "attract", "auction",
        "audit", "august", "aunt", "author", "auto", "autumn", "average", "avocado",
        "avoid", "awake", "aware", "away", "awesome", "awful", "awkward", "axis",
        "baby", "bachelor", "bacon", "badge", "bag", "balance", "balcony", "ball",
        "bamboo", "banana", "banner", "bar", "barely", "bargain", "barrel", "base",
        "basic", "basket", "battle", "beach", "bean", "beauty", "because", "become",
        "beef", "before", "begin", "behave", "behind", "believe", "below", "belt",
        "bench", "benefit", "best", "betray", "better", "between", "beyond", "bicycle",
        "bid", "bike", "bind", "biology", "bird", "birth", "bitter", "black",
        "blade", "blame", "blanket", "blast", "bleak", "bless", "blind", "blood",
        "blossom", "blouse", "blue", "blur", "blush", "board", "boat", "body",
        "boil", "bomb", "bone", "bonus", "book", "boost", "border", "boring",
        "borrow", "boss", "bottom", "bounce", "box", "boy", "bracket", "brain",
        "brand", "brass", "brave", "bread", "breeze", "brick", "bridge", "brief"
    };
}

std::string SyncEngine::Generate24WordPhrase() const {
    static std::random_device rd;
    static std::mt19937 gen(rd());
    std::uniform_int_distribution<size_t> dis(0, wordlist_.size() - 1);

    std::string phrase;
    for (int i = 0; i < 24; ++i) {
        if (i > 0) phrase += " ";
        phrase += wordlist_[dis(gen)];
    }
    return phrase;
}

bool SyncEngine::ValidatePhrase(const std::string& phrase) const {
    std::stringstream ss(phrase);
    std::string word;
    int count = 0;
    while (ss >> word) {
        if (std::find(wordlist_.begin(), wordlist_.end(), word) == wordlist_.end()) {
            return false;
        }
        count++;
    }
    return count == 24;
}

std::string SyncEngine::GeneratePairingCode() const {
    static std::random_device rd;
    static std::mt19937 gen(rd());
    std::uniform_int_distribution<size_t> dis(0, wordlist_.size() - 1);

    std::string code;
    for (int i = 0; i < 6; ++i) {
        if (i > 0) code += "-";
        code += wordlist_[dis(gen)];
    }
    return code;
}

std::string SyncEngine::DeriveSyncKey(const std::string& phrase) const {
    uint64_t hash = 14695981039346656037ULL;
    for (char c : phrase) {
        hash ^= static_cast<uint8_t>(c);
        hash *= 1099511628211ULL;
    }
    std::stringstream ss;
    ss << "SYNC_KEY_" << std::hex << std::setfill('0') << std::setw(16) << hash;
    return ss.str();
}

} // namespace fireball::win
