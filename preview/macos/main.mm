#import <Cocoa/Cocoa.h>

#include <algorithm>
#include <cstdlib>
#include <map>
#include <optional>
#include <string>

#include "fireball/browser/domain_model.h"
#include "fireball/components/transfer/transfer_queue.h"

namespace {

using fireball::browser::BrowserModel;
using fireball::browser::ProfileId;
using fireball::browser::Space;
using fireball::browser::SpaceId;
using fireball::browser::SpaceKind;
using fireball::browser::StorageMode;
using fireball::browser::TabId;
using fireball::browser::TabLayout;
using fireball::browser::TabPlacement;
using fireball::browser::TabResidency;
using fireball::transfer::Aria2RpcResult;
using fireball::transfer::Aria2TransferState;
using fireball::transfer::Aria2TransferStatus;
using fireball::transfer::MediaCandidateKind;
using fireball::transfer::TransferBackend;
using fireball::transfer::TransferItem;
using fireball::transfer::TransferPersistence;
using fireball::transfer::TransferQueue;
using fireball::transfer::TransferRequest;
using fireball::transfer::TransferSourceKind;
using fireball::transfer::TransferState;

constexpr CGFloat kCanvasWidth = 1440.0;
constexpr CGFloat kCanvasHeight = 900.0;
bool gCapturingPreview = false;

template <typename Id>
Id ParseId(const char* value) {
  auto parsed = Id::Parse(std::string(value));
  if (!parsed.has_value()) {
    std::abort();
  }
  return std::move(*parsed);
}

NSColor* RGB(NSUInteger value, CGFloat alpha = 1.0) {
  return [NSColor colorWithRed:((value >> 16) & 0xff) / 255.0
                         green:((value >> 8) & 0xff) / 255.0
                          blue:(value & 0xff) / 255.0
                         alpha:alpha];
}

NSFont* DisplayFont(CGFloat size) {
  return [NSFont fontWithName:@"AvenirNextCondensed-DemiBold" size:size] ?:
      [NSFont boldSystemFontOfSize:size];
}

NSFont* BodyFont(CGFloat size) {
  return [NSFont fontWithName:@"AvenirNext-Medium" size:size] ?:
      [NSFont systemFontOfSize:size weight:NSFontWeightMedium];
}

NSFont* MonoFont(CGFloat size) {
  return [NSFont fontWithName:@"Menlo-Bold" size:size] ?:
      [NSFont monospacedSystemFontOfSize:size weight:NSFontWeightBold];
}

NSImage* BrandMark() {
  static NSImage* image = nil;
  static dispatch_once_t once;
  dispatch_once(&once, ^{
    NSString* path = [[NSBundle mainBundle] pathForResource:@"FireballMeteorMark"
                                                     ofType:@"png"];
    if (path != nil) {
      image = [[NSImage alloc] initWithContentsOfFile:path];
    }
  });
  return image;
}

void DrawBrandMark(NSRect rect) {
  NSImage* image = BrandMark();
  if (image == nil) {
    return;
  }
  [image drawInRect:rect
           fromRect:NSZeroRect
          operation:NSCompositingOperationSourceOver
           fraction:1.0
     respectFlipped:YES
              hints:@{NSImageHintInterpolation : @(NSImageInterpolationHigh)}];
}

void RoundedRect(NSRect rect,
                 CGFloat radius,
                 NSColor* fill,
                 NSColor* stroke = nil,
                 CGFloat line_width = 1.0) {
  NSBezierPath* path = [NSBezierPath bezierPathWithRoundedRect:rect
                                                      xRadius:radius
                                                      yRadius:radius];
  [fill setFill];
  [path fill];
  if (stroke != nil) {
    [stroke setStroke];
    [path setLineWidth:line_width];
    [path stroke];
  }
}

void Line(NSPoint from, NSPoint to, NSColor* color, CGFloat width = 1.0) {
  NSBezierPath* path = [NSBezierPath bezierPath];
  [path moveToPoint:from];
  [path lineToPoint:to];
  [path setLineWidth:width];
  [color setStroke];
  [path stroke];
}

void Text(NSString* value,
          NSRect rect,
          NSFont* font,
          NSColor* color,
          NSTextAlignment alignment = NSTextAlignmentLeft) {
  NSMutableParagraphStyle* style = [[NSMutableParagraphStyle alloc] init];
  style.alignment = alignment;
  style.lineBreakMode = NSLineBreakByTruncatingTail;
  NSDictionary* attributes = @{
    NSFontAttributeName : font,
    NSForegroundColorAttributeName : color,
    NSParagraphStyleAttributeName : style,
  };
  if (gCapturingPreview) {
    [NSGraphicsContext saveGraphicsState];
    NSAffineTransform* text_flip = [NSAffineTransform transform];
    [text_flip translateXBy:0 yBy:NSMinY(rect) * 2 + NSHeight(rect)];
    [text_flip scaleXBy:1 yBy:-1];
    [text_flip concat];
    [value drawInRect:rect withAttributes:attributes];
    [NSGraphicsContext restoreGraphicsState];
  } else {
    [value drawInRect:rect withAttributes:attributes];
  }
}

NSString* LayoutName(TabLayout layout) {
  switch (layout) {
    case TabLayout::kChromiumClassic:
      return @"CLASSIC";
    case TabLayout::kSafariFloating:
      return @"FLOATING";
    case TabLayout::kVerticalSidebar:
      return @"VERTICAL";
    case TabLayout::kTabGrid:
      return @"GRID";
  }
}

NSString* PlacementName(TabPlacement placement) {
  switch (placement) {
    case TabPlacement::kFavorite:
      return @"FAVORITE";
    case TabPlacement::kPinned:
      return @"PINNED";
    case TabPlacement::kToday:
      return @"TODAY";
  }
}

NSString* ResidencyName(TabResidency residency) {
  return residency == TabResidency::kLoaded ? @"LIVE" : @"SLEEP";
}

NSString* TransferStateName(TransferState state) {
  switch (state) {
    case TransferState::kQueued:
      return @"QUEUED";
    case TransferState::kActive:
      return @"ACTIVE";
    case TransferState::kPaused:
      return @"PAUSED";
    case TransferState::kComplete:
      return @"COMPLETE";
    case TransferState::kFailed:
      return @"FAILED";
    case TransferState::kCancelled:
      return @"CANCELLED";
  }
  return @"UNKNOWN";
}

NSString* TransferKindName(const TransferItem& item) {
  if (item.source_kind == TransferSourceKind::kMagnet ||
      item.source_kind == TransferSourceKind::kTorrentMetainfo) {
    return @"TORRENT";
  }
  switch (item.media_kind) {
    case MediaCandidateKind::kDirectAudio:
      return @"DIRECT AUDIO";
    case MediaCandidateKind::kDirectVideo:
      return @"DIRECT VIDEO";
    case MediaCandidateKind::kHlsManifest:
      return @"HLS";
    case MediaCandidateKind::kDashManifest:
      return @"DASH";
    case MediaCandidateKind::kNone:
      return @"HTTP";
  }
  return @"HTTP";
}

NSString* FormatByteCount(std::uint64_t bytes) {
  constexpr double kMiB = 1024.0 * 1024.0;
  constexpr double kGiB = 1024.0 * 1024.0 * 1024.0;
  if (bytes >= static_cast<std::uint64_t>(kGiB)) {
    return [NSString stringWithFormat:@"%.1f GB", bytes / kGiB];
  }
  return [NSString stringWithFormat:@"%.0f MB", bytes / kMiB];
}

std::optional<TabLayout> ParseLayout(NSString* name) {
  NSString* normalized = name.lowercaseString;
  if ([normalized isEqualToString:@"classic"]) {
    return TabLayout::kChromiumClassic;
  }
  if ([normalized isEqualToString:@"floating"]) {
    return TabLayout::kSafariFloating;
  }
  if ([normalized isEqualToString:@"vertical"]) {
    return TabLayout::kVerticalSidebar;
  }
  if ([normalized isEqualToString:@"grid"]) {
    return TabLayout::kTabGrid;
  }
  return std::nullopt;
}

class PreviewTransferBackend final : public TransferBackend {
 public:
  Aria2RpcResult<std::string> Enqueue(
      const TransferRequest& request) override {
    const bool peer_to_peer =
        request.source_kind == TransferSourceKind::kMagnet ||
        request.source_kind == TransferSourceKind::kTorrentMetainfo;
    const std::string gid =
        peer_to_peer ? "0123456789abcde1" : "0123456789abcde0";
    Aria2TransferStatus status;
    status.gid = gid;
    status.state = peer_to_peer ? Aria2TransferState::kPaused
                                : Aria2TransferState::kActive;
    status.total_bytes = peer_to_peer ? 4'294'967'296ULL : 1'610'612'736ULL;
    status.completed_bytes =
        peer_to_peer ? 1'073'741'824ULL : 1'095'217'152ULL;
    status.bytes_per_second = peer_to_peer ? 0 : 18'874'368ULL;
    statuses_[gid] = std::move(status);
    return {gid, {}};
  }

