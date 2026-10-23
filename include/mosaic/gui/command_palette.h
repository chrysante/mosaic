#pragma once

#include <functional>
#include <string>

#include <utl/hashtable.hpp>

#include <mosaic/application/input.h>
#include <mosaic/application/input_event.h>

namespace mosaic {

namespace internal { struct ApplicationImpl; }

///
struct KeyCombination {
    ///
    Key key;

    ///
    ModFlags mod_flags = ModFlags::none;
};

///
struct CommandDesc {
    std::string name;
    KeyCombination key_comb;
    std::function<void()> command;
    std::function<bool()> enabled = {};
};

/// Element type of the command palette. A command is a named action
class Command {
public:
    Command(CommandDesc desc)
        : m_name(std::move(desc.name))
        , m_key_comb(desc.key_comb)
        , m_fn(std::move(desc.command))
        , m_enabled(std::move(desc.enabled)) {}

    /// \returns the name of this command
    std::string const& name() const { return m_name; }

    /// \returns the associated key combination
    KeyCombination key_combination() const { return m_key_comb; }

    /// Performs the command
    /// \pre `enabled()` must return `true`
    void execute() const;

    /// \returns `true` if this command can be executed
    bool enabled() const { return !m_enabled || m_enabled(); }

private:
    friend class CommandPalette;

    std::string m_name;
    KeyCombination m_key_comb;
    std::function<void()> m_fn;
    std::function<bool()> m_enabled;
};

/// Set of commands
class CommandPalette {
public:
    /// Inserts a command.
    ///
    /// \p key_comb is canonicalized depending on the OS. This means that on
    /// Apple systems the control modifier is mapped to cmd
    ///
    /// \returns a pointer to the added command
    Command const* add(std::string name, KeyCombination key_comb,
                       std::function<void()> action,
                       std::function<bool()> enabled = {});

    /// \overload
    Command const* add(Command command);

    /// \overload
    Command const* add(CommandDesc command_desc);

    /// Executes the command \p name if a matching command exists
    void do_command(std::string_view name) const;

    ///
    Command const* find(std::string_view name) const;

private:
    friend struct internal::ApplicationImpl;
    friend class Panel;

    struct ModCommandPair {
        ModFlags mod_flags;
        Command command;
    };

    ///
    void on_input(InputEvent const& event) const;

    /// This is a node hashmap because we give out pointers to elements
    utl::node_hashmap<std::string, Command> m_name_map;
    utl::hashmap<Key, std::vector<ModCommandPair>> m_key_map;
};

} // namespace mosaic
