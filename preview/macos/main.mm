#import <Cocoa/Cocoa.h>

#include <cstdlib>
#include <optional>
#include <string>

#include "fireball/browser/domain_model.h"

namespace {

using fireball::browser::BrowserModel;
using fireball::browser::ProfileId;
using fireball::browser::Space;
using fireball::browser::SpaceId;
using fireball::browser::SpaceKind;
using fireball::browser::StorageMode;
using fireball::browser::TabId;
using fireball::browser::TabLayout;

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
           "https://fireball.example/architecture", "Architecture", false);
    AddTab("30000000-0000-4000-8000-000000000002", *main_space_,
           "https://chromium.googlesource.com/chromium/src", "Chromium", false);
    AddTab("30000000-0000-4000-8000-000000000003", *main_space_,
           "https://github.com/brave/brave-core", "Brave overlay", true);
    AddTab("30000000-0000-4000-8000-000000000004", *main_space_,
           "https://github.com/imputnet/helium", "Helium provenance", false);
    AddTab("30000000-0000-4000-8000-000000000005", *research_space_,
           "https://chromiumdash.appspot.com/releases", "Security releases",
           true);
    AddTab("30000000-0000-4000-8000-000000000006", *burner_space_,
           "https://private.invalid/", "Burner tab", true);
  }

  BrowserModel& model() { return model_; }
  const BrowserModel& model() const { return model_; }
  const SpaceId& main_space() const { return *main_space_; }
  const SpaceId& research_space() const { return *research_space_; }
  const SpaceId& burner_space() const { return *burner_space_; }

 private:
  void AddTab(const char* id,
              const SpaceId& space,
              const char* url,
              const char* title,
              bool activate) {
    model_.AddTab(ParseId<TabId>(id), space, url, title, activate);
  }

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
}
- (instancetype)initWithFrame:(NSRect)frame layout:(TabLayout)layout;
@end

@implementation FireballPreviewView

- (instancetype)initWithFrame:(NSRect)frame layout:(TabLayout)layout {
  self = [super initWithFrame:frame];
  if (self != nil) {
    _layout = layout;
    _preview.model().SetTabLayout(layout);
  }
  return self;
}

- (BOOL)isFlipped {
  return YES;
}

- (BOOL)acceptsFirstResponder {
  return YES;
}

- (void)drawRect:(NSRect)dirtyRect {
  [super drawRect:dirtyRect];
  NSGraphicsContext.currentContext.imageInterpolation =
      NSImageInterpolationHigh;

  NSAffineTransform* transform = [NSAffineTransform transform];
  [transform scaleXBy:self.bounds.size.width / kCanvasWidth
                  yBy:self.bounds.size.height / kCanvasHeight];
  [transform concat];

  [RGB(0x070B09) setFill];
  NSRectFill(NSMakeRect(0, 0, kCanvasWidth, kCanvasHeight));
  [self drawAtmosphere];
  [self drawHeader];
  [self drawNavigationRail];
  [self drawBrowserStage];
}

- (void)mouseDown:(NSEvent*)event {
  NSPoint point = [self convertPoint:event.locationInWindow fromView:nil];
  point.x *= kCanvasWidth / self.bounds.size.width;
  point.y *= kCanvasHeight / self.bounds.size.height;
  const CGFloat segment_x = 514.0;
  const CGFloat segment_width = 111.0;
  if (point.y >= 17.0 && point.y <= 53.0 && point.x >= segment_x &&
      point.x < segment_x + segment_width * 4.0) {
    NSInteger index = (NSInteger)((point.x - segment_x) / segment_width);
    const TabLayout layouts[] = {
        TabLayout::kChromiumClassic,
        TabLayout::kSafariFloating,
        TabLayout::kVerticalSidebar,
        TabLayout::kTabGrid,
    };
    _layout = layouts[index];
    _preview.model().SetTabLayout(_layout);
    self.needsDisplay = YES;
  }
}

- (void)drawAtmosphere {
  NSColor* grid = RGB(0x183025, 0.18);
  for (CGFloat x = 0; x < kCanvasWidth; x += 40) {
    Line(NSMakePoint(x, 0), NSMakePoint(x, kCanvasHeight), grid, 0.5);
  }
  for (CGFloat y = 0; y < kCanvasHeight; y += 40) {
    Line(NSMakePoint(0, y), NSMakePoint(kCanvasWidth, y), grid, 0.5);
  }

  NSGradient* glow = [[NSGradient alloc]
      initWithStartingColor:RGB(0x53F587, 0.10)
             endingColor:RGB(0x53F587, 0.0)];
  [glow drawFromCenter:NSMakePoint(1170, 710)
                radius:20
              toCenter:NSMakePoint(1170, 710)
                radius:360
               options:0];
}

