#pragma once

#include <functional>

#include <vml/vml.hpp>

#include "application/input_event.h"

namespace mosaic {

class Window;
class Input;

void input_update(Input& input, InputEvent const& event);

void input_end_frame(Input& input);

/// Registers \p callback to be invoked with every input event that is sent to
/// the application \Returns an ID that can be used to unregister the callback
[[nodiscard]] int register_global_input_callback(
    std::function<void(InputEventUnion const&)> handler);

/// Unregisters the callback with ID \p id
void unregister_global_input_callback(int id);

///
void register_window_for_input(Window& window);

} // namespace mosaic