  Aria2RpcResult<Aria2TransferStatus> TellStatus(
      std::string_view gid) override {
    auto status = statuses_.find(gid);
    if (status == statuses_.end()) {
      return {std::nullopt, "unknown preview transfer"};
    }
    return {status->second, {}};
  }

  Aria2RpcResult<std::string> Pause(std::string_view gid) override {
    return ChangeState(gid, Aria2TransferState::kPaused);
  }

  Aria2RpcResult<std::string> Unpause(std::string_view gid) override {
    return ChangeState(gid, Aria2TransferState::kActive);
  }

  Aria2RpcResult<std::string> Remove(std::string_view gid) override {
    return ChangeState(gid, Aria2TransferState::kRemoved);
  }

  Aria2RpcResult<std::string> ForgetDownloadResult(
      std::string_view gid) override {
    if (statuses_.erase(std::string(gid)) != 1) {
      return {std::nullopt, "unknown preview transfer"};
    }
    return {std::string("OK"), {}};
  }

 private:
  Aria2RpcResult<std::string> ChangeState(std::string_view gid,
                                          Aria2TransferState state) {
    auto status = statuses_.find(gid);
    if (status == statuses_.end()) {
      return {std::nullopt, "unknown preview transfer"};
    }
    status->second.state = state;
    status->second.bytes_per_second = 0;
    return {std::string(gid), {}};
  }

  std::map<std::string, Aria2TransferStatus, std::less<>> statuses_;
};

class PreviewModel final {
 public:
  PreviewModel() {
    persistent_profile_ = ParseId<ProfileId>(
        "10000000-0000-4000-8000-000000000001");
    burner_profile_ = ParseId<ProfileId>(
        "10000000-0000-4000-8000-000000000002");
    main_space_ = ParseId<SpaceId>("20000000-0000-4000-8000-000000000001");
    research_space_ =
        ParseId<SpaceId>("20000000-0000-4000-8000-000000000002");
    burner_space_ =
        ParseId<SpaceId>("20000000-0000-4000-8000-000000000003");

    model_.AddProfile(*persistent_profile_, StorageMode::kPersistent);
    model_.AddProfile(*burner_profile_, StorageMode::kOffTheRecord);
    model_.AddSpace(*main_space_, *persistent_profile_, SpaceKind::kRegular);
    model_.AddSpace(*research_space_, *persistent_profile_,
                    SpaceKind::kRegular);
    model_.AddSpace(*burner_space_, *burner_profile_, SpaceKind::kBurner);

    AddTab("30000000-0000-4000-8000-000000000001", *main_space_,
           "https://fireball.example/architecture", "Architecture", false,
           TabPlacement::kFavorite);
    AddTab("30000000-0000-4000-8000-000000000002", *main_space_,
           "https://chromium.googlesource.com/chromium/src", "Chromium",
           false, TabPlacement::kPinned);
    AddTab("30000000-0000-4000-8000-000000000003", *main_space_,
           "https://github.com/brave/brave-core", "Brave Shields", true,
           TabPlacement::kPinned);
    const TabId helium = AddTab(
        "30000000-0000-4000-8000-000000000004", *main_space_,
        "https://github.com/imputnet/helium", "Helium provenance", false,
        TabPlacement::kToday);
    AddTab("30000000-0000-4000-8000-000000000005", *research_space_,
           "https://chromiumdash.appspot.com/releases", "Security releases",
           true, TabPlacement::kPinned);
    AddTab("30000000-0000-4000-8000-000000000006", *burner_space_,
           "https://private.invalid/", "Burner tab", true,
           TabPlacement::kToday);
    AddTab("30000000-0000-4000-8000-000000000007", *main_space_,
           "https://fireball.example/transfers", "Transfer deck", false,
           TabPlacement::kToday);
    if (!model_.MarkTabDiscarded(helium)) {
      std::abort();
    }

    auto video = fireball::transfer::MakeUriTransferRequest(
        "https://media.example.test/fireball-launch.mp4?signature=preview",
        TransferPersistence::kPersistent, "Fireball launch.mp4");
    auto torrent = fireball::transfer::MakeUriTransferRequest(
        "magnet:?xt=urn:btih:0123456789abcdef0123456789abcdef01234567",
        TransferPersistence::kPersistent);
    if (!video.has_value() || !torrent.has_value() ||
        !transfers_.Enqueue("40000000-0000-4000-8000-000000000001", *video,
                            "Fireball launch.mp4",
                            MediaCandidateKind::kDirectVideo) ||
        !transfers_.Enqueue("40000000-0000-4000-8000-000000000002",
                            *torrent, "Linux image.torrent") ||
        transfers_.RefreshAll() != 2) {
      std::abort();
    }
  }

  BrowserModel& model() { return model_; }
  const BrowserModel& model() const { return model_; }
  const SpaceId& main_space() const { return *main_space_; }
  const SpaceId& research_space() const { return *research_space_; }
  const SpaceId& burner_space() const { return *burner_space_; }
  const TransferQueue& transfers() const { return transfers_; }

 private:
  TabId AddTab(const char* id,
               const SpaceId& space,
               const char* url,
               const char* title,
               bool activate,
               TabPlacement placement) {
    TabId tab_id = ParseId<TabId>(id);
    if (!model_.AddTab(tab_id, space, url, title, activate, placement)) {
      std::abort();
    }
    return tab_id;
  }

  PreviewTransferBackend transfer_backend_;
  TransferQueue transfers_{&transfer_backend_,
                           TransferPersistence::kPersistent};
  BrowserModel model_;
  std::optional<ProfileId> persistent_profile_;
  std::optional<ProfileId> burner_profile_;
  std::optional<SpaceId> main_space_;
  std::optional<SpaceId> research_space_;
  std::optional<SpaceId> burner_space_;
};

}  // namespace