- (void)drawHeader {
  [RGB(0x0A100D, 0.96) setFill];
  NSRectFill(NSMakeRect(0, 0, kCanvasWidth, 70));
  Line(NSMakePoint(0, 69), NSMakePoint(kCanvasWidth, 69), RGB(0x25342B));

  Text(@"FIREBALL", NSMakeRect(28, 18, 165, 36), DisplayFont(27),
       RGB(0xF4F6F2));
  Text(@"// BLINK", NSMakeRect(190, 21, 150, 32), MonoFont(17),
       RGB(0x53F587));
  Text(@"MACOS MODEL PREVIEW", NSMakeRect(330, 23, 185, 28), MonoFont(12),
       RGB(0x849188));

  const TabLayout layouts[] = {
      TabLayout::kChromiumClassic,
      TabLayout::kSafariFloating,
      TabLayout::kVerticalSidebar,
      TabLayout::kTabGrid,
  };
  for (NSInteger index = 0; index < 4; ++index) {
    NSRect segment = NSMakeRect(514 + index * 111, 17, 105, 36);
    const bool selected = layouts[index] == _layout;
    RoundedRect(segment, 10, selected ? RGB(0x53F587) : RGB(0x101813),
                selected ? RGB(0x53F587) : RGB(0x2C3A31));
    Text(LayoutName(layouts[index]), NSInsetRect(segment, 4, 9), MonoFont(10),
         selected ? RGB(0x07100A) : RGB(0xAAB4AD), NSTextAlignmentCenter);
  }

  RoundedRect(NSMakeRect(1000, 17, 185, 36), 18, RGB(0x111A15),
              RGB(0x2C3A31));
  RoundedRect(NSMakeRect(1014, 30, 8, 8), 4, RGB(0x53F587));
  Text(@"MODEL CONNECTED", NSMakeRect(1030, 25, 145, 22), MonoFont(10),
       RGB(0xC8D0CA));

  RoundedRect(NSMakeRect(1196, 17, 216, 36), 18, RGB(0x251713),
              RGB(0x6E3A29));
  Text(@"NO CHROMIUM ENGINE", NSMakeRect(1208, 25, 192, 22), MonoFont(10),
       RGB(0xFFAE73), NSTextAlignmentCenter);
}

- (void)drawNavigationRail {
  const NSRect rail = NSMakeRect(22, 92, 238, 780);
  RoundedRect(rail, 18, RGB(0x0D1410, 0.98), RGB(0x27352C));

  Text(@"01 / PROFILE", NSMakeRect(42, 116, 180, 22), MonoFont(11),
       RGB(0x53F587));
  RoundedRect(NSMakeRect(40, 148, 202, 82), 13, RGB(0x141D17),
              RGB(0x34443A));
  Text(@"PRIMARY", NSMakeRect(56, 164, 130, 25), MonoFont(15),
       RGB(0xF4F6F2));
  Text(@"PERSISTENT / ISOLATED", NSMakeRect(56, 194, 170, 18), MonoFont(9),
       RGB(0x909C94));

  Text(@"02 / SPACES", NSMakeRect(42, 256, 180, 22), MonoFont(11),
       RGB(0x53F587));
  const Space* main = _preview.model().FindSpace(_preview.main_space());
  const Space* research =
      _preview.model().FindSpace(_preview.research_space());
  const Space* burner = _preview.model().FindSpace(_preview.burner_space());
  [self drawSpace:@"MAIN"
            count:main == nullptr ? 0 : main->tab_order.size()
                y:288
         selected:YES
           burner:NO];
  [self drawSpace:@"RESEARCH"
            count:research == nullptr ? 0 : research->tab_order.size()
                y:342
         selected:NO
           burner:NO];
  [self drawSpace:@"BURNER"
            count:burner == nullptr ? 0 : burner->tab_order.size()
                y:396
         selected:NO
           burner:YES];

  Text(@"03 / STARTUP NETWORK", NSMakeRect(42, 486, 185, 22), MonoFont(11),
       RGB(0x53F587));
  RoundedRect(NSMakeRect(40, 518, 202, 102), 13, RGB(0x101814),
              RGB(0x34443A));
  Text(@"DEFAULT DENY", NSMakeRect(56, 538, 170, 25), MonoFont(14),
       RGB(0xF4F6F2));
  Text(@"Every request needs\nan owner + policy.",
       NSMakeRect(56, 574, 165, 42), BodyFont(12), RGB(0x98A49C));

  Line(NSMakePoint(40, 666), NSMakePoint(242, 666), RGB(0x28362D));
  Text(@"REFERENCE DISCIPLINE", NSMakeRect(42, 686, 190, 20), MonoFont(9),
       RGB(0x6E7D73));
  Text(@"BRAVE", NSMakeRect(42, 718, 82, 22), MonoFont(13), RGB(0xF4F6F2));
  Text(@"overlay → override → patch", NSMakeRect(42, 742, 185, 20),
       BodyFont(10), RGB(0x87938B));
  Text(@"HELIUM", NSMakeRect(42, 782, 82, 22), MonoFont(13), RGB(0xF4F6F2));
  Text(@"pin → checksum → provenance", NSMakeRect(42, 806, 188, 20),
       BodyFont(10), RGB(0x87938B));
  Text(@"PREVIEW • NOT A BROWSER BUILD", NSMakeRect(42, 846, 190, 16),
       MonoFont(8), RGB(0xFFAE73));
}

