#pragma once

#include <optional>
#include <ranges>
#include <span>

#include <utl/hashtable.hpp>

namespace mosaic {

/// Different modes in which selections can be amended
enum class SelectionMode {
    replace, /// Default, existing selection is replaced
    add,     /// Additive selection, new selection does not unselect existing
             /// selection
    toggle   /// "Exclusive-or" selection. Items are selected iff not already
             /// selected
};

/// Determines the current selection mode based on held modifier keys. Must be
/// called within an ImGui frame
SelectionMode get_selection_mode();

/// Generic selection manager that supports replace-, add-, toggle- and multiple
/// seletion
template <typename T,
          typename OptT =
              std::conditional_t<std::is_pointer_v<T>, T, std::optional<T>>,
          typename Hash = utl::hash<T>, typename KeyEq = std::equal_to<>>
class BasicSelectionManager {
public:
    using SetType = utl::hashset<T, Hash, KeyEq>;

    /// \Returns `true` if \p elem is selected
    template <typename U> bool contains(U const& elem) const {
        if (!m_candidate_mode) {
            return m_elems.contains(elem);
        }
        switch (*m_candidate_mode) {
        case SelectionMode::replace:
            return m_candidates.contains(elem);
        case SelectionMode::add:
            return m_elems.contains(elem) || m_candidates.contains(elem);
        case SelectionMode::toggle:
            return m_elems.contains(elem) != m_candidates.contains(elem);
        }
    }

    /// Calls `set`, `add` or `toggle` depending on \p mode
    /// \Returns `true` if \p elem is selected after the call
    bool amend(T const& elem, SelectionMode mode = get_selection_mode()) {
        return amend_impl(mode, elem);
    }

    template <typename Itr, typename Sentinel>
    void amend(Itr first, Sentinel last,
               SelectionMode mode = get_selection_mode()) {
        amend_impl(mode, first, last);
    }

    void amend(std::ranges::range auto&& rng,
               SelectionMode mode = get_selection_mode())
        requires(!std::convertible_to<decltype(rng), T const&>) {
        amend_impl(mode, std::forward<decltype(rng)>(rng));
    }

    /// Adds \p elem to the selection
    void add(T const& elem) {
        m_elems.insert(elem);
        m_last = elem;
    }

    /// \overload for ranges
    template <typename Itr, typename Sentinel>
    void add(Itr first, Sentinel last) {
        m_elems.insert(first, last);
        if (first != last) m_last = *std::prev(last);
    }

    /// \overload for ranges
    void add(std::ranges::range auto&& rng)
        requires(!std::convertible_to<decltype(rng), T const&>) {
        add(std::ranges::begin(rng), std::ranges::end(rng));
    }

    /// If \p elem is not selected, adds \p elem to the selection. Otherwise
    /// removes \p elem from the selection
    /// \Returns `true` if \p elem is selected after the call
    bool toggle(T const& elem) {
        auto itr = m_elems.find(elem);
        if (itr != m_elems.end()) {
            m_elems.erase(itr);
            m_last = OptT{};
            return false;
        } else {
            add(elem);
            m_last = elem;
            return true;
        }
    }

    template <typename Itr, typename Sentinel>
    void toggle(Itr first, Sentinel last) {
        for (; first != last; ++first)
            toggle(*first);
    }

    void toggle(std::ranges::range auto&& rng)
        requires(!std::convertible_to<decltype(rng), T const&>) {
        toggle(std::ranges::begin(rng), std::ranges::end(rng));
    }

    /// Clears the selection and selects \p elem
    void set(T const& elem) {
        set(&elem, &elem + 1);
        m_last = elem;
    }

    /// \overload for ranges
    template <typename Itr, typename Sentinel>
    void set(Itr first, Sentinel last) {
        clear();
        add(first, last);
    }

    /// \overload for ranges
    void set(std::ranges::range auto&& rng)
        requires(!std::convertible_to<decltype(rng), T const&>) {
        set(std::ranges::begin(rng), std::ranges::end(rng));
    }

    /// Erases \p elem from the selection
    /// \Returns `true` if the element was selected
    template <typename U> bool erase(U const& elem) {
        if (m_last == elem) m_last = std::nullopt;
        return m_elems.erase(elem);
    }

    /// Unselectes all elements
    void clear() {
        m_elems.clear();
        m_last = OptT{};
    }

    /// \Returns a view over all selected elements
    std::span<T const> elements() const { return m_elems.values(); }

    auto begin() const { return elements().begin(); }

    auto end() const { return elements().end(); }

    /// \Returns the number of selected elements
    size_t size() const { return m_elems.size(); }

    /// \Returns `true` if no elements are selected
    bool empty() const { return m_elems.empty(); }

    /// @{ Stateful selection API

    /// Begin multi selection with selection mode \p mode
    void begin_candidates(SelectionMode mode) { this->m_candidate_mode = mode; }

    /// Update multi selection
    void set_candidates(SetType elems) { m_candidates = std::move(elems); }

    /// Finish multi selection
    void apply_candidates() {
        if (!m_candidate_mode) {
            return;
        }
        amend(std::move(m_candidates), *m_candidate_mode);
        m_candidates.clear();
        m_candidate_mode = std::nullopt;
    }

    /// @}

    /// Applies \p fn to all selected elements
    ///
    /// \p fn should not be invoked for side effects, as it may be invoked
    /// multiple times with selected elements
    void transform(std::invocable<T const&> auto fn) {
        auto values = m_elems.values();
        m_elems.clear();
        for (auto& elem : values)
            m_elems.insert(std::invoke(fn, elem));
        if (m_last) m_last = std::invoke(fn, *m_last);
    }

    /// \Returns the last selected element
    OptT last() const { return m_last; }

    /// \Returns the single selected element or `OptT{}` or none or
    /// multiple elements are selected
    OptT single() const {
        return m_elems.size() == 1 ? OptT(*m_elems.begin()) : OptT{};
    }

private:
    template <typename... Args>
    bool amend_impl(SelectionMode mode, Args&&... args) {
        switch (mode) {
        case SelectionMode::replace:
            set(std::forward<Args>(args)...);
            return true;
        case SelectionMode::add:
            add(std::forward<Args>(args)...);
            return true;
        case SelectionMode::toggle:
            if constexpr (std::is_same_v<decltype(toggle(
                                             std::forward<Args>(args)...)),
                                         void>) {
                toggle(std::forward<Args>(args)...);
                return bool{};
            } else {
                return toggle(std::forward<Args>(args)...);
            }
        }
    }

    SetType m_elems;
    SetType m_candidates;
    OptT m_last = {};
    std::optional<SelectionMode> m_candidate_mode;
};

} // namespace mosaic
