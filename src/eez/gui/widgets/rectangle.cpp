/*
 * EEZ Generic Firmware
 * Copyright (C) 2018-present, Envox d.o.o.
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.

 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.

 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#include <eez/gui/widgets/rectangle.h>

#include <eez/gui/draw.h>
#include <eez/gui/gui.h>
#include <eez/gui/widget.h>
#include <eez/util.h>

namespace eez {
namespace gui {

void RectangleWidget_draw(const WidgetCursor &widgetCursor) {
    widgetCursor.currentState->size = sizeof(WidgetState);

    bool refresh = !widgetCursor.previousState || widgetCursor.previousState->flags.active !=
                                                      widgetCursor.currentState->flags.active;

    if (refresh) {
        const Widget *widget = widgetCursor.widget;
        DECL_WIDGET_SPECIFIC(RectangleWidget, rectangle_widget, widget);
        DECL_WIDGET_STYLE(style, widget);
        drawRectangle(
            widgetCursor.x, widgetCursor.y, (int)widget->w, (int)widget->h, style, nullptr,
            rectangle_widget->flags.invertColors ? !widgetCursor.currentState->flags.active
                                                 : widgetCursor.currentState->flags.active,
            rectangle_widget->flags.ignoreLuminosity);

        g_painted = true;
    }
}

} // namespace gui
} // namespace eez