@interface FireballPreviewView : NSView {
 @private
  PreviewModel _preview;
  TabLayout _layout;
  BOOL _showTransfers;
  NSInteger _hoveredLayoutIndex;
  BOOL _transferHovered;
  NSTrackingArea* _trackingArea;
}
- (instancetype)initWithFrame:(NSRect)frame
                        layout:(TabLayout)layout
                 showTransfers:(BOOL)showTransfers;
@end

@implementation FireballPreviewView

- (instancetype)initWithFrame:(NSRect)frame
                        layout:(TabLayout)layout
                 showTransfers:(BOOL)showTransfers {
  self = [super initWithFrame:frame];
  if (self != nil) {
    _layout = layout;
    _showTransfers = showTransfers;
    _hoveredLayoutIndex = -1;
    _transferHovered = NO;
    _preview.model().SetTabLayout(layout);
    self.accessibilityRole = NSAccessibilityGroupRole;
    self.accessibilityLabel = @"Fireball Blink tab-layout model preview";
    self.accessibilityHelp =
        @"Use keys 1 through 4 to switch layouts and D to toggle transfers.";
  }
  return self;
}

- (BOOL)isFlipped {
  return YES;
}

- (BOOL)acceptsFirstResponder {
  return YES;
}

- (void)viewDidMoveToWindow {
  [super viewDidMoveToWindow];
  [self.window makeFirstResponder:self];
}

- (void)resetCursorRects {
  [super resetCursorRects];
  const CGFloat scale_x = self.bounds.size.width / kCanvasWidth;
  const CGFloat scale_y = self.bounds.size.height / kCanvasHeight;
  [self addCursorRect:NSMakeRect(493 * scale_x, 20 * scale_y, 464 * scale_x,
                                 38 * scale_y)
               cursor:NSCursor.pointingHandCursor];
  [self addCursorRect:NSMakeRect(1162 * scale_x, 106 * scale_y,
                                 108 * scale_x, 42 * scale_y)
               cursor:NSCursor.pointingHandCursor];
}

- (void)updateTrackingAreas {
  [super updateTrackingAreas];
  if (_trackingArea != nil) {
    [self removeTrackingArea:_trackingArea];
  }
  _trackingArea = [[NSTrackingArea alloc]
      initWithRect:NSZeroRect
           options:NSTrackingMouseMoved | NSTrackingMouseEnteredAndExited |
                   NSTrackingActiveInKeyWindow | NSTrackingInVisibleRect
             owner:self
          userInfo:nil];
  [self addTrackingArea:_trackingArea];
}

- (NSPoint)canvasPointForEvent:(NSEvent*)event {
  NSPoint point = [self convertPoint:event.locationInWindow fromView:nil];
  point.x *= kCanvasWidth / self.bounds.size.width;
  point.y *= kCanvasHeight / self.bounds.size.height;
  return point;
}

- (void)mouseMoved:(NSEvent*)event {
  const NSPoint point = [self canvasPointForEvent:event];
  NSInteger layout_index = -1;
  if (point.y >= 20.0 && point.y <= 58.0 && point.x >= 493.0 &&
      point.x < 957.0) {
    layout_index = static_cast<NSInteger>((point.x - 493.0) / 116.0);
  }
  const BOOL transfer_hovered =
      point.x >= 1162.0 && point.x <= 1270.0 && point.y >= 106.0 &&
      point.y <= 148.0;
  if (layout_index != _hoveredLayoutIndex ||
      transfer_hovered != _transferHovered) {
    _hoveredLayoutIndex = layout_index;
    _transferHovered = transfer_hovered;
    self.needsDisplay = YES;
  }
}

- (void)mouseExited:(NSEvent*)event {
  (void)event;
  _hoveredLayoutIndex = -1;
  _transferHovered = NO;
  self.needsDisplay = YES;
}

- (void)selectLayoutAtIndex:(NSInteger)index {
  const TabLayout layouts[] = {
      TabLayout::kChromiumClassic,
      TabLayout::kSafariFloating,
      TabLayout::kVerticalSidebar,
      TabLayout::kTabGrid,
  };
  if (index < 0 || index >= 4) {
    return;
  }
  _layout = layouts[index];
  _preview.model().SetTabLayout(_layout);
  self.needsDisplay = YES;
}

- (void)keyDown:(NSEvent*)event {
  NSString* characters = event.charactersIgnoringModifiers;
  if (characters.length == 1) {
    unichar key = [characters characterAtIndex:0];
    if (key >= '1' && key <= '4') {
      [self selectLayoutAtIndex:key - '1'];
      return;
    }
    if (key == NSLeftArrowFunctionKey || key == NSRightArrowFunctionKey) {
      NSInteger current = static_cast<NSInteger>(_layout);
      NSInteger delta = key == NSLeftArrowFunctionKey ? -1 : 1;
      [self selectLayoutAtIndex:(current + delta + 4) % 4];
      return;
    }
    if (key == 'd' || key == 'D') {
      _showTransfers = !_showTransfers;
      self.needsDisplay = YES;
      return;
    }
  }
  [super keyDown:event];
}

- (void)drawRect:(NSRect)dirtyRect {
  [super drawRect:dirtyRect];
  NSGraphicsContext.currentContext.imageInterpolation =
      NSImageInterpolationHigh;

  NSAffineTransform* transform = [NSAffineTransform transform];
  [transform scaleXBy:self.bounds.size.width / kCanvasWidth
                  yBy:self.bounds.size.height / kCanvasHeight];
  [transform concat];

  [RGB(0x060806) setFill];
  NSRectFill(NSMakeRect(0, 0, kCanvasWidth, kCanvasHeight));
  [self drawAtmosphere];
  [self drawHeader];
  [self drawNavigationRail];
  [self drawBrowserStage];
}

- (void)mouseDown:(NSEvent*)event {
  const NSPoint point = [self canvasPointForEvent:event];
  const CGFloat segment_x = 493.0;
  const CGFloat segment_width = 116.0;
  if (point.y >= 20.0 && point.y <= 58.0 && point.x >= segment_x &&
      point.x < segment_x + segment_width * 4.0) {
    NSInteger index = (NSInteger)((point.x - segment_x) / segment_width);
    [self selectLayoutAtIndex:index];
    return;
  }
  if (point.x >= 1162.0 && point.x <= 1270.0 && point.y >= 106.0 &&
      point.y <= 148.0) {
    _showTransfers = !_showTransfers;
    self.needsDisplay = YES;
  }
}

- (void)drawAtmosphere {
  NSColor* grid = RGB(0x2A3129, 0.15);
  for (CGFloat x = 0; x < kCanvasWidth; x += 48) {
    Line(NSMakePoint(x, 0), NSMakePoint(x, kCanvasHeight), grid, 0.5);
  }
  for (CGFloat y = 0; y < kCanvasHeight; y += 48) {
    Line(NSMakePoint(0, y), NSMakePoint(kCanvasWidth, y), grid, 0.5);
  }

  NSGradient* glow = [[NSGradient alloc]
      initWithStartingColor:RGB(0xFF5A1F, 0.11)
             endingColor:RGB(0xFF5A1F, 0.0)];
  [glow drawFromCenter:NSMakePoint(1140, 120)
                radius:18
              toCenter:NSMakePoint(1140, 120)
                radius:420
               options:0];

  NSBezierPath* trajectory = [NSBezierPath bezierPath];
  [trajectory moveToPoint:NSMakePoint(80, 856)];
  [trajectory curveToPoint:NSMakePoint(1320, 82)
             controlPoint1:NSMakePoint(470, 720)
             controlPoint2:NSMakePoint(920, 226)];
  [trajectory setLineWidth:1.0];
  CGFloat dash[] = {6, 10};
  [trajectory setLineDash:dash count:2 phase:0];
  [RGB(0xFF5A1F, 0.17) setStroke];
  [trajectory stroke];
}

