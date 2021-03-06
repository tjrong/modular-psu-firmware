/*
 * EEZ Modular Firmware
 * Copyright (C) 2015-present, Envox d.o.o.
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

#if OPTION_DISPLAY

#include <eez/util.h>

#include <eez/gui/gui.h>

namespace eez {
namespace gui {

struct MultilineTextWidget {
    const char *text;
    int16_t firstLineIndent;
    int16_t hangingIndent;
};

FixPointersFunctionType MULTILINE_TEXT_fixPointers = [](Widget *widget, Assets *assets) {
    MultilineTextWidget *multilineTextWidget = (MultilineTextWidget *)widget->specific;
    multilineTextWidget->text = (const char *)((uint8_t *)assets->document + (uint32_t)multilineTextWidget->text);
};

EnumFunctionType MULTILINE_TEXT_enum = nullptr;

DrawFunctionType MULTILINE_TEXT_draw = [](const WidgetCursor &widgetCursor) {
    const Widget *widget = widgetCursor.widget;

    widgetCursor.currentState->size = sizeof(WidgetState);
    widgetCursor.currentState->data =
        widget->data ? get(widgetCursor.cursor, widget->data) : 0;

    bool refresh =
        !widgetCursor.previousState ||
        widgetCursor.previousState->flags.active != widgetCursor.currentState->flags.active ||
        widgetCursor.previousState->data != widgetCursor.currentState->data;

    if (refresh) {
        const Style* style = getStyle(widget->style);

        const MultilineTextWidget *display_string_widget = GET_WIDGET_PROPERTY(widget, specific, const MultilineTextWidget *);

        if (widget->data) {
            if (widgetCursor.currentState->data.isString()) {
                drawMultilineText(widgetCursor.currentState->data.getString(), widgetCursor.x,
                    widgetCursor.y, (int)widget->w, (int)widget->h, style,
                    widgetCursor.currentState->flags.active,
                    display_string_widget->firstLineIndent, display_string_widget->hangingIndent);
            } else {
                char text[64];
                widgetCursor.currentState->data.toText(text, sizeof(text));
                drawMultilineText(text, widgetCursor.x, widgetCursor.y, (int)widget->w,
                    (int)widget->h, style,
                    widgetCursor.currentState->flags.active,
                    display_string_widget->firstLineIndent, display_string_widget->hangingIndent);
            }
        } else {
            drawMultilineText(
                GET_WIDGET_PROPERTY(display_string_widget, text, const char *), 
                widgetCursor.x, widgetCursor.y, (int)widget->w, (int)widget->h,
                style, widgetCursor.currentState->flags.active,
                display_string_widget->firstLineIndent, display_string_widget->hangingIndent);
        }
    }
};

OnTouchFunctionType MULTILINE_TEXT_onTouch = nullptr;

} // namespace gui
} // namespace eez

#endif