- (void)drawSpace:(NSString*)name
            count:(size_t)count
                y:(CGFloat)y
         selected:(BOOL)selected
           burner:(BOOL)burner {
  NSRect rect = NSMakeRect(40, y, 202, 44);
  RoundedRect(rect, 11, selected ? RGB(0x19241D) : RGB(0x0D1410),
              selected ? RGB(0x53F587, 0.7) : RGB(0x26342B));
  RoundedRect(NSMakeRect(54, y + 16, 8, 8), 4,
              burner ? RGB(0xFF7B48) : RGB(0x53F587));
  Text(name, NSMakeRect(72, y + 12, 100, 22), MonoFont(11),
       selected ? RGB(0xF4F6F2) : RGB(0xAAB4AD));
  Text([NSString stringWithFormat:@"%zu", count], NSMakeRect(178, y + 12, 44, 22),
       MonoFont(11), RGB(0xAAB4AD), NSTextAlignmentRight);
}

- (void)drawBrowserStage {
  const NSRect stage = NSMakeRect(280, 92, 1138, 780);
  RoundedRect(stage, 20, RGB(0x0B110E, 0.98), RGB(0x2A3930));

  Text(@"‹", NSMakeRect(306, 108, 26, 34), BodyFont(27), RGB(0x657168));
  Text(@"›", NSMakeRect(344, 108, 26, 34), BodyFont(27), RGB(0x657168));
  Text(@"↻", NSMakeRect(388, 110, 28, 30), BodyFont(22), RGB(0xD5DAD6));

  RoundedRect(NSMakeRect(432, 106, 706, 42), 12, RGB(0x121B16),
              RGB(0x33433A));
  RoundedRect(NSMakeRect(448, 123, 8, 8), 4, RGB(0x53F587));
  Text(@"fireball://architecture", NSMakeRect(468, 117, 560, 24),
       MonoFont(12), RGB(0xC0C9C2));
  Text(@"LOCAL MODEL", NSMakeRect(1018, 118, 102, 22), MonoFont(9),
       RGB(0x708077), NSTextAlignmentRight);

  RoundedRect(NSMakeRect(1152, 106, 104, 42), 12, RGB(0x152019),
              RGB(0x3B5043));
  Text(@"SHIELDS", NSMakeRect(1160, 118, 88, 20), MonoFont(9),
       RGB(0x53F587), NSTextAlignmentCenter);
  RoundedRect(NSMakeRect(1268, 106, 126, 42), 12, RGB(0x251713),
              RGB(0x71402E));
  Text(@"B0 BLOCKED", NSMakeRect(1278, 118, 106, 20), MonoFont(9),
       RGB(0xFFAE73), NSTextAlignmentCenter);

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
}

- (NSArray<NSDictionary<NSString*, NSString*>*>*)tabs {
  const Space* space = _preview.model().FindSpace(_preview.main_space());
  if (space == nullptr) {
    return @[];
  }
  NSMutableArray* result = [[NSMutableArray alloc] init];
  for (const TabId& id : space->tab_order) {
    const fireball::browser::Tab* tab = _preview.model().FindTab(id);
    if (tab == nullptr) {
      continue;
    }
    [result addObject:@{
      @"title" : [NSString stringWithUTF8String:tab->title.c_str()],
      @"url" : [NSString stringWithUTF8String:tab->url.c_str()],
      @"active" : space->active_tab == id ? @"1" : @"0",
    }];
  }
  return result;
}

