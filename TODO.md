# TODO

## High priority
- [x] Fix binding classes, allow returning references via std::reference_wrapper from computed properties
- [x] Rework view measurement using size constraints 
- [ ] Add transparent view wrappers for tooltips and decoration

## Bugs
When a window is moved and released above a foreign window with another application window underneath, the window is set transparent for one frame, making it flash

## Improvements
- [ ] Make `WorkDispatcher::on_main_thread()` run the callback directly if called on the main thread.

## Technical debt
- [ ] Clean up `View`, `ViewRenderer` and `ViewManager` classes

## Ideas
- [ ] Style editor
- [ ] Visual effects backgrounds
- [ ] Allow GPU selection
- [ ] Add node editor
- [ ] Add ImGui Test Engine