- (void)drawHeader {
  [RGB(0x090C09, 0.98) setFill];
  NSRectFill(NSMakeRect(0, 0, kCanvasWidth, 78));
  Line(NSMakePoint(0, 77), NSMakePoint(kCanvasWidth, 77), RGB(0x283128));

  DrawBrandMark(NSMakeRect(20, 10, 58, 58));
  Text(@"FIREBALL", NSMakeRect(86, 16, 150, 30), DisplayFont(25),
       RGB(0xF4F1E8));
  Text(@"BLINK / ORBITAL", NSMakeRect(88, 45, 168, 18), MonoFont(9),
       RGB(0xB8FF3D));
  Text(@"MACOS MODEL PREVIEW", NSMakeRect(286, 28, 188, 24), MonoFont(10),
       RGB(0x7F887F));

  const TabLayout layouts[] = {
      TabLayout::kChromiumClassic,
      TabLayout::kSafariFloating,
      TabLayout::kVerticalSidebar,
      TabLayout::kTabGrid,
  };
  for (NSInteger index = 0; index < 4; ++index) {
    NSRect segment = NSMakeRect(493 + index * 116, 20, 108, 38);
    const bool selected = layouts[index] == _layout;
    const bool hovered = index == _hoveredLayoutIndex;
    RoundedRect(segment, 9,
                selected ? RGB(0xB8FF3D)
                         : (hovered ? RGB(0x1A241A) : RGB(0x101510)),
                selected ? RGB(0xB8FF3D) : RGB(0x2B342B));
    Text(LayoutName(layouts[index]), NSInsetRect(segment, 4, 9), MonoFont(10),
         selected ? RGB(0x071007) : RGB(0xA8B0A6), NSTextAlignmentCenter);
  }

  RoundedRect(NSMakeRect(974, 20, 190, 38), 9, RGB(0x121812),
              RGB(0x303A30));
  RoundedRect(NSMakeRect(988, 35, 8, 8), 4, RGB(0xB8FF3D));
  Text(@"DOMAIN MODEL READY", NSMakeRect(1004, 30, 150, 22), MonoFont(9),
       RGB(0xD1D5CE));

  RoundedRect(NSMakeRect(1176, 20, 236, 38), 9, RGB(0x21110B),
              RGB(0x813513));
  Text(@"NO CHROMIUM ENGINE", NSMakeRect(1188, 30, 212, 22), MonoFont(9),
       RGB(0xFF9A6A), NSTextAlignmentCenter);
}

- (void)drawNavigationRail {
  const NSRect rail = NSMakeRect(22, 98, 250, 774);
  RoundedRect(rail, 16, RGB(0x0B0F0C, 0.98), RGB(0x283128));

  Text(@"01 / FLIGHT PROFILE", NSMakeRect(42, 120, 190, 22), MonoFont(10),
       RGB(0xFF7A3D));
  RoundedRect(NSMakeRect(40, 150, 214, 86), 12, RGB(0x151A14),
              RGB(0x394239));
  Text(@"PRIMARY", NSMakeRect(56, 166, 130, 25), MonoFont(14),
       RGB(0xF4F1E8));
  Text(@"PERSISTENT · ISOLATED", NSMakeRect(56, 197, 180, 18), MonoFont(8),
       RGB(0xA8B0A6));
  Text(@"01", NSMakeRect(202, 165, 34, 24), MonoFont(12), RGB(0xB8FF3D),
       NSTextAlignmentRight);

  Text(@"02 / ORBITS", NSMakeRect(42, 260, 180, 22), MonoFont(10),
       RGB(0xFF7A3D));
  const Space* burner = _preview.model().FindSpace(_preview.burner_space());
  [self drawSpace:@"MAIN"
            count:_preview.model().VisibleTabOrder(_preview.main_space()).size()
                y:290
         selected:YES
           burner:NO];
  [self drawSpace:@"RESEARCH"
            count:_preview.model()
                      .VisibleTabOrder(_preview.research_space())
                      .size()
                y:344
         selected:NO
           burner:NO];
  [self drawSpace:@"BURNER"
            count:burner == nullptr ? 0 : burner->tab_order.size()
                y:398
         selected:NO
           burner:YES];

  Text(@"03 / TAB LIFECYCLE", NSMakeRect(42, 474, 190, 22), MonoFont(10),
       RGB(0xFF7A3D));
  RoundedRect(NSMakeRect(40, 506, 214, 114), 12, RGB(0x111611),
              RGB(0x394239));
  size_t loaded = 0;
  size_t sleeping = 0;
  for (const TabId& id :
       _preview.model().VisibleTabOrder(_preview.main_space())) {
    const fireball::browser::Tab* tab = _preview.model().FindTab(id);
    if (tab != nullptr && tab->residency == TabResidency::kDiscarded) {
      ++sleeping;
    } else if (tab != nullptr) {
      ++loaded;
    }
  }
  Text([NSString stringWithFormat:@"%zu LIVE · %zu SLEEP", loaded, sleeping],
       NSMakeRect(56, 526, 180, 25), MonoFont(12),
       RGB(0xF4F1E8));
  Text(@"LRU releases background state.\nAudio · capture · forms stay live.",
       NSMakeRect(56, 560, 182, 46), BodyFont(10), RGB(0xA8B0A6));

  Text(@"04 / PRIVATE ROUTES", NSMakeRect(42, 648, 190, 22), MonoFont(10),
       RGB(0xFF7A3D));
  RoundedRect(NSMakeRect(40, 680, 214, 126), 12, RGB(0x111611),
              RGB(0x394239));
  RoundedRect(NSMakeRect(56, 700, 8, 8), 4, RGB(0xB8FF3D));
  Text(@"WARP", NSMakeRect(76, 692, 70, 22), MonoFont(10),
       RGB(0xF4F1E8));
  Text(@"LOCAL PROXY", NSMakeRect(144, 693, 92, 20), MonoFont(7),
       RGB(0x8C978F), NSTextAlignmentRight);
  RoundedRect(NSMakeRect(56, 736, 8, 8), 4, RGB(0xFF7A3D));
  Text(@"TOR", NSMakeRect(76, 728, 70, 22), MonoFont(10), RGB(0xF4F1E8));
  Text(@"SIDECAR", NSMakeRect(144, 729, 92, 20), MonoFont(7),
       RGB(0x8C978F), NSTextAlignmentRight);
  Line(NSMakePoint(56, 762), NSMakePoint(238, 762), RGB(0x2D372F));
  Text(@"CONSENT + LEAK TEST REQUIRED", NSMakeRect(56, 775, 182, 16),
       MonoFont(7), RGB(0xA8B0A6));
  RoundedRect(NSMakeRect(40, 832, 214, 26), 6, RGB(0x21110B), RGB(0x813513));
  Text(@"PREVIEW · NOT A BROWSER BUILD", NSMakeRect(48, 839, 198, 14),
       MonoFont(7), RGB(0xFF9A6A), NSTextAlignmentCenter);
}

