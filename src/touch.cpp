#include "touch.h"
#include "config.h"
#include <Wire.h>

static uint8_t touchRotation = 0;

static constexpr int16_t SWIPE_MIN_PIXELS = 45;
static constexpr float SWIPE_HORIZONTAL_BIAS = 1.25f;

void touchInit() {
    Wire.begin(PIN_TOUCH_SDA, PIN_TOUCH_SCL);
}

void touchSetRotation(uint8_t rotationQuarterTurns) {
    touchRotation = rotationQuarterTurns % 4;
}

static bool touchReadBytes(uint8_t reg, uint8_t *buffer, size_t len) {
    Wire.beginTransmission(CST816_ADDR);
    Wire.write(reg);
    if (Wire.endTransmission(false) != 0) {
        return false;
    }

    size_t read = Wire.requestFrom(static_cast<int>(CST816_ADDR), static_cast<int>(len));
    if (read != len) {
        return false;
    }

    for (size_t i = 0; i < len; i++) {
        buffer[i] = Wire.read();
    }
    return true;
}

static bool readTouchPoint(TouchPoint &point) {
    uint8_t data[6] = {};
    if (!touchReadBytes(0x01, data, sizeof(data))) {
        point.touched = false;
        return false;
    }

    uint8_t points = data[1] & 0x0F;
    if (points == 0) {
        point.touched = false;
        return true;
    }

    point.touched = true;
    int16_t rawX = ((data[2] & 0x0F) << 8) | data[3];
    int16_t rawY = ((data[4] & 0x0F) << 8) | data[5];

    switch (touchRotation) {
        case 1:
            point.x = rawY;
            point.y = 239 - rawX;
            break;
        case 2:
            point.x = 239 - rawX;
            point.y = 239 - rawY;
            break;
        case 3:
            point.x = 239 - rawY;
            point.y = rawX;
            break;
        default:
            point.x = rawX;
            point.y = rawY;
            break;
    }
    return true;
}

bool touchPollForSwipe(int8_t *direction) {
    static bool wasTouched = false;
    static int16_t startX = 0, startY = 0, lastX = 0, lastY = 0;
    static uint32_t startMs = 0;

    TouchPoint point;
    if (!readTouchPoint(point)) {
        return false;
    }

    if (point.touched) {
        if (!wasTouched) {
            startX = point.x;
            startY = point.y;
            startMs = millis();
        }
        lastX = point.x;
        lastY = point.y;
        wasTouched = true;
        return false;
    }

    if (!wasTouched) {
        return false;
    }
    wasTouched = false;

    int16_t dx = lastX - startX;
    int16_t dy = lastY - startY;
    int16_t absDx = abs(dx);
    int16_t absDy = abs(dy);
    uint32_t durationMs = millis() - startMs;

    if (durationMs < 1000 &&
        absDx >= SWIPE_MIN_PIXELS &&
        absDx > static_cast<int16_t>(absDy * SWIPE_HORIZONTAL_BIAS)) {
        *direction = (dx < 0) ? -1 : 1;
        return true;
    }

    return false;
}
