#pragma once

#include <cassert>
#include <memory>
#include <ranges>
#include <span>
#include <string>
#include <vector>

#include <csp.hpp>
#include <nlohmann/json_fwd.hpp>
#include <utl/common.hpp>
#include <utl/function_view.hpp>
#include <utl/hashtable.hpp>
#include <utl/uuid.hpp>
#include <utl/vector.hpp>
#include <vml/vml.hpp>

#include <mosaic/application/input_event.h>

namespace mosaic {

class NodeEditor;
class NodeEditorGraph;
class CommandPalette;

} // namespace mosaic

namespace mosaic::node_editor {

class Pin;
class InputPin;
class OutputPin;
class Node;

enum class PinType { Pin, InputPin, OutputPin, LAST = OutputPin };

} // namespace mosaic::node_editor

// clang-format off
CSP_DEFINE(mosaic::node_editor::Pin,       mosaic::node_editor::PinType::Pin,        void,                     Abstract)
CSP_DEFINE(mosaic::node_editor::InputPin,  mosaic::node_editor::PinType::InputPin,   mosaic::node_editor::Pin, Concrete)
CSP_DEFINE(mosaic::node_editor::OutputPin, mosaic::node_editor::PinType::OutputPin,  mosaic::node_editor::Pin, Concrete)
// clang-format on

namespace mosaic::node_editor {

enum class PinFlags { none = 0, optional = 1 << 0, disabled = 1 << 1 };

UTL_ENUM_OPERATORS(PinFlags)

enum class PinUserType : uint64_t;

struct PinDesc {
    std::string name;
    PinFlags flags;
    PinUserType user_type;
    std::optional<vml::float4> color;
};

/// Node controller interface. Objects of derived classes can be attached to any
/// node to customize its behaviour
class NodeController {
public:
    virtual ~NodeController() = default;

    /// Called when serializing the containing graph. Deserialization is done
    /// through a callback to `NodeEditorGraph::deserialize`, because it must
    /// determine the static type of the node controller
    virtual nlohmann::json serialize() const;

    /// Callback to display content in the node body
    virtual void body_content() {}

    /// Callback to display the content of the nodes context menu
    virtual void context_menu_content() {}

    /// Decides whether to always display input names or only on hover
    virtual bool always_display_input_names() const { return true; }

    /// Decides whether to always display output names or only on hover
    virtual bool always_display_output_names() const { return false; }

    /// Used by the `Node` class to create pins
    virtual std::vector<PinDesc> make_input_descriptors() { return {}; }

    /// See `makeInputDescriptors()`
    virtual std::vector<PinDesc> make_output_descriptors() { return {}; }

    /// The content size of the node will at least this large
    virtual vml::float2 preferred_content_size() const { return {0, 0}; }

    /// The color of the node body
    virtual vml::float4 node_color() { return {1, 0, 0.5, 1}; }

    /// Called for every input and output pin whenever a link is added or
    /// removed
    virtual void pin_connection_changed(Pin&) {}

    /// Called after the user has editd the node name
    virtual void did_edit_name(std::string /* newName */) {}

    /// \returns the node that this controller is attached to
    Node* node() const { return m_node; }

    /// \returns the node editor in which this node is opened
    NodeEditor* editor() const { return m_editor; }

private:
    friend class Node;
    friend class mosaic::NodeEditorGraph;

    NodeEditor* m_editor = nullptr;
    Node* m_node = nullptr;
};

/// Descriptor structure used to create nodes
struct NodeDesc {
    /// Unique identifier of this node. This will be set by the model if not set
    /// by the user
    utl::uuid id;

    /// The label of the node in the editor
    std::string name;

    /// The position at which the node will be created
    vml::float2 position;

    ///
    vml::float2 min_size;

    ///
    bool resizable = false;

    /// The controller that will be added to the node
    std::unique_ptr<NodeController> controller;
};

/// Base class of pins
class Pin {
public:
    /// The description from which this pin was created
    PinDesc const& desc() const { return m_desc; }

    ///
    void set_desc(PinDesc desc) { m_desc = std::move(desc); }

    /// Runtime type of this pin, i.e., `InputPin` or `OutputPin`
    PinType type() const { return m_type; }

    /// Label name
    std::string const& name() const { return desc().name; }

    /// \returns the user defined type of the pin
    PinUserType user_type() const { return desc().user_type; }

    /// The node in which this pin exists
    Node& parent() { return *m_parent; }

    /// \overload
    Node const& parent() const { return *m_parent; }

    /// Computes the index of this pin in its parent node
    size_t index_in_parent() const;

    /// Removes all links from this pin
    void clear_links();

