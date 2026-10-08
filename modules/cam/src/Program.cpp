#include "readycad/cam/Program.hpp"

#include <glm/geometric.hpp>

#include <cmath>
#include <optional>

namespace rc::cam {

ProgramStats analyse(const Program& program, Real rapidSpeed, Real toolChangeSeconds)
{
    ProgramStats stats;
    std::optional<Vec3> at; // unknown until the first move
    Real minutes = 0.0;
    const auto rapid = [&](Vec3 to) {
        if (at) {
            // Controllers lift first and descend last: XY and Z are travelled one after the other.
            const Real length = glm::distance(Vec2(at->x, at->y), Vec2(to.x, to.y))
                              + std::abs(to.z - at->z);
            stats.rapidLength += length;
            minutes += rapidSpeed > 0.0 ? length / rapidSpeed : 0.0;
        }
        at = to;
    };
    const auto cut = [&](Vec3 to, Real feed) {
        if (at) {
            const Real length = glm::distance(*at, to);
            stats.cutLength += length;
            minutes += feed > 0.0 ? length / feed : 0.0;
        }
        at = to;
    };
    for (const Step& step : program.steps) {
        if (const auto* r = std::get_if<Rapid>(&step)) {
            rapid(r->to);
        } else if (const auto* f = std::get_if<Feed>(&step)) {
            cut(f->to, f->feed);
        } else if (const auto* d = std::get_if<DrillCycle>(&step)) {
            const Real z = at ? at->z : d->clearance;
            rapid({d->at.x, d->at.y, z});
            rapid({d->at.x, d->at.y, d->clearance});
            cut({d->at.x, d->at.y, d->bottom}, d->feed);
            rapid({d->at.x, d->at.y, d->clearance});
            ++stats.holes;
        } else if (std::holds_alternative<ToolChange>(step)) {
            ++stats.toolChanges;
        }
    }
    stats.seconds = minutes * 60.0 + stats.toolChanges * toolChangeSeconds;
    return stats;
}

} // namespace rc::cam
