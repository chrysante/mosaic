#include "editor/node_editor.h"

#include <cassert>
#include <functional>
#include <optional>
#include <ranges>
#include <utility>
#include <vector>

#include <csp.hpp>
#include <imgui.h>
#include <imgui_internal.h>
#include <utl/stack.hpp>
#include <utl/strcat.hpp>
#include <utl/utility.hpp>

#include "application/input_event.h"
#include "editor/selection_manager.h"
#include "gui/command_palette.h"
#include "gui/font.h"
#include "nlohmann/json_fwd.hpp"
#include "support/json_support.h"
#include "support/logger.h"

using namespace mosaic;
using namespace node_editor;
using namespace vml::short_types;
using namespace std::views;

// =============================================================================
// NodeController
// =============================================================================

nlohmann::json NodeController::serialize() const {
    return nlohmann::json::object();
}

// =============================================================================
// Style
// =============================================================================

namespace {

struct NodeEditorStyleColors {
    float4 white_outline;
    float4 black_outline;
    float4 node_selected_outline;
    float4 node_body;
    float4 node_body_text;
    float4 node_title_text;
    float4 pin_bg;
    float4 pin_opt_bg;
    float4 pin_missing_indicator;
    float4 pin_link_indicator;
    float4 link;
    float4 background_gridline;
    float4 selection_rect;
    float4 selection_rect_outline;
    float disabled_pin_alpha = 0.33;
    float disabled_link_alpha = 0.33;
};

struct NodeEditorStyleVars {
    float link_width = 1.5;
    float link_width_hovered = 3;
    float link_hover_dist = 7;
    /// The side length of the square that is occupied by a pin. This is the
    /// interaction area, the rendered size is smaller
    float pin_area_size = 20;
    float pin_radius = 6;
    float pin_connect_indicator_radius = 2;
    float pin_connect_indicator_radius_hovered = 4;
    float node_body_title_height = 20;
    /// Additional bottom padding because it looks better (or not)
    float node_body_bottom_padding = 0;
    float node_body_rounding = 10;
    float node_body_selection_outline_inset = 2;
    /// How far the pins are moved into the node. If this is zero the outer edge
    /// of the node aligns with the edge of the pin
    float pin_inset = 0;
    float2 min_node_body_padding = 5;
    float2 background_line_dist = {30, 30};
    float link_bezier_control_point_x_min = 100;
    float link_bezier_control_point_x_max = 100;
    float link_bezier_control_point_y_falloff = 150;
    float min_zoom = .25;
    float max_zoom = 1;
};

struct NodeEditorStyle {
    std::array<NodeEditorStyleColors, 2> colors;
    NodeEditorStyleVars vars;

    NodeEditorStyle() {
        auto& light = colors[0];
        auto& dark = colors[1];
        light.white_outline = {1, 1, 1, 0.5};
        light.black_outline = {0, 0, 0, 0.7};
        light.node_selected_outline = {0, 0.6, 1, 1};
        light.node_body = {1, 1, 1, 1};
        light.node_body_text = {0, 0, 0, 1};
        light.node_title_text = {1, 1, 1, 1};
        light.pin_bg = {0.2, 0.2, 0.2, 1};
        light.pin_opt_bg = {0.4, 0.4, 0.4, 1};
        light.pin_missing_indicator = {1, 0, 0.2, 1};
        light.link = {0, 0, 0, 0.5};
        light.pin_link_indicator = {1, 1, 1, 0.8};
        light.background_gridline = {0, 0, 0, 0.2};
        light.selection_rect = {0, 0, 0, 0.14};
        light.selection_rect_outline = {0, 0, 0, 0.3};

        dark.white_outline = light.white_outline;
        dark.black_outline = light.black_outline;
        dark.node_selected_outline = light.node_selected_outline;
        dark.node_body = {0.08, 0.08, 0.08, 1};
        dark.node_body_text = {1, 1, 1, 1};
        dark.node_title_text = light.node_title_text;
        dark.pin_bg = {1, 1, 1, 0.667};
        dark.pin_opt_bg = {1, 1, 1, 0.4};
        dark.pin_missing_indicator = light.pin_missing_indicator;
        dark.link = {1, 1, 1, 0.8};
        dark.pin_link_indicator = {0, 0, 0, 0.8};
        dark.background_gridline = {0.33, 0.33, 0.33, 0.5};
        dark.selection_rect = {1, 1, 1, 0.3};
        dark.selection_rect_outline = {1, 1, 1, 0.3};
    }
};

} // namespace

/// This is not const so we can edit the values with the style editor window
static NodeEditorStyle style;

static NodeEditorStyleColors const& style_colors() { return style.colors[0]; }

static NodeEditorStyleVars const& style_vars() { return style.vars; }

/// Derived properties

///
static float2 NodeBodyPadding() { return style_vars().min_node_body_padding; }

static float2 NodeOuterPadding() {
    return vml::max(float2(1), float2(-style_vars().pin_inset, 0));
}

static float PaddedTitleHeight() {
    return style_vars().node_body_title_height + 2 * NodeBodyPadding().y;
}

static void style_editor(bool* open) {
    auto& col = style.colors[0];
    auto& var = style.vars;
    ImGui::Begin("Node Editor Style Viewer", open);
    // clang-format off
    ImGui::SeparatorText("Style Colors");
    ImGui::ColorEdit4("White Outline", col.white_outline.data());
    ImGui::ColorEdit4("Black Outline", col.black_outline.data());
    ImGui::ColorEdit4("Node Selection Outline", col.node_selected_outline.data());
    ImGui::ColorEdit4("Node Body", col.node_body.data());
    ImGui::ColorEdit4("Node Body Text", col.node_body_text.data());
    ImGui::ColorEdit4("Node Title Text", col.node_title_text.data());
    ImGui::ColorEdit4("Link", col.link.data());
    ImGui::ColorEdit4("Background Grid Line", col.background_gridline.data());
    ImGui::ColorEdit4("Selection Rect", col.selection_rect.data());
    ImGui::ColorEdit4("Selection Rect Outline", col.selection_rect_outline.data());
    ImGui::ColorEdit4("Pin Background", col.pin_bg.data());
    ImGui::ColorEdit4("Optional Pin Background", col.pin_opt_bg.data());
    ImGui::ColorEdit4("Missing Pin Indicator", col.pin_missing_indicator.data());
    ImGui::ColorEdit4("Pin Link Indicator", col.pin_link_indicator.data());
    ImGui::SliderFloat("Disabled Pin Alpha", &col.disabled_pin_alpha, 0, 1);
    ImGui::SliderFloat("Disabled Link Alpha", &col.disabled_link_alpha, 0, 1);
    ImGui::SeparatorText("Style Vars");
    ImGui::SliderFloat("LinkWidth", &var.link_width, 0, 10);
    ImGui::SliderFloat("LinkWidthHovered", &var.link_width_hovered, 0, 10);
    ImGui::SliderFloat("link_hover_dist", &var.link_hover_dist, 0, 20);
    ImGui::SliderFloat("pin_area_size", &var.pin_area_size, 0, 100);
    ImGui::SliderFloat("PinRadius", &var.pin_radius, 0, 100);
    ImGui::SliderFloat("PinConnectIndicatorRadius", &var.pin_connect_indicator_radius, 0, var.pin_radius);
    ImGui::SliderFloat("PinConnectIndicatorRadiusHovered", &var.pin_connect_indicator_radius_hovered, 0, var.pin_radius);
    ImGui::SliderFloat("NodeBodyTitleHeight", &var.node_body_title_height, 0, 100);
    ImGui::SliderFloat("NodeBodyBottomPadding", &var.node_body_bottom_padding, 0, 100);
    ImGui::SliderFloat("NodeBodyRounding", &var.node_body_rounding, 0, 100);
    ImGui::SliderFloat("NodeBodySelectionOutlineInset", &var.node_body_selection_outline_inset, 0, 100);
    ImGui::SliderFloat("PinInset", &var.pin_inset, -100, 100);
    ImGui::SliderFloat2("MinNodeBodyPadding", var.min_node_body_padding.data(), 0, 50);
    ImGui::SliderFloat2("BackgroundLineDist", var.background_line_dist.data(), 0, 200);
    ImGui::SliderFloat("LinkBezierControlPointXMin", &var.link_bezier_control_point_x_min, 0, var.link_bezier_control_point_x_max);
    ImGui::SliderFloat("LinkBezierControlPointXMax", &var.link_bezier_control_point_x_max, var.link_bezier_control_point_x_min, 1000);
    ImGui::SliderFloat("link_bezier_control_point_y_falloff", &var.link_bezier_control_point_y_falloff, 0, 1000);
    ImGui::SliderFloat("min_zoom", &var.min_zoom, 0.01, var.max_zoom);
    ImGui::SliderFloat("max_zoom", &var.max_zoom, var.min_zoom, 1000);
    // clang-format on
    ImGui::End();
}

using Impl = NodeEditor::Impl;

static constexpr auto LinkPopupName = "LinkPopup";
static constexpr auto BackgroundPopupName = "BackgroundPopup";
static constexpr auto DraggingLinkReleasedPopupName =
    "DraggingLinkReleasedPopup";

static ImRect spanning_rect(float2 begin, float2 end, auto... rest) {
    return ImRect(vml::min(begin, end, rest...), vml::max(begin, end, rest...));
}

static ImRect to_rect(Node const& node) {
    return ImRect(node.position(), node.position() + node.size());
}

size_t Pin::index_in_parent() const {
    auto& node = parent();
    // clang-format off
    return csp::visit(*this, csp::overload{
        [&](OutputPin const& pin) {
            auto itr = std::ranges::find(node.m_outputs, &pin, 
                                         [](auto& p) { return p.get(); });
            assert(itr != node.m_outputs.end());
            return utl::narrow_cast<size_t>(itr - node.m_outputs.begin());
        },
        [&](InputPin const& pin) {
            auto itr = std::ranges::find(node.m_inputs, &pin, 
                                         [](auto& p) { return p.get(); });
            assert(itr != node.m_inputs.end());
            return utl::narrow_cast<size_t>(itr - node.m_inputs.begin());
        }
    }); // clang-format on
}

static void clear_links_impl(Pin& pin, auto callback) {
    // clang-format off
    visit(pin, csp::overload{
        [&](OutputPin& pin) {
            auto targets = pin.targets() | 
                std::ranges::to<utl::small_vector<InputPin*>>();
            pin.clear_targets();
            callback(pin, targets);
        },
        [&](InputPin& pin) {
            auto* origin = pin.origin();
            pin.set_origin(nullptr);
            callback(pin, origin);
        }
    }); // clang-format on
}

