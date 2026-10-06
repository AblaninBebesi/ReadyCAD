#include "readycad/core/Hardware.hpp"

#include <string>

namespace rc::core {

std::vector<HandleModel> defaultHandleCatalog()
{
    std::vector<HandleModel> catalog;
    for (const int spacing : {96, 128, 160, 192, 256, 320}) {
        const std::string size = std::to_string(spacing);
        HandleModel bar;
        bar.id = "bar-" + size;
        bar.name = "Bar " + size;
        bar.supplier = "Generic";
        bar.article = "BAR-" + size;
        bar.type = HandleType::Bar;
        bar.holeSpacing = spacing;
        bar.length = spacing + 20.0;
        catalog.push_back(bar);
    }

    HandleModel knob;
    knob.id = "knob-30";
    knob.name = "Knob 30";
    knob.supplier = "Generic";
    knob.article = "KNOB-30";
    knob.type = HandleType::Knob;
    knob.holeSpacing = 0.0;
    knob.length = 30.0;
    knob.width = 30.0;
    knob.projection = 25.0;
    catalog.push_back(knob);

    return catalog;
}

} // namespace rc::core
