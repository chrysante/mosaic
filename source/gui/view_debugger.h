#pragma once

#ifndef NDEBUG

#include <functional>

namespace mosaic::ui {

class View;

void draw_view_debugger();

void debug_begin_node(View const& view);
void debug_end_node();
void debug_node_info(std::function<void()> draw);

} // namespace mosaic::ui

#endif