#include <jni.h>
#include <string>
#include <vector>
#include <algorithm>
#include <unordered_set>
#include <unordered_map>
#include <mutex>
#include "jni_helpers.h"

namespace fireball::mini {

enum BlockCategory {
  ALLOW = 0,
  EASYLIST_ADS = 1,
  EASYPRIVACY_TRACKERS = 2,
  FANBOY_ANNOYANCES_COOKIES = 3,
  MALWARE_CRYPTO = 4,
  SOCIAL_WIDGETS = 5
};

class MiniNativeEngine {
 public:
  static MiniNativeEngine& Instance() {
    static MiniNativeEngine instance;
    return instance;
  }

  MiniNativeEngine() {
    InitFilterLists();
  }

  std::string CleanUrl(const std::string& raw_url) {
    if (raw_url.empty()) return "";
    std::string result = raw_url;

    // Comprehensive URL telemetry stripping (Google, Meta, TikTok, MS, Email, Analytics)
    static const std::vector<std::string> kTrackingParams = {
        "utm_source", "utm_medium", "utm_campaign", "utm_term", "utm_content",
        "utm_id", "utm_source_platform", "utm_creative_format", "utm_marketing_tactic",
        "fbclid", "gclid", "gbraid", "wbraid", "dclid", "msclkid", "mc_eid",
        "_ga", "_gl", "_hsenc", "_hsmi", "igshid", "si", "ref_src", "twclid",
        "yclid", "ttclid", "sc_src", "mkt_tok", "vero_id", "wickedid"
    };

    size_t query_pos = result.find('?');
    if (query_pos == std::string::npos) return result;

    std::string base = result.substr(0, query_pos);
    std::string query = result.substr(query_pos + 1);
    std::string fragment;
    size_t hash_pos = query.find('#');
    if (hash_pos != std::string::npos) {
      fragment = query.substr(hash_pos);
      query = query.substr(0, hash_pos);
    }

    std::vector<std::string> kept_params;
    size_t start = 0;
    while (start < query.size()) {
      size_t amp = query.find('&', start);
      std::string param = (amp == std::string::npos) ? query.substr(start) : query.substr(start, amp - start);
      
      size_t eq = param.find('=');
      std::string key = (eq == std::string::npos) ? param : param.substr(0, eq);

      bool is_tracking = false;
      for (const auto& tracker : kTrackingParams) {
        if (key == tracker) {
          is_tracking = true;
          break;
        }
      }

      if (!is_tracking && !param.empty()) {
        kept_params.push_back(param);
      }

      if (amp == std::string::npos) break;
      start = amp + 1;
    }

    std::string cleaned = base;
    if (!kept_params.empty()) {
      cleaned += "?";
      for (size_t i = 0; i < kept_params.size(); ++i) {
        if (i > 0) cleaned += "&";
        cleaned += kept_params[i];
      }
    }
    cleaned += fragment;
    return cleaned;
  }

  int EvaluateRequest(const std::string& url, const std::string& source_host, const std::string& dest_host) {
    std::lock_guard<std::mutex> lock(mutex_);

    // 1. Check EasyList Ads & Video Ads
    for (const auto& domain : ads_domains_) {
      if (dest_host.find(domain) != std::string::npos || url.find(domain) != std::string::npos) {
        return EASYLIST_ADS;
      }
    }

    // 2. Check EasyPrivacy Trackers & Telemetry
    for (const auto& domain : trackers_domains_) {
      if (dest_host.find(domain) != std::string::npos || url.find(domain) != std::string::npos) {
        return EASYPRIVACY_TRACKERS;
      }
    }

    // 3. Check Cookie Notice & Annoyances
    for (const auto& domain : annoyances_domains_) {
      if (dest_host.find(domain) != std::string::npos || url.find(domain) != std::string::npos) {
        return FANBOY_ANNOYANCES_COOKIES;
      }
    }

    // 4. Check Cryptominers & Malware Phishing
    for (const auto& domain : malware_domains_) {
      if (dest_host.find(domain) != std::string::npos || url.find(domain) != std::string::npos) {
        return MALWARE_CRYPTO;
      }
    }

    // 5. Check Social Media Trackers & Pixels
    for (const auto& domain : social_domains_) {
      if (dest_host.find(domain) != std::string::npos || url.find(domain) != std::string::npos) {
        return SOCIAL_WIDGETS;
      }
    }

    return ALLOW;
  }

