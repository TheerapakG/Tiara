#ifndef TIARA_CORE_WIDGETS_WIDGETTYPE
#define TIARA_CORE_WIDGETS_WIDGETTYPE

#include "tiara/core/event/handler.hpp"

namespace tiara::widgets::events {
    struct DrawEvent: core::event::Event {
        using RetType = bool;
    };
}

namespace tiara::widgets {
    struct distanceSize {
        int top, bottom, left, right;
    };

    struct Widget: core::event::Handler<events::DrawEvent> {
        virtual distanceSize margin() = 0;
    };
}

#endif
