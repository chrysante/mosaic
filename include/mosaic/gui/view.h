#pragma once

#include "imgui.h"
#include <cassert>
#include <concepts>
#include <type_traits>

#include <imgui_internal.h>
#include <utl/type_erased_object_storage.hpp>
#include <vml/vml.hpp>

namespace mosaic::ui {

struct Rect {
    vml::float2 min() const { return origin; }
    vml::float2 max() const { return origin + size; }

    vml::float2 origin;
    vml::float2 size;
};

namespace internal {

struct ViewInterface {
    virtual void display(Rect frame) = 0;
    // negative values mean "fill available space"
    virtual vml::float2 desired_size() const = 0;
};

} // namespace internal

class View {
    using ViewInterface = internal::ViewInterface;

public:
    template <std::derived_from<ViewInterface> Derived>
    View(Derived&& view)
        : m_storage(std::forward<Derived>(view)) {
        auto* object =
            static_cast<std::remove_cvref_t<Derived>*>(m_storage.data());
        assert(static_cast<void*>(static_cast<ViewInterface*>(object)) ==
               static_cast<void*>(object));
    }

    void display(Rect frame) { interface().display(frame); }

    vml::float2 desired_size() const { return interface().desired_size(); }

private:
    ViewInterface& interface() {
        return *static_cast<ViewInterface*>(m_storage.data());
    }

    ViewInterface const& interface() const {
        return *static_cast<ViewInterface const*>(m_storage.data());
    }

    utl::type_erased_object_storage m_storage;
};

// =============================================================================
// Concrete Views
// =============================================================================

template <std::invocable Action>
View button(std::string label, Action&& action) {
    using F = std::remove_cvref_t<Action>;
    struct Impl final : internal::ViewInterface {
        explicit Impl(std::string&& label, F&& action)
            : label(std::move(label))
            , action(std::move(action)) {}

        void display(Rect frame) override {
            ImGui::SetCursorScreenPos(frame.origin);
            if (ImGui::ButtonEx(label.c_str(), frame.size)) action();
        }
        // negative values mean "fill available space"
        vml::float2 desired_size() const override {
            auto& style = ImGui::GetStyle();
            vml::float2 text_size = ImGui::CalcTextSize(
                label.data(), label.data() + label.size(), true);
            return text_size + 2.0f * (vml::float2)style.FramePadding;
        }

        std::string label;
        F action;
    };
    return View(Impl(std::move(label), std::forward<Action>(action)));
}

template <std::invocable<bool> Action>
View checkbox(char const* str_id, bool current_value, Action&& action) {
    using F = std::remove_cvref_t<Action>;
    struct Impl final : internal::ViewInterface {
        explicit Impl(char const* str_id, bool current_value, F&& action)
            : str_id(str_id)
            , current_value(current_value)
            , action(std::move(action)) {}

        void display(Rect frame) override {
            ImGui::SetCursorScreenPos(frame.origin);

            if (ImGui::Checkbox(str_id, &current_value)) action(current_value);
        }
        // negative values mean "fill available space"
        vml::float2 desired_size() const override {
            auto& style = ImGui::GetStyle();
            return vml::float2(ImGui::GetFontSize()) +
                   2.0f * (vml::float2)style.FramePadding;
        }

        char const* str_id;
        bool current_value;
        F action;
    };
    return View(Impl(str_id, current_value, std::forward<Action>(action)));
}

} // namespace mosaic::ui