- (void)drawSpace:(NSString*)name
            count:(size_t)count
                y:(CGFloat)y
         selected:(BOOL)selected
           burner:(BOOL)burner {
  NSRect rect = NSMakeRect(40, y, 214, 44);
  RoundedRect(rect, 9, selected ? RGB(0x1A241A) : RGB(0x0D120D),
              selected ? RGB(0xB8FF3D, 0.75) : RGB(0x2B342B));
  RoundedRect(NSMakeRect(54, y + 16, 8, 8), 4,
              burner ? RGB(0xFF5A1F) : RGB(0xB8FF3D));
  Text(name, NSMakeRect(72, y + 12, 100, 22), MonoFont(11),
       selected ? RGB(0xF4F1E8) : RGB(0xA8B0A6));
  Text([NSString stringWithFormat:@"%zu", count], NSMakeRect(190, y + 12, 44, 22),
       MonoFont(11), RGB(0xA8B0A6), NSTextAlignmentRight);
}

- (void)drawBrowserStage {
  const NSRect stage = NSMakeRect(292, 98, 1126, 774);
  RoundedRect(stage, 16, RGB(0x0B0F0C, 0.985), RGB(0x293229));

  RoundedRect(NSMakeRect(312, 121, 10, 10), 5, RGB(0xFF5F57));
  RoundedRect(NSMakeRect(329, 121, 10, 10), 5, RGB(0xFEBB2E));
  RoundedRect(NSMakeRect(346, 121, 10, 10), 5, RGB(0x28C840));
  Text(@"‹", NSMakeRect(376, 108, 26, 34), BodyFont(27), RGB(0x657168));
  Text(@"›", NSMakeRect(410, 108, 26, 34), BodyFont(27), RGB(0x657168));
  Text(@"↻", NSMakeRect(444, 110, 28, 30), BodyFont(22), RGB(0xD5DAD6));

  RoundedRect(NSMakeRect(478, 106, 550, 42), 13, RGB(0x151A14),
              RGB(0x3A4339));
  RoundedRect(NSMakeRect(494, 123, 8, 8), 4, RGB(0xB8FF3D));
  Text(@"fireball://architecture", NSMakeRect(514, 117, 386, 24),
       MonoFont(12), RGB(0xD1D5CE));
  Text(@"PRIVATE PROFILE", NSMakeRect(902, 118, 106, 22), MonoFont(7),
       RGB(0x727C72), NSTextAlignmentRight);

  RoundedRect(NSMakeRect(1042, 106, 108, 42), 11, RGB(0x182117),
              RGB(0x455542));
  Text(@"BLOCKER", NSMakeRect(1052, 118, 88, 20), MonoFont(8),
       RGB(0xB8FF3D), NSTextAlignmentCenter);
  RoundedRect(NSMakeRect(1162, 106, 108, 42), 11,
              _showTransfers
                  ? RGB(0xF4F1E8)
                  : (_transferHovered ? RGB(0x1A241A) : RGB(0x121812)),
              _showTransfers ? nil : RGB(0x3A4339));
  Text(@"TRANSFER 02", NSMakeRect(1172, 118, 88, 20), MonoFont(7),
       _showTransfers ? RGB(0x07100A) : RGB(0xD1D5CE),
       NSTextAlignmentCenter);
  RoundedRect(NSMakeRect(1282, 106, 112, 42), 11, RGB(0x21110B),
              RGB(0x813513));
  Text(@"B0 / GATED", NSMakeRect(1292, 118, 92, 20), MonoFont(8),
       RGB(0xFF9A6A), NSTextAlignmentCenter);

  switch (_layout) {
    case TabLayout::kChromiumClassic:
      [self drawClassicLayout];
      break;
    case TabLayout::kSafariFloating:
      [self drawFloatingLayout];
      break;
    case TabLayout::kVerticalSidebar:
      [self drawVerticalLayout];
      break;
    case TabLayout::kTabGrid:
      [self drawGridLayout];
      break;
  }
  if (_showTransfers) {
    [self drawTransferDeck];
  }
}

- (NSArray<NSDictionary<NSString*, NSString*>*>*)tabs {
  const Space* space = _preview.model().FindSpace(_preview.main_space());
  if (space == nullptr) {
    return @[];
  }
  NSMutableArray* result = [[NSMutableArray alloc] init];
  for (const TabId& id :
       _preview.model().VisibleTabOrder(_preview.main_space())) {
    const fireball::browser::Tab* tab = _preview.model().FindTab(id);
    if (tab == nullptr) {
      continue;
    }
    [result addObject:@{
      @"title" : [NSString stringWithUTF8String:tab->title.c_str()],
      @"url" : [NSString stringWithUTF8String:tab->url.c_str()],
      @"active" : space->active_tab == id ? @"1" : @"0",
      @"placement" : PlacementName(tab->placement),
      @"residency" : ResidencyName(tab->residency),
    }];
  }
  return result;
}

- (void)drawClassicLayout {
  NSArray* tabs = [self tabs];
  RoundedRect(NSMakeRect(300, 164, 1098, 60), 12, RGB(0x0E1712),
              RGB(0x29382F));
  CGFloat x = 310;
  for (NSDictionary* tab in tabs) {
    const BOOL active = [tab[@"active"] isEqualToString:@"1"];
    const BOOL sleeping = [tab[@"residency"] isEqualToString:@"SLEEP"];
    NSRect rect = NSMakeRect(x, 173, 206, 42);
    RoundedRect(rect, 9, active ? RGB(0x1A2820) : RGB(0x101713),
                active ? RGB(0xB8FF3D) : nil);
    Text(tab[@"title"], NSMakeRect(x + 12, 179, 126, 18), BodyFont(10),
         active ? RGB(0xF4F1E8) : RGB(0x89958D));
    Text(sleeping ? @"SLEEP" : tab[@"placement"],
         NSMakeRect(x + 140, 180, 54, 16), MonoFont(6),
         sleeping ? RGB(0xFF9A6A) : RGB(0x657168), NSTextAlignmentRight);
    Text(tab[@"url"], NSMakeRect(x + 12, 198, 180, 12), MonoFont(6),
         RGB(0x5E6962));
    x += 216;
  }
  [self drawEngineBoundary:NSMakeRect(300, 236, 1098, 614)
                     label:@"CHROMIUM CLASSIC"];
}

- (void)drawFloatingLayout {
  const NSRect viewport = NSMakeRect(300, 218, 1098, 632);
  [self drawEngineBoundary:viewport label:@"SAFARI FLOATING"];
  NSArray* tabs = [self tabs];
  CGFloat x = 316;
  for (NSDictionary* tab in tabs) {
    const BOOL active = [tab[@"active"] isEqualToString:@"1"];
    const BOOL sleeping = [tab[@"residency"] isEqualToString:@"SLEEP"];
    NSRect rect = NSMakeRect(x, 174, 198, 40);
    RoundedRect(rect, 14, active ? RGB(0xF4F1E8) : RGB(0x151F19, 0.98),
                active ? nil : RGB(0x3A4A40));
    RoundedRect(NSMakeRect(x + 12, 190, 7, 7), 3.5,
                sleeping ? RGB(0xFF7A3D) : RGB(0xB8FF3D));
    Text(tab[@"title"], NSMakeRect(x + 28, 184, 118, 18), BodyFont(9),
         active ? RGB(0x07100A) : RGB(0xC3CBC5));
    Text(sleeping ? @"SLEEP" : tab[@"placement"],
         NSMakeRect(x + 142, 185, 44, 16), MonoFont(6),
         active ? RGB(0x344035) : RGB(0x718078), NSTextAlignmentRight);
    x += 214;
  }
}

