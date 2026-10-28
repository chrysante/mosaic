# TODO

## High priority
- [x] Fix binding classes, allow returning references via std::reference_wrapper from computed properties
- [x] Rework view measurement using size constraints 
- [ ] Add transparent view wrappers for tooltips and decoration
- [ ] Rewrite properties UI using the new `View` API
- [ ] Make `View` tree less static/more dynamic

## Bugs
When a window is moved and released above a foreign window with another application window underneath, the window is set transparent for one frame, making it flash


## Improvements
- [ ] Make `WorkDispatcher::on_main_thread()` run the callback directly if called on the main thread.

## Technical debt
- [ ] Clean up `Panel`, `PanelRenderer` and `PanelManager` classes

## Ideas
- [ ] Style editor
            Application-defined list of system-colors 
                Defined on each system in some way, but the set of colors is the same.
         -> Application Style Colors (editable by style-editor)
                Here e.g. `frame_hovered_color` could be derived from `frame_color`
                or overridden with a raw color value
                These are saved as a .style file (or whatever), either as raw colors, 
                or as derived from either a system-color or another application style color, 
                in which case we would only save the tweaks.
         -> Non-customizable mapping from Application Colors to ImGui Colors

            Application defined set of style variables 
         -> Non-customizable mapping from application style variables to ImGui.

- [ ] Visual effects backgrounds
- [ ] Allow GPU selection
- [ ] Add node editor
- [ ] Add ImGui Test Engine