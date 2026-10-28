#include <catch2/catch_test_macros.hpp>

#include <array>
#include <concepts>
#include <memory>
#include <utility>
#include <vector>

// Include view.h as well as view_support.h so the real View pipe adapters are
// tested. Adjust this include to match your repository layout.
#include <mosaic/gui/view.h>
#include <mosaic/gui/view_support.h>

using namespace mosaic;
using namespace ui;

namespace {

class TestView final : public View {
public:
    TestView()
        : View(SizeReq2{SizeReq::fit(), SizeReq::fit()}) {}

    int display_calls = 0;
    Rect last_frame{{0.0f, 0.0f}, {0.0f, 0.0f}};
    vml::float2 intrinsic{10.0f, 20.0f};

private:
    void do_display(Rect frame) override {
        ++display_calls;
        last_frame = frame;
    }

    vml::float2
    do_calc_intrinsic_size(vml::float2 /*max_size*/) const override {
        return intrinsic;
    }
};

struct ArrowWrapper {
    View* ptr = nullptr;

    View* operator->() const { return ptr; }
    View& operator*() const { return *ptr; }
};

static_assert(PointerLike<View*, View>);
static_assert(PointerLike<std::unique_ptr<View>, View>);
static_assert(PointerLike<ArrowWrapper, View>);
static_assert(!PointerLike<int, View>);

} // namespace

// -----------------------------------------------------------------------------
// Pipe adapters
// -----------------------------------------------------------------------------

TEST_CASE("View pipe adapter applies an action and returns a pointer value",
          "[pipe]") {
    TestView view;
    View* ptr = &view;
    auto result = ptr | fixed_height(42.0f);
    STATIC_REQUIRE(std::same_as<decltype(result), View*>);
    REQUIRE(result == ptr);
    REQUIRE(view.size_requirement()[1].is_fixed());
    REQUIRE(view.size_requirement()[1].fixed_size() == 42.0f);
}

TEST_CASE("View pipe adapter accepts smart pointers and wrapper types",
          "[pipe]") {
    auto owned = std::make_unique<TestView>();
    auto* raw = owned.get();
    auto result = std::move(owned) | fixed_width(80.0f);
    STATIC_REQUIRE(std::same_as<decltype(result), std::unique_ptr<TestView>>);
    REQUIRE(result.get() == raw);
    REQUIRE(raw->size_requirement()[0].is_fixed());
    REQUIRE(raw->size_requirement()[0].fixed_size() == 80.0f);
    TestView view;
    ArrowWrapper wrapper{&view};
    auto wrapper_result = wrapper | vfit;
    STATIC_REQUIRE(std::same_as<decltype(wrapper_result), ArrowWrapper>);
    REQUIRE(wrapper_result.ptr == &view);
    REQUIRE(view.size_requirement()[1].is_fit());
}

TEST_CASE("Composed pipe adapters run left to right", "[pipe]") {
    TestView view;
    View* ptr = &view;
    auto result = ptr | (fixed_height(100.0f) | fixed_height(200.0f));
    REQUIRE(result == ptr);
    REQUIRE(view.size_requirement()[1].is_fixed());
    REQUIRE(view.size_requirement()[1].fixed_size() == 200.0f);
}

TEST_CASE(
    "View pipe |= mutates the pointee and returns a const wrapper reference",
    "[pipe]") {
    TestView view;
    View* ptr = &view;
    auto const& result = (ptr |= fixed_width(55.0f));
    STATIC_REQUIRE(std::same_as<decltype(result), View* const&>);
    REQUIRE(&result == &ptr);
    REQUIRE(view.size_requirement()[0].is_fixed());
    REQUIRE(view.size_requirement()[0].fixed_size() == 55.0f);
}

TEST_CASE("Built-in view adapters set size, alignment, and enabled properties",
          "[pipe]") {
    TestView view;
    View* ptr = &view;
    ptr | hfit | vfill | hcenter | align_bottom;
    REQUIRE(view.size_requirement()[0].is_fit());
    REQUIRE(view.size_requirement()[1].is_fill());
    REQUIRE(view.size_requirement()[1].fill_weight() == 1.0f);
    REQUIRE(view.alignment_requirement()[0] == Alignment::mid);
    REQUIRE(view.alignment_requirement()[1] == Alignment::high);
    ptr | hfill_weight(2.5f);
    REQUIRE(view.size_requirement()[0].is_fill());
    REQUIRE(view.size_requirement()[0].fill_weight() == 2.5f);
    ptr | enabled(Property<bool const>{false});
    REQUIRE_FALSE(view.enabled());
    ptr | enabled(Property<bool const>{true});
    REQUIRE(view.enabled());
}

// -----------------------------------------------------------------------------
// Rect and SizeReq
// -----------------------------------------------------------------------------