- (void)drawClassicLayout {
  NSArray* tabs = [self tabs];
  RoundedRect(NSMakeRect(300, 164, 1098, 54), 12, RGB(0x0E1712),
              RGB(0x29382F));
  CGFloat x = 312;
  for (NSDictionary* tab in tabs) {
    const BOOL active = [tab[@"active"] isEqualToString:@"1"];
    NSRect rect = NSMakeRect(x, 174, 236, 36);
    RoundedRect(rect, 9, active ? RGB(0x1A2820) : RGB(0x101713),
                active ? RGB(0x53F587) : nil);
    Text(tab[@"title"], NSInsetRect(rect, 14, 9), BodyFont(11),
         active ? RGB(0xF4F6F2) : RGB(0x89958D));
    x += 246;
  }
  [self drawEngineBoundary:NSMakeRect(300, 230, 1098, 620)
                     label:@"CHROMIUM CLASSIC"];
}

- (void)drawFloatingLayout {
  const NSRect viewport = NSMakeRect(300, 218, 1098, 632);
  [self drawEngineBoundary:viewport label:@"SAFARI FLOATING"];
  NSArray* tabs = [self tabs];
  CGFloat x = 332;
  for (NSDictionary* tab in tabs) {
    const BOOL active = [tab[@"active"] isEqualToString:@"1"];
    NSRect rect = NSMakeRect(x, 174, 220, 40);
    RoundedRect(rect, 20, active ? RGB(0x53F587) : RGB(0x151F19, 0.95),
                active ? nil : RGB(0x3A4A40));
    Text(tab[@"title"], NSInsetRect(rect, 16, 11), MonoFont(9),
         active ? RGB(0x07100A) : RGB(0xC3CBC5));
    x += 232;
  }
}

- (void)drawVerticalLayout {
  RoundedRect(NSMakeRect(300, 164, 248, 686), 15, RGB(0x0E1712),
              RGB(0x29382F));
  Text(@"OPEN TABS", NSMakeRect(322, 188, 180, 20), MonoFont(10),
       RGB(0x718078));
  NSArray* tabs = [self tabs];
  CGFloat y = 222;
  for (NSDictionary* tab in tabs) {
    const BOOL active = [tab[@"active"] isEqualToString:@"1"];
    NSRect rect = NSMakeRect(316, y, 216, 72);
    RoundedRect(rect, 12, active ? RGB(0x19251E) : RGB(0x101713),
                active ? RGB(0x53F587) : RGB(0x243129));
    Text(tab[@"title"], NSMakeRect(332, y + 14, 182, 22), BodyFont(12),
         active ? RGB(0xF4F6F2) : RGB(0xA0AAA3));
    Text(tab[@"url"], NSMakeRect(332, y + 42, 182, 16), MonoFont(8),
         RGB(0x657168));
    y += 82;
  }
  [self drawEngineBoundary:NSMakeRect(560, 164, 838, 686)
                     label:@"VERTICAL SIDEBAR"];
}

- (void)drawGridLayout {
  RoundedRect(NSMakeRect(300, 164, 1098, 686), 15, RGB(0x09100C),
              RGB(0x29382F));
  Text(@"TAB GRID / MAIN SPACE", NSMakeRect(328, 190, 260, 26), MonoFont(12),
       RGB(0x53F587));
  Text(@"One domain model. Four presentations. No WebContents reload.",
       NSMakeRect(328, 220, 520, 24), BodyFont(12), RGB(0x929D95));
  NSArray* tabs = [self tabs];
  CGFloat x = 328;
  CGFloat y = 270;
  NSInteger index = 0;
  for (NSDictionary* tab in tabs) {
    const BOOL active = [tab[@"active"] isEqualToString:@"1"];
    NSRect card = NSMakeRect(x, y, 498, 236);
    RoundedRect(card, 16, active ? RGB(0x14241A) : RGB(0x101713),
                active ? RGB(0x53F587) : RGB(0x2C3931), active ? 2 : 1);
    Text([NSString stringWithFormat:@"0%ld", (long)index + 1],
         NSMakeRect(x + 22, y + 22, 54, 24), MonoFont(11),
         active ? RGB(0x53F587) : RGB(0x657168));
    Text(active ? @"ACTIVE" : @"BACKGROUND",
         NSMakeRect(x + 320, y + 22, 150, 22), MonoFont(9),
         active ? RGB(0x53F587) : RGB(0x657168), NSTextAlignmentRight);
    Text(tab[@"title"], NSMakeRect(x + 22, y + 74, 440, 38),
         DisplayFont(25), RGB(0xF4F6F2));
    Text(tab[@"url"], NSMakeRect(x + 22, y + 122, 440, 22), MonoFont(9),
         RGB(0x87938B));
    Line(NSMakePoint(x + 22, y + 174), NSMakePoint(x + 476, y + 174),
         RGB(0x2B3930));
    Text(@"MODEL STATE RETAINED", NSMakeRect(x + 22, y + 193, 220, 20),
         MonoFont(9), RGB(0xAAB4AD));
    ++index;
    if (index % 2 == 0) {
      x = 328;
      y += 252;
    } else {
      x = 842;
    }
  }
}