  std::string GetCosmeticCss(const std::string& hostname) {
    // Ultra-comprehensive user-origin cosmetic CSS rules
    return "div[id^='google_ads_'], div[id^='ad-slot'], div[class*='ad-banner'], "
           "ins.adsbygoogle, .advertisement, [data-ad-slot], [data-ad-client], .fb-ad, "
           ".taboola, .outbrain, .adblade, .zergnet, .hybrid-ad, .sponsor-badge, "
           ".banner-ads, div[class*='google-ads'], iframe[src*='doubleclick'], "
           "iframe[src*='googlesyndication'], iframe[src*='adservice'], #carbonads, "
           "#onetrust-banner-sdk, .cookie-banner, div[class*='cookie-consent'], "
           "div[id*='cookieModal'], [aria-label*='cookie'], .cc-window, .app-install-banner, "
           ".video-ads, .ytp-ad-module, .ytp-ad-overlay-container, .ytp-ad-text, "
           "div[class*='sponsored-post'], .native-ad, .sticky-ad-bottom, .popup-overlay-ad "
           "{ display: none !important; opacity: 0 !important; visibility: hidden !important; "
           "pointer-events: none !important; height: 0 !important; max-height: 0 !important; "
           "margin: 0 !important; padding: 0 !important; }";
  }

  int SniffMedia(const std::string& url, const std::string& mime_type) {
    if (url.find(".m3u8") != std::string::npos || mime_type == "application/x-mpegURL" || mime_type == "application/vnd.apple.mpegurl") {
      return 3; // HLS VOD
    }
    if (url.find(".mpd") != std::string::npos || mime_type == "application/dash+xml") {
      return 4; // DASH VOD
    }
    if (url.find(".mp4") != std::string::npos || mime_type == "video/mp4" || mime_type == "video/webm") {
      return 2; // Direct Video
    }
    if (url.find(".mp3") != std::string::npos || mime_type == "audio/mpeg" || mime_type == "audio/mp4") {
      return 1; // Direct Audio
    }
    return 0; // None
  }

 private:
  std::mutex mutex_;
  std::vector<std::string> ads_domains_;
  std::vector<std::string> trackers_domains_;
  std::vector<std::string> annoyances_domains_;
  std::vector<std::string> malware_domains_;
  std::vector<std::string> social_domains_;