    /// \returns `true` if the pin flags \p f are set on this pin
    bool has_flags(PinFlags f) const { return test(desc().flags & f); }

    /// \returns `true` if disabled flag is set
    bool disabled() const { return has_flags(PinFlags::disabled); }

protected:
    explicit Pin(PinType type, PinDesc const& desc, Node& parent)
        : m_desc(desc)
        , m_type(type)
        , m_parent(&parent) {}

private:
    friend inline PinType get_rtti(Pin const& pin) { return pin.type(); }

    PinDesc m_desc;
    PinType m_type;
    Node* m_parent;
};

///
void link(OutputPin& output, InputPin& input);

///
class InputPin : public Pin {
public:
    explicit InputPin(PinDesc const& desc, Node& parent)
        : Pin(PinType::InputPin, desc, parent) {}

    /// \returns the output that is linked to this pin
    OutputPin* origin() { return m_origin; }

    /// \overload
    OutputPin const* origin() const { return m_origin; }

    /// Sets the output that is linked to this pin
    void set_origin(OutputPin* origin);

private:
    friend void link(OutputPin&, InputPin&);

    OutputPin* m_origin = nullptr;
};

///
class OutputPin : public Pin {
public:
    explicit OutputPin(PinDesc const& desc, Node& parent)
        : Pin(PinType::OutputPin, desc, parent) {}

    /// \returns a view over the input pins that are linked to this putput
    std::span<InputPin* const> targets() { return m_targets; }

    /// \overload
    std::span<InputPin const* const> targets() const { return m_targets; }

    /// Adds \p pin as a target of this pin. This does not update \p target
    void add_target(InputPin* pin);

    /// \returns `true` if a link from this pin to \p target exists
    bool is_target(InputPin const* target) const;

    /// Removes the link to \p target
    /// This also updates \p target
    void remove_target(InputPin* target);

    /// Removes all targets. This also updates all targets
    void clear_targets();

private:
    friend class InputPin;
    friend void link(OutputPin&, InputPin&);

    bool remove_target_weak(InputPin* pin);

    utl::small_vector<InputPin*> m_targets;
};

/// Models a node in the node editor
class Node {
public:
    struct LayoutData;

    explicit Node(NodeDesc desc);

    /// Nodes need address stability and are therefore not copyable
    Node(Node const&) = delete;

    /// Calls `clearLinks` on all input and output pins
    ~Node();

    NodeDesc const& desc() const { return m_desc; };

    /// \returns the attached controller. This may be null
    NodeController* controller() const { return desc().controller.get(); }

    /// \returns a view over all inputs
    auto inputs() {
        return m_inputs | std::views::transform(
                              [](auto& p) -> InputPin* { return p.get(); });
    }

    /// \overload
    auto inputs() const {
        return m_inputs | std::views::transform([](auto& p) -> InputPin const* {
                   return p.get();
               });
    }

    /// \returns the input pin at index \p index
    InputPin* input(size_t index) {
        return const_cast<InputPin*>(std::as_const(*this).input(index));
    }

    /// \overload
    InputPin const* input(size_t index) const {
        assert(index < m_inputs.size());
        return m_inputs[index].get();
    }

    /// \returns a view over all outputs
    auto outputs() {
        return m_outputs | std::views::transform(
                               [](auto& p) -> OutputPin* { return p.get(); });
    }

    /// \overload
    auto outputs() const {
        return m_outputs |
               std::views::transform(
                   [](auto& p) -> OutputPin const* { return p.get(); });
    }

    /// \returns the output pin at index \p index
    OutputPin* output(size_t index) {
        return const_cast<OutputPin*>(std::as_const(*this).output(index));
    }

    /// \overload
    OutputPin const* output(size_t index) const {
        assert(index < m_outputs.size());
        return m_outputs[index].get();
    }

    /// \returns the set of nodes that is directly linked to any output of this
    /// node
    utl::hashset<Node*> successors();

    /// \overload
    utl::hashset<Node const*> successors() const;

    /// \returns the set of nodes that is directly linked to any input of this
    /// node
    utl::hashset<Node*> predecessors();

    /// \overload
    utl::hashset<Node const*> predecessors() const;

    /// \returns the unique ID of this node
    utl::uuid id() const { return desc().id; }

    /// \returns the name of this node
    std::string const& name() const { return desc().name; }

    /// Sets the name of this node
    void set_name(std::string name);

    /// \returns the size of this node
    vml::float2 size() const { return m_size; }

    /// Sets the size of this node to \p size
    void set_size(vml::float2 size) { m_size = size; }

    /// \returns the position of this node
    vml::float2 position() const { return desc().position; }

