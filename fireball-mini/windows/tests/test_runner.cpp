#include "fireball/win/domain_models.h"
#include "fireball/win/search_engines.h"
#include "fireball/win/shields_engine.h"
#include "fireball/win/password_vault.h"
#include "fireball/win/sync_engine.h"
#include "fireball/win/beam_client.h"
#include "fireball/win/webview2_host.h"
#include "fireball/win/app_window.h"

#include <iostream>
#include <cassert>
#include <vector>
#include <string>

using namespace fireball::win;

void TestDomainModels() {
    std::cout << "  [1/8] Testing Domain Models & Spaces...\n";
    Space main = Space::CreateDefaultMain();
    assert(main.id == "space-main");
    assert(!main.is_burner);
    assert(main.accent_color_hex == "#D8FF3E");

    Space burner = Space::CreateDefaultBurner();
    assert(burner.is_burner);
    assert(burner.accent_color_hex == "#FF5A1F");

    Profile def_prof = Profile::CreateDefault();
    assert(!def_prof.is_off_the_record);
    assert(def_prof.user_data_folder == "Profiles\\Main");

    Profile burner_prof = Profile::CreateBurner();
    assert(burner_prof.is_off_the_record);
    assert(burner_prof.user_data_folder.rfind("Profiles\\Burner_", 0) == 0);

    FireballTab tab = FireballTab::Create("space-main", "profile-main", "https://duckduckgo.com", TabSection::TODAY);
    assert(!tab.id.empty());
    assert(tab.url == "https://duckduckgo.com");
    assert(tab.title == "DuckDuckGo");
    assert(tab.IsSafeToDiscard());
    assert(tab.GetTier(false) == TabTier::TODAY);
    assert(tab.GetTier(true) == TabTier::BURNER);
}

void TestSearchEnginesAndBangs() {
    std::cout << "  [2/8] Testing Search Bangs & Omnibox Parser...\n";
    assert(SearchEngineParser::ParseQuery("!g rust language") == "https://www.google.com/search?q=rust+language");
    assert(SearchEngineParser::ParseQuery("!b privacy browser") == "https://search.brave.com/search?q=privacy+browser");
    assert(SearchEngineParser::ParseQuery("!yt lofi beats") == "https://www.youtube.com/results?search_query=lofi+beats");
    assert(SearchEngineParser::ParseQuery("!gh LamPPKK/fireball-blink") == "https://github.com/search?q=LamPPKK%2ffireball-blink");
    assert(SearchEngineParser::ParseQuery("!w Quantum computing") == "https://en.wikipedia.org/wiki/Special:Search?search=Quantum+computing");
    assert(SearchEngineParser::ParseQuery("!r technology") == "https://www.reddit.com/search/?q=technology");

    // Standard URL recognition
    assert(SearchEngineParser::ParseQuery("github.com") == "https://github.com");
    assert(SearchEngineParser::ParseQuery("https://duckduckgo.com") == "https://duckduckgo.com");
    assert(SearchEngineParser::ParseQuery("localhost:8080") == "https://localhost:8080" || SearchEngineParser::ParseQuery("localhost:8080").find("localhost") != std::string::npos);

    // Fallback search
    assert(SearchEngineParser::ParseQuery("what is fireball browser") == "https://duckduckgo.com/?q=what+is+fireball+browser");
}

void TestShieldsEngine() {
    std::cout << "  [3/8] Testing Shields & URL Parameter Stripper...\n";
    ShieldsEngine shields;

    std::string dirty_url = "https://example.com/shop?product=123&utm_source=facebook&utm_medium=cpc&fbclid=IwAR0123&gclid=Cj0K456#top";
    std::string clean = shields.CleanTrackingParameters(dirty_url);
    assert(clean == "https://example.com/shop?product=123#top");

    std::string only_tracking = "https://news.ycombinator.com?utm_campaign=daily&utm_term=tech";
    assert(shields.CleanTrackingParameters(only_tracking) == "https://news.ycombinator.com");

    assert(shields.ShouldBlockResource("https://adservice.google.com/adsid/google/ui"));
    assert(shields.ShouldBlockResource("https://connect.facebook.net/tr?id=12345"));
    assert(!shields.ShouldBlockResource("https://cdn.example.com/style.css"));

    std::string css_script = shields.GenerateCosmeticCssScript();
    assert(css_script.find("display: none !important") != std::string::npos);
}

void TestPasswordVault() {
    std::cout << "  [4/8] Testing Zero-Knowledge Password Vault...\n";
    PasswordVault vault;
    assert(!vault.IsUnlocked());
    assert(vault.GetAllCredentials().empty());

    // Unlock with master password
    assert(vault.Unlock("MasterPass123!"));
    assert(vault.IsUnlocked());

    // Store credentials
    assert(vault.StoreCredential("github.com", "octocat", "super_secret_token_1"));
    assert(vault.StoreCredential("google.com", "user@gmail.com", "gpass_2026"));

    auto creds = vault.GetCredentialsForDomain("github.com");
    assert(creds.size() == 1);
    assert(creds[0].username == "octocat");
    assert(creds[0].password == "super_secret_token_1");

    // Export and import
    std::string blob = vault.ExportEncryptedBlob();
    assert(!blob.empty());

    PasswordVault vault2;
    assert(vault2.ImportEncryptedBlob(blob, "MasterPass123!"));
    assert(vault2.GetAllCredentials().size() == 2);

    vault.Lock();
    assert(!vault.IsUnlocked());
    assert(vault.GetAllCredentials().empty());
}