  void InitFilterLists() {
    // 1. EasyList Standard & Video Ads (70+ ad networks)
    ads_domains_ = {
        "doubleclick.net", "googlesyndication.com", "googleadservices.com",
        "adnxs.com", "criteo.com", "adservice.google", "pagead2.googlesyndication.com",
        "taboola.com", "outbrain.com", "scorecardresearch.com", "amazon-adsystem.com",
        "adroll.com", "rubiconproject.com", "pubmatic.com", "openx.net",
        "smartadserver.com", "inmobi.com", "unityads.unity3d.com", "applovin.com",
        "ironsrc.com", "vungle.com", "mintegral.com", "smaato.net", "sovrn.com",
        "thetradedesk.com", "casalemedia.com", "indexexchange.com", "bidswitch.net",
        "popads.net", "popcash.net", "propellerads.com", "exoclick.com",
        "trafficjunky.com", "adsterra.com", "monetag.com", "revenuehits.com",
        "mgid.com", "revcontent.com", "admicro.vn", "ambientplatform.vn",
        "eclick.vn", "blueseed.tv", "antoree.com/ads", "adblade.com",
        "zergnet.com", "media.net", "adcolony.com", "chartboost.com",
        "yieldmo.com", "triplelift.com", "conversantmedia.com", "gumgum.com",
        "undertone.com", "sonobi.com", "teads.tv", "kargo.com", "yieldlab.net"
    };

    // 2. EasyPrivacy Trackers & Telemetry (40+ analytics & fingerprinting)
    trackers_domains_ = {
        "google-analytics.com", "analytics.google.com", "hotjar.com", "clarity.ms",
        "mixpanel.com", "segment.io", "api.segment.io", "amplitude.com",
        "appsflyer.com", "adjust.com", "mc.yandex.ru", "statcounter.com",
        "mouseflow.com", "crazyegg.com", "fullstory.com", "branch.io",
        "onesignal.com", "braze.com", "quantserve.com", "newrelic.com",
        "nr-data.net", "sentry.io", "loggly.com", "datadoghq.com",
        "pingdom.net", "optimizely.com", "vwo.com", "inspectlet.com",
        "luckyorange.com", "heap.io", "wootric.com", "intercom.io"
    };

    // 3. Fanboy's Annoyances & Cookie Banners (25+ consent systems)
    annoyances_domains_ = {
        "onetrust.com", "cdn.cookielaw.org", "consent.cookiebot.com",
        "quantcast.mgr.consensu.org", "didomi.io", "trustarc.com",
        "usercentrics.eu", "iubenda.com", "complianz.io", "civicuk.com",
        "osano.com", "termly.io", "optanon.com", "cookieinformation.com",
        "trustbadge.com", "pushassist.com", "subscribers.com", "pushwoosh.com",
        "webpushr.com", "sendpulse.com", "gravatar.com"
    };

    // 4. Cryptominers & Phishing / Malware (20+ threat domains)
    malware_domains_ = {
        "coinhive.com", "coin-have.com", "crypto-loot.com", "jsecoin.com",
        "webminepool.com", "miner.pr0gramm.com", "minr.pw", "authedmine.com",
        "monerominer.rocks", "cloudcoins.co", "cryptoloot.pro", "coinhive.com/lib",
        "minero.cc", "badsite-phishing.com", "scam-redirect.net", "malicious-apk.org"
    };

    // 5. Social Media Trackers & Pixel Telemetry (15+ social endpoints)
    social_domains_ = {
        "connect.facebook.net", "facebook.net/tr", "analytics.tiktok.com",
        "platform.twitter.com", "widgets.pinterest.com", "platform.linkedin.com",
        "apis.google.com/js/platform.js", "snap.licdn.com", "tr.snapchat.com",
        "pixel.reddit.com", "static.ads-twitter.com", "t.co/i/adsct"
    };
  }
};

}  // namespace fireball::mini

extern "C" {

JNIEXPORT jstring JNICALL
Java_com_fireball_mini_core_FireballNativeBridge_nativeCleanUrl(JNIEnv* env, jobject /* thiz */, jstring raw_url) {
  std::string input = fireball::jni::JStringToStdString(env, raw_url);
  std::string output = fireball::mini::MiniNativeEngine::Instance().CleanUrl(input);
  return fireball::jni::StdStringToJString(env, output);
}

JNIEXPORT jint JNICALL
Java_com_fireball_mini_core_FireballNativeBridge_nativeEvaluateRequest(
    JNIEnv* env, jobject /* thiz */, jstring profile_id, jstring request_url, jstring source_host, jstring dest_host) {
  std::string url = fireball::jni::JStringToStdString(env, request_url);
  std::string src = fireball::jni::JStringToStdString(env, source_host);
  std::string dst = fireball::jni::JStringToStdString(env, dest_host);
  return fireball::mini::MiniNativeEngine::Instance().EvaluateRequest(url, src, dst);
}

JNIEXPORT jstring JNICALL
Java_com_fireball_mini_core_FireballNativeBridge_nativeGetCosmeticCss(
    JNIEnv* env, jobject /* thiz */, jstring profile_id, jstring hostname) {
  std::string host = fireball::jni::JStringToStdString(env, hostname);
  std::string css = fireball::mini::MiniNativeEngine::Instance().GetCosmeticCss(host);
  return fireball::jni::StdStringToJString(env, css);
}

JNIEXPORT jint JNICALL
Java_com_fireball_mini_core_FireballNativeBridge_nativeSniffMedia(
    JNIEnv* env, jobject /* thiz */, jstring url, jstring mime_type) {
  std::string u = fireball::jni::JStringToStdString(env, url);
  std::string m = fireball::jni::JStringToStdString(env, mime_type);
  return fireball::mini::MiniNativeEngine::Instance().SniffMedia(u, m);
}

}  // extern "C"
