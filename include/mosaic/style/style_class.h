#pragma once

#include <array>
#include <cassert>
#include <optional>
#include <string>
#include <vector>

#include <utl/hashtable.hpp>

namespace mosaic {

enum class StyleVarType { color, control_color, size, size2 };

template <StyleVarType Type> struct StyleVarId { unsigned index; };

using StyleColorId = StyleVarId<StyleVarType::color>;
using StyleControlColorId = StyleVarId<StyleVarType::control_color>;
using StyleSizeId = StyleVarId<StyleVarType::size>;
using StyleSize2Id = StyleVarId<StyleVarType::size2>;

struct StyleVarMetadata { std::string name; };

struct StyleVarKey {
    StyleVarType type;
    unsigned index;
};

class StyleClass {
public:
    static StyleClass& get();

    StyleClass(StyleClass const&) = delete;
    StyleClass& operator=(StyleClass const&) = delete;

    std::size_t num_style_vars(StyleVarType type) const {
        return m_vars[(unsigned)type].size();
    }

    template <StyleVarType Type>
    StyleVarMetadata const& get_metadata(StyleVarId<Type> id) const {
        assert(id.index < num_style_vars(Type));
        return m_vars[(unsigned)Type][id.index];
    }

    std::optional<StyleVarKey> find_style_var(std::string_view name) {
        auto itr = m_name_map.find(name);
        if (itr != m_name_map.end()) return itr->second;
        return std::nullopt;
    }

    template <StyleVarType Type>
    StyleVarId<Type> register_style_var(StyleVarMetadata metadata) {
        auto& list = m_vars[(unsigned)Type];
        unsigned index = list.size();
        assert(!m_name_map.contains(metadata.name));
        m_name_map[metadata.name] = {.type = Type, .index = index};
        list.push_back(std::move(metadata));
        return {index};
    }

private:
    StyleClass() = default;

    std::array<std::vector<StyleVarMetadata>, 4> m_vars;
    utl::hashmap<std::string, StyleVarKey> m_name_map;
};

StyleColorId register_style_color(StyleVarMetadata metadata);
StyleControlColorId register_style_control_color(StyleVarMetadata metadata);

} // namespace mosaic