static void clear_links_with_callback(Pin& pin) {
    // clang-format off
    clear_links_impl(pin, csp::overload{
        [](OutputPin& pin, std::span<InputPin* const> targets) {
            for (auto* target: targets) {
                target->parent().controller()->pin_connection_changed(*target);
            }
            pin.parent().controller()->pin_connection_changed(pin);
        },
        [](InputPin& pin, OutputPin* origin) {
            if (origin) {
                origin->parent().controller()->pin_connection_changed(*origin);
            }
            pin.parent().controller()->pin_connection_changed(pin);
        }
    }); // clang-format on
}

void Pin::clear_links() {
    clear_links_impl(*this, [](auto&&...) {});
}

void InputPin::set_origin(OutputPin* origin) {
    if (m_origin == origin) return;
    auto* last = m_origin;
    m_origin = origin;
    if (last) last->remove_target_weak(this);
}

void OutputPin::add_target(InputPin* target) {
    if (!is_target(target)) m_targets.push_back(target);
}

bool OutputPin::is_target(InputPin const* target) const {
    return std::ranges::contains(m_targets, target);
}

void OutputPin::remove_target(InputPin* pin) {
    if (remove_target_weak(pin)) pin->set_origin(nullptr);
}

bool OutputPin::remove_target_weak(InputPin* pin) {
    auto itr = std::ranges::find(m_targets, pin);
    if (itr != m_targets.end()) {
        m_targets.erase(itr);
        return true;
    }
    return false;
}

void OutputPin::clear_targets() {
    auto copy = targets() | std::ranges::to<utl::small_vector<InputPin*>>();
    for (auto* target : copy)
        target->set_origin(nullptr);
    assert(targets().empty());
}

static bool find_cycle_dfs(Node const* node, utl::stack<Node const*>& stack) {
    return std::ranges::any_of(node->successors(), [&](Node const* succ) {
        if (std::ranges::contains(stack, succ)) return true;
        stack.push(succ);
        bool result = find_cycle_dfs(succ, stack);
        stack.pop();
        return result;
    });
}

static bool find_cycle(Node const* node) {
    utl::stack<Node const*> stack;
    return find_cycle_dfs(node, stack);
}

void node_editor::link(OutputPin& out, InputPin& in) {
    assert(!find_cycle(&out.parent()));
    out.m_targets.push_back(&in);
    bool would_introduce_cycle = find_cycle(&out.parent());
    out.m_targets.pop_back();
    if (would_introduce_cycle) {
        Logger::error("Link would introduce a cycle. Not linking");
        return;
    }
    out.add_target(&in);
    in.set_origin(&out);
    in.parent().controller()->pin_connection_changed(in);
    out.parent().controller()->pin_connection_changed(out);
}

/// Links the pins \p a and \p b if compatible
static void link(Pin& a, Pin& b) {
    // clang-format off
    csp::visit(a, b, csp::overload{
        [&](InputPin& in, OutputPin& out) { link(out, in); },
        [&](OutputPin& out, InputPin& in) { link(out, in); },
        [&](Pin&, Pin&) {},
    }); // clang-format on
}

/// \returns `true` if there is a link between pin \p a and pin \p b
static bool is_linked(Pin const& a, Pin const& b) {
    // clang-format off
    return csp::visit(a, b, csp::overload{
        [&](InputPin const& in, OutputPin const& out) {
            return in.origin() == &out;
        },
        [&](OutputPin const& out, InputPin const& in) {
            return in.origin() == &out;
        },
        [&](Pin const&, Pin const&) { return false; },
    }); // clang-format on
}

static auto succs_impl(auto const& outputs) {
    return outputs |
           std::views::transform([](auto* pin) { return pin->targets(); }) |
           std::views::join |
           std::views::transform([](auto* pin) { return &pin->parent(); });
}

Node* NodeEditorGraph::add_node(std::unique_ptr<Node> node) {
    assert(node && "Node must not be null");
    if (auto* controller = node->controller()) controller->m_editor = m_editor;
    m_nodes.push_back(std::move(node));
    return m_nodes.back().get();
}

Node* NodeEditorGraph::add_node(NodeDesc desc) {
    return add_node(std::make_unique<Node>(std::move(desc)));
}

bool NodeEditorGraph::has_cycles() const {
    return std::ranges::any_of(sources(),
                               [](auto* source) { return find_cycle(source); });
}

static auto sources_impl(auto const& nodes) {
    return nodes | filter([](Node const* node) {
               return std::ranges::all_of(node->inputs(),
                                          [](InputPin const* pin) {
                                              return pin->origin() == nullptr;
                                          });
           });
}

utl::small_vector<Node*> NodeEditorGraph::sources() {
    return sources_impl(nodes()) | std::ranges::to<utl::small_vector<Node*>>();
}

utl::small_vector<Node const*> NodeEditorGraph::sources() const {
    return sources_impl(nodes()) |
           std::ranges::to<utl::small_vector<Node const*>>();
}

static auto sinks_impl(auto const& nodes) {
    return nodes | filter([](Node const* node) {
               return std::ranges::all_of(
                   node->outputs(),
                   [](OutputPin const* pin) { return pin->targets().empty(); });
           });
}

utl::small_vector<Node*> NodeEditorGraph::sinks() {
    return sinks_impl(nodes()) | std::ranges::to<utl::small_vector<Node*>>();
}

utl::small_vector<Node const*> NodeEditorGraph::sinks() const {
    return sinks_impl(nodes()) |
           std::ranges::to<utl::small_vector<Node const*>>();
}

static nlohmann::json serialize_node(Node const& node, utl::uuid id) {
    auto& desc = node.desc();
    nlohmann::json root = nlohmann::json::object();
    root["ID"] = id;
    root["Name"] = desc.name;
    root["MinSize"] = desc.min_size;
    root["Position"] = desc.position;
    root["Resizable"] = desc.resizable;
    if (desc.resizable) root["Size"] = node.size();
    root["Controller"] = desc.controller->serialize();
    return root;
}

static nlohmann::json serialize_link(OutputPin const& out, utl::uuid out_id,
                                     InputPin const& in, utl::uuid in_id) {
    nlohmann::json root = nlohmann::json::object();
    root["FromID"] = out_id;
    root["FromIndex"] = out.index_in_parent();
    root["ToID"] = in_id;
    root["ToIndex"] = in.index_in_parent();
    return root;
}

namespace {

struct SerializationOptions {
    /// If set to true, the existing node IDs are written, otherwise new IDs are
    /// generated
    bool generate_new_ids = false;
    /// If set to false, `serializeNodes()` will only serialize links to nodes
    /// given to the function, i.e., if only a subset of nodes is serialized,
    /// only links to nodes within the subset are written
    bool serialize_links_to_other_nodes = true;
};

} // namespace

template <std::ranges::range Range,
          typename T = std::ranges::range_value_t<Range>>
static utl::hashset<T> to_hashset(Range&& range) {
    utl::hashset<T> result;
    if constexpr (std::ranges::sized_range<Range>)
        result.reserve(std::ranges::size(range));
    for (auto&& elem : std::forward<Range>(range))
        result.emplace(std::forward<decltype(elem)>(elem));
    return result;
}

static nlohmann::json serialize_nodes(std::span<Node const* const> nodes,
                                      SerializationOptions options = {}) {
    nlohmann::json root = nlohmann::json::object();
    auto& node_list = root["Nodes"];
    auto& link_list = root["Links"];
    utl::hashmap<Node const*, utl::uuid> node_id_map;
    for (auto* node : nodes) {
        if (!node->controller()) {
            Logger::error("Failed to serialize node without controller: ",
                          node->name());
            continue;
        }
        auto id = options.generate_new_ids ? utl::uuid::generate() : node->id();
        node_id_map.insert({node, id});
        node_list.push_back(serialize_node(*node, id));
    }
    utl::hashset<Node const*> node_set;
    if (!options.serialize_links_to_other_nodes) node_set = to_hashset(nodes);
    for (auto* node : nodes) {
        for (auto* input : node->inputs()) {
            if (!input->origin()) continue;
            if (!options.serialize_links_to_other_nodes &&
                !node_set.contains(&input->origin()->parent()))
                continue;
            auto* origin = input->origin();
            link_list.push_back(
                serialize_link(*origin, node_id_map[&origin->parent()], *input,
                               node_id_map[&input->parent()]));
        }
    }
    return root;
}

nlohmann::json NodeEditorGraph::serialize() const {
    return serialize_nodes(nodes() |
                           std::ranges::to<utl::small_vector<Node const*>>());
}

static std::unique_ptr<Node>
deserialize_node(nlohmann::json const& source,
                 NodeEditorGraph::ControllerFactory controller_factory) {
    NodeDesc desc{};
    source.at("ID").get_to(desc.id);
    source.at("Name").get_to(desc.name);
    source.at("MinSize").get_to(desc.min_size);
    source.at("Position").get_to(desc.position);
    bool resizable = desc.resizable = source.at("Resizable").get<bool>();
    desc.controller = controller_factory(source.at("Controller"));
    auto node = std::make_unique<Node>(std::move(desc));
    if (resizable) node->set_size(source.at("Size").get<vml::float2>());
    return node;
}

static Node* find_node(utl::hashmap<utl::uuid, Node*> const& map,
                       utl::uuid id) {
    auto itr = map.find(id);
    if (itr != map.end()) return itr->second;
    return nullptr;
}

static void deserialize_link(nlohmann::json const& source,
                             utl::hashmap<utl::uuid, Node*> const& map) {
    auto from_id = source.at("FromID").get<utl::uuid>();
    auto from_index = source.at("FromIndex").get<size_t>();
    auto to_id = source.at("ToID").get<utl::uuid>();
    auto to_index = source.at("ToIndex").get<size_t>();
    Node* from_node = find_node(map, from_id);
    Node* to_node = find_node(map, to_id);
    if (!from_node || !to_node) {
        Logger::warn("Failed to deserialize link: node not found");
        return;
    }
    if (from_index >= from_node->outputs().size()) {
        Logger::warn("Failed to deserialize link: output index out of range: ",
                     from_index, " (on node ", from_node->name(), ")");
        return;
    }
    if (to_index >= to_node->inputs().size()) {
        Logger::warn("Failed to deserialize link: input index out of range: ",
                     to_index, " (on node ", to_node->name(), ")");
        return;
    }
    link(*from_node->output(from_index), *to_node->input(to_index));
}

static nlohmann::json const* try_find(nlohmann::json const& root,
                                      std::string_view key) {
    try {
        return &root.at(key);
    } catch (nlohmann::json::exception const&) {
        return nullptr;
    }
}

