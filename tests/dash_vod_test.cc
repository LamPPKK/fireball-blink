#include "fireball/components/transfer/dash_vod.h"

#include <cassert>
#include <string>

int main() {
  using fireball::transfer::DashTrackKind;
  using fireball::transfer::DashVodError;
  using fireball::transfer::DashVodErrorCode;
  using fireball::transfer::ParseDashVodManifest;

  constexpr char kManifestUri[] =
      "https://origin.example.test/manifests/main.mpd";
  constexpr char kDurationManifest[] = R"xml(<?xml version="1.0"?>
<MPD xmlns="urn:mpeg:dash:schema:mpd:2011" type="static"
     mediaPresentationDuration="PT6S">
  <BaseURL>https://cdn.example.test/media/</BaseURL>
  <Period>
    <AdaptationSet contentType="video" mimeType="video/mp4"
                   codecs="avc1.640028">
      <SegmentTemplate timescale="1000" duration="2000" startNumber="7"
          initialization="$RepresentationID$/init.mp4"
          media="$RepresentationID$/segment-$Number%05d$.m4s"/>
      <Representation id="video-low" bandwidth="1000000"
                      width="1280" height="720"/>
      <Representation id="video-high" bandwidth="4000000"
                      width="1920" height="1080"/>
    </AdaptationSet>
    <AdaptationSet contentType="audio" mimeType="audio/mp4"
                   codecs="mp4a.40.2">
      <SegmentTemplate timescale="48000" duration="96000"
          initialization="$RepresentationID$/init.mp4"
          media="$RepresentationID$/segment-$Number$.m4s"/>
      <Representation id="audio-main" bandwidth="128000"/>
    </AdaptationSet>
  </Period>
</MPD>)xml";

  auto duration = ParseDashVodManifest(kManifestUri, kDurationManifest,
                                       2'000'000);
  assert(duration.ok());
  assert(duration.value->duration_ms == 6'000);
  assert(duration.value->video.kind == DashTrackKind::kVideo);
  assert(duration.value->video.representation_id == "video-low");
  assert(duration.value->video.width == 1280);
  assert(duration.value->video.height == 720);
  assert(duration.value->video.initialization_uri ==
         "https://cdn.example.test/media/video-low/init.mp4");
  assert(duration.value->video.segment_uris.size() == 3);
  assert(duration.value->video.segment_uris[0] ==
         "https://cdn.example.test/media/video-low/segment-00007.m4s");
  assert(duration.value->video.segment_uris[2] ==
         "https://cdn.example.test/media/video-low/segment-00009.m4s");
  assert(duration.value->audio.has_value());
  assert(duration.value->audio->kind == DashTrackKind::kAudio);
  assert(duration.value->audio->segment_uris.size() == 3);

  std::string plain_decimal_format(kDurationManifest);
  const std::size_t padded_number =
      plain_decimal_format.find("$Number%05d$");
  assert(padded_number != std::string::npos);
  plain_decimal_format.replace(padded_number, 12, "$Number%d$");
  auto plain_decimal =
      ParseDashVodManifest(kManifestUri, plain_decimal_format, 2'000'000);
  assert(plain_decimal.ok());
  assert(plain_decimal.value->video.segment_uris[0] ==
         "https://cdn.example.test/media/video-low/segment-7.m4s");

  auto unrestricted =
      ParseDashVodManifest(kManifestUri, kDurationManifest, 0);
  assert(unrestricted.ok());
  assert(unrestricted.value->video.representation_id == "video-high");

  constexpr char kTimelineManifest[] = R"xml(
<d:MPD xmlns:d="urn:mpeg:dash:schema:mpd:2011" type="static"
       mediaPresentationDuration="PT0H0M6.250S">
  <!-- a bounded comment is accepted -->
  <d:Period>
    <d:AdaptationSet contentType="video" mimeType="video/mp4">
      <d:Representation id="timeline" bandwidth="500000" width="640"
                        height="360" codecs="avc1.4d401e">
        <d:BaseURL>../video/</d:BaseURL>
        <d:SegmentTemplate timescale="1000"
            initialization="init-$RepresentationID$.mp4?token=a&amp;b"
            media="chunk-$Time%05d$.m4s?token=a&amp;b">
          <d:SegmentTimeline>
            <d:S t="500" d="2000" r="2"/>
          </d:SegmentTimeline>
        </d:SegmentTemplate>
      </d:Representation>
    </d:AdaptationSet>
  </d:Period>
</d:MPD>)xml";
  auto timeline = ParseDashVodManifest(kManifestUri, kTimelineManifest);
  assert(timeline.ok());
  assert(timeline.value->duration_ms == 6'250);
  assert(!timeline.value->audio.has_value());
  assert(timeline.value->video.initialization_uri ==
         "https://origin.example.test/video/init-timeline.mp4?token=a&b");
  assert(timeline.value->video.segment_uris.size() == 3);
  assert(timeline.value->video.segment_uris[0] ==
         "https://origin.example.test/video/chunk-00500.m4s?token=a&b");
  assert(timeline.value->video.segment_uris[2] ==
         "https://origin.example.test/video/chunk-04500.m4s?token=a&b");

  constexpr char kDynamic[] =
      "<MPD type=\"dynamic\" mediaPresentationDuration=\"PT1S\">"
      "<Period/></MPD>";
  auto dynamic = ParseDashVodManifest(kManifestUri, kDynamic);
  assert(!dynamic.ok() && dynamic.error == DashVodError::kUnsupportedManifest);

  constexpr char kDrm[] =
      "<MPD type=\"static\" mediaPresentationDuration=\"PT1S\">"
      "<Period><AdaptationSet><ContentProtection/></AdaptationSet></Period>"
      "</MPD>";
  auto drm = ParseDashVodManifest(kManifestUri, kDrm);
  assert(!drm.ok() && drm.error == DashVodError::kUnsupportedManifest);

  constexpr char kDoctype[] =
      "<!DOCTYPE MPD [<!ENTITY leak SYSTEM \"file:///etc/passwd\">]>"
      "<MPD type=\"static\" mediaPresentationDuration=\"PT1S\">"
      "<Period>&leak;</Period></MPD>";
  assert(!ParseDashVodManifest(kManifestUri, kDoctype).ok());
  assert(!ParseDashVodManifest(
              kManifestUri,
              "<MPD type=\"static\" mediaPresentationDuration=\"PT1S\">"
              "<Period>&custom;</Period></MPD>")
              .ok());

  std::string negative_repeat(kTimelineManifest);
  const std::size_t repeat = negative_repeat.find("r=\"2\"");
  assert(repeat != std::string::npos);
  negative_repeat.replace(repeat, 5, "r=\"-1\"");
  assert(!ParseDashVodManifest(kManifestUri, negative_repeat).ok());

  std::string excessive_repeat(kTimelineManifest);
  const std::size_t excessive = excessive_repeat.find("r=\"2\"");
  assert(excessive != std::string::npos);
  excessive_repeat.replace(excessive, 5, "r=\"4096\"");
  auto limited = ParseDashVodManifest(kManifestUri, excessive_repeat);
  assert(!limited.ok() && limited.error == DashVodError::kLimitExceeded);

  std::string unsafe_base(kDurationManifest);
  const std::size_t safe_host = unsafe_base.find("https://cdn.example.test");
  assert(safe_host != std::string::npos);
  unsafe_base.replace(safe_host, 24, "https://user:pass@example.test");
  auto unsafe = ParseDashVodManifest(kManifestUri, unsafe_base);
  assert(!unsafe.ok() && unsafe.error == DashVodError::kUnsafeUri);

  std::string segment_list(kDurationManifest);
  segment_list.insert(segment_list.find("<Period>") + 8,
                      "<SegmentList/>");
  auto unsupported = ParseDashVodManifest(kManifestUri, segment_list);
  assert(!unsupported.ok() &&
         unsupported.error == DashVodError::kUnsupportedManifest);

  std::string invalid_utf8 = "<MPD>";
  invalid_utf8.push_back(static_cast<char>(0xc0));
  invalid_utf8.push_back(static_cast<char>(0xaf));
  invalid_utf8 += "</MPD>";
  assert(!ParseDashVodManifest(kManifestUri, invalid_utf8).ok());

  assert(DashVodErrorCode(DashVodError::kNone) == "DASH_OK");
  assert(DashVodErrorCode(DashVodError::kLimitExceeded) ==
         "DASH_LIMIT_EXCEEDED");
  return 0;
}
