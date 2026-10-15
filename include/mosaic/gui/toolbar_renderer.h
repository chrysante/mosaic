#pragma once

#include <vector>

#include <vml/vml.hpp>

namespace mosaic {

class ToolbarItem;
class Toolbar;

struct ToolbarPadding {
    float top;
    float bottom;
    float left;
    float right;

    vml::float2 top_left() const { return {left, top}; }
    vml::float2 bottom_right() const { return {right, bottom}; }
    float total_x() const { return left + right; }
    float total_y() const { return top + bottom; }
};

struct ToolbarGeometry {
    // Position of the toolbar in window space
    vml::float2 position;
    vml::float2 size;
    ToolbarPadding padding;
};

ToolbarPadding resolve_toolbar_padding(vml::float2 padding);

class ToolbarRenderer {
public:
    void set_toolbar(Toolbar const* toolbar) { m_toolbar = toolbar; }

    /// Displays the toolbar
    void display(ToolbarGeometry geometry);

    /// \returns the acccumulated width of all items including padding but
    /// ignoring all spacers. This is used by the dockspace for the triple
    /// toolbar layout
    float get_width_without_spacers(ToolbarPadding padding) const;

private:
    /// A block is a list of items without spacers
    struct Block {
        float width = 0;
        float offset = 0;
        std::size_t item_begin_index = 0;
        std::size_t item_end_index = 0;
        bool visible;
        ToolbarItem const* ending_spacer = nullptr;
    };

    void display_blocks(vml::float2 position, float item_height,
                        struct ToolbarPadding padding);
    void display_block(Block const&, float item_height);

    /// Must be called after adding items
    void cook(float item_height);

    /// Called every frame
    void calc_offsets(float total_width);

    Toolbar const* m_toolbar;
    std::vector<ToolbarItem*> m_active_items;
    std::vector<Block> m_blocks;
};

} // namespace mosaic