static utl::small_vector<Node*>
deserialize_nodes(NodeEditorGraph& dest, nlohmann::json source,
                  NodeEditorGraph::ControllerFactory controller_factory,
                  utl::function_view<void(Node&)> transform = {}) {
    utl::small_vector<Node*> all_nodes;
    utl::hashmap<utl::uuid, Node*> map;
    nlohmann::json const* node_list = try_find(source, "Nodes");
    if (!node_list || !node_list->is_array()) {
        Logger::error("No nodes in yaml node");
        return {};
    }
    for (auto node_source : *node_list) {
        std::unique_ptr<Node> node;
        try {
            node = deserialize_node(node_source, controller_factory);
        } catch (nlohmann::json::exception const& e) {
            Logger::error("Failed to deserialize node: ", e.what());
        }
        if (!node) continue;
        if (transform) transform(*node);
        if (!utl::is_null(node->id())) map[node->id()] = node.get();
        all_nodes.push_back(node.get());
        dest.add_node(std::move(node));
    }
    nlohmann::json const* link_list = try_find(source, "Links");
    if (!link_list || !link_list->is_array()) {
        Logger::error("No links in yaml node");
        return {};
    }
    for (auto link_source : *link_list) {
        try {
            deserialize_link(link_source, map);
        } catch (nlohmann::json::exception const& e) {
            Logger::error("Failed to deserialize: ", e.what());
        }
    }
    return all_nodes;
}

void NodeEditorGraph::deserialize(nlohmann::json const& source,
                                  ControllerFactory controller_factory) {
    m_nodes.clear();
    deserialize_nodes(*this, source, controller_factory);
}

void NodeEditorGraph::delete_if(utl::function_view<bool(Node const&)> pred) {
    auto itr = std::remove_if(m_nodes.begin(), m_nodes.end(),
                              [&](auto& node) { return pred(*node); });
    m_nodes.erase(itr, m_nodes.end());
}

void NodeEditorGraph::order_front(Node* node) {
    auto itr =
        std::ranges::find_if(m_nodes, [&](auto& n) { return n.get() == node; });
    assert(itr != m_nodes.end() && "Node not found");
    auto n = std::move(*itr);
    m_nodes.erase(itr);
    m_nodes.push_back(std::move(n));
}

void NodeEditorGraph::set_editor(NodeEditor* editor) {
    m_editor = editor;
    for (auto* node : nodes()) {
        if (auto* c = node->controller()) c->m_editor = editor;
    }
}

namespace {

struct Link {
    OutputPin* origin;
    InputPin* dest;

    bool operator==(Link const&) const = default;

    bool contains(Pin const* pin) const {
        if (!pin) return false;
        // clang-format off
        return visit(*pin, csp::overload {
            [&](OutputPin const& pin) { return &pin == origin; },
            [&](InputPin const& pin) { return &pin == dest; }
        }); // clang-format on
    }

    explicit operator bool() const { return origin && dest; }
};

static void delete_link(Link link) { clear_links_with_callback(*link.dest); }

enum class Axis { horizontal, vertical };

struct PersistentViewData {
    /// Position of the view in surface coordinates
    vml::float2 position = {0.0, 0.0};
    float zoom = 1.0;
    /// In screen coordinates
    float2 popup_position = {};
    Node const* start_renaming_node = nullptr;
    Node const* renaming_node = nullptr;
    /// This is here and not in `ActiveViewData` because we want to prolong its
    /// lifetime when the dragging link popup menu is open
    Pin* active_pin = nullptr;
    char const* active_popup_name = nullptr;
};

void to_json(nlohmann::json& j, PersistentViewData const& v) {
    j["Position"] = v.position;
    j["Zoom"] = v.zoom;
}

void from_json(nlohmann::json const& j, PersistentViewData& v) {
    j.at("Position").get_to(v.position);
    j.at("Zoom").get_to(v.zoom);
}

/// Data that persists between frames when performing some actions.
/// This is reset whenever a mouse button is released
struct ActiveViewData {
    /// True if we already changed selection state since the mouse was pressed
    bool handled_selection_since_mouse_pressed = false;
    /// True if we are selecting by dragging the background
    bool is_selection_dragging = false;
    bool begin_move_dragging = false;
    bool is_move_dragging = false;
    float2 resize_size = 0;
    /// In screen coordinates
    float2 selection_begin = 0;
    float2 selection_end = 0;
    /// Set to the node currently being dragged
    Node* dragging_node = nullptr;
    Node* active_node = nullptr;
    Node* active_resize = nullptr;

    ImRect get_selection_rect() const {
        return spanning_rect(selection_begin, selection_end);
    }
};

template <typename T> struct HoverItem {
    void set(T item) { this_frame = item; }

    T get() const { return this_frame == last_frame ? this_frame : T(); }

    T get_last() const { return last_frame; }

    void update() {
        last_frame = this_frame;
        this_frame = T();
    }

private:
    T this_frame{}, last_frame{};
};

/// This is reset at the start of each new frame
struct FrameViewData {
    ImRect clip_rect = {};
    vml::rectangle<float> node_rect_union = {};
    bool window_hovered = false;
    bool window_or_child_hovered = false;
    bool disable_background_scrolling = false;
    HoverItem<Pin*> hovered_pin;
    HoverItem<Node*> hovered_node;
    HoverItem<Link> hovered_link;
    HoverItem<Link> hovered_link_under_node;

    void update() {
        auto* window = ImGui::GetCurrentWindow();
        clip_rect = window->ContentRegionRect;
        hovered_pin.update();
        hovered_node.update();
        hovered_link.update();
        hovered_link_under_node.update();
        window_hovered = false;
        window_or_child_hovered = false;
        disable_background_scrolling = false;
    }
};

struct ViewData : PersistentViewData, ActiveViewData, FrameViewData {
    Link popup_link = {};

    void begin_frame() { FrameViewData::update(); }

    void reset_active_data() {
        static_cast<ActiveViewData&>(*this) = ActiveViewData{};
    }
};

static bool draw_debug_helpers = false;
static bool display_pin_user_type = false;

static void debugger_item(char const* name, Node const* node) {
    ImGui::Text("%s: %s", name, node ? node->name().data() : "");
}

static void debugger_item(char const* name, Pin const* pin) {
    ImGui::Text("%s: %s", name, pin ? pin->name().data() : "");
}

static void debugger_item(char const* name, Link link) {
    if (!link) ImGui::Text("%s", name);
    else
        ImGui::Text("%s: %s -> %s", name, link.origin->name().data(),
                    link.dest->name().data());
}

static void debugger_item(char const* name, float value) {
    ImGui::Text("%s: %f", name, value);
}

static void debugger_item(char const* name, float2 value) {
    ImGui::Text("%s: (%f, %f)", name, value.x, value.y);
}

static void debugger_item(char const* name, bool value) {
    ImGui::Text("%s: %s", name, value ? "true" : "false");
}

static void debugger_item(char const* name, char const* value) {
    ImGui::Text("%s: %s", name, value ? value : "NULL");
}

template <typename T>
static void debugger_item(char const* name, HoverItem<T> item) {
    return debugger_item(name, item.get());
}

static void display_debugger(ViewData const& view_data,
                             std::span<Node const* const> selection,
                             bool* open) {
#define ITEM(name) debugger_item(#name, view_data.name)
    ImGui::Begin("Node Editor Debugger", open);
    ImGui::Checkbox("Draw Debug Helpers", &draw_debug_helpers);
    ImGui::Checkbox("Display Pin User Types", &display_pin_user_type);
    ImGui::SeparatorText("Persistent Data");
    ITEM(position);
    ITEM(zoom);
    ImGui::SeparatorText("Active Data");
    ITEM(dragging_node);
    ITEM(handled_selection_since_mouse_pressed);
    ITEM(is_selection_dragging);
    ITEM(begin_move_dragging);
    ITEM(is_move_dragging);
    ITEM(resize_size);
    ITEM(selection_begin);
    ITEM(selection_end);
    ITEM(active_node);
    ITEM(active_resize);
    ITEM(active_pin);
    ITEM(active_popup_name);
    ITEM(popup_position);
    ITEM(node_rect_union.lower_bound());
    ITEM(node_rect_union.upper_bound());
    ImGui::SeparatorText("Frame Data");
    ITEM(window_hovered);
    ITEM(window_or_child_hovered);
    ITEM(hovered_pin);
    ITEM(hovered_node);
    ITEM(hovered_link);
    ITEM(hovered_link_under_node);
    ImGui::SeparatorText("Selection");
    for (auto* node : selection) {
        ImGui::Text("%s",
                    utl::strcat(node->name(), " ", node->position()).c_str());
    }
    ImGui::End();
#undef ITEM
}

using SelectionManager = BasicSelectionManager<Node*>;

} // namespace

struct Node::LayoutData {
    /// Minimal node size due to layout constrains
    float2 min_size = 0;
};

Node::Node(NodeDesc desc)
    : m_desc(std::move(desc))
    , m_layout_data(std::make_unique<LayoutData>()) {
    if (utl::is_null(m_desc.id)) m_desc.id = utl::uuid::generate();
    if (controller()) {
        controller()->m_node = this;
        auto input_descs = controller()->make_input_descriptors();
        m_inputs = input_descs | transform([&](PinDesc const& pin) {
                       return csp::make_unique<InputPin>(pin, *this);
                   }) |
                   std::ranges::to<std::vector>();
        auto output_descs = controller()->make_output_descriptors();
        m_outputs = output_descs | transform([&](PinDesc const& pin) {
                        return csp::make_unique<OutputPin>(pin, *this);
                    }) |
                    std::ranges::to<std::vector>();
    }
}

Node::~Node() {
    for (auto* pin : inputs())
        pin->clear_links();
    for (auto* pin : outputs())
        pin->clear_links();
}

utl::hashset<Node*> Node::successors() {
    return to_hashset(succs_impl(outputs()));
}

utl::hashset<Node const*> Node::successors() const {
    return to_hashset(succs_impl(outputs()));
}

static auto preds_impl(auto const& inputs) {
    return inputs |
           std::views::transform([](auto* pin) { return pin->origin(); }) |
           std::views::filter([](auto* node) { return node != nullptr; }) |
           std::views::transform([](auto* pin) { return &pin->parent(); });
}

utl::hashset<Node*> Node::predecessors() {
    return to_hashset(preds_impl(inputs()));
}

utl::hashset<Node const*> Node::predecessors() const {
    return to_hashset(preds_impl(inputs()));
}

void Node::set_name(std::string name) {
    if (controller()) controller()->did_edit_name(name);
    m_desc.name = std::move(name);
}

struct NodeEditor::Impl {
    explicit Impl(CommandPalette& commandPalette);

