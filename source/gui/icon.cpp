#include "gui/icon.h"

#include "gui/ui_context.h"
#include "service/icon_manager.h"

using namespace mosaic;

Icon ui::resolve_icon(IconId icon_id) {
    return get_current_ui_context().icon_manager.get_icon(icon_id);
}

IconId ui::lucide_icon_id(std::string_view name) {
    auto& table = get_current_ui_context().lucide_icon_table;
    auto itr = table.find(name);
    return itr != table.end() ? itr->second : IconId::invalid_id;
}