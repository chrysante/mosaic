#pragma once

#include <functional>

#include <vml/vml.hpp>

namespace mosaic {

class Input;
class InputEvent;

void input_update(Input& input, InputEvent const& event);

void input_end_frame(Input& input);

/// Registers \p callback to be invoked with every input event that is sent to
/// the application \Returns an ID that can be used to unregister the callback
[[nodiscard]] int register_global_input_callback(
    std::function<void(InputEvent const&, void* native_event)> handler);

/// Unregisters the callback with ID \p id
void unregister_global_input_callback(int id);

} // namespace mosaic