    void on_input(InputEvent const& event);

    bool do_on_input(InputEvent const&) { return false; }
    bool do_on_input(MouseDragEvent const& event);
    bool do_on_input(ScrollEvent const& event);
    bool do_on_input(MagnificationEvent const& event);

    void display();

    bool is_user_type_compatible(Pin const& a, Pin const& b) const;
    bool is_linkable(Pin const& a, Pin const& b) const;
    float4 get_link_color(PinUserType type) const;
    void link_popup();
    void dragging_link_released_popup();
    void background_popup();
    void display_popups();
    void background_behaviour();
    void draw_overlays();
    void display_links(Node& node);
    void display_pin_link(InputPin& pin, float2 pin_position);
    void display_dragging_link();
    void pin_popup(Pin& pin, bool show);
    void display_pin(Pin& pin, float2 position, float& max_label_width,
                     bool display_label_in_body);
    void display_pins(Node& node);
    ImRect draw_node_body(Node& node, bool selected);
    void node_body_popup(Node& node, bool show);
    void display_node_body(Node& node);
    bool can_drop_node_into_link(Node const& node, Link link) const;
    void drop_node_into_link(Node& node, Link link);
    void display_node(Node& node);
    void handle_dragging_link_mouse_released();

    void order_front(Node* node);

    void delete_selected();
    void copy_nodes_to_clipboard(std::span<Node const* const> nodes);
    void paste_nodes_from_clipboard();

    void open_popup(char const* name);
    void defer(auto f) { defer_list.push_back(f); }

    NodeEditorGraph* model = nullptr;
    SelectionManager selection;
    ViewData view_data;
    std::unique_ptr<NodeEditorDelegate> delegate;
    std::vector<std::function<void()>> defer_list;
    bool show_debugger = false;
    bool show_style_editor = false;
};

NodeEditor::NodeEditor(CommandPalette& commandPalette)
    : impl(std::make_unique<Impl>(commandPalette)) {}

NodeEditor::~NodeEditor() { set_model(nullptr); }

void NodeEditor::set_delegate(std::unique_ptr<NodeEditorDelegate> delegate) {
    impl->delegate = std::move(delegate);
}

NodeEditorDelegate* NodeEditor::get_delegate() { return impl->delegate.get(); }

static bool
begin_invisible_window(char const* name, float2 pos, float2 size,
                       ImGuiWindowFlags flags = ImGuiWindowFlags_None) {
    ImGui::SetNextWindowPos(pos);
    ImGui::PushStyleColor(ImGuiCol_ChildBg, IM_COL32_BLACK_TRANS);
    flags |= ImGuiWindowFlags_NoDecoration;
    bool open = ImGui::BeginChildEx(name, ImGui::GetID(name), size,
                                    ImGuiChildFlags_None, flags);
    ImGui::PopStyleColor();
    return open;
}

/// Draws lines perpendicular to \p axis starting from coordinate \p begin
/// spanning the entire width or height of the current view
static void draw_lines(float begin, float end, float dist, Axis axis,
                       float4 color) {
    auto* DL = ImGui::GetWindowDrawList();
    float2 p1 = {begin, 0.0}, p2 = {begin, 0.0};
    float* pos1 = nullptr;
    float* pos2 = nullptr;
    float2 winpos = ImGui::GetWindowPos();
    switch (axis) {
    case Axis::horizontal:
        end += winpos.x;
        p1.x += winpos.x;
        p2.x += winpos.x;
        p1.y = winpos.y;
        p2.y = winpos.y + ImGui::GetWindowHeight();
        pos1 = &p1.x;
        pos2 = &p2.x;
        break;
    case Axis::vertical:
        end += winpos.y;
        p1 = p1.swizzle(1, 0);
        p2 = p2.swizzle(1, 0);
        p1.y += winpos.y;
        p2.y += winpos.y;
        p1.x = winpos.x;
        p2.x = winpos.x + ImGui::GetWindowWidth();
        pos1 = &p1.y;
        pos2 = &p2.y;
        break;
    }
    auto colU32 = ImGui::ColorConvertFloat4ToU32(color);
    for (; *pos1 <= end; *pos1 += dist, *pos2 += dist) {
        DL->AddLine(p1, p2, colU32);
    }
}

/// \p center is in screen coordinates
[[maybe_unused]] static void draw_cross(float2 center, float size, ImU32 col) {
    auto* DL = ImGui::GetWindowDrawList();
    DL->AddLine(center - float2(size, 0), center + float2(size, 0), col);
    DL->AddLine(center - float2(0, size), center + float2(0, size), col);
}

static void draw_background(ViewData const& view_data, float zoom) {
    while (zoom <= 0.5)
        zoom *= 2;
    while (zoom >= 2)
        zoom /= 2;
    float4 color = style_colors().background_gridline;
    float2 dist = style_vars().background_line_dist * zoom;
    float2 begin = vml::fmod(-view_data.position, dist);
    draw_lines(begin.x, ImGui::GetWindowWidth(), dist.x, Axis::horizontal,
               color);
    draw_lines(begin.y, ImGui::GetWindowHeight(), dist.y, Axis::vertical,
               color);
    if (draw_debug_helpers) {
        float2 view_pos_ss = (float2)ImGui::GetWindowPos() - view_data.position;
        draw_cross(view_pos_ss, 250, IM_COL32_WHITE);
        auto* DL = ImGui::GetWindowDrawList();
        DL->AddRect(view_pos_ss + view_data.node_rect_union.lower_bound() *
                                      view_data.zoom,
                    view_pos_ss + view_data.node_rect_union.upper_bound() *
                                      view_data.zoom,
                    IM_COL32(255, 0, 255, 255));
    }
}

/// \p rect is expected in screen space coordinates
static bool node_overlaps_rect(ViewData const& view_data, Node const& node,
                               ImRect rect) {
    auto node_rect = to_rect(node);
    node_rect.Min = (float2)node_rect.Min * view_data.zoom;
    node_rect.Max = (float2)node_rect.Max * view_data.zoom;
    node_rect.Translate((float2)view_data.clip_rect.Min - view_data.position);
    return rect.Overlaps(node_rect);
}

bool Impl::is_user_type_compatible(Pin const& a, Pin const& b) const {
    auto impl = [&](OutputPin const& a, InputPin const& b) {
        if (!delegate) return a.user_type() == b.user_type();
        return delegate->is_pin_type_compatible(a.user_type(), b.user_type());
    };
    // clang-format off
    return visit(a, b, csp::overload{
        impl,
        [&](InputPin const& b, OutputPin const& a) { return impl(a, b); },
        [](Pin const&, Pin const&) { return false; }
    }); // clang-format on
}

static bool is_io_compatible(Pin const& a, Pin const& b) {
    return &a.parent() != &b.parent() && a.type() != b.type();
}

bool Impl::is_linkable(Pin const& a, Pin const& b) const {
    return is_io_compatible(a, b) && is_user_type_compatible(a, b);
}

/// Computes the position of \p pin relative to its parent node
/// This is the single source of truth of pin positions
static float2 compute_pin_position(Pin const& pin, size_t index) {
    float posy = PaddedTitleHeight() + index * style_vars().pin_area_size;
    // clang-format off
    return visit(pin, csp::overload{
        [&](InputPin const&) { 
            return float2(style_vars().pin_inset, posy); 
        },
        [&](OutputPin const& pin) {
            return float2(pin.parent().size().x - style_vars().pin_inset -
                              style_vars().pin_area_size,
                          posy);
        }
    }); // clang-format on
}

/// Computes the position of \p pin relative to the node "world" / surface
static float2 compute_world_pin_position(Pin const& pin) {
    return pin.parent().position() +
           compute_pin_position(pin, pin.index_in_parent());
}

/// Computes the position of \p pin in screen coordinates
static float2 compute_screen_pin_position(ViewData const& view_data,
                                          Pin const& pin) {
    float2 basePos = (float2)view_data.clip_rect.Min - view_data.position;
    return basePos + compute_world_pin_position(pin) * view_data.zoom;
}

float4 Impl::get_link_color(PinUserType type) const {
    if (!delegate) return style_colors().link;
    return delegate->link_color(type).value_or(style_colors().link);
}

static bool begin_popup(ViewData& view_data, char const* name,
                        auto close_callback) {
    bool result = ImGui::BeginPopup(name);
    if (result) {
        view_data.active_popup_name = name;
    } else if ((void*)view_data.active_popup_name == (void*)name) {
        view_data.active_popup_name = nullptr;
        close_callback();
    }
    return result;
}

static bool begin_popup(ViewData& view_data, char const* name) {
    return begin_popup(view_data, name, []() {});
}

void Impl::link_popup() {
    if (!begin_popup(view_data, LinkPopupName)) return;
    if (ImGui::MenuItem("Delete Link")) delete_link(view_data.popup_link);
    ImGui::EndPopup();
}

static float2 screen_to_world_space(float2 position,
                                    ViewData const& view_data) {
    float2 posVS = position - (float2)view_data.clip_rect.Min;
    return (posVS + view_data.position) / view_data.zoom;
}

static float2 get_node_spawn_from_popup_position(ViewData const& view_data) {
    return screen_to_world_space(view_data.popup_position, view_data);
}

static Node* show_node_create_popup(NodeEditorDelegate* delegate,
                                    ViewData const& view_data) {
    if (!delegate) return nullptr;
    if (auto* node = delegate->create_node_popup_menu()) {
        ImGui::CloseCurrentPopup();
        float2 position = get_node_spawn_from_popup_position(view_data) -
                          float2(PaddedTitleHeight() / 2);
        node->set_position(position);
        return node;
    }
    return nullptr;
}

Pin* get_new_node_target_pin(Pin const& activePin, Node& node) {
    // clang-format off
    return visit(activePin, csp::overload{
        [&](OutputPin const&) -> Pin* {
            if (!node.inputs().empty()) return node.input(0);
            return nullptr;
        },
        [&](InputPin const&) -> Pin* {
            if (!node.outputs().empty()) return node.output(0);
            return nullptr;
        }
    }); // clang-format on
}

void Impl::dragging_link_released_popup() {
    auto close_callback = [this] { view_data.active_pin = nullptr; };
    if (!begin_popup(view_data, DraggingLinkReleasedPopupName, close_callback))
        return;
    auto* node = show_node_create_popup(delegate.get(), view_data);
    if (node && view_data.active_pin) {
        auto* otherPin = get_new_node_target_pin(*view_data.active_pin, *node);
        if (otherPin && is_linkable(*view_data.active_pin, *otherPin))
            link(*view_data.active_pin, *otherPin);
        if (otherPin) {
            float2 pinPos =
                compute_pin_position(*otherPin, otherPin->index_in_parent());
            node->set_position(node->position() - pinPos +
                               style_vars().pin_area_size / 2);
        }
    }
    ImGui::EndPopup();
}