- (void)drawEngineBoundary:(NSRect)rect label:(NSString*)label {
  RoundedRect(rect, 15, RGB(0x0C1410), RGB(0x29382F));
  Text(label, NSMakeRect(rect.origin.x + 28, rect.origin.y + 28, 260, 24),
       MonoFont(11), RGB(0x53F587));
  Text(@"ENGINE BOUNDARY", NSMakeRect(rect.origin.x + 28, rect.origin.y + 86,
                                      rect.size.width - 56, 62),
       DisplayFont(44), RGB(0xF4F6F2));
  Text(@"The macOS artifact exercises Fireball's C++ Profile / Space / Tab model.\nChromium Profile and WebContents adapters are intentionally absent until B0.",
       NSMakeRect(rect.origin.x + 30, rect.origin.y + 156,
                  rect.size.width - 60, 58),
       BodyFont(15), RGB(0x9EA9A1));

  const CGFloat available = rect.size.width - 76;
  const CGFloat card_width = (available - 32) / 3;
  NSArray<NSArray<NSString*>*>* cards = @[
    @[@"PROFILE", @"Storage boundary", @"Persistent + off-the-record"],
    @[@"SPACE", @"Tab collection", @"Multiple spaces / profile"],
    @[@"BURNER", @"Never restorable", @"Off-the-record enforced"],
  ];
  for (NSInteger index = 0; index < 3; ++index) {
    CGFloat x = rect.origin.x + 30 + index * (card_width + 16);
    CGFloat y = rect.origin.y + 252;
    RoundedRect(NSMakeRect(x, y, card_width, 144), 13, RGB(0x121B16),
                RGB(0x314038));
    Text(cards[index][0], NSMakeRect(x + 18, y + 18, card_width - 36, 22),
         MonoFont(10), index == 2 ? RGB(0xFF8D5B) : RGB(0x53F587));
    Text(cards[index][1], NSMakeRect(x + 18, y + 54, card_width - 36, 28),
         DisplayFont(17), RGB(0xF4F6F2));
    Text(cards[index][2], NSMakeRect(x + 18, y + 94, card_width - 36, 34),
         BodyFont(10), RGB(0x87938B));
  }

  RoundedRect(NSMakeRect(rect.origin.x + 30, NSMaxY(rect) - 132,
                         rect.size.width - 60, 96),
              12, RGB(0x241712), RGB(0x6A3C2C));
  Text(@"B0 / FULL CHROMIUM BUILD NOT PRESENT",
       NSMakeRect(rect.origin.x + 52, NSMaxY(rect) - 112,
                  rect.size.width - 104, 24),
       MonoFont(11), RGB(0xFFAE73));
  Text(@"This preview is documentation evidence for domain state and layout direction—not a browser binary, rendered webpage, sandbox, or security-rebase artifact.",
       NSMakeRect(rect.origin.x + 52, NSMaxY(rect) - 78,
                  rect.size.width - 104, 36),
       BodyFont(11), RGB(0xD3B6A3));
}

@end

@interface PreviewAppDelegate : NSObject <NSApplicationDelegate>
@property(nonatomic, strong) NSWindow* window;
@property(nonatomic) TabLayout initialLayout;
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
  FireballPreviewView* view =
      [[FireballPreviewView alloc] initWithFrame:frame layout:self.initialLayout];
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

bool CapturePreview(NSString* output_path, TabLayout layout) {
  NSRect frame = NSMakeRect(0, 0, kCanvasWidth, kCanvasHeight);
  FireballPreviewView* view =
      [[FireballPreviewView alloc] initWithFrame:frame layout:layout];
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
      } else {
        fprintf(stderr,
                "usage: FireballBlinkPreview [--layout classic|floating|vertical|grid] "
                "[--capture path.png]\n");
        return 2;
      }
    }

    if (capture_path != nil) {
      return CapturePreview(capture_path, layout) ? 0 : 1;
    }

    [NSApplication sharedApplication];
    PreviewAppDelegate* delegate = [[PreviewAppDelegate alloc] init];
    delegate.initialLayout = layout;
    NSApp.delegate = delegate;
    [NSApp setActivationPolicy:NSApplicationActivationPolicyRegular];
    [NSApp run];
  }
  return 0;
}
