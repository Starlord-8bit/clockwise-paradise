#pragma once

#include "core/CWDateTime.h"

class IClockface {
public:
    virtual void setup(CWDateTime *dateTime) = 0;
    virtual void update() = 0;
    virtual ~IClockface() {}
};