- (void)drawVerticalLayout {
  RoundedRect(NSMakeRect(300, 164, 300, 686), 15, RGB(0x0E1712),
              RGB(0x29382F));
  Text(@"MAIN / PRIMARY", NSMakeRect(322, 186, 180, 20), DisplayFont(15),
       RGB(0xF4F1E8));
  Text(@"ARC-STYLE TAB LIBRARY", NSMakeRect(322, 211, 210, 18), MonoFont(7),
       RGB(0x718078));
  NSArray* tabs = [self tabs];
  NSMutableArray* favorites = [[NSMutableArray alloc] init];
  NSMutableArray* pinned = [[NSMutableArray alloc] init];
  NSMutableArray* today = [[NSMutableArray alloc] init];
  for (NSDictionary* tab in tabs) {
    if ([tab[@"placement"] isEqualToString:@"FAVORITE"]) {
      [favorites addObject:tab];
    } else if ([tab[@"placement"] isEqualToString:@"PINNED"]) {
      [pinned addObject:tab];
    } else {
      [today addObject:tab];
    }
  }

  Text(@"FAVORITES · EVERY SPACE", NSMakeRect(322, 246, 240, 18),
       MonoFont(8), RGB(0xFF7A3D));
  CGFloat favorite_x = 322;
  for (NSDictionary* tab in favorites) {
    const BOOL active = [tab[@"active"] isEqualToString:@"1"];
    NSRect tile = NSMakeRect(favorite_x, 272, 58, 58);
    RoundedRect(tile, 14, active ? RGB(0xF4F1E8) : RGB(0x172019),
                active ? RGB(0xB8FF3D) : RGB(0x243129));
    NSString* initial = [tab[@"title"] substringToIndex:1];
    Text(initial, NSMakeRect(favorite_x, 286, 58, 28), DisplayFont(22),
         active ? RGB(0x07100A) : RGB(0xB8FF3D), NSTextAlignmentCenter);
    favorite_x += 68;
  }

  Text(@"PINNED", NSMakeRect(322, 354, 220, 18), MonoFont(8),
       RGB(0x718078));
  CGFloat y = 380;
  for (NSDictionary* tab in pinned) {
    const BOOL active = [tab[@"active"] isEqualToString:@"1"];
    NSRect rect = NSMakeRect(316, y, 268, 56);
    RoundedRect(rect, 11, active ? RGB(0xF4F1E8) : RGB(0x101713),
                active ? nil : RGB(0x243129));
    RoundedRect(NSMakeRect(330, y + 22, 7, 7), 3.5,
                active ? RGB(0xFF5A1F) : RGB(0xB8FF3D));
    Text(tab[@"title"], NSMakeRect(350, y + 11, 164, 20), BodyFont(11),
         active ? RGB(0x07100A) : RGB(0xD0D6D1));
    Text(@"PIN", NSMakeRect(522, y + 12, 44, 18), MonoFont(7),
         active ? RGB(0x4B554C) : RGB(0x657168), NSTextAlignmentRight);
    Text(tab[@"url"], NSMakeRect(350, y + 32, 214, 14), MonoFont(7),
         active ? RGB(0x4B554C) : RGB(0x657168));
    y += 64;
  }

  Text(@"TODAY · AUTO ARCHIVE 12H", NSMakeRect(322, 524, 240, 18),
       MonoFont(8), RGB(0x718078));
  y = 550;
  for (NSDictionary* tab in today) {
    const BOOL active = [tab[@"active"] isEqualToString:@"1"];
    const BOOL sleeping = [tab[@"residency"] isEqualToString:@"SLEEP"];
    NSRect rect = NSMakeRect(316, y, 268, 56);
    RoundedRect(rect, 11, active ? RGB(0x19251E) : RGB(0x101713),
                active ? RGB(0xB8FF3D) : RGB(0x243129));
    RoundedRect(NSMakeRect(330, y + 22, 7, 7), 3.5,
                sleeping ? RGB(0xFF7A3D) : RGB(0x657168));
    Text(tab[@"title"], NSMakeRect(350, y + 11, 154, 20), BodyFont(11),
         active ? RGB(0xF4F1E8) : RGB(0xA0AAA3));
    Text(sleeping ? @"SLEEP" : @"LIVE", NSMakeRect(510, y + 12, 56, 18),
         MonoFont(7), sleeping ? RGB(0xFF9A6A) : RGB(0x8A958D),
         NSTextAlignmentRight);
    Text(tab[@"url"], NSMakeRect(350, y + 32, 214, 14), MonoFont(7),
         RGB(0x657168));
    y += 64;
  }

  Line(NSMakePoint(322, 764), NSMakePoint(578, 764), RGB(0x2B3930));
  Text(@"⌘T  COMMAND BAR", NSMakeRect(322, 784, 180, 20), MonoFont(8),
       RGB(0xA8B0A6));
  Text(@"ARCHIVE 02", NSMakeRect(478, 784, 100, 20), MonoFont(8),
       RGB(0xFF7A3D), NSTextAlignmentRight);

  [self drawEngineBoundary:NSMakeRect(612, 164, 786, 686)
                     label:@"VERTICAL / SAFARI STAGE"];
}

- (void)drawGridLayout {
  RoundedRect(NSMakeRect(300, 164, 1098, 686), 15, RGB(0x09100C),
              RGB(0x29382F));
  Text(@"TAB GRID / MAIN SPACE", NSMakeRect(328, 190, 260, 26), MonoFont(12),
       RGB(0xB8FF3D));
  Text(@"Favorite · Pinned · Today · lifecycle state — one stable tab model.",
       NSMakeRect(328, 220, 620, 24), BodyFont(12), RGB(0x929D95));
  NSArray* tabs = [self tabs];
  NSInteger index = 0;
  for (NSDictionary* tab in tabs) {
    const NSInteger column = index % 3;
    const NSInteger row = index / 3;
    const CGFloat x = 328 + column * 348;
    const CGFloat y = 270 + row * 238;
    const BOOL active = [tab[@"active"] isEqualToString:@"1"];
    const BOOL sleeping = [tab[@"residency"] isEqualToString:@"SLEEP"];
    NSRect card = NSMakeRect(x, y, 332, 218);
    RoundedRect(card, 16, active ? RGB(0x14241A) : RGB(0x101713),
                active ? RGB(0xB8FF3D) : RGB(0x2C3931), active ? 2 : 1);
    Text([NSString stringWithFormat:@"0%ld", (long)index + 1],
         NSMakeRect(x + 22, y + 22, 54, 24), MonoFont(11),
         active ? RGB(0xB8FF3D) : RGB(0x657168));
    Text(tab[@"placement"], NSMakeRect(x + 146, y + 22, 164, 22), MonoFont(8),
         active ? RGB(0xB8FF3D) : RGB(0x657168), NSTextAlignmentRight);
    Text(tab[@"title"], NSMakeRect(x + 22, y + 66, 288, 34),
         DisplayFont(21), RGB(0xF4F1E8));
    Text(tab[@"url"], NSMakeRect(x + 22, y + 108, 288, 22), MonoFont(8),
         RGB(0x87938B));
    Line(NSMakePoint(x + 22, y + 154), NSMakePoint(x + 310, y + 154),
         RGB(0x2B3930));
    RoundedRect(NSMakeRect(x + 22, y + 177, 7, 7), 3.5,
                sleeping ? RGB(0xFF7A3D) : RGB(0xB8FF3D));
    Text(sleeping ? @"DISCARDED · RESTORE ON ACTIVATE"
                  : (active ? @"ACTIVE · PROTECTED" : @"LOADED · LRU ELIGIBLE"),
         NSMakeRect(x + 40, y + 170, 270, 20), MonoFont(7),
         sleeping ? RGB(0xFF9A6A) : RGB(0xAAB4AD));
    ++index;
  }
}