    /// Sets the position of this node to \p position
    void set_position(vml::float2 position) { m_desc.position = position; }

    /// \returns the body color of the node
    vml::float4 color() const {
        return controller() ? controller()->node_color()
                            : vml::float4{0, 0, 0, 1};
    }

    /// Internal to the node editor
    LayoutData& layout_data() { return *m_layout_data; }

    /// \overload
    LayoutData const& layout_data() const { return *m_layout_data; }

private:
    friend class Pin;

    NodeDesc m_desc;
    vml::float2 m_size;
    std::unique_ptr<LayoutData> m_layout_data;
    std::vector<csp::unique_ptr<node_editor::InputPin>> m_inputs;
    std::vector<csp::unique_ptr<node_editor::OutputPin>> m_outputs;
};

} // namespace mosaic::node_editor

namespace mosaic {

/// Underlying document model of the node editor
class NodeEditorGraph {
public:
    using Node = node_editor::Node;

    /// Adds \p node to the model and return a pointer to it
    Node* add_node(std::unique_ptr<node_editor::Node> node);

    /// Creates a node from \p desc and adds it to the model
    Node* add_node(node_editor::NodeDesc desc);

    /// \returns a view over the nodes in this graph
    auto nodes() {
        return m_nodes |
               std::views::transform([](auto& p) -> Node* { return p.get(); });
    }

    /// \overload
    auto nodes() const {
        return m_nodes | std::views::transform(
                             [](auto& p) -> Node const* { return p.get(); });
    }

    ///
    bool empty() const { return m_nodes.empty(); }

    /// \returns `true` if there are any cycles in this graph
    bool has_cycles() const;

    /// \returns all nodes that have no inputs
    utl::small_vector<Node*> sources();

    /// \overload
    utl::small_vector<Node const*> sources() const;

    /// \returns all nodes that have no outputs
    utl::small_vector<Node*> sinks();

    /// \overload
    utl::small_vector<Node const*> sinks() const;

    /// \returns a JSON node representing this document
    nlohmann::json serialize() const;

    using ControllerFactory =
        utl::function_view<std::unique_ptr<node_editor::NodeController>(
            nlohmann::json const&)>;

    /// Deserializes the document stored in \p node
    void deserialize(nlohmann::json const& node, ControllerFactory factory);

    /// Deletes all nodes for which \p predicate returns true
    void delete_if(utl::function_view<bool(Node const&)> predicate);

    /// Moves the node \p node to the front. This changes the display order of
    /// the nodes
    void order_front(Node* node);

    /// \returns the node editor in which is graph is opened
    NodeEditor* editor() const { return m_editor; }

    ///
    void set_editor(NodeEditor* editor);

private:
    std::vector<std::unique_ptr<Node>> m_nodes;
    NodeEditor* m_editor;
};

/// Provides callbacks to customize the behaviour of a node editor
class NodeEditorDelegate {
public:
    virtual ~NodeEditorDelegate() = default;

    /// Customization point that decides if the node editor can create a link
    /// from an output pin of type \p output to an input pin of type \p input
    virtual bool is_pin_type_compatible(node_editor::PinUserType output,
                                        node_editor::PinUserType input) const {
        return input == output;
    }

    /// Customization point that assigns colors to links based on their type
    virtual std::optional<vml::float4>
    link_color(node_editor::PinUserType /* type */) const {
        return std::nullopt;
    }

    ///
    virtual void background_context_menu() {}

    /// Context menu content to create new nodes. Shall return a pointer to node
    /// if a node has been added
    virtual node_editor::Node* create_node_popup_menu() { return nullptr; }

    /// Must be implemented for copy pasting to work
    virtual std::unique_ptr<node_editor::NodeController>
    deserialize_node_controller(nlohmann::json const&) {
        return nullptr;
    }
};

/// Generic customizable node editor
class NodeEditor {
public:
    /// Registers commands with \p command_palette
    explicit NodeEditor(CommandPalette& commandPalette);

    ~NodeEditor();

    void set_delegate(std::unique_ptr<NodeEditorDelegate> delegate);

    NodeEditorDelegate* get_delegate();

    void display();

    void on_input(InputEvent& event);

    /// \returns the underlying graph
    NodeEditorGraph* model() const;

    ///
    void set_model(NodeEditorGraph* model);

    /// Serializes view data. This does not serialize the current model
    nlohmann::json serialize() const;

    /// Deserializes view data
    void deserialize(nlohmann::json const& node) const;

    ///
    void start_renaming_node(node_editor::Node const* node);

    struct Impl;

private:
    std::unique_ptr<Impl> impl;
};

} // namespace mosaic
