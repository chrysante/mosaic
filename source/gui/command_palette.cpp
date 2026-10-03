#include "gui/command_palette.h"

#include <cassert>

#include "application/input_event.h"
#include "support/logger.h"

using namespace mosaic;

void Command::execute() const {
    assert(enabled());
    m_fn();
}

static ModFlags canonicalize(ModFlags mods) {
    using enum ModFlags;
#if defined(__APPLE__)
    if (test(mods & control)) {
        mods &= ~control;
        mods |= super;
    }
#endif
    return mods;
}

Command const* CommandPalette::add(Command cmd) {
    cmd.m_key_comb.mod_flags = canonicalize(cmd.m_key_comb.mod_flags);
    auto* result = &m_name_map.insert({cmd.name(), cmd}).first->second;
    auto key_comb = cmd.key_combination();
    if (key_comb.key != Key::none) {
        auto& list = m_key_map[key_comb.key];
        list.push_back({canonicalize(key_comb.mod_flags), cmd});
    }
    return result;
}

Command const* CommandPalette::add(CommandDesc desc) {
    return add(Command(std::move(desc)));
}

void CommandPalette::do_command(std::string_view name) const {
    auto* cmd = find(name);
    if (!cmd) {
        Logger::warn("Command not found: \"", name, "\"");
        return;
    }
    cmd->execute();
}

Command const* CommandPalette::find(std::string_view name) const {
    auto itr = m_name_map.find(name);
    if (itr == m_name_map.end()) {
        return nullptr;
    }
    return &itr->second;
}

void CommandPalette::on_input(InputEvent const& event) const {
    // clang-format off
    csp::visit(event, csp::overload {
        [&](KeyDownEvent const& event) {
            auto list_itr = m_key_map.find(event.key);
            if (list_itr == m_key_map.end()) return false;
            auto& list = list_itr->second;
            /// Sometimes numlock gets set even if not held
            auto mod_flags = event.modifier_flags & ~ModFlags::num_lock;
            auto itr = std::find_if(list.begin(), list.end(), [&](auto& p) {
                return p.mod_flags == mod_flags;
            });
            if (itr == list.end()) return false;
            if (itr->command.enabled()) itr->command.execute();
            return true;
        },
        [&](InputEvent const&) { return false; },
    });
    // clang-format on
}
