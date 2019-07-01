/*
 * EEZ Generic Firmware
 * Copyright (C) 2019-present, Envox d.o.o.
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

#include <spi.h>
#include <cmsis_os.h>

#include <eez/platform/stm32/spi.h>

#include <eez/index.h>

namespace eez {
namespace psu {
namespace spi {

static SPI_HandleTypeDef *spiHandle[] = { &hspi2, &hspi4, &hspi5 };

static GPIO_TypeDef *SPI_CSA_GPIO_Port[] = { SPI2_CSA_GPIO_Port, SPI4_CSA_GPIO_Port, SPI5_CSA_GPIO_Port };
static GPIO_TypeDef *SPI_CSB_GPIO_Port[] = { SPI2_CSB_GPIO_Port, SPI4_CSB_GPIO_Port, SPI5_CSB_GPIO_Port };

static const uint16_t SPI_CSA_Pin[] = { SPI2_CSA_Pin, SPI4_CSA_Pin, SPI5_CSA_Pin };
static const uint16_t SPI_CSB_Pin[] = { SPI2_CSB_Pin, SPI4_CSB_Pin, SPI5_CSB_Pin };

void select(uint8_t slotIndex, int chip) {
	taskENTER_CRITICAL();

	__HAL_SPI_DISABLE(spiHandle[slotIndex]);
    if (chip == CHIP_IOEXP) {
        // set SPI mode 0
        WRITE_REG(spiHandle[slotIndex]->Instance->CR1, READ_REG(spiHandle[slotIndex]->Instance->CR1) & ~SPI_PHASE_2EDGE);
    } else {
        // set SPI mode 1
        WRITE_REG(spiHandle[slotIndex]->Instance->CR1, READ_REG(spiHandle[slotIndex]->Instance->CR1) | SPI_PHASE_2EDGE);
    }
	__HAL_SPI_ENABLE(spiHandle[slotIndex]);

    if (g_slots[slotIndex].moduleType == MODULE_TYPE_DCP405) {
    	if (chip == CHIP_DAC) {
			// 00
			HAL_GPIO_WritePin(SPI_CSA_GPIO_Port[slotIndex], SPI_CSA_Pin[slotIndex], GPIO_PIN_RESET);
			HAL_GPIO_WritePin(SPI_CSB_GPIO_Port[slotIndex], SPI_CSB_Pin[slotIndex], GPIO_PIN_RESET);
		} else if (chip == CHIP_ADC) {
    		// 01
    	    HAL_GPIO_WritePin(SPI_CSA_GPIO_Port[slotIndex], SPI_CSA_Pin[slotIndex], GPIO_PIN_SET);
    	    HAL_GPIO_WritePin(SPI_CSB_GPIO_Port[slotIndex], SPI_CSB_Pin[slotIndex], GPIO_PIN_RESET);
    	} else {
    		// IOEXP
    		// 10
    	    HAL_GPIO_WritePin(SPI_CSA_GPIO_Port[slotIndex], SPI_CSA_Pin[slotIndex], GPIO_PIN_RESET);
    	    HAL_GPIO_WritePin(SPI_CSB_GPIO_Port[slotIndex], SPI_CSB_Pin[slotIndex], GPIO_PIN_SET);
    	}
    } else {
    	if (chip == CHIP_DAC) {
			// 00
			HAL_GPIO_WritePin(SPI_CSA_GPIO_Port[slotIndex], SPI_CSA_Pin[slotIndex], GPIO_PIN_RESET);
			HAL_GPIO_WritePin(SPI_CSB_GPIO_Port[slotIndex], SPI_CSB_Pin[slotIndex], GPIO_PIN_RESET);
		} else if (chip == CHIP_ADC) {
    		// 01
    	    HAL_GPIO_WritePin(SPI_CSA_GPIO_Port[slotIndex], SPI_CSA_Pin[slotIndex], GPIO_PIN_SET);
    	    HAL_GPIO_WritePin(SPI_CSB_GPIO_Port[slotIndex], SPI_CSB_Pin[slotIndex], GPIO_PIN_RESET);
    	} else {
    		// IOEXP
    		// 11
    	    HAL_GPIO_WritePin(SPI_CSA_GPIO_Port[slotIndex], SPI_CSA_Pin[slotIndex], GPIO_PIN_SET);
    	    HAL_GPIO_WritePin(SPI_CSB_GPIO_Port[slotIndex], SPI_CSB_Pin[slotIndex], GPIO_PIN_SET);
    	}
    }
}

void deselect(uint8_t slotIndex) {
	if (g_slots[slotIndex].moduleType == MODULE_TYPE_DCP405) {
		// 11
		HAL_GPIO_WritePin(SPI_CSA_GPIO_Port[slotIndex], SPI_CSA_Pin[slotIndex], GPIO_PIN_SET);
		HAL_GPIO_WritePin(SPI_CSB_GPIO_Port[slotIndex], SPI_CSB_Pin[slotIndex], GPIO_PIN_SET);
	} else {
		// 10
		HAL_GPIO_WritePin(SPI_CSA_GPIO_Port[slotIndex], SPI_CSA_Pin[slotIndex], GPIO_PIN_RESET);
		HAL_GPIO_WritePin(SPI_CSB_GPIO_Port[slotIndex], SPI_CSB_Pin[slotIndex], GPIO_PIN_SET);
	}

	taskEXIT_CRITICAL();
}

void transfer(uint8_t slotIndex, uint8_t *input, uint8_t *output, uint16_t size) {
    HAL_SPI_TransmitReceive(spiHandle[slotIndex], input, output, size, 10);
}

} // namespace spi
} // namespace psu
} // namespace eez
