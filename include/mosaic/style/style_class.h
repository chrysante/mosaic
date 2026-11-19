#pragma once

#include <array>
#include <cassert>
#include <optional>
#include <ranges>
#include <string>
#include <vector>

#include <utl/hashtable.hpp>
#include <utl/lazy.hpp>

namespace mosaic {

enum class StyleVarType { color, control_color, size, size2 };

template <StyleVarType Type> struct StyleVarId { unsigned index; };

using StyleColorId = StyleVarId<StyleVarType::color>;
using StyleControlColorId = StyleVarId<StyleVarType::control_color>;
using StyleSizeId = StyleVarId<StyleVarType::size>;
using StyleSize2Id = StyleVarId<StyleVarType::size2>;

struct StyleVarMetadata {
    std::string group;
    std::string name;
};

struct StyleVarKey {
    StyleVarType type;
    unsigned index;
};

class StyleClass {
public:
    class Group {
    public:
        std::string const& name() const { return m_name; }

        std::optional<StyleVarKey> find_style_var(std::string_view name) const {
            auto itr = m_name_map.find(name);
            if (itr != m_name_map.end()) return itr->second;
            return std::nullopt;
        }

        auto const& elements() const { return m_name_map.values(); }

    private:
        friend class StyleClass;

        explicit Group(std::string name)
            : m_name(std::move(name)) {}

        std::string m_name;
        utl::hashmap<std::string, StyleVarKey> m_name_map;
    };

    static StyleClass& get();

    StyleClass(StyleClass const&) = delete;
    StyleClass& operator=(StyleClass const&) = delete;

    std::size_t num_style_vars(StyleVarType type) const {
        return m_vars[(unsigned)type].size();
    }

    StyleVarMetadata const& get_metadata(StyleVarKey key) const {
        assert(key.index < num_style_vars(key.type));
        return m_vars[(unsigned)key.type][key.index];
    }

    template <StyleVarType Type>
    StyleVarMetadata const& get_metadata(StyleVarId<Type> id) const {
        return get_metadata(StyleVarKey{Type, id.index});
    }

    Group const* find_group(std::string_view group_name) const {
        auto itr = m_groups.find(group_name);
        return itr != m_groups.end() ? &itr->second : nullptr;
    }

    template <StyleVarType Type>
    StyleVarId<Type> register_style_var(StyleVarMetadata metadata) {
        auto& list = m_vars[(unsigned)Type];
        unsigned index = list.size();
        auto [group_itr, _] = m_groups.emplace(
            metadata.group, utl::lazy([&] { return Group(metadata.group); }));
        auto [itr, success] = group_itr->second.m_name_map.emplace(
            metadata.name, StyleVarKey{.type = Type, .index = index});
        assert(success);
        list.push_back(std::move(metadata));
        return StyleVarId<Type>{.index = index};
    }

    auto groups() const { return m_groups.values() | std::views::values; }

private:
    StyleClass() = default;

    std::array<std::vector<StyleVarMetadata>, 4> m_vars;
    utl::hashmap<std::string, Group> m_groups;
};

StyleColorId register_style_color(StyleVarMetadata metadata);
StyleControlColorId register_style_control_color(StyleVarMetadata metadata);

} // namespace mosaic
