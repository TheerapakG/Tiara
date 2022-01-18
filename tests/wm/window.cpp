#include "spdlog/spdlog.h"

#include "tiara/core/core.hpp"
#include "tiara/wm/wm.hpp"

#include "skia/core/SkFont.h"
#include "skia/core/SkPaint.h"

#include <iostream>

int main() {
    spdlog::set_pattern("%Y-%m-%d %T.%e %n [%t] %^%l:%$ %v");
    spdlog::set_level(spdlog::level::debug);
    tiara::core::application_name = "Tiara Window Test";
    tiara::core::application_version = {1, 0, 0};
    tiara::core::vulkan_instance_layers = {"VK_LAYER_KHRONOS_validation"};
    auto tiara_raii = tiara::core::Tiara<tiara::wm::WMExtension>::init_ext();
    std::iostream::sync_with_stdio(false);
    std::cin.tie(nullptr);
    tiara::wm::Window window_1{{1280, 720}, "tiara engine test window 1"};
    auto window_1_draw_handler = tiara::core::event::make_function_handler<tiara::common::events::DrawEvent>(
        [](const tiara::common::events::DrawEvent& event){
            event.canvas->clear(SK_ColorBLACK);
            event.canvas->drawString("tiara engine test window 1", 0, 64, {nullptr, 64}, SkPaint{SkColor4f::FromColor(SK_ColorWHITE)});
            return true;
        }
    );
    window_1.start_dispatch(window_1_draw_handler);
    window_1.draw();
    window_1.stop();
    tiara::wm::Window window_2{{1280, 720}, "tiara engine test window 2"};
    auto window_2_draw_handler = tiara::core::event::make_function_handler<tiara::common::events::DrawEvent>(
        [](const tiara::common::events::DrawEvent& event){
            event.canvas->clear(SK_ColorBLACK);
            event.canvas->drawString("tiara engine test window 2", 0, 64, {nullptr, 64}, SkPaint{SkColor4f::FromColor(SK_ColorWHITE)});
            return true;
        }
    );
    window_2.start_dispatch(window_2_draw_handler);
    window_2.draw();
    window_2.stop();
    std::cin.ignore();
}
