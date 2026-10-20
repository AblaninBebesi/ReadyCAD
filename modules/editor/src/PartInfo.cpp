#include "readycad/editor/PartInfo.hpp"

#include "readycad/core/model/Queries.hpp"

namespace rc::editor {

PartInfo describe(const core::FurnitureComponent& component, const core::ConstructionRules& rules)
{
    PartInfo info;
    info.kind = component.kind();
    info.name = component.name();
    info.size = component.size();
    if (component.kind() == core::ComponentKind::Kitchen) {
        // An L or a U kitchen reaches beyond its back run: the room it takes.
        info.size = core::Size3D::fromVec3(core::panelBounds(component).size());
    }
    info.position = component.position();

    if (const core::Panel* panel = component.panel()) {
        PanelInfo p;
        p.material = panel->material();
        p.finishLength = panel->finishLength();
        p.finishWidth = panel->finishWidth();
        p.thickness = panel->thickness();
        p.cut = detailing::cutSize(*panel, rules);
        p.edges = panel->edges();
        for (const core::Drilling& d : panel->drillings()) {
            ++p.holes[d.fastener];
        }
        p.holeCount = static_cast<int>(panel->drillings().size());
        p.grooveCount = static_cast<int>(panel->grooves().size());
        info.panel = p;
    }
    return info;
}

} // namespace rc::editor
