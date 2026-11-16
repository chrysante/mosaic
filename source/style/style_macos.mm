#import <AppKit/AppKit.h>

#include "style/style.h"

using namespace mosaic;

StyleType mosaic::get_current_system_style_type() {
    NSAppearance* appearance = NSApp.effectiveAppearance;
    NSAppearanceName best_match =
        [appearance bestMatchFromAppearancesWithNames:@[
            NSAppearanceNameAqua, NSAppearanceNameDarkAqua
        ]];
    return [best_match isEqualToString:NSAppearanceNameDarkAqua]
               ? StyleType::dark
               : StyleType::light;
}
