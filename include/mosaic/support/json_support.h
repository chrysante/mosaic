#include <optional>
#include <string>
#include <string_view>

#include <nlohmann/json.hpp>
#include <vml/fwd.hpp>

#include <mosaic/support/logger.h>

namespace mosaic {

inline nlohmann::json const* json_find(nlohmann::json const& object,
                                       std::string_view key) {
    auto it = object.find(key);
    if (it == object.end() || it->is_null()) return nullptr;
    return &*it;
}

template <typename T>
std::optional<T> json_find(nlohmann::json const& json, std::string_view key) {
    auto* node = json_find(json, key);
    if (!node) {
        Logger::error("Missing or invalid JSON key: ", key);
        return std::nullopt;
    }
    try {
        return node->get<T>();
    } catch (nlohmann::json::exception const& e) {
        Logger::error("Failed to deserialize JSON key '", key, "': ", e.what());
        return std::nullopt;
    }
}

} // namespace mosaic

// =============================================================================
// Support for vml types
// =============================================================================

// vml::vector<>
template <typename T, std::size_t Size, vml::vector_options O>
struct nlohmann::adl_serializer<vml::vector<T, Size, O>> {
    using vector_type = vml::vector<T, Size, O>;

    static void to_json(nlohmann::json& j, vector_type const& v) {
        j = nlohmann::json::array();
        for (T const& value : v)
            j.push_back(value);
    }

    static void from_json(nlohmann::json const& j, vector_type& v) {
        if (!j.is_array() || j.size() != v.size())
            throw nlohmann::json::type_error::create(
                302, "expected an array of size " + std::to_string(v.size()),
                &j);
        std::size_t i = 0;
        for (T& value : v)
            j[i++].get_to(value);
    }
};

// vml::matrix<>
template <typename T, std::size_t Rows, std::size_t Columns,
          vml::vector_options O>
struct nlohmann::adl_serializer<vml::matrix<T, Rows, Columns, O>> {
    using matrix_type = vml::matrix<T, Rows, Columns, O>;

    static void to_json(nlohmann::json& j, matrix_type const& m) {
        j = nlohmann::json::array();
        for (std::size_t r = 0; r < Rows; ++r) {
            j.push_back(nlohmann::json::array());
            for (std::size_t c = 0; c < Columns; ++c)
                j.back().push_back(m(r, c));
        }
    }

    static void from_json(nlohmann::json const& j, matrix_type& m) {
        if (!j.is_array() || j.size() != Rows)
            throw nlohmann::json::type_error::create(
                302, "expected an array of size " + std::to_string(Rows), &j);
        for (std::size_t r = 0; r < Rows; ++r) {
            auto const& row = j[r];
            if (!row.is_array() || row.size() != Columns)
                throw nlohmann::json::type_error::create(
                    302, "expected an array of size " + std::to_string(Columns),
                    &row);
            for (std::size_t c = 0; c < Columns; ++c)
                row[c].get_to(m(r, c));
        }
    }
};

// vml::complex<>
template <typename T> struct nlohmann::adl_serializer<vml::complex<T>> {
    static void to_json(nlohmann::json& j, vml::complex<T> const& c) {
        using base = vml::complex<T>::__vml_base;
        to_json(j, static_cast<base const&>(c));
    }

    static void from_json(nlohmann::json const& j, vml::complex<T>& c) {
        using base = vml::complex<T>::__vml_base;
        from_json(j, static_cast<base&>(c));
    }
};

// vml::quaternion<>
template <typename T> struct nlohmann::adl_serializer<vml::quaternion<T>> {
    static void to_json(nlohmann::json& j, vml::quaternion<T> const& q) {
        using base = vml::quaternion<T>::__vml_base;
        to_json(j, static_cast<base const&>(q));
    }

    static void from_json(nlohmann::json const& j, vml::quaternion<T>& q) {
        using base = vml::quaternion<T>::__vml_base;
        from_json(j, static_cast<base&>(q));
    }
};
