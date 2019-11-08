/*
* EEZ PSU Firmware
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

#pragma once

#include <eez/gui/data.h>

#define NUM_STEPS_PER_UNIT 5

namespace eez {
namespace psu {
namespace gui {
namespace edit_mode_step {

int getStepIndex();

int getStepValuesCount();
const eez::gui::data::Value *getStepValues();

const eez::gui::data::Value *getUnitStepValues(Unit unit);

void setStepIndex(int value);

#if OPTION_ENCODER
void onEncoder(int counter);
#endif

void onTouchDown();
void onTouchMove();
void onTouchUp();

void switchToNextStepIndex();

eez::gui::data::Value getCurrentEncoderStepValue();
void showCurrentEncoderMode();

} // namespace edit_mode_step
} // namespace gui
} // namespace psu
} // namespace eez