TEST_CASE("Rect computes minimum and maximum corners", "[Rect]") {
    Rect rect{{2.0f, 3.0f}, {5.0f, 7.0f}};
    REQUIRE(rect.min()[0] == 2.0f);
    REQUIRE(rect.min()[1] == 3.0f);
    REQUIRE(rect.max()[0] == 7.0f);
    REQUIRE(rect.max()[1] == 10.0f);
}

TEST_CASE("SizeReq factories preserve their mode and value", "[SizeReq]") {
    const auto fit = SizeReq::fit();
    REQUIRE(fit.is_fit());
    REQUIRE(fit.type() == SizeReq::Type::fit);
    const auto fixed = SizeReq::fixed(12.5f);
    REQUIRE(fixed.is_fixed());
    REQUIRE(fixed.type() == SizeReq::Type::fixed);
    REQUIRE(fixed.fixed_size() == 12.5f);
    const auto fill_default = SizeReq::fill();
    REQUIRE(fill_default.is_fill());
    REQUIRE(fill_default.type() == SizeReq::Type::fill);
    REQUIRE(fill_default.fill_weight() == 1.0f);
    const auto fill_weighted = SizeReq::fill(2.25f);
    REQUIRE(fill_weighted.is_fill());
    REQUIRE(fill_weighted.fill_weight() == 2.25f);
}

TEST_CASE("SizeReq accepts zero fill weight and zero fixed size", "[SizeReq]") {
    REQUIRE(SizeReq::fill(0.0f).fill_weight() == 0.0f);
    REQUIRE(SizeReq::fixed(0.0f).fixed_size() == 0.0f);
}

// Note: precondition violations in SizeReq accessors and factory assertions
// should be tested in a separate death-test setup if your Catch2/platform
// configuration supports it. Do not call fixed_size() on fit/fill or
// fill_weight() on fit/fixed in ordinary tests.

// -----------------------------------------------------------------------------
// ValueOrRef
// -----------------------------------------------------------------------------

TEST_CASE("ValueOrRef stores and returns an owned value", "[ValueOrRef]") {
    ValueOrRef<int> value{42};
    REQUIRE(value.value() == 42);
    value.value() = 43;
    REQUIRE(value.value() == 43);
}

TEST_CASE("ValueOrRef references existing mutable storage", "[ValueOrRef]") {
    int original = 10;
    ValueOrRef<int> value{std::ref(original)};
    REQUIRE(value.value() == 10);
    value.value() = 11;
    REQUIRE(original == 11);
}

TEST_CASE("ValueOrRef const conversion preserves an owned value",
          "[ValueOrRef]") {
    ValueOrRef<int> source{42};
    ValueOrRef<int const> converted{source};
    REQUIRE(converted.value() == 42);
    source.value() = 99;
    REQUIRE(converted.value() == 42);
}

TEST_CASE("ValueOrRef const conversion preserves reference aliasing",
          "[ValueOrRef]") {
    int original = 10;
    ValueOrRef<int> source{std::ref(original)};
    ValueOrRef<int const> converted{source};
    REQUIRE(converted.value() == 10);
    original = 12;
    REQUIRE(converted.value() == 12);
}

TEST_CASE("ValueOrRef const conversion works from an rvalue", "[ValueOrRef]") {
    ValueOrRef<int const> converted{ValueOrRef<int>{73}};
    REQUIRE(converted.value() == 73);
}

// -----------------------------------------------------------------------------
// Property
// -----------------------------------------------------------------------------

TEST_CASE("own creates an independently owned mutable property", "[Property]") {
    auto property = own(5);
    REQUIRE(property.materialize() == 5);
    property.set(8);
    REQUIRE(property.materialize() == 8);
}

TEST_CASE("copying an owned property clones its value", "[Property]") {
    auto original = own(5);
    auto copy = original;
    copy.set(9);
    REQUIRE(original.materialize() == 5);
    REQUIRE(copy.materialize() == 9);
}

TEST_CASE("bind reads and writes the external object", "[Property]") {
    int external = 5;
    auto property = bind(&external);
    REQUIRE(property.materialize() == 5);
    property.set(14);
    REQUIRE(external == 14);
}

TEST_CASE("copying a bound property preserves the binding", "[Property]") {
    int external = 5;
    auto original = bind(&external);
    auto copy = original;
    copy.set(21);
    REQUIRE(external == 21);
    REQUIRE(original.materialize() == 21);
}

TEST_CASE("binding const storage is readable", "[Property]") {
    const int external = 17;
    auto property = bind(&external);
    REQUIRE(property.materialize() == 17);
}

TEST_CASE("computed getter is read-only and evaluated on access",
          "[Property]") {
    int source = 3;
    int calls = 0;
    auto property = computed([&] {
        ++calls;
        return source * 2;
    });
    REQUIRE(property.materialize() == 6);
    REQUIRE(calls == 1);
    source = 4;
    REQUIRE(property.materialize() == 8);
    REQUIRE(calls == 2);
}

