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

#include <eez/gui/widgets/layout_view.h>

#include <eez/gui/assets.h>
#include <eez/gui/draw.h>
#include <eez/gui/widgets/container.h>

namespace eez {
namespace gui {

int getLayoutId(const WidgetCursor &widgetCursor) {
    if (widgetCursor.widget->data) {
        auto layoutValue = data::get(widgetCursor.cursor, widgetCursor.widget->data);
        return layoutValue.getInt();
    }
    
    const LayoutViewWidgetSpecific *layoutViewSpecific = GET_WIDGET_PROPERTY(widgetCursor.widget, specific, const LayoutViewWidgetSpecific *);
    return layoutViewSpecific->layout;
}

void LayoutViewWidget_enum(WidgetCursor &widgetCursor, EnumWidgetsCallback callback) {
    auto cursor = widgetCursor.cursor;

    const LayoutViewWidgetSpecific *layoutViewSpecific = GET_WIDGET_PROPERTY(widgetCursor.widget, specific, const LayoutViewWidgetSpecific *);

    if (widgetCursor.previousState && (widgetCursor.previousState->data != widgetCursor.currentState->data || ((LayoutViewWidgetState *)widgetCursor.previousState)->context != ((LayoutViewWidgetState *)widgetCursor.currentState)->context)) {
        widgetCursor.previousState = 0;
    }

    Value oldContext;
    Value newContext;
    if (layoutViewSpecific->context) {
        data::setContext(widgetCursor.cursor, layoutViewSpecific->context, oldContext, newContext);
    }

    int layoutId = getLayoutId(widgetCursor);
    const Widget *layout = layoutId != -1 ? getPageWidget(layoutId) : nullptr;

    if (layout) {
		const PageWidget *layoutSpecific = GET_WIDGET_PROPERTY(layout, specific, const PageWidget *);
		enumContainer(widgetCursor, callback, layoutSpecific->widgets);
	}

    if (layoutViewSpecific->context) {
        data::restoreContext(widgetCursor.cursor, layoutViewSpecific->context, oldContext);
    }

    widgetCursor.cursor = cursor;
}

void LayoutViewWidget_draw(const WidgetCursor &widgetCursor) {
    const Widget *widget = widgetCursor.widget;
    const LayoutViewWidgetSpecific *layoutViewSpecific = GET_WIDGET_PROPERTY(widgetCursor.widget, specific, const LayoutViewWidgetSpecific *);

    Value oldContext;
    Value newContext;
    if (layoutViewSpecific->context) {
        data::setContext(((WidgetCursor &)widgetCursor).cursor, layoutViewSpecific->context, oldContext, newContext);
        ((LayoutViewWidgetState *)widgetCursor.currentState)->context = newContext;
    } else {
        ((LayoutViewWidgetState *)widgetCursor.currentState)->context = Value();
    }

    widgetCursor.currentState->data = getLayoutId(widgetCursor);

    if (layoutViewSpecific->context) {
        data::restoreContext(((WidgetCursor &)widgetCursor).cursor, layoutViewSpecific->context, oldContext);
    }

    bool refresh =
        !widgetCursor.previousState ||
        widgetCursor.previousState->flags.active != widgetCursor.currentState->flags.active ||
        widgetCursor.previousState->data != widgetCursor.currentState->data ||
        ((LayoutViewWidgetState *)widgetCursor.previousState)->context != ((LayoutViewWidgetState *)widgetCursor.currentState)->context;

    if (refresh) {
        const Style* style = getStyle(widget->style);
        drawRectangle(widgetCursor.x, widgetCursor.y, (int)widget->w, (int)widget->h, style, !widgetCursor.currentState->flags.active, false);
    }
}

} // namespace gui
} // namespace eez

#endif