void TestSyncEngine() {
    std::cout << "  [5/8] Testing BIP-39 Sync Engine...\n";
    SyncEngine sync;
    std::string phrase = sync.Generate24WordPhrase();
    assert(sync.ValidatePhrase(phrase));

    std::string invalid_phrase = "invalid word not in bip39 wordlist test test";
    assert(!sync.ValidatePhrase(invalid_phrase));

    std::string pairing_code = sync.GeneratePairingCode();
    assert(!pairing_code.empty());
    assert(pairing_code.find('-') != std::string::npos);

    std::string sync_key = sync.DeriveSyncKey(phrase);
    assert(sync_key.rfind("SYNC_KEY_", 0) == 0);
}

void TestBeamClient() {
    std::cout << "  [6/8] Testing Fireball Beam Streaming Protocol...\n";
    BeamPacket pkt;
    pkt.type = BeamMessageType::TOUCH_MOVE;
    pkt.session_id = 0x12345678;
    pkt.payload = {0x01, 0x02, 0x03, 0x04};

    auto encoded = BeamPacket::Encode(pkt);
    assert(encoded.size() == 13 + 4);

    BeamPacket decoded;
    assert(BeamPacket::Decode(encoded, decoded));
    assert(decoded.type == BeamMessageType::TOUCH_MOVE);
    assert(decoded.session_id == 0x12345678);
    assert(decoded.payload.size() == 4);

    NormalizedInput input{0.5f, 0.75f, 1};
    auto pixels = input.ToHostPixels(1920, 1080);
    assert(pixels.first == 960);
    assert(pixels.second == 810);
}

void TestWebView2Host() {
    std::cout << "  [7/8] Testing WebView2 Host & Profile Isolation...\n";
    WebView2HostConfig cfg;
    cfg.base_data_directory = "C:\\Users\\User\\AppData\\Local\\Fireball";
    WebView2Host host(cfg);

    Profile main_prof = Profile::CreateDefault();
    std::string main_path = host.GetUserDataFolderForProfile(main_prof);
    assert(main_path == "C:\\Users\\User\\AppData\\Local\\Fireball\\Profiles\\Main");

    Profile burner_prof = Profile::CreateBurner();
    std::string burner_path = host.GetUserDataFolderForProfile(burner_prof);
    assert(burner_path.find("C:\\Users\\User\\AppData\\Local\\Fireball\\Temp\\Profiles\\Burner_") == 0);

    host.Navigate("https://duckduckgo.com/?q=fireball&utm_source=tracker");
    assert(host.GetCurrentUrl() == "https://duckduckgo.com/?q=fireball");
    assert(!host.IsLoading());
    assert(host.GetLoadingProgress() == 100);
}

void TestAppWindow() {
    std::cout << "  [8/8] Testing AppWindow & Multi-Space Management...\n";
    AppWindow app;
    assert(app.Initialize());
    assert(app.GetSpaces().size() == 3);
    assert(app.GetActiveSpace().name == "Main");

    auto tabs = app.GetTabsForActiveSpace();
    assert(tabs.size() == 1);
    assert(tabs[0].title == "DuckDuckGo");

    // Create New Tab
    const auto& tab2 = app.CreateNewTab("https://github.com");
    assert(tab2.url == "https://github.com");
    assert(app.GetTabsForActiveSpace().size() == 2);

    // Switch Space to Work
    assert(app.SwitchSpace("space-work"));
    assert(app.GetActiveSpace().name == "Work");
    assert(app.GetTabsForActiveSpace().size() == 1);

    // Switch to Burner
    assert(app.SwitchSpace("space-burner"));
    assert(app.GetActiveSpace().is_burner);

    // Keyboard Shortcuts
    assert(app.HandleShortcut(AppWindow::ShortcutCommand::NEW_TAB));
    assert(app.GetTabsForActiveSpace().size() == 2);

    // Engine Switcher
    assert(app.GetEngineMode() == AppWindow::EngineMode::NATIVE_WEBVIEW2);
    assert(app.HandleShortcut(AppWindow::ShortcutCommand::TOGGLE_ENGINE));
    assert(app.GetEngineMode() == AppWindow::EngineMode::FIREBALL_BEAM_STREAM);
    app.ToggleEngineMode();
    assert(app.GetEngineMode() == AppWindow::EngineMode::NATIVE_WEBVIEW2);

    assert(app.HandleShortcut(AppWindow::ShortcutCommand::NEXT_TAB));
    assert(app.HandleShortcut(AppWindow::ShortcutCommand::PREV_TAB));
}


int main() {
    std::cout << "==========================================\n";
    std::cout << "🚀 Running Fireball Windows Lite Test Suite\n";
    std::cout << "==========================================\n";

    TestDomainModels();
    TestSearchEnginesAndBangs();
    TestShieldsEngine();
    TestPasswordVault();
    TestSyncEngine();
    TestBeamClient();
    TestWebView2Host();
    TestAppWindow();

    std::cout << "==========================================\n";
    std::cout << "✅ All 8 Fireball Windows Test Suites PASSED!\n";
    std::cout << "==========================================\n";
    return 0;
}
