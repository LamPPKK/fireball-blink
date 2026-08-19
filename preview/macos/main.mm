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

  [RGB(0x060806) setFill];
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
  const CGFloat segment_x = 493.0;
  const CGFloat segment_width = 116.0;
  if (point.y >= 20.0 && point.y <= 58.0 && point.x >= segment_x &&
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
    RoundedRect(segment, 9, selected ? RGB(0xB8FF3D) : RGB(0x101510),
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
  const Space* main = _preview.model().FindSpace(_preview.main_space());
  const Space* research =
      _preview.model().FindSpace(_preview.research_space());
  const Space* burner = _preview.model().FindSpace(_preview.burner_space());
  [self drawSpace:@"MAIN"
            count:main == nullptr ? 0 : main->tab_order.size()
                y:290
         selected:YES
           burner:NO];
  [self drawSpace:@"RESEARCH"
            count:research == nullptr ? 0 : research->tab_order.size()
                y:344
         selected:NO
           burner:NO];
  [self drawSpace:@"BURNER"
            count:burner == nullptr ? 0 : burner->tab_order.size()
                y:398
         selected:NO
           burner:YES];

  Text(@"03 / NETWORK LAUNCH", NSMakeRect(42, 486, 190, 22), MonoFont(10),
       RGB(0xFF7A3D));
  RoundedRect(NSMakeRect(40, 518, 214, 108), 12, RGB(0x111611),
              RGB(0x394239));
  Text(@"DEFAULT DENY", NSMakeRect(56, 538, 170, 25), MonoFont(13),
       RGB(0xF4F1E8));
  Text(@"Every startup request needs\nan owner, purpose and opt-in.",
       NSMakeRect(56, 573, 182, 48), BodyFont(11), RGB(0xA8B0A6));

  Line(NSMakePoint(40, 660), NSMakePoint(254, 660), RGB(0x303830));
  Text(@"REFERENCE FLIGHT PLAN", NSMakeRect(42, 680, 196, 20), MonoFont(8),
       RGB(0x727C72));
  Text(@"BRAVE", NSMakeRect(42, 712, 82, 22), MonoFont(12), RGB(0xF4F1E8));
  Text(@"overlay → override → patch", NSMakeRect(42, 736, 198, 20),
       BodyFont(10), RGB(0xA8B0A6));
  Text(@"HELIUM", NSMakeRect(42, 772, 82, 22), MonoFont(12), RGB(0xF4F1E8));
  Text(@"pin → checksum → provenance", NSMakeRect(42, 796, 202, 20),
       BodyFont(10), RGB(0xA8B0A6));
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

  Text(@"‹", NSMakeRect(306, 108, 26, 34), BodyFont(27), RGB(0x657168));
  Text(@"›", NSMakeRect(344, 108, 26, 34), BodyFont(27), RGB(0x657168));
  Text(@"↻", NSMakeRect(388, 110, 28, 30), BodyFont(22), RGB(0xD5DAD6));

  RoundedRect(NSMakeRect(432, 106, 696, 42), 10, RGB(0x151A14),
              RGB(0x3A4339));
  RoundedRect(NSMakeRect(448, 123, 8, 8), 4, RGB(0xB8FF3D));
  Text(@"fireball://architecture", NSMakeRect(468, 117, 540, 24),
       MonoFont(12), RGB(0xD1D5CE));
  Text(@"LOCAL MODEL", NSMakeRect(1006, 118, 102, 22), MonoFont(8),
       RGB(0x727C72), NSTextAlignmentRight);

  RoundedRect(NSMakeRect(1142, 106, 108, 42), 10, RGB(0x182117),
              RGB(0x455542));
  Text(@"SHIELDS", NSMakeRect(1160, 118, 88, 20), MonoFont(9),
       RGB(0xB8FF3D), NSTextAlignmentCenter);
  RoundedRect(NSMakeRect(1262, 106, 132, 42), 10, RGB(0x21110B),
              RGB(0x813513));
  Text(@"B0 / GATED", NSMakeRect(1272, 118, 112, 20), MonoFont(9),
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
                active ? RGB(0xB8FF3D) : nil);
    Text(tab[@"title"], NSInsetRect(rect, 14, 9), BodyFont(11),
         active ? RGB(0xF4F1E8) : RGB(0x89958D));
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
    RoundedRect(rect, 20, active ? RGB(0xB8FF3D) : RGB(0x151F19, 0.95),
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
                active ? RGB(0xB8FF3D) : RGB(0x243129));
    Text(tab[@"title"], NSMakeRect(332, y + 14, 182, 22), BodyFont(12),
         active ? RGB(0xF4F1E8) : RGB(0xA0AAA3));
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
       RGB(0xB8FF3D));
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
                active ? RGB(0xB8FF3D) : RGB(0x2C3931), active ? 2 : 1);
    Text([NSString stringWithFormat:@"0%ld", (long)index + 1],
         NSMakeRect(x + 22, y + 22, 54, 24), MonoFont(11),
         active ? RGB(0xB8FF3D) : RGB(0x657168));
    Text(active ? @"ACTIVE" : @"BACKGROUND",
         NSMakeRect(x + 320, y + 22, 150, 22), MonoFont(9),
         active ? RGB(0xB8FF3D) : RGB(0x657168), NSTextAlignmentRight);
    Text(tab[@"title"], NSMakeRect(x + 22, y + 74, 440, 38),
         DisplayFont(25), RGB(0xF4F1E8));
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
       MonoFont(11), RGB(0xB8FF3D));
  Text(@"ENGINE BOUNDARY", NSMakeRect(rect.origin.x + 28, rect.origin.y + 86,
                                      rect.size.width - 56, 62),
       DisplayFont(44), RGB(0xF4F1E8));
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
         MonoFont(10), index == 2 ? RGB(0xFF8D5B) : RGB(0xB8FF3D));
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