- (void)drawTransferDeck {
  const NSRect panel = NSMakeRect(774, 164, 624, 686);
  RoundedRect(NSOffsetRect(panel, 10, 10), 16, RGB(0x000000, 0.42));
  RoundedRect(panel, 16, RGB(0x0B0F0C), RGB(0x596459), 1.5);
  RoundedRect(NSMakeRect(774, 164, 8, 686), 4, RGB(0xFF5A1F));

  Text(@"TRANSFER DECK", NSMakeRect(806, 190, 300, 38), DisplayFont(25),
       RGB(0xF4F1E8));
  Text(@"ARIA2 QUEUE MODEL · SOURCE URL DROPPED AFTER ENQUEUE",
       NSMakeRect(808, 229, 430, 18), MonoFont(7), RGB(0x8C978F));
  RoundedRect(NSMakeRect(1244, 190, 122, 34), 9, RGB(0x182117),
              RGB(0x455542));
  Text(@"02 TRANSFERS", NSMakeRect(1254, 199, 102, 18), MonoFont(8),
       RGB(0xB8FF3D), NSTextAlignmentCenter);

  const std::vector<TransferItem> transfers = _preview.transfers().Snapshot();
  CGFloat y = 270;
  for (const TransferItem& item : transfers) {
    const bool active = item.state == TransferState::kActive;
    const bool paused = item.state == TransferState::kPaused;
    NSRect card = NSMakeRect(806, y, 560, 140);
    RoundedRect(card, 14, active ? RGB(0x142019) : RGB(0x111611),
                active ? RGB(0xB8FF3D, 0.62) : RGB(0x344036));
    RoundedRect(NSMakeRect(826, y + 21, 8, 8), 4,
                active ? RGB(0xB8FF3D) : RGB(0xFF7A3D));
    Text(TransferKindName(item), NSMakeRect(846, y + 14, 150, 20),
         MonoFont(8), active ? RGB(0xB8FF3D) : RGB(0xFF9A6A));
    Text(TransferStateName(item.state), NSMakeRect(1212, y + 14, 132, 20),
         MonoFont(8), active ? RGB(0xB8FF3D) : RGB(0xA8B0A6),
         NSTextAlignmentRight);
    Text([NSString stringWithUTF8String:item.display_name.c_str()],
         NSMakeRect(826, y + 47, 360, 30), DisplayFont(18), RGB(0xF4F1E8));
    const CGFloat progress =
        item.total_bytes == 0
            ? 0
            : std::min<CGFloat>(
                  1.0, static_cast<CGFloat>(item.completed_bytes) /
                           static_cast<CGFloat>(item.total_bytes));
    RoundedRect(NSMakeRect(826, y + 84, 518, 7), 3.5, RGB(0x283128));
    if (progress > 0) {
      RoundedRect(NSMakeRect(826, y + 84, 518 * progress, 7), 3.5,
                  active ? RGB(0xB8FF3D) : RGB(0xFF7A3D));
    }
    NSString* progress_text = [NSString
        stringWithFormat:@"%@ / %@ · %.0f%%",
                         FormatByteCount(item.completed_bytes),
                         FormatByteCount(item.total_bytes), progress * 100.0];
    Text(progress_text, NSMakeRect(826, y + 103, 310, 18), MonoFont(8),
         RGB(0xA8B0A6));
    NSString* action = active ? @"PAUSE" : (paused ? @"RESUME" : @"OPEN");
    Text(action, NSMakeRect(1242, y + 103, 102, 18), MonoFont(8),
         active ? RGB(0xB8FF3D) : RGB(0xFF9A6A), NSTextAlignmentRight);
    if (item.bytes_per_second > 0) {
      Text([FormatByteCount(item.bytes_per_second)
               stringByAppendingString:@"/s"],
           NSMakeRect(1138, y + 103, 92, 18), MonoFont(8), RGB(0xA8B0A6),
           NSTextAlignmentRight);
    }
    y += 154;
  }

  RoundedRect(NSMakeRect(806, 588, 560, 92), 13, RGB(0x101713),
              RGB(0x344036));
  Text(@"MEDIA DISCOVERY", NSMakeRect(826, 606, 180, 18), MonoFont(8),
       RGB(0xFF7A3D));
  Text(@"DIRECT AUDIO / VIDEO", NSMakeRect(826, 635, 190, 18), MonoFont(8),
       RGB(0xF4F1E8));
  Text(@"READY", NSMakeRect(1006, 635, 60, 18), MonoFont(7), RGB(0xB8FF3D),
       NSTextAlignmentRight);
  Text(@"HLS + DASH", NSMakeRect(1100, 635, 100, 18), MonoFont(8),
       RGB(0xF4F1E8));
  Text(@"DETECTED · GATED", NSMakeRect(1204, 635, 140, 18), MonoFont(7),
       RGB(0xFF9A6A), NSTextAlignmentRight);

  RoundedRect(NSMakeRect(806, 696, 560, 126), 13, RGB(0x21110B),
              RGB(0x813513));
  Text(@"PRIVATE BY CONSTRUCTION", NSMakeRect(826, 716, 260, 20), MonoFont(9),
       RGB(0xFF9A6A));
  Text(@"No source URL in UI snapshots · no uploaded .torrent retained\n"
        "Torrent peers disabled on WARP / Tor proxy routes",
       NSMakeRect(826, 748, 500, 48), BodyFont(10), RGB(0xD3B6A3));
}

