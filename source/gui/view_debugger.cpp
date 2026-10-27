#include "gui/view_debugger.h"

#ifndef NDEBUG

#include <cassert>
#include <cstdlib>
#include <cxxabi.h>
#include <memory>
#include <stack>
#include <string>
#include <typeinfo>
#include <vector>

#include <imgui.h>

#include "gui/view.h"

using namespace mosaic;
using namespace ui;

static std::stack<bool> g_open_stack;
static std::vector<std::function<void()>> g_debugger_draws;
static int g_last_frame_submitted;

static void do_append(std::function<void()>&& f) {
    int frame_count = ImGui::GetFrameCount();
    if (g_last_frame_submitted < frame_count) {
        g_debugger_draws.clear();
        g_last_frame_submitted = frame_count;
    }
    g_debugger_draws.push_back(std::move(f));
}

static std::string demangle(const char* name) {
    int status = -1;
    std::unique_ptr<char, decltype(&std::free)> result(
        abi::__cxa_demangle(name, nullptr, nullptr, &status), &std::free);
    return (status == 0 && result) ? result.get() : name;
}

template <typename T> static std::string type_name(const T& obj) {
    return demangle(typeid(obj).name());
}

void ui::debug_begin_node(View const& view) {
    do_append([&view] {
        if (g_open_stack.empty() || g_open_stack.top()) {
            int flags = ImGuiTreeNodeFlags_DefaultOpen;
            if (view.is_leaf()) flags |= ImGuiTreeNodeFlags_Leaf;
            bool tree_open = ImGui::TreeNodeEx(
                &view, flags, "%s (%p)", type_name(view).c_str(), (void*)&view);
            g_open_stack.push(tree_open);
            if (!tree_open) return;
        } else {
            g_open_stack.push(false);
        }
    });
}

void ui::debug_end_node() {
    do_append([] {
        bool tree_open = g_open_stack.top();
        g_open_stack.pop();
        if (tree_open) ImGui::TreePop();
    });
}

void ui::debug_node_info(std::function<void()> f) {
    do_append([f = std::move(f)] {
        if (g_open_stack.top()) f();
    });
}

void ui::draw_view_debugger() {
    assert(g_open_stack.empty());
    for (auto& f : g_debugger_draws)
        f();
    g_debugger_draws.clear();
    assert(g_open_stack.empty());
}

#endif // NDEBUG