void Impl::background_popup() {
    if (!begin_popup(view_data, BackgroundPopupName)) return;
    show_debugger |= ImGui::MenuItem("Show Debugger");
    show_style_editor |= ImGui::MenuItem("Show Style Editor");
    ImGui::Separator();
    if (delegate) delegate->background_context_menu();
    show_node_create_popup(delegate.get(), view_data);
    ImGui::EndPopup();
}

void Impl::display_popups() {
    link_popup();
    dragging_link_released_popup();
    background_popup();
}

void Impl::background_behaviour() {
    bool have_node_interaction = view_data.active_node ||
                                 view_data.active_pin ||
                                 view_data.active_resize;
    if (!view_data.window_hovered || have_node_interaction) return;
    if (ImGui::IsMouseClicked(ImGuiMouseButton_Left) &&
        !view_data.is_selection_dragging) {
        view_data.is_selection_dragging = true;
        view_data.selection_begin = ImGui::GetMousePos();
        view_data.selection_end = ImGui::GetMousePos();
        selection.begin_candidates(get_selection_mode());
    }
    if (ImGui::IsMouseClicked(ImGuiMouseButton_Right))
        view_data.begin_move_dragging = true;
    bool showPopup = ImGui::IsMouseReleased(ImGuiMouseButton_Right) &&
                     !ImGui::IsMouseDragPastThreshold(ImGuiMouseButton_Right) &&
                     !view_data.is_move_dragging;
    if (showPopup) {
        if (view_data.hovered_link.get()) {
            open_popup(LinkPopupName);
            view_data.popup_link = view_data.hovered_link.get();
        } else {
            open_popup(BackgroundPopupName);
        }
    }
}

Impl::Impl(CommandPalette& cmd) {
    cmd.add({"Delete Selected Nodes",
             {Key::backspace, ModFlags::control},
             [this] { delete_selected(); }});
    cmd.add({"Select All", {Key::A, ModFlags::control}, [this] {
                 if (model)
                     selection.set(model->nodes() |
                                   std::ranges::to<utl::small_vector<Node*>>());
             }});
    cmd.add({"Copy Selected Nodes", {Key::C, ModFlags::control}, [this] {
                 if (!selection.empty()) copy_nodes_to_clipboard(selection);
             }});
    cmd.add({"Cut Selected Nodes", {Key::X, ModFlags::control}, [this] {
                 if (!selection.empty()) copy_nodes_to_clipboard(selection);
                 delete_selected();
             }});
    cmd.add({"Paste Nodes", {Key::V, ModFlags::control}, [this] {
                 paste_nodes_from_clipboard();
             }});
}

void NodeEditor::on_input(InputEvent const& event) { impl->on_input(event); }

void Impl::on_input(InputEvent const& event) {
    csp::visit(event, [this](auto const& event) { do_on_input(event); });
}

bool Impl::do_on_input(MouseDragEvent const& event) {
    if (view_data.begin_move_dragging) {
        view_data.is_move_dragging = true;
        view_data.position -= event.offset;
    }
    if (view_data.is_selection_dragging) {
        view_data.selection_end = event.location_on_screen;
        ImRect selection_rect = view_data.get_selection_rect();
        if (model) {
            auto candidates =
                model->nodes() | filter([&](Node const* node) {
                    return node_overlaps_rect(view_data, *node, selection_rect);
                });
            selection.set_candidates(to_hashset(candidates));
        }
    }
    return true;
}

bool Impl::do_on_input(ScrollEvent const& event) {
    if (!event.is_trackpad || view_data.disable_background_scrolling ||
        !view_data.window_or_child_hovered)
        return false;
    view_data.position -= event.offset;
    return true;
}

bool Impl::do_on_input(MagnificationEvent const& event) {
    float zoom = view_data.zoom;
    float2 mouse_loc_ws =
        screen_to_world_space(event.location_on_screen, view_data);
    float new_zoom = std::clamp(zoom * (1 + (float)event.magnification),
                                style_vars().min_zoom, style_vars().max_zoom);
    float effective_offset = new_zoom - zoom;
    view_data.position += mouse_loc_ws * effective_offset;
    view_data.zoom = new_zoom;
    return true;
}

NodeEditorGraph* NodeEditor::model() const { return impl->model; }

void NodeEditor::set_model(NodeEditorGraph* model) {
    if (impl->model) impl->model->set_editor(nullptr);
    impl->model = model;
    if (model) model->set_editor(this);
}

nlohmann::json NodeEditor::serialize() const {
    nlohmann::json root = nlohmann::json::object();
    root["ViewData"] = static_cast<PersistentViewData const&>(impl->view_data);
    return root;
}

void NodeEditor::deserialize(nlohmann::json const& node) const {
    try {
        node.at("ViewData").get_to((PersistentViewData&)impl->view_data);
    } catch (nlohmann::json::exception const& err) {
        Logger::error("Failed to deserialize node editor view data: ",
                      err.what());
    }
}

void NodeEditor::start_renaming_node(node_editor::Node const* node) {
    impl->view_data.start_renaming_node = node;
}

namespace {

struct PinDrawOptions {
    float4 color;
    bool missing_input;
    bool connected;
    bool hovered;
    bool disabled;
};

} // namespace

static void draw_pin(float2 position, float zoom, PinDrawOptions options) {
    auto* DL = ImGui::GetWindowDrawList();
    float2 center = position + (style_vars().pin_area_size / 2) * zoom;
    if (options.disabled)
        options.color.alpha *= style_colors().disabled_pin_alpha;
    DL->AddCircleFilled(center, style_vars().pin_radius * zoom,
                        ImGui::ColorConvertFloat4ToU32(options.color));
    if (options.connected || options.missing_input) {
        float radius = (options.hovered || options.missing_input)
                           ? style_vars().pin_connect_indicator_radius_hovered
                           : style_vars().pin_connect_indicator_radius;
        float4 indicator_color = options.missing_input && !options.connected
                                     ? style_colors().pin_missing_indicator
                                     : style_colors().pin_link_indicator;
        if (options.disabled)
            indicator_color.alpha *= style_colors().disabled_pin_alpha;
        DL->AddCircleFilled(center, radius * zoom,
                            ImGui::ColorConvertFloat4ToU32(indicator_color));
    }
}

static float2 pin_label_position(Pin const& pin, float2 bg_padding,
                                 float2 pin_position, float2 text_size,
                                 float zoom, float ypos) {
    // clang-format off
    return visit(pin, csp::overload{
        [&](InputPin const&) -> float2 {
            return { pin_position.x - std::max(style_vars().pin_inset, 1.0f) * zoom -
                         text_size.x - bg_padding.x,
                     ypos + pin_position.y };
        },
        [&](OutputPin const&) -> float2 {
            return { pin_position.x + (std::max(style_vars().pin_inset, 1.0f) +
                         style_vars().pin_area_size) * zoom + bg_padding.x,
                     ypos + pin_position.y };
        },
    }); // clang-format on
}

static std::string_view select_pin_label(Pin const& pin) {
    if (!display_pin_user_type) return pin.name();
    static std::string tmp_buffer;
    tmp_buffer = utl::strcat((int)pin.user_type());
    return tmp_buffer;
}

static void display_pin_label_overlay(ImDrawList* DL, Pin const& pin,
                                      ViewData const& view_data) {
    std::string_view label = select_pin_label(pin);
    if (label.empty()) return;
    auto* text_begin = label.data();
    auto* text_end = text_begin + label.size();
    float2 pin_position = compute_screen_pin_position(view_data, pin);
    float2 text_size =
        (float2)ImGui::CalcTextSize(text_begin, text_end) * view_data.zoom;
    float ypos =
        style_vars().pin_area_size * view_data.zoom / 2 - text_size.y / 2;
    float2 bg_padding = 3 * view_data.zoom;
    float bg_rounding = 3 * view_data.zoom;
    auto bgCol = ImGui::GetColorU32(ImGuiCol_PopupBg);
    float2 text_pos = pin_label_position(pin, bg_padding, pin_position,
                                         text_size, view_data.zoom, ypos);
    DL->AddRectFilled(text_pos - bg_padding, text_pos + text_size + bg_padding,
                      bgCol, bg_rounding);
    DL->AddText(nullptr, ImGui::GetFontSize() * view_data.zoom, text_pos,
                ImGui::GetColorU32(ImGuiCol_Text), text_begin, text_end);
}

namespace {

/// Bezier curve discriptor for drawing a link. All points are in screen
/// coordinates
struct LinkDrawData {
    /// The start point of the link
    float2 begin;

    /// The end point of the link
    float2 end;

    /// The mid point between `begin` and `end`
    float2 mid;

    /// The size of the AABB spanned by `begin` and `end`
    float2 area;

    /// The bezier control point for the start point
    float2 begin_control_point;

    /// The bezier control point for the end point
    float2 end_control_point;
};

} // namespace

static LinkDrawData compute_link_draw_data(float2 begin, float2 end,
                                           float zoom) {
    LinkDrawData dd{};
    dd.begin = begin;
    dd.end = end;
    dd.mid = (begin + end) / 2;
    dd.area = vml::abs(end - begin);
    float control_point_factor =
        zoom * 2.0f *
        std::atan(dd.area.y / zoom /
                  style_vars().link_bezier_control_point_y_falloff) /
        vml::constants<float>::pi;
    float control_point_x =
        control_point_factor *
        std::clamp(dd.area.x, style_vars().link_bezier_control_point_x_min,
                   style_vars().link_bezier_control_point_x_max);
    dd.begin_control_point = dd.begin + float2(control_point_x, 0);
    dd.end_control_point = dd.end - float2(control_point_x, 0);
    return dd;
}

static bool test_path_overlap(std::span<ImVec2 const> path, float2 point,
                              float dist) {
    for (size_t i = 0; i < path.size() - 1; ++i) {
        float2 p1 = path[i];
        float2 p2 = path[i + 1];
        if (vml::distance(vml::line_segment_2D<>(p1, p2), point) <= dist)
            return true;
    }
    return false;
}

static bool test_link_overlap(LinkDrawData const& dd,
                              std::span<ImVec2 const> path, float2 point,
                              float dist) {
    vml::AABB<float, 2> rect(dd.begin, dd.end, dd.begin_control_point,
                             dd.end_control_point);
    vml::AABB<float, 2> wide_rect(rect.lower_bound() - float2(dist),
                                  rect.upper_bound() + float2(dist));
    if (!vml::do_intersect(wide_rect, point)) return false;
    return test_path_overlap(path, point, dist);
}

/// Draws a link from \p begin to \p end in screen coordinates
/// \returns `true` if \p mouse_pos overlaps the link
static bool display_link(float2 begin, float2 end, float4 color,
                         std::optional<float2> mouse_pos, bool may_highlight,
                         bool must_highlight, float zoom) {
    auto* DL = ImGui::GetWindowDrawList();
    auto dd = compute_link_draw_data(begin, end, zoom);
    DL->PathLineTo(begin);
    DL->PathBezierQuadraticCurveTo(dd.begin_control_point, dd.mid, 0);
    DL->PathBezierQuadraticCurveTo(dd.end_control_point, end, 0);
    auto col = ImGui::ColorConvertFloat4ToU32(color);
    bool hovered =
        mouse_pos && test_link_overlap(dd, DL->_Path, *mouse_pos,
                                       style_vars().link_hover_dist * zoom);
    bool draw_highlighted = (may_highlight && hovered) || must_highlight;
    float width = draw_highlighted ? style_vars().link_width_hovered
                                   : style_vars().link_width;
    DL->PathStroke(col, width * zoom, ImDrawFlags_None);
    return hovered;
}

static void draw_link(float2 begin, float2 end, float4 color, bool highlighted,
                      float zoom) {
    display_link(begin, end, color, std::nullopt, /* may_highlight: */ true,
                 /* must_highlight: */ highlighted, zoom);
}

void Impl::display_links(Node& node) {
    float2 nodepos = (float2)ImGui::GetWindowPos() +
                     node.position() * view_data.zoom - view_data.position;
    ImGui::PushID("Inputs");
    for (std::size_t index = 0; auto* pin : node.inputs())
        display_pin_link(*pin, nodepos + compute_pin_position(*pin, index++) *
                                             view_data.zoom);
    ImGui::PopID();
}

void Impl::display_pin_link(InputPin& pin, float2 pin_position) {
    auto* origin = pin.origin();
    if (!origin) return;
    Link link = {origin, &pin};
    float2 begin = compute_screen_pin_position(view_data, *origin) +
                   style_vars().pin_area_size * view_data.zoom / 2;
    float2 end = pin_position + style_vars().pin_area_size * view_data.zoom / 2;
    float4 color = get_link_color(origin->user_type());
    if (origin->disabled() || pin.disabled())
        color.alpha *= style_colors().disabled_link_alpha;
    bool may_highlight = view_data.hovered_link.get_last() == link;
    bool must_highlight = link.contains(view_data.hovered_pin.get_last()) &&
                          !view_data.active_pin;
    bool hovered = display_link(begin, end, color, ImGui::GetMousePos(),
                                may_highlight, must_highlight, view_data.zoom);
    if (hovered && view_data.window_or_child_hovered && !view_data.active_pin) {
        view_data.hovered_link.set(link);
        view_data.hovered_link_under_node.set(link);
    }
}

static bool is_dragging_link_visible(ViewData const& view_data) {
    bool mouse_down = ImGui::IsMouseDown(ImGuiMouseButton_Left) ||
                      ImGui::IsMouseReleased(ImGuiMouseButton_Left);
    bool have_popup =
        view_data.active_popup_name == (void*)DraggingLinkReleasedPopupName;
    return view_data.active_pin && !view_data.active_pin->disabled() &&
           (mouse_down || have_popup);
}

void Impl::display_dragging_link() {
    if (!is_dragging_link_visible(view_data)) return;
    float2 pos1 =
        compute_screen_pin_position(view_data, *view_data.active_pin) +
        style_vars().pin_area_size * view_data.zoom / 2;
    float2 pos2 =
        view_data.active_popup_name == (void*)DraggingLinkReleasedPopupName
            ? view_data.popup_position
            : (float2)ImGui::GetMousePos();
    if (csp::isa<InputPin>(view_data.active_pin)) std::swap(pos1, pos2);
    float4 color = get_link_color(view_data.active_pin->user_type());
    draw_link(pos1, pos2, color, /* highlighted: */ true, view_data.zoom);
}

void Impl::pin_popup(Pin& pin, bool show) {
    static constexpr auto name = "Pin_Popup";
    if (show) ImGui::OpenPopup(name);
    if (!ImGui::BeginPopup(name)) return;
    if (ImGui::MenuItem("Clear Links")) clear_links_with_callback(pin);
    ImGui::EndPopup();
}

namespace { enum class TextAlign { left, right }; } // namespace

static void display_pin_label_in_body(std::string_view label, float2 position,
                                      TextAlign align, float zoom,
                                      float& max_label_width) {
    char const* begin = label.data();
    char const* end = begin + label.size();
    float2 text_size = (float2)ImGui::CalcTextSize(begin, end) * zoom;
    position.y -= text_size.y / 2;
    if (align == TextAlign::right) position.x -= text_size.x;
    auto* DL = ImGui::GetWindowDrawList();
    DL->AddText(nullptr, ImGui::GetFontSize() * zoom, position,
                ImGui::ColorConvertFloat4ToU32(style_colors().node_body_text),
                begin, end);
    max_label_width = std::max(max_label_width, text_size.x);
}

static bool is_optional(Pin const& pin) {
    return test(pin.desc().flags & PinFlags::optional);
}

static float4 select_pin_color(Pin const& pin) {
    if (pin.desc().color) return *pin.desc().color;
    if (is_optional(pin)) return style_colors().pin_opt_bg;
    return style_colors().pin_bg;
}

static bool input_missing(Pin const& pin) {
    if (is_optional(pin)) return false;
    auto* input = csp::dyncast<InputPin const*>(&pin);
    if (!input) return false;
    return input->origin() == nullptr;
}

static bool is_connected(Pin const& pin) {
    // clang-format off
    return visit(pin, csp::overload{
        [](InputPin const& pin) { return pin.origin() != nullptr; },
        [](OutputPin const& pin) { return !pin.targets().empty(); },
    }); // clang-format on
}

static float2 pin_in_body_label_position(Pin const& pin, float2 pin_position,
                                         float zoom) {
    float area_size = style_vars().pin_area_size * zoom;
    // clang-format off
    return pin_position + visit(pin, csp::overload{
        [&](InputPin const&) { return float2(area_size, area_size / 2); },
        [&](OutputPin const&) { return float2(0,  area_size / 2); },
    }); // clang-format on
}

void Impl::display_pin(Pin& pin, float2 position, float& max_label_width,
                       bool display_label_in_body) {
    ImGui::PushID(&pin);
    float area_size = style_vars().pin_area_size * view_data.zoom;
    ImRect bb(position, position + float2(area_size));
    ImGui::ItemSize(float2(area_size));
    auto ID = ImGui::GetID("Pin_Handle");
    ImGui::ItemAdd(bb, ID);
    bool disabled = pin.disabled();
    bool hovered = ImGui::ItemHoverable(bb, ID, ImGuiItemFlags_AllowOverlap);
    bool clicked_left = ImGui::IsItemClicked(ImGuiMouseButton_Left);
    bool clicked_right = ImGui::IsItemClicked(ImGuiMouseButton_Right);
    if (clicked_left || clicked_right) {
        view_data.active_pin = &pin;
        if (ImGui::GetMouseClickedCount(ImGuiMouseButton_Left) == 2)
            clear_links_with_callback(pin);
    }
    pin_popup(pin, /* show: */ clicked_right);
    bool is_dragging_compatible_pin = !disabled && view_data.active_pin &&
                                      is_linkable(*view_data.active_pin, pin);
    bool draw_connected =
        is_connected(pin) || (hovered && is_dragging_compatible_pin) ||
        (view_data.active_pin == &pin && is_dragging_link_visible(view_data));
    bool draw_hovered =
        (hovered && (!view_data.active_pin || is_dragging_compatible_pin)) ||
        view_data.hovered_link.get().contains(&pin) ||
        (!view_data.active_pin && view_data.hovered_pin.get_last() &&
         is_linked(pin, *view_data.hovered_pin.get_last())) ||
        view_data.active_pin == &pin;
    draw_pin(
        position, view_data.zoom,
        {.color = select_pin_color(pin),
         .missing_input = input_missing(pin) && view_data.active_pin != &pin,
         .connected = draw_connected,
         .hovered = draw_hovered,
         .disabled = pin.disabled()});
    if (display_label_in_body) {
        display_pin_label_in_body(
            select_pin_label(pin),
            pin_in_body_label_position(pin, position, view_data.zoom),
            csp::isa<InputPin>(pin) ? TextAlign::left : TextAlign::right,
            view_data.zoom, max_label_width);
    }
    if (hovered) view_data.hovered_pin.set(&pin);
    if (hovered && is_dragging_compatible_pin &&
        ImGui::IsMouseReleased(ImGuiMouseButton_Left))
        link(*view_data.active_pin, pin);
    ImGui::PopID();
}

static float2 compute_node_min_size(Node const& node) {
    float2 body_padding = NodeBodyPadding();
    float pin_height = style_vars().pin_area_size *
                       std::max(node.inputs().size(), node.outputs().size());
    float2 preferred_size = node.controller()
                                ? node.controller()->preferred_content_size()
                                : float2();
    float2 content_size = vml::max(preferred_size, float2(0, pin_height));
    content_size +=
        float2(0, style_vars().node_body_title_height + body_padding.y);
    float2 result = vml::max(node.desc().min_size, node.layout_data().min_size,
                             content_size);
    result.y += style_vars().node_body_bottom_padding + 2 * body_padding.y;
    float leftInset =
        node.inputs().empty() ? body_padding.x : style_vars().pin_area_size;
    float rightInset =
        node.outputs().empty() ? body_padding.x : style_vars().pin_area_size;
    result.x += leftInset + rightInset;
    return result;
}

static bool always_display_input_names(Node const& node) {
    return !node.controller() ||
           node.controller()->always_display_input_names();
}

static bool always_display_output_names(Node const& node) {
    return node.controller() &&
           node.controller()->always_display_output_names();
}

static bool always_display_pin_names(Node const& node, PinType type) {
    switch (type) {
    case PinType::InputPin:
        return always_display_input_names(node);
    case PinType::OutputPin:
        return always_display_output_names(node);
    default:
        std::unreachable();
    }
}

