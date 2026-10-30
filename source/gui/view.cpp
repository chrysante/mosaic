#include "gui/view.h"

#include <imgui.h>
#include <imgui_internal.h>

#include "application/debug_settings.h"
#include "gui/font.h"
#include "gui/view_debugger.h"
#include "gui/view_support.h"
#include "support/memory.h"

using namespace mosaic;
using namespace ui;

void View::display(Rect frame) {
    assert(vml::isfinite(frame.origin).all());
    assert(vml::isfinite(frame.size).all());
#ifndef NDEBUG
    debug_begin_node(*this);
#endif
    do_display(frame);
#ifndef NDEBUG
    if (want_debug_overlays()) {
        auto* DL = ImGui::GetWindowDrawList();
        DL->AddRect(frame.min(), frame.max(), IM_COL32(64, 255, 32, 200));
    }
    debug_end_node();
#endif
}

vml::float2 View::calc_intrinsic_size(vml::float2 max_size) const {
    assert((vml::isfinite(max_size) | vml::isinf(max_size)).all());
    return do_calc_intrinsic_size(max_size);
}

// -----------------------------------------------------------------------------

template <typename WrapperView>
static Owner<WrapperView> make_wrapper(Owner<View> in) {
    if (auto* wrapper = dynamic_cast<WrapperView*>(in.get())) {
        (void)in.release();
        return Owner<WrapperView>(wrapper);
    }
    return allocate<WrapperView>(std::move(in));
}

namespace {

struct DecoratorView : View {
    explicit DecoratorView(Owner<View> wrapped)
        : View(wrapped->borrow_layout())
        , wrapped(std::move(wrapped)) {}

    void do_display(Rect frame) {
        bool const disabled = !is_enabled.materialize();
        if (disabled) ImGui::BeginDisabled();
        if (font_mod) push_font(*font_mod);

        wrapped->display(frame);

        if (font_mod) pop_font();
        if (disabled) ImGui::EndDisabled();

        if (frame_thickness > 0.0f) {
            auto* DL = ImGui::GetWindowDrawList();
            DL->AddRect(frame.min(), frame.max(),
                        ImGui::GetColorU32(ImGuiCol_Border), 0.0f,
                        frame_thickness);
        }
    }
    vml::float2 do_calc_intrinsic_size(vml::float2 max_size) const {
        if (font_mod) push_font(*font_mod);
        vml::float2 result = wrapped->calc_intrinsic_size(max_size);
        if (font_mod) pop_font();
        return result;
    }
    bool do_is_leaf() const { return false; }

    Property<bool const> is_enabled = true;
    std::optional<FontModifier> font_mod;
    float frame_thickness = 0.0f;

    Owner<View> wrapped;
};

} // namespace

Owner<View> ui::internal::EnabledAction::operator()(Owner<View> in) const {
    auto decorator = make_wrapper<DecoratorView>(std::move(in));
    decorator->is_enabled = is_enabled;
    return decorator;
}

Owner<View> ui::internal::FontModAction::operator()(Owner<View> in) const {
    auto decorator = make_wrapper<DecoratorView>(std::move(in));
    decorator->font_mod = mod;
    return decorator;
}

Owner<View> ui::internal::FrameAction::operator()(Owner<View> in) const {
    auto wrapper = make_wrapper<DecoratorView>(std::move(in));
    wrapper->frame_thickness = thickness;
    return wrapper;
}
