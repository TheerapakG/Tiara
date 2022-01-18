#ifndef TIARA_COMMON_EVENTS_DRAW
#define TIARA_COMMON_EVENTS_DRAW

#include "tiara/core/event/eventtype.hpp"

#include "skia/core/SkCanvas.h"

namespace tiara::common::events {
    struct DrawEvent: core::event::Event {
        using RetType = bool;
        SkCanvas* canvas;
    };
}

#endif