void Impl::display_pins(Node& node) {
    float2 nodepos =
        (float2)ImGui::GetWindowPos() + NodeOuterPadding() * view_data.zoom;
    float max_input_label_width = 0;
    float max_output_label_width = 0;
    {
        ImGui::PushID("Inputs");
        bool display_in_body = always_display_input_names(node);
        for (std::size_t index = 0; auto* pin : node.inputs()) {
            float2 pinpos = compute_pin_position(*pin, index) * view_data.zoom;
            display_pin(*pin, nodepos + pinpos, max_input_label_width,
                        display_in_body);
            ++index;
        }
        ImGui::PopID();
    }
    {
        ImGui::PushID("Outputs");
        bool display_in_body = always_display_output_names(node);
        for (std::size_t index = 0; auto* pin : node.outputs()) {
            float2 pinpos = compute_pin_position(*pin, index) * view_data.zoom;
            display_pin(*pin, nodepos + pinpos, max_output_label_width,
                        display_in_body);
            ++index;
        }
        ImGui::PopID();
    }
    node.layout_data().min_size.x =
        (max_input_label_width + max_output_label_width) / view_data.zoom;
}

static float2 compute_label_size(char const* label, char const* label_end,
                                 float2 scaled_size, float zoom) {
    float2 desired_label_size =
        (float2)ImGui::CalcTextSize(label, label_end) * zoom;
    float2 avail_label_size =
        scaled_size -
        float2(2 * (style_vars().node_body_selection_outline_inset + 1), 0) *
            zoom;
    return vml::min(desired_label_size, avail_label_size);
}

/// \returns the area in which content can be drawn
ImRect Impl::draw_node_body(Node& node, bool selected) {
    float zoom = view_data.zoom;
    auto* DL = ImGui::GetWindowDrawList();
    ui::push_font({.weight_override = FontWeight::bold});
    float2 pos = (float2)ImGui::GetWindowPos() + NodeOuterPadding() * zoom;
    float2 scaled_size = node.size() * zoom;
    float2 body_padding = NodeBodyPadding() * zoom;
    auto* label = node.name().data();
    auto* label_end = node.name().data() + node.name().size();
    float2 label_size = compute_label_size(label, label_end, scaled_size, zoom);
    float2 label_pos =
        pos + float2((scaled_size.x - label_size.x) / 2, body_padding.y);
    float rounding = style_vars().node_body_rounding * zoom;
    /// Selection outline
    if (selected) {
        float inset = style_vars().node_body_selection_outline_inset * zoom;
        float inset2 = inset + 1 * zoom;
        DL->AddRect(pos - inset2, pos + scaled_size + inset2,
                    IM_COL32(0, 0, 0, 127), rounding + inset2, 2 * inset2);
        DL->AddRect(pos - inset, pos + scaled_size + inset,
                    ImGui::ColorConvertFloat4ToU32(
                        style_colors().node_selected_outline),
                    rounding + inset, 2 * inset);
    }
    /// Title bar
    float2 title_bar_size =
        float2(scaled_size.x, label_size.y + 2 * body_padding.y);
    DL->AddRectFilled(pos, pos + title_bar_size,
                      ImGui::ColorConvertFloat4ToU32(node.color()), rounding,
                      ImDrawFlags_RoundCornersTop);
    /// Content BG
    float2 content_pos = pos + float2(0, title_bar_size.y);
    DL->AddRectFilled(content_pos, pos + scaled_size,
                      ImGui::ColorConvertFloat4ToU32(style_colors().node_body),
                      rounding, ImDrawFlags_RoundCornersBottom);
    /// Outlines
    float highlight_outline_inset = 1 * zoom;
    DL->AddRect(pos, pos + scaled_size,
                ImGui::ColorConvertFloat4ToU32(style_colors().black_outline),
                rounding, 1 * zoom);
    DL->AddLine(pos + float2(0, title_bar_size.y), pos + title_bar_size,
                ImGui::ColorConvertFloat4ToU32(style_colors().black_outline),
                1 * zoom);
    DL->AddRect(pos + highlight_outline_inset,
                pos + title_bar_size - highlight_outline_inset,
                ImGui::ColorConvertFloat4ToU32(style_colors().white_outline),
                rounding - highlight_outline_inset, 1 * zoom,
                ImDrawFlags_RoundCornersTop);
    DL->AddRect(content_pos + highlight_outline_inset,
                pos + scaled_size - highlight_outline_inset,
                ImGui::ColorConvertFloat4ToU32(style_colors().white_outline *
                                               float4(1, 1, 1, 0.5)),
                rounding - highlight_outline_inset, 1 * zoom,
                ImDrawFlags_RoundCornersBottom);
    /// Label
    static std::array<char, 512> rename_buffer{};
    if (view_data.start_renaming_node == &node) {
        view_data.renaming_node = &node;
        std::strncpy(rename_buffer.data(), node.name().c_str(),
                     rename_buffer.size());
        ImGui::SetKeyboardFocusHere();
    }
    if (view_data.renaming_node == &node) {
        auto& g = *GImGui;
        auto& style = ImGui::GetStyle();
        float font_size = g.FontSize;
        ImGui::PushStyleVar(ImGuiStyleVar_FramePadding,
                            zoom * (float2)style.FramePadding);
        ImGui::PushStyleColor(ImGuiCol_FrameBg, 0);
        ImGui::PushStyleColor(ImGuiCol_FrameBgHovered, 0);
        ImGui::PushStyleColor(ImGuiCol_FrameBgActive, 0);
        ImGui::PushStyleColor(ImGuiCol_Border, 0);
        ImGui::PushStyleColor(ImGuiCol_BorderShadow, 0);
        ImGui::PushStyleColor(ImGuiCol_NavCursor, 0);
        ImGui::PushStyleColor(ImGuiCol_Text, IM_COL32_WHITE);
        ImGui::PushStyleColor(ImGuiCol_TextSelectedBg,
                              IM_COL32(255, 255, 255, 127));
        float2 rename_label_size = compute_label_size(
            rename_buffer.data(), nullptr, scaled_size, zoom);
        float2 rename_label_pos =
            pos +
            float2((scaled_size.x - rename_label_size.x) / 2, body_padding.y) -
            (float2)style.FramePadding;
        float rename_widget_width =
            rename_label_size.x + 2 * style.FramePadding.x + 100.0f * zoom;
        g.FontSize *= zoom;
        ImGui::SetCursorScreenPos(rename_label_pos);
        ImGui::SetNextItemWidth(rename_widget_width);
        ImGui::InputText("##rename-label", rename_buffer.data(),
                         rename_buffer.size());
        ImGui::PopStyleColor(8);
        ImGui::PopStyleVar();
        g.FontSize = font_size;
        if (view_data.start_renaming_node != &node &&
            (!ImGui::IsItemFocused() || ImGui::IsKeyPressed(ImGuiKey_Enter))) {
            view_data.renaming_node = nullptr;
            node.set_name(rename_buffer.data());
        }
    } else {
        auto label_col =
            ImGui::ColorConvertFloat4ToU32(style_colors().node_title_text);
        ImGui::PushClipRect(label_pos, label_pos + label_size, true);
        DL->AddText(nullptr, ImGui::GetFontSize() * zoom, label_pos, label_col,
                    label, label_end);
        ImGui::PopClipRect();
    }
    if (view_data.start_renaming_node == &node)
        view_data.start_renaming_node = nullptr;
    ui::pop_font();
    if (ImGui::IsWindowHovered() &&
        ImGui::IsMouseHoveringRect(label_pos, label_pos + label_size) &&
        ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Left))
        view_data.start_renaming_node = &node;
    float2 content_rect_min =
        pos + (body_padding + float2(0, label_size.y + 2 * body_padding.y));
    float2 content_rect_max = pos + scaled_size - body_padding;
    if (!node.inputs().empty())
        content_rect_min.x = content_rect_min.x - body_padding.x +
                             style_vars().pin_area_size * zoom;
    if (!node.outputs().empty())
        content_rect_max.x = content_rect_max.x + body_padding.x -
                             style_vars().pin_area_size * zoom;
    return ImRect(content_rect_min, content_rect_max);
}

/// \returns true if the background has been clicked with the left or right
/// mouse button
static bool display_node_content(Node const& node, ViewData&,
                                 ImRect content_rect) {
    ImGui::SetCursorScreenPos(content_rect.Min);
    ImGui::SetNextItemAllowOverlap();
    int button_flags = (int)ImGuiButtonFlags_PressedOnClick |
                       (int)ImGuiButtonFlags_MouseButtonLeft |
                       (int)ImGuiButtonFlags_MouseButtonRight;
    bool bg_pressed = ImGui::InvisibleButton(
        "Content_View_Background_Button", content_rect.GetSize(), button_flags);
    if (node.controller()) node.controller()->body_content();
    return bg_pressed;
}

static void display_node_resize_grip(ViewData& view_data, Node& node) {
    float2 area_size = {10, 10};
    float2 pos = (float2)ImGui::GetWindowPos() +
                 NodeOuterPadding() * view_data.zoom +
                 node.size() * view_data.zoom - area_size;
    ImGuiID id = ImGui::GetID("Node_Resize_Grip");
    ImRect bb(pos, pos + area_size);
    ImGui::ItemSize(area_size);
    ImGui::ItemAdd(bb, id);
    bool hovered = ImGui::ItemHoverable(bb, id, ImGuiItemFlags_AllowOverlap);
    if (ImGui::IsItemClicked()) {
        view_data.active_resize = &node;
        view_data.resize_size = node.size() * view_data.zoom;
    }
    bool dragging = view_data.active_resize == &node &&
                    ImGui::IsMouseDown(ImGuiMouseButton_Left);
    if (dragging) {
        float2 min_size = compute_node_min_size(node);
        view_data.resize_size += ImGui::GetIO().MouseDelta;
        float2 new_size =
            vml::max(view_data.resize_size / view_data.zoom, min_size);
        node.set_size(new_size);
    }
    if (hovered || dragging) ImGui::SetMouseCursor(ImGuiMouseCursor_ResizeNWSE);
}

void Impl::node_body_popup(Node& node, bool show) {
    static constexpr auto name = "Node_Popup";
    if (show) ImGui::OpenPopup(name);
    if (!ImGui::BeginPopup(name)) return;
    assert(selection.contains(&node) &&
           "Popup should not open on unselected node");
    if (ImGui::MenuItem(selection.size() == 1 ? "Delete Node"
                                              : "Delete Selected Nodes"))
        defer([this] { delete_selected(); });
    if (ImGui::MenuItem("Rename")) view_data.start_renaming_node = &node;
    if (auto* controller = node.controller())
        controller->context_menu_content();
    ImGui::EndPopup();
}

bool Impl::can_drop_node_into_link(Node const& node, Link link) const {
    if (node.inputs().size() != 1 || node.outputs().size() != 1) return false;
    InputPin const* in = node.inputs().front();
    OutputPin const* out = node.outputs().front();
    return !in->origin() && out->targets().empty() &&
           is_linkable(*link.origin, *in) && is_linkable(*out, *link.dest);
}