TEST_CASE("computed getter and setter forward writes", "[Property]") {
    int source = 3;
    auto property =
        computed([&] { return source; }, [&](int value) { source = value; });
    REQUIRE(property.materialize() == 3);
    property.set(12);
    REQUIRE(source == 12);
    REQUIRE(property.materialize() == 12);
}

TEST_CASE("computed(get) accepts lvalue callable objects", "[Property]") {
    int source = 6;
    auto getter = [&] { return source; };
    auto property = computed(getter);
    REQUIRE(property.materialize() == 6);
}

TEST_CASE("computed(get, set) accepts lvalue/rvalue mixed callable objects",
          "[Property]") {
    int source = 6;
    auto getter = [&] { return source; };
    auto property =
        computed(getter, [&](int new_value) { source = new_value; });
    REQUIRE(property.materialize() == 6);
    property.set(3);
    REQUIRE(source == 3);
    REQUIRE(property.materialize() == 3);
}

TEST_CASE(
    "Property<T const> can be initialized from a value and mutable Property",
    "[Property]") {
    Property<int const> from_value{7};
    REQUIRE(from_value.materialize() == 7);
    auto mutable_property = own(13);
    Property<int const> from_property{mutable_property};
    REQUIRE(from_property.materialize() == 13);
}

namespace {

template <typename T> struct PropertyCompileTest {
    static_assert(
        std::is_same_v<decltype(bind(static_cast<T*>(nullptr))), Property<T>>);

    static_assert(std::is_same_v<decltype(bind(static_cast<T const*>(nullptr))),
                                 Property<T const>>);

    static_assert(!requires(Property<T const> & property, T value) {
        property.set(value);
    });
};

template struct PropertyCompileTest<int>;

} // namespace

// -----------------------------------------------------------------------------
// Array
// -----------------------------------------------------------------------------

TEST_CASE("Array supports iterator-range construction", "[Array]") {
    const std::array values{2, 4, 6, 8};
    Array<int> result{values.begin(), values.end()};
    REQUIRE(result.size() == values.size());
    REQUIRE(result[0] == 2);
    REQUIRE(result[3] == 8);
}

TEST_CASE("Array moves elements from an rvalue C array", "[Array]") {
    std::unique_ptr<int> values[] = {
        std::make_unique<int>(3),
        std::make_unique<int>(7),
    };
    Array<std::unique_ptr<int>> result{std::move(values)};
    REQUIRE(result.size() == 2);
    REQUIRE(*result[0] == 3);
    REQUIRE(*result[1] == 7);
    REQUIRE_FALSE(values[0]);
    REQUIRE_FALSE(values[1]);
}

// -----------------------------------------------------------------------------
// ViewContainer
// -----------------------------------------------------------------------------

TEST_CASE("ViewContainer supports empty construction and indexing",
          "[ViewContainer]") {
    ViewContainer<TestView> views;
    REQUIRE(views.empty());
    REQUIRE(views.size() == 0);
    views.push_back(allocate<TestView>());
    views.push_back(allocate<TestView>());
    REQUIRE_FALSE(views.empty());
    REQUIRE(views.size() == 2);
    REQUIRE(&views[0] != &views[1]);
}

TEST_CASE("ViewContainer iteration visits every view", "[ViewContainer]") {
    ViewContainer<TestView> views;
    auto first = allocate<TestView>();
    auto second = allocate<TestView>();
    auto* first_ptr = first.get();
    auto* second_ptr = second.get();
    views.push_back(std::move(first));
    views.push_back(std::move(second));
    std::vector<TestView*> visited;
    for (TestView& view : views) {
        visited.push_back(&view);
    }
    REQUIRE(visited.size() == 2);
    REQUIRE(visited[0] == first_ptr);
    REQUIRE(visited[1] == second_ptr);
}

TEST_CASE("const ViewContainer iteration yields const views",
          "[ViewContainer]") {
    ViewContainer<TestView> views;
    views.push_back(allocate<TestView>());
    auto const& const_views = views;
    for (TestView const& view : const_views) {
        REQUIRE(&view == &const_views[0]);
    }
}

TEST_CASE("ViewContainer erase removes only the matching view",
          "[ViewContainer]") {
    ViewContainer<TestView> views;
    views.push_back(allocate<TestView>());
    views.push_back(allocate<TestView>());
    views.push_back(allocate<TestView>());
    auto* middle = &views[1];
    TestView outsider;
    auto* missing = &outsider;
    REQUIRE(views.erase(middle));
    REQUIRE(views.size() == 2);
    REQUIRE_FALSE(views.erase(middle));
    REQUIRE_FALSE(views.erase(missing));
    REQUIRE(views.size() == 2);
}

TEST_CASE("ViewContainer accepts an array of owned views", "[ViewContainer]") {
    Owner<TestView> elements[] = {
        allocate<TestView>(),
        allocate<TestView>(),
    };
    ViewContainer<TestView> views{std::move(elements)};
    REQUIRE(views.size() == 2);
}