- (void)drawEngineBoundary:(NSRect)rect label:(NSString*)label {
  RoundedRect(rect, 15, RGB(0x0C1410), RGB(0x29382F));
  Text(label, NSMakeRect(rect.origin.x + 28, rect.origin.y + 28, 260, 24),
       MonoFont(11), RGB(0xB8FF3D));
  Text(@"FOCUS, THEN FLY.",
       NSMakeRect(rect.origin.x + 28, rect.origin.y + 86,
                  rect.size.width - 56, 62),
       DisplayFont(rect.size.width < 820 ? 38 : 44), RGB(0xF4F1E8));
  Text(@"A Safari-like stage around Arc-style organization, native blocking and a discard-safe lifecycle.\nThis artifact exercises the C++ model; Chromium adapters remain behind B0.",
       NSMakeRect(rect.origin.x + 30, rect.origin.y + 156,
                  rect.size.width - 60, 58),
       BodyFont(rect.size.width < 820 ? 12 : 14), RGB(0x9EA9A1));

  const CGFloat available = rect.size.width - 76;
  const CGFloat card_width = (available - 32) / 3;
  NSArray<NSArray<NSString*>*>* cards = @[
    @[@"BLOCK", @"Native adblock core", @"Pinned adblock-rust · profile policy"],
    @[@"ORGANIZE", @"Favorite / Pin / Today", @"Spaces share only one Profile"],
    @[@"SLEEP", @"Discard-safe LRU", @"Protect audio · capture · forms"],
  ];
  for (NSInteger index = 0; index < 3; ++index) {
    CGFloat x = rect.origin.x + 30 + index * (card_width + 16);
    CGFloat y = rect.origin.y + 252;
    RoundedRect(NSMakeRect(x, y, card_width, 144), 13, RGB(0x121B16),
                RGB(0x314038));
    Text(cards[index][0], NSMakeRect(x + 18, y + 18, card_width - 36, 22),
         MonoFont(9), index == 2 ? RGB(0xFF8D5B) : RGB(0xB8FF3D));
    Text(cards[index][1], NSMakeRect(x + 18, y + 54, card_width - 36, 28),
         DisplayFont(17), RGB(0xF4F1E8));
    Text(cards[index][2], NSMakeRect(x + 18, y + 94, card_width - 36, 34),
         BodyFont(10), RGB(0x87938B));
  }

  RoundedRect(NSMakeRect(rect.origin.x + 30, NSMaxY(rect) - 132,
                         rect.size.width - 60, 96),
              12, RGB(0x241712), RGB(0x6A3C2C));
  Text(@"B0 / FULL CHROMIUM BUILD NOT PRESENT",
       NSMakeRect(rect.origin.x + 52, NSMaxY(rect) - 112,
                  rect.size.width - 104, 24),
       MonoFont(11), RGB(0xFF9A6A));
  Text(@"Model/UI evidence only—not a browser binary, rendered webpage, sandbox, released blocker, or measured memory claim.",
       NSMakeRect(rect.origin.x + 52, NSMaxY(rect) - 78,
                  rect.size.width - 104, 36),
       BodyFont(11), RGB(0xD3B6A3));
}

@end

@interface PreviewAppDelegate : NSObject <NSApplicationDelegate>
@property(nonatomic, strong) NSWindow* window;
@property(nonatomic) TabLayout initialLayout;
@property(nonatomic) BOOL showTransfers;
@end

@implementation PreviewAppDelegate

- (void)applicationDidFinishLaunching:(NSNotification*)notification {
  (void)notification;
  NSRect frame = NSMakeRect(0, 0, kCanvasWidth, kCanvasHeight);
  self.window = [[NSWindow alloc]
      initWithContentRect:frame
                styleMask:NSWindowStyleMaskTitled | NSWindowStyleMaskClosable |
                          NSWindowStyleMaskMiniaturizable |
                          NSWindowStyleMaskResizable
                  backing:NSBackingStoreBuffered
                    defer:NO];
  self.window.title = @"Fireball Blink — macOS Model Preview";
  self.window.minSize = NSMakeSize(960, 600);
  self.window.backgroundColor = RGB(0x070B09);
  self.window.acceptsMouseMovedEvents = YES;
  FireballPreviewView* view =
      [[FireballPreviewView alloc] initWithFrame:frame
                                         layout:self.initialLayout
                                  showTransfers:self.showTransfers];
  view.autoresizingMask = NSViewWidthSizable | NSViewHeightSizable;
  self.window.contentView = view;
  [self.window center];
  [self.window makeKeyAndOrderFront:nil];
  [NSApp activateIgnoringOtherApps:YES];
}

- (BOOL)applicationShouldTerminateAfterLastWindowClosed:(NSApplication*)sender {
  (void)sender;
  return YES;
}

@end

namespace {

bool CapturePreview(NSString* output_path,
                    TabLayout layout,
                    bool show_transfers) {
  NSRect frame = NSMakeRect(0, 0, kCanvasWidth, kCanvasHeight);
  FireballPreviewView* view =
      [[FireballPreviewView alloc] initWithFrame:frame
                                         layout:layout
                                  showTransfers:show_transfers];
  NSBitmapImageRep* image = [[NSBitmapImageRep alloc]
      initWithBitmapDataPlanes:nil
                   pixelsWide:(NSInteger)kCanvasWidth
                   pixelsHigh:(NSInteger)kCanvasHeight
                bitsPerSample:8
              samplesPerPixel:4
                     hasAlpha:YES
                     isPlanar:NO
               colorSpaceName:NSDeviceRGBColorSpace
                  bytesPerRow:0
                 bitsPerPixel:0];
  NSGraphicsContext* context =
      [NSGraphicsContext graphicsContextWithBitmapImageRep:image];
  [NSGraphicsContext saveGraphicsState];
  NSGraphicsContext.currentContext = context;
  NSAffineTransform* flip = [NSAffineTransform transform];
  [flip translateXBy:0 yBy:kCanvasHeight];
  [flip scaleXBy:1 yBy:-1];
  [flip concat];
  gCapturingPreview = true;
  [view drawRect:frame];
  gCapturingPreview = false;
  [context flushGraphics];
  [NSGraphicsContext restoreGraphicsState];
  NSData* png = [image representationUsingType:NSBitmapImageFileTypePNG
                                    properties:@{}];
  return png != nil && [png writeToFile:output_path atomically:YES];
}

}  // namespace

int main(int argc, const char* argv[]) {
  @autoreleasepool {
    NSString* capture_path = nil;
    TabLayout layout = TabLayout::kTabGrid;
    bool show_transfers = false;
    for (int index = 1; index < argc; ++index) {
      NSString* argument = [NSString stringWithUTF8String:argv[index]];
      if ([argument isEqualToString:@"--capture"] && index + 1 < argc) {
        capture_path = [NSString stringWithUTF8String:argv[++index]];
      } else if ([argument isEqualToString:@"--layout"] && index + 1 < argc) {
        auto parsed =
            ParseLayout([NSString stringWithUTF8String:argv[++index]]);
        if (!parsed.has_value()) {
          fprintf(stderr, "unknown layout\n");
          return 2;
        }
        layout = *parsed;
      } else if ([argument isEqualToString:@"--panel"] && index + 1 < argc) {
        NSString* panel = [NSString stringWithUTF8String:argv[++index]];
        if (![panel isEqualToString:@"transfers"]) {
          fprintf(stderr, "unknown panel\n");
          return 2;
        }
        show_transfers = true;
      } else {
        fprintf(stderr,
                "usage: FireballBlinkPreview [--layout classic|floating|vertical|grid] "
                "[--panel transfers] [--capture path.png]\n");
        return 2;
      }
    }

    if (capture_path != nil) {
      return CapturePreview(capture_path, layout, show_transfers) ? 0 : 1;
    }

    [NSApplication sharedApplication];
    PreviewAppDelegate* delegate = [[PreviewAppDelegate alloc] init];
    delegate.initialLayout = layout;
    delegate.showTransfers = show_transfers;
    NSApp.delegate = delegate;
    [NSApp setActivationPolicy:NSApplicationActivationPolicyRegular];
    [NSApp run];
  }
  return 0;
}