void Impl::drop_node_into_link(Node& node, Link link) {
    assert(can_drop_node_into_link(node, link));
    InputPin* in = node.inputs().front();
    OutputPin* out = node.outputs().front();
    delete_link(link);
    ::link(*link.origin, *in);
    ::link(*out, *link.dest);
}

template <typename T, size_t Dim, vml::vector_options O>
static vml::AABB<T, Dim, O> union_(vml::AABB<T, Dim, O> a,
                                   vml::AABB<T, Dim, O> b) {
    return vml::AABB<T, Dim, O>(vml::min(a.lower_bound(), b.lower_bound()),
                                vml::max(a.upper_bound(), b.upper_bound()));
}

void Impl::display_node_body(Node& node) {
    float2 padding = NodeOuterPadding() * view_data.zoom;
    float2 pos = (float2)ImGui::GetWindowPos() - view_data.position +
                 node.position() * view_data.zoom;
    float2 window_pos = pos - padding;
    float2 scaled_node_size = node.size() * view_data.zoom;
    float2 window_size = scaled_node_size + 2 * padding;
    bool open = begin_invisible_window("Body", window_pos, window_size);
    if (!open && view_data.active_node != &node) {
        ImGui::EndChild();
        return;
    }
    auto* DL = ImGui::GetWindowDrawList();
    DL->PushClipRect(view_data.clip_rect.Min, view_data.clip_rect.Max);
    bool is_selected = selection.contains(&node);
    ImRect content_rect = draw_node_body(node, is_selected);
    ImGuiID id = ImGui::GetID("Node_Body");
    ImRect bb(pos, pos + scaled_node_size);
    ImGui::SetNextItemAllowOverlap();
    ImGui::ItemSize(scaled_node_size);
    ImGui::ItemAdd(bb, id);
    vml::rectangle<float> node_rect(node.position(),
                                    node.position() + node.size());
    view_data.node_rect_union =
        vml::area(view_data.node_rect_union) == 0
            ? node_rect
            : union_(view_data.node_rect_union, node_rect);
    if (ImGui::IsMouseHoveringRect(window_pos + vml::float2(2.0f, 2.0f),
                                   window_pos + window_size -
                                       vml::float2(4.0f, 4.0f))) {
        view_data.hovered_link.set({});
        view_data.hovered_node.set(&node);
    }
    bool bg_clicked_left = ImGui::IsItemClicked(ImGuiMouseButton_Left);
    bool bg_clicked_right = ImGui::IsItemClicked(ImGuiMouseButton_Right);
    display_pins(node);
    if (display_node_content(node, view_data, content_rect)) {
        bg_clicked_left |= ImGui::IsMouseDown(ImGuiMouseButton_Left);
        bg_clicked_right |= ImGui::IsMouseDown(ImGuiMouseButton_Right);
    }
    if (node.desc().resizable) display_node_resize_grip(view_data, node);
    if (bg_clicked_left || bg_clicked_right) {
        defer([this, node = &node] { order_front(node); });
        view_data.active_node = &node;
    }
    /// Selected nodes change selection state when mouse is released
    if (is_selected && view_data.active_node == &node &&
        ImGui::IsMouseReleased(ImGuiMouseButton_Left) &&
        !view_data.dragging_node &&
        !view_data.handled_selection_since_mouse_pressed) {
        selection.amend(&node, get_selection_mode());
    }
    /// True if ...
    bool have_secondary_interaction = view_data.active_node != &node ||
                                      view_data.active_resize ||
                                      view_data.active_pin;
    /// Not selected nodes change selection state when mouse is pressed
    if (!is_selected && (bg_clicked_left || bg_clicked_right) &&
        !have_secondary_interaction) {
        /// We use this flag so we don't select and immediately unselect when
        /// the mouse is released
        view_data.handled_selection_since_mouse_pressed = true;
        selection.amend(&node, get_selection_mode());
    }
    node_body_popup(node,
                    /* show: */ bg_clicked_right &&
                        !have_secondary_interaction);
    bool dragging = [&] {
        if (!ImGui::IsMouseDown(ImGuiMouseButton_Left)) return false;
        return !have_secondary_interaction;
    }();
    Link hovered_link_below = view_data.hovered_link_under_node.get();
    if (hovered_link_below && view_data.dragging_node == &node &&
        ImGui::IsMouseReleased(ImGuiMouseButton_Left) &&
        can_drop_node_into_link(node, hovered_link_below)) {
        drop_node_into_link(node, hovered_link_below);
    }
    if (dragging) {
        if (ImGui::IsMouseDragging(ImGuiMouseButton_Left))
            view_data.dragging_node = &node;
        float2 delta = ImGui::GetIO().MouseDelta;
        for (auto* node : selection.elements())
            node->set_position(node->position() + delta / view_data.zoom);
        if (hovered_link_below &&
            can_drop_node_into_link(node, hovered_link_below))
            view_data.hovered_link.set(hovered_link_below);
    }
    DL->PopClipRect();
    ImGui::EndChild();
}

void Impl::display_node(Node& node) {
    if (node.desc().resizable)
        node.set_size(vml::max(node.size(), compute_node_min_size(node)));
    else node.set_size(compute_node_min_size(node));
    ImGui::PushID(&node);
    display_node_body(node);
    ImGui::PopID();
}

void NodeEditor::display() { impl->display(); }

void Impl::handle_dragging_link_mouse_released() {
    if (ImGui::IsPopupOpen(ImGuiID(), ImGuiPopupFlags_AnyPopupId)) return;
    bool shall_open_dragging_link_menu =
        view_data.active_pin && !view_data.active_pin->disabled() &&
        !view_data.hovered_pin.get() && !view_data.hovered_node.get() &&
        !view_data.active_popup_name;
    if (shall_open_dragging_link_menu)
        open_popup(DraggingLinkReleasedPopupName);
    else if (view_data.active_popup_name !=
             (void*)DraggingLinkReleasedPopupName)
        view_data.active_pin = nullptr;
}

void Impl::display() {
    auto flags = ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoInputs |
                 ImGuiWindowFlags_NoScrollWithMouse;
    if (!ImGui::BeginChild("Node_Editor", {}, ImGuiChildFlags_None, flags)) {
        ImGui::EndChild();
        return;
    }
    view_data.begin_frame();
    draw_background(view_data, view_data.zoom);
    bool is_mouse_over_window_rect =
        ImGui::GetCurrentWindow()->Rect().Contains(ImGui::GetMousePos());
    bool have_popup = ImGui::IsPopupOpen((ImGuiID)0, ImGuiPopupFlags_AnyPopup);
    view_data.window_hovered =
        is_mouse_over_window_rect && !have_popup &&
        ImGui::IsWindowHovered(ImGuiHoveredFlags_RootWindow |
                               ImGuiHoveredFlags_AllowWhenBlockedByActiveItem |
                               ImGuiHoveredFlags_ChildWindows);
    view_data.window_or_child_hovered =
        is_mouse_over_window_rect && !have_popup &&
        ImGui::IsWindowHovered(ImGuiHoveredFlags_RootWindow |
                               ImGuiHoveredFlags_AllowWhenBlockedByActiveItem |
                               ImGuiHoveredFlags_ChildWindows);
    if (model) {
        for (auto* node : model->nodes())
            display_links(*node);
        display_dragging_link();
        view_data.node_rect_union = {};
        for (auto* node : model->nodes())
            display_node(*node);
    }
    background_behaviour();
    draw_overlays();
    display_popups();
    if (ImGui::IsMouseReleased(ImGuiMouseButton_Left))
        handle_dragging_link_mouse_released();
    if (ImGui::IsMouseReleased(ImGuiMouseButton_Left) ||
        ImGui::IsMouseReleased(ImGuiMouseButton_Right)) {
        view_data.reset_active_data();
        selection.apply_candidates();
    }
    for (auto const& f : defer_list)
        f();
    defer_list.clear();
    ImGui::EndChild();
    if (show_debugger)
        display_debugger(view_data, selection.elements(), &show_debugger);
    if (show_style_editor) style_editor(&show_style_editor);
}

static bool need_draw_label_overlay(ViewData const& view_data,
                                    Pin const& hovered_pin) {
    auto& node = hovered_pin.parent();
    if (always_display_pin_names(node, hovered_pin.type())) return false;
    if (!view_data.active_pin) return true;
    return is_io_compatible(hovered_pin, *view_data.active_pin);
}

static void draw_selection_rect(ImDrawList* DL, ImRect rect) {
    auto size = rect.GetSize();
    if (size.x <= 1 || size.y <= 1) return;
    DL->AddRectFilled(
        rect.Min, rect.Max,
        ImGui::ColorConvertFloat4ToU32(style_colors().selection_rect));
    DL->AddRect(
        rect.Min, rect.Max,
        ImGui::ColorConvertFloat4ToU32(style_colors().selection_rect_outline));
}

void Impl::draw_overlays() {
    auto* DL = ImGui::GetForegroundDrawList();
    if (auto* pin = view_data.hovered_pin.get();
        pin && need_draw_label_overlay(view_data, *pin))
        display_pin_label_overlay(DL, *pin, view_data);
    if (view_data.is_selection_dragging)
        draw_selection_rect(DL, view_data.get_selection_rect());
}

void Impl::order_front(Node* node) {
    if (model) model->order_front(node);
}

void Impl::delete_selected() {
    if (!model) return;
    auto pred = [&](Node const& node) { return selection.contains(&node); };
    model->delete_if(pred);
    selection.clear();
}

void Impl::copy_nodes_to_clipboard(std::span<Node const* const> nodes) {
    std::string text =
        serialize_nodes(nodes, {.generate_new_ids = true,
                                .serialize_links_to_other_nodes = false})
            .dump();
    ImGui::SetClipboardText(text.c_str());
}

void Impl::paste_nodes_from_clipboard() {
    if (!model || !delegate) return;
    auto controller_factory = [this](nlohmann::json const& node) {
        return delegate->deserialize_node_controller(node);
    };
    auto transform = [](Node& node) {
        node.set_position(node.position() + float2(50));
    };
    nlohmann::json source;
    try {
        source = nlohmann::json::parse(ImGui::GetClipboardText());
    } catch (nlohmann::json::exception const& e) {
        Logger::error("Failed to paste nodes from clipboard: ", e.what());
    }
    auto nodes =
        deserialize_nodes(*model, source, controller_factory, transform);
    selection.set(nodes);
}

void Impl::open_popup(char const* name) {
    ImGui::OpenPopup(name);
    view_data.popup_position = ImGui::GetMousePos();
    view_data.active_popup_name = name;
}
