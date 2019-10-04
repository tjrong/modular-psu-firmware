/*
* EEZ PSU Firmware
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

#if OPTION_SD_CARD
#include <eez/apps/psu/psu.h>
#include <eez/libs/sd_fat/sd_fat.h>

#include <math.h>

#include <eez/scpi/scpi.h>

#include <eez/apps/psu/channel_dispatcher.h>
#include <eez/apps/psu/datetime.h>
#include <eez/apps/psu/scpi/psu.h>
#include <eez/apps/psu/sd_card.h>
#include <eez/system.h>
#include <eez/apps/psu/dlog.h>

namespace eez {

using namespace scpi;

namespace psu {
namespace dlog {

bool g_logVoltage[CH_MAX];
bool g_logCurrent[CH_MAX];
bool g_logPower[CH_MAX];
float g_period = PERIOD_DEFAULT;
float g_time = TIME_DEFAULT;
trigger::Source g_triggerSource = trigger::SOURCE_IMMEDIATE;
char g_filePath[MAX_PATH_LENGTH + 1];

enum State { STATE_IDLE, STATE_INITIATED, STATE_TRIGGERED, STATE_EXECUTING };
static State g_state = STATE_IDLE;

File g_file;
uint32_t g_lastTickCount;
uint32_t g_seconds;
uint32_t g_micros;
uint32_t g_iSample;
double g_currentTime;
double g_nextTime;

static const unsigned int BUFFER_SIZE = 4096;
uint8_t g_buffers[2][BUFFER_SIZE];
int g_selectedbufferIndex;
int g_bufferIndex;

void setState(State newState) {
    if (g_state != newState) {
        if (newState == STATE_EXECUTING) {
            setOperBits(OPER_DLOG, true);
        } else if (g_state == STATE_EXECUTING) {
            setOperBits(OPER_DLOG, false);
        }

        g_state = newState;
    }
}

int checkDlogParameters() {
    bool somethingToLog = false;
    for (int i = 0; i < CH_NUM; ++i) {
        if (g_logVoltage[i] || g_logCurrent[i] || g_logPower[i]) {
            somethingToLog = true;
            break;
        }
    }

    if (!somethingToLog) {
        // TODO replace with more specific error
        return SCPI_ERROR_EXECUTION_ERROR;
    }

    if (!*g_filePath) {
        // TODO replace with more specific error
        return SCPI_ERROR_EXECUTION_ERROR;
    }

    return 0;
}

bool isIdle() {
    return g_state == STATE_IDLE;
}

bool isInitiated() {
    return g_state == STATE_INITIATED;
}

int initiate(const char *filePath) {
    int error = SCPI_RES_OK;

    strcpy(g_filePath, filePath);

    if (g_triggerSource == trigger::SOURCE_IMMEDIATE) {
        error = startImmediately();
    } else {
        error = checkDlogParameters();
        if (!error) {
            setState(STATE_INITIATED);
        }
    }

    if (error) {
        g_filePath[0] = 0;
    }

    return error;
}

void triggerGenerated(bool startImmediatelly) {
    if (startImmediatelly) {
        int err = startImmediately();
        if (err != SCPI_RES_OK) {
            generateError(err);
        }
    } else {
        setState(STATE_TRIGGERED);
    }
}

#define MAGIC1 0x2D5A4545L
#define MAGIC2 0x474F4C44L
#define VERSION 0x00000001L

#define DISK_OPERATION_OPEN 1
#define DISK_OPERATION_WRITE 2
#define DISK_OPERATION_CLOSE 3

void executeDiskOperation(int diskOperation) {
    switch (diskOperation) {
    case DISK_OPERATION_OPEN:
        if (!g_file.open(g_filePath, FILE_OPEN_APPEND | FILE_WRITE)) {
            // return SCPI_ERROR_EXECUTION_ERROR;
            return;
        }

        if (!g_file.truncate(0)) {
            // return SCPI_ERROR_MASS_STORAGE_ERROR;
            return;
        }
        break;

    case DISK_OPERATION_WRITE:
        g_file.write(&g_buffers[g_selectedbufferIndex ? 0 : 1][0], BUFFER_SIZE);
        break;

    case DISK_OPERATION_CLOSE:
        if (g_bufferIndex > 0) {
            g_file.write(&g_buffers[g_selectedbufferIndex][0], g_bufferIndex);
        }
        g_file.close();
        break;
    }
}

void queueDiskOperation(int diskOperation) {
    if (osThreadGetId() != g_scpiTaskHandle) {
        osMessagePut(g_scpiMessageQueueId, SCPI_QUEUE_MESSAGE(SCPI_QUEUE_MESSAGE_TARGET_NONE, SCPI_QUEUE_MESSAGE_DLOG_DISK_OPERATION, diskOperation), osWaitForever);
    } else {
        executeDiskOperation(diskOperation);
    }
}

void writeUint8(uint8_t value) {
    g_buffers[g_selectedbufferIndex][g_bufferIndex] = value;
    if (++g_bufferIndex == BUFFER_SIZE) {
        g_selectedbufferIndex = g_selectedbufferIndex ? 0 : 1;
        g_bufferIndex = 0;
        queueDiskOperation(DISK_OPERATION_WRITE);
    }
}

void writeUint16(uint16_t value) {
    writeUint8(value & 0xFF);
    writeUint8((value >> 8) & 0xFF);
}

void writeUint32(uint32_t value) {
    writeUint8(value & 0xFF);
    writeUint8((value >> 8) & 0xFF);
    writeUint8((value >> 16) & 0xFF);
    writeUint8(value >> 24);
}

void writeFloat(float value) {
    writeUint32(*((uint32_t *)&value));
}

int startImmediately() {
    int err = checkDlogParameters();
    if (err) {
        return err;
    }

    queueDiskOperation(DISK_OPERATION_OPEN);

    g_selectedbufferIndex = 0;
    g_bufferIndex = 0;

    setState(STATE_EXECUTING);

    writeUint32(MAGIC1);
    writeUint32(MAGIC2);

    writeUint16(VERSION);

    if (CONF_DLOG_JITTER) {
        writeUint16(1);
    } else {
        writeUint16(0);
    }

    uint32_t columns = 0;
    for (int iChannel = 0; iChannel < CH_NUM; ++iChannel) {
        if (g_logVoltage[iChannel]) {
            columns |= 1 << (4 * iChannel);
        }
        if (g_logCurrent[iChannel]) {
            columns |= 2 << (4 * iChannel);
        }
        if (g_logPower[iChannel]) {
            columns |= 4 << (4 * iChannel);
        }
    }
    writeUint32(columns);

    writeFloat(g_period);
    writeFloat(g_time);
    writeUint32(datetime::nowUtc());

    g_lastTickCount = micros();
    g_seconds = 0;
    g_micros = 0;
    g_iSample = 0;
    g_currentTime = 0;
    g_nextTime = 0;

    log(g_lastTickCount);

    return SCPI_RES_OK;
}

void finishLogging() {
    setState(STATE_IDLE);
    queueDiskOperation(DISK_OPERATION_CLOSE);
    for (int i = 0; i < CH_NUM; ++i) {
        g_logVoltage[i] = 0;
        g_logCurrent[i] = 0;
        g_logPower[i] = 0;
    }
}

void abort() {
    if (g_state == STATE_EXECUTING) {
        finishLogging();
    } else if (g_state == STATE_INITIATED || g_state == STATE_TRIGGERED) {
        setState(STATE_IDLE);
    }
}

void log(uint32_t tickCount) {
    g_micros += tickCount - g_lastTickCount;
    g_lastTickCount = tickCount;

    if (g_micros >= 1000000) {
        ++g_seconds;
        g_micros -= 1000000;
    }

    g_currentTime = g_seconds + g_micros * 1E-6;

    if (g_currentTime >= g_nextTime) {
        while (1) {
            g_nextTime = ++g_iSample * g_period;
            if (g_currentTime < g_nextTime || g_nextTime > g_time) {
                break;
            }

            // we missed a sample, write NAN
#ifdef DLOG_JITTER
            writeFloat(NAN);
#endif
            for (int i = 0; i < CH_NUM; ++i) {
                if (g_logVoltage[i]) {
                    writeFloat(NAN);
                }
                if (g_logCurrent[i]) {
                    writeFloat(NAN);
                }
                if (g_logPower[i]) {
                    writeFloat(NAN);
                }
            }
        }

        // write sample
#ifdef DLOG_JITTER
        writeFloat(dt);
#endif
        for (int i = 0; i < CH_NUM; ++i) {
            Channel &channel = Channel::get(i);

            float uMon = 0;
            float iMon = 0;

            if (g_logVoltage[i]) {
                uMon = channel_dispatcher::getUMonLast(channel);
                writeFloat(uMon);
            }

            if (g_logCurrent[i]) {
                iMon = channel_dispatcher::getIMonLast(channel);
                writeFloat(iMon);
            }

            if (g_logPower[i]) {
                if (!g_logVoltage[i]) {
                    uMon = channel_dispatcher::getUMonLast(channel);
                }
                if (!g_logCurrent[i]) {
                    iMon = channel_dispatcher::getIMonLast(channel);
                }
                writeFloat(uMon * iMon);
            }
        }

        if (g_nextTime > g_time) {
            finishLogging();
        }
    }
}

void tick(uint32_t tickCount) {
    if (g_state == STATE_TRIGGERED) {
        int err = startImmediately();
        if (err != SCPI_RES_OK) {
            generateError(err);
        }
    } else if (g_state == STATE_EXECUTING) {
        log(tickCount);
    }
}

void reset() {
    abort();

    for (int i = 0; i < CH_NUM; ++i) {
        g_logVoltage[i] = 0;
        g_logCurrent[i] = 0;
        g_logPower[i] = 0;
    }

    g_period = PERIOD_DEFAULT;
    g_time = TIME_DEFAULT;
    g_triggerSource = trigger::SOURCE_IMMEDIATE;
    g_filePath[0] = 0;
}

} // namespace dlog
} // namespace psu
} // namespace eez

#endif // OPTION_SD_CARD
