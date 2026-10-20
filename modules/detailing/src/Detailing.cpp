#include "readycad/detailing/Detailing.hpp"

#include "readycad/core/model/Carcass.hpp"
#include "readycad/core/model/CornerCabinet.hpp"
#include "readycad/core/model/Parts.hpp"
#include "readycad/detailing/Joints.hpp"
#include "readycad/detailing/PanelFrame.hpp"

#include <glm/geometric.hpp>

#include <algorithm>
#include <cmath>
#include <optional>
#include <string>

namespace rc::detailing {

using core::Axis;
using core::ConstructionRules;
using core::FastenerType;
using core::PanelComponent;
using core::PanelFace;
using core::Real;
using core::Vec3;

namespace {

constexpr Real kTol = 1e-3;

/// Adds a hole whose entry centre is `world` (projected exactly onto `face`).
void addHole(PanelComponent& part, PanelFace face, const Vec3& world, FastenerType type,
             Real diameter, Real depth)
{
    const PanelFrame frame(part);
    const Vec3 local = frame.projectOnto(face, frame.toLocal(world));
    const Vec3 size = frame.extents();
    // A hole can never be deeper than the panel in its direction.
    const Real limit = (face == PanelFace::Top || face == PanelFace::Bottom) ? size.z
                     : (face == PanelFace::EdgeL1 || face == PanelFace::EdgeL2) ? size.y
                                                                                : size.x;
    part.panel()->addDrilling({type, face, local, diameter, std::min(depth, limit)});
}

/// True if a hole of `diameter` at `world` on `part` would hit an existing hole of the part's
/// faces (holes through the panel from both sides collide whichever face they come from).
bool collidesWithFaceHole(const PanelComponent& part, const Vec3& world, Real diameter)
{
    const PanelFrame frame(part);
    const Vec3 local = frame.toLocal(world);
    for (const core::Drilling& d : part.panel()->drillings()) {
        if (!d.isFaceHole()) {
            continue;
        }
        const Real distance = glm::length(core::Vec2(d.position.x - local.x,
                                                     d.position.y - local.y));
        if (distance < (d.diameter + diameter) / 2.0) {
            return true;
        }
    }
    return false;
}

// ---------------------------------------------------------------------------------------------
// Joints
// ---------------------------------------------------------------------------------------------

void addConnector(const Joint& j, const Vec3& p, ConstructionRules::JointFastener fastener,
                  const ConstructionRules& r)
{
    PanelComponent& facePart = *j.face;
    PanelComponent& edgePart = *j.edge;
    const int n = index(j.normal);

    const bool contactAtFaceMax = std::abs(facePart.box().max()[n] - j.plane) <= kTol;
    const PanelFace contactFace = PanelFrame(facePart).faceAt(j.normal, contactAtFaceMax);
    const bool contactAtEdgeMax = std::abs(edgePart.box().max()[n] - j.plane) <= kTol;
    const PanelFace edgeFace = PanelFrame(edgePart).faceAt(j.normal, contactAtEdgeMax);
    const Real faceThickness = facePart.panel()->thickness();

    switch (fastener) {
    case ConstructionRules::JointFastener::Confirmat: {
        // Through hole from the OUTER face (the screw head sits there), screw into the edge.
        constexpr Real kReserve = 2.0; // room for chips at the bottom of the edge hole
        addHole(facePart, opposite(contactFace), p, FastenerType::Confirmat,
                r.confirmat.faceDiameter, faceThickness);
        addHole(edgePart, edgeFace, p, FastenerType::Confirmat, r.confirmat.edgeDiameter,
                r.confirmat.length - faceThickness + kReserve);
        break;
    }
    case ConstructionRules::JointFastener::Dowel: {
        constexpr Real kReserve = 1.0;
        addHole(facePart, contactFace, p, FastenerType::Dowel, r.dowel.diameter,
                r.dowel.faceDepth);
        addHole(edgePart, edgeFace, p, FastenerType::Dowel, r.dowel.diameter,
                r.dowel.length - r.dowel.faceDepth + kReserve);
        break;
    }
    case ConstructionRules::JointFastener::Eccentric: {
        // Bolt screws into the face panel; its channel runs in the edge panel up to the cam,
        // which sits in the edge panel's face that looks towards the face panel.
        addHole(facePart, contactFace, p, FastenerType::Eccentric, r.eccentric.boltHoleDiameter,
                r.eccentric.boltHoleDepth);
        addHole(edgePart, edgeFace, p, FastenerType::Eccentric, r.eccentric.channelDiameter,
                r.eccentric.camDistance);
        Vec3 cam = p;
        cam[n] += contactAtEdgeMax ? -r.eccentric.camDistance : r.eccentric.camDistance;
        const PanelFace camFace = PanelFrame(edgePart).faceToward(facePart.box().center());
        addHole(edgePart, camFace, cam, FastenerType::Eccentric, r.eccentric.camDiameter,
                r.eccentric.camDepth);
        break;
    }
    }
}

void addJointFasteners(const Joint& j, const ConstructionRules& r, DetailingReport& report)
{
    const int along = index(j.along);
    const int across = index(j.across);
    const Real from = j.contact.min()[along];
    const Real to = j.contact.max()[along];
    const Real pitch = r.system32.pitch;

    Vec3 base = j.contact.center();
    base[index(j.normal)] = j.plane;
    base[across] = j.edge->box().center()[across]; // middle of the edge panel's thickness

    const Real largest = std::max({r.confirmat.faceDiameter, r.dowel.diameter,
                                   r.eccentric.camDiameter});
    auto inside = [&](Real q) { return q - largest / 2.0 >= from && q + largest / 2.0 <= to; };

    for (const Real position : connectorPositions(from, to, r.joints)) {
        // A face panel joined on both faces at the same place (a partition with fixed shelves
        // left and right) cannot take two connectors in one spot: move along the joint by whole
        // pitches (towards the middle first) to the nearest free place.
        const Real towardsMiddle = position < (from + to) / 2.0 ? 1.0 : -1.0;
        std::optional<Vec3> placed;
        for (int step = 0; step <= 3 && !placed; ++step) {
            for (const Real direction : {towardsMiddle, -towardsMiddle}) {
                Vec3 p = base;
                p[along] = position + direction * step * pitch;
                if (inside(p[along]) && !collidesWithFaceHole(*j.face, p, largest)) {
                    placed = p;
                    break;
                }
            }
        }
        if (!placed) {
            report.warnings.push_back("No free place for a connector of a "
                                      + std::string(toString(j.edge->kind())) + " joint");
            continue;
        }
        const Vec3& p = *placed;
        addConnector(j, p, r.joints.fastener, r);

        if (r.joints.alignmentDowels
            && r.joints.fastener != ConstructionRules::JointFastener::Dowel) {
            Vec3 d = p;
            d[along] += p[along] < (from + to) / 2.0 ? pitch : -pitch;
            if (inside(d[along]) && !collidesWithFaceHole(*j.face, d, r.dowel.diameter)) {
                addConnector(j, d, ConstructionRules::JointFastener::Dowel, r);
            }
        }
    }
}

// ---------------------------------------------------------------------------------------------
// Shelf supports
// ---------------------------------------------------------------------------------------------

bool isWall(const core::FurnitureComponent& c)
{
    return c.kind() == core::ComponentKind::SideWall || c.kind() == core::ComponentKind::Partition;
}

/// Z of the support rows under a shelf: front row 37 mm behind the wall's front edge, rear row
/// a whole number of pitches behind it, at least rearRowMinOffset in front of the shelf's rear.
std::vector<Real> supportRows(const core::Aabb& wall, const core::Aabb& shelf,
                              const ConstructionRules& r)
{
    const Real pitch = r.system32.pitch;
    Real front = wall.max().z - r.system32.frontRowOffset;
    while (front > shelf.max().z - r.shelfSupport.diameter && front - pitch > shelf.min().z) {
        front -= pitch; // shelf set far back: keep the pin under it
    }
    std::vector<Real> rows{front};
    const Real rearLimit = shelf.min().z + r.system32.rearRowMinOffset;
    const auto steps = static_cast<int>(std::floor((front - rearLimit) / pitch));
    if (steps >= 1) {
        rows.push_back(front - steps * pitch);
    }
    return rows;
}

void addShelfSupports(core::Carcass& carcass, const std::vector<PanelComponent*>& walls,
                      const ConstructionRules& r, DetailingReport& report)
{
    const auto& sr = r.shelfSupport;
    for (core::Shelf* shelf : core::findAll<core::Shelf>(carcass)) {
        if (shelf->mount() != core::ShelfMount::Supports) {
            continue;
        }
        const core::Aabb& s = shelf->box();
        const Real holeY = s.min().y - sr.holeBelowShelf;

        for (const bool leftWall : {true, false}) {
            const Real plane = leftWall ? s.min().x - r.shelfSideGap : s.max().x + r.shelfSideGap;
            PanelComponent* wall = nullptr;
            for (PanelComponent* w : walls) {
                const Real wallFace = leftWall ? w->box().max().x : w->box().min().x;
                if (std::abs(wallFace - plane) <= kTol && holeY > w->box().min().y
                    && holeY < w->box().max().y) {
                    wall = w;
                }
            }
            if (wall == nullptr) {
                report.warnings.push_back("No wall found for the supports of a shelf");
                continue;
            }

            const PanelFace face = PanelFrame(*wall).faceAt(Axis::X, leftWall);
            for (const Real z : supportRows(wall->box(), s, r)) {
                for (int k = -sr.extraHoles; k <= sr.extraHoles; ++k) {
                    const Real y = holeY + k * r.system32.pitch;
                    if (y - sr.diameter / 2.0 < wall->box().min().y
                        || y + sr.diameter / 2.0 > wall->box().max().y) {
                        continue;
                    }
                    addHole(*wall, face, Vec3(plane, y, z), FastenerType::ShelfSupport,
                            sr.diameter, sr.depth);
                }
            }
        }
    }
}

// ---------------------------------------------------------------------------------------------
// Hinges
// ---------------------------------------------------------------------------------------------

std::vector<Real> hingeAxes(const core::Aabb& door, Real gridOrigin, const ConstructionRules& r)
{
    const auto& h = r.hinge;
    const Real height = door.size().y;
    int count = 2;
    for (const Real limit : h.heightLimits) {
        if (height > limit) {
            ++count;
        }
    }
    const Real first = door.min().y + h.endOffset;
    const Real last = door.max().y - h.endOffset;

    std::vector<Real> axes;
    for (int i = 0; i < count; ++i) {
        Real axis = first + (last - first) * i / (count - 1);
        // Mounting-plate holes at axis +- spacing/2 must land on the System 32 grid.
        const Real half = h.plateHoleSpacing / 2.0;
        const Real pitch = r.system32.pitch;
        if (pitch > 0.0) {
            const Real snapped =
                gridOrigin + half + std::round((axis - gridOrigin - half) / pitch) * pitch;
            const Real margin = h.cupDiameter / 2.0;
            if (snapped - margin >= door.min().y && snapped + margin <= door.max().y) {
                axis = snapped;
            }
        }
        axes.push_back(axis);
    }
    return axes;
}

/// Cups in the door, mounting plates on the wall behind its hinge edge.
void hangDoor(core::Door* door, const std::vector<PanelComponent*>& walls, Real gridOrigin,
              const ConstructionRules& r, DetailingReport& report)
{
    const auto& h = r.hinge;
    {
        const core::Aabb& d = door->box();
        const bool hingeLeft = door->hinge() == core::HingeSide::Left;
        const Real hingeEdge = hingeLeft ? d.min().x : d.max().x;

        // The wall the door is hung on: the one right behind its hinge edge.
        PanelComponent* wall = nullptr;
        for (PanelComponent* w : walls) {
            const core::Aabb& b = w->box();
            const bool behindEdge = hingeEdge >= b.min().x - kTol && hingeEdge <= b.max().x + kTol;
            const bool sameHeight = b.min().y < d.max().y && b.max().y > d.min().y;
            if (behindEdge && sameHeight) {
                wall = w;
            }
        }
        if (wall == nullptr) {
            report.warnings.push_back("No wall found behind the hinge edge of a door");
            return;
        }

        const PanelFace cupFace = PanelFrame(*door).faceAt(Axis::Z, false); // inner face
        const PanelFace plateFace = PanelFrame(*wall).faceToward(d.center());
        const Real cupX = hingeEdge + (hingeLeft ? h.cupEdgeDistance : -h.cupEdgeDistance);
        const Real plateZ = wall->box().max().z - r.system32.frontRowOffset;

        for (const Real axis : hingeAxes(d, gridOrigin, r)) {
            addHole(*door, cupFace, Vec3(cupX, axis, d.min().z), FastenerType::Hinge,
                    h.cupDiameter, h.cupDepth);
            for (const Real dy : {-h.plateHoleSpacing / 2.0, h.plateHoleSpacing / 2.0}) {
                addHole(*wall, plateFace, Vec3(0.0, axis + dy, plateZ), FastenerType::Hinge,
                        h.plateHoleDiameter, h.plateHoleDepth);
            }
        }
    }
}

void addHinges(core::Carcass& carcass, const std::vector<PanelComponent*>& walls, Real gridOrigin,
               const ConstructionRules& r, DetailingReport& report)
{
    for (core::Door* door : core::findAll<core::Door>(carcass)) {
        hangDoor(door, walls, gridOrigin, r, report);
    }
}

// ---------------------------------------------------------------------------------------------
// L corner cabinets
// ---------------------------------------------------------------------------------------------

/// Support rows along one axis under a shelf, as supportRows(): the front row 37 mm behind the
/// wall's front edge (still under the shelf), the rear one whole pitches further back and at
/// least rearRowMinOffset in front of the shelf's rear edge. `toRear`: +1 or -1, the direction
/// from the front towards the back on that axis.
std::vector<Real> rowsAlong(Real wallFront, Real shelfFront, Real shelfRear, Real toRear,
                            const ConstructionRules& r)
{
    const Real pitch = r.system32.pitch;
    Real front = wallFront + toRear * r.system32.frontRowOffset;
    while ((front - shelfFront) * toRear < r.shelfSupport.diameter
           && (shelfRear - (front + toRear * pitch)) * toRear > 0.0) {
        front += toRear * pitch; // shelf set far back: keep the pin under it
    }
    std::vector<Real> rows{front};
    const Real rearLimit = shelfRear - toRear * r.system32.rearRowMinOffset;
    const auto steps = static_cast<int>(std::floor((rearLimit - front) * toRear / pitch));
    if (steps >= 1) {
        rows.push_back(front + toRear * steps * pitch);
    }
    return rows;
}

/// The L shelf on supports rests on the wall at the end of each leg: two pins in each, under
/// the shelf's leg there.
void addCornerShelfSupports(core::CornerCabinet& corner, const ConstructionRules& r)
{
    const auto& sr = r.shelfSupport;
    const bool right = corner.end() == core::Side::Right;
    const core::Aabb& wall = corner.side().box();
    const core::Aabb& end = corner.endWall().box();
    const Real frontZ = wall.max().z;                        // the back leg's front edge
    const Real frontX = right ? end.min().x : end.max().x; // the side leg's front edge
    for (core::CornerShelf* shelf : corner.shelves()) {
        if (shelf->mount() != core::ShelfMount::Supports) {
            continue;
        }
        const core::Aabb& s = shelf->box();
        const Real holeY = s.min().y - sr.holeBelowShelf;
        // In the wall at the end of the back leg: rows along Z, the front towards +Z.
        const Real wallPlane = right ? wall.max().x : wall.min().x;
        const PanelFace wallFace = PanelFrame(corner.side()).faceAt(Axis::X, right);
        for (const Real z : rowsAlong(frontZ, frontZ - r.shelfFrontSetback, s.min().z, -1.0, r)) {
            addHole(corner.side(), wallFace, Vec3(wallPlane, holeY, z), FastenerType::ShelfSupport,
                    sr.diameter, sr.depth);
        }
        // In the end wall of the side leg: rows along X, the front towards the room.
        const Real toRear = right ? 1.0 : -1.0;
        const Real shelfRear = right ? s.max().x : s.min().x;
        const PanelFace endFace = PanelFrame(corner.endWall()).faceAt(Axis::Z, false);
        for (const Real x : rowsAlong(frontX, frontX + toRear * r.shelfFrontSetback, shelfRear, toRear, r)) {
            addHole(corner.endWall(), endFace, Vec3(x, holeY, end.min().z), FastenerType::ShelfSupport,
                    sr.diameter, sr.depth);
        }
    }
}

/// The folding door hangs on the first one: hinge cups at its folding edge, at the heights of
/// the first door's hinges. (The plates go on the first door's inner face, screwed on by the
/// fitter with the hinge's template.)
void addFoldingHinges(core::CornerCabinet& corner, Real gridOrigin, const ConstructionRules& r)
{
    const auto& h = r.hinge;
    core::FoldingDoor& door = corner.foldingDoor();
    const core::Aabb& d = door.box(); // its own coordinates: an ordinary door
    const bool foldLeft = door.hinge() == core::HingeSide::Left;
    const Real cupX = foldLeft ? d.min().x + h.cupEdgeDistance : d.max().x - h.cupEdgeDistance;
    const PanelFace cupFace = PanelFrame(door).faceAt(Axis::Z, false);
    for (const Real axis : hingeAxes(d, gridOrigin, r)) {
        addHole(door, cupFace, Vec3(cupX, axis, d.min().z), FastenerType::Hinge, h.cupDiameter,
                h.cupDepth);
    }
}

// ---------------------------------------------------------------------------------------------
// Handles
// ---------------------------------------------------------------------------------------------

void addHandles(core::Product& product, const ConstructionRules& r, DetailingReport& report)
{
    for (core::FrontPanel* front : core::findAll<core::FrontPanel>(product)) {
        if (!front->handle()) {
            continue;
        }
        const auto handle = front->handleGeometry(r);
        if (!handle) {
            const std::string& id = front->handle()->modelId;
            report.warnings.push_back("Unknown handle model '" + (id.empty() ? "<default>" : id)
                                      + "' on a " + std::string(toString(front->kind())));
            continue;
        }
        if (!handle->fits) {
            report.warnings.push_back("Handle '" + handle->model->name + "' does not fit on a "
                                      + std::string(toString(front->kind())));
            continue;
        }
        if (!r.handles.drillOnMachine) {
            continue; // drilled by the installer on site
        }
        // Through holes drilled from the visible face: the drill exits (and may chip) on the
        // hidden inner face.
        const PanelFace outer = PanelFrame(*front).faceAt(Axis::Z, true);
        for (const Vec3& hole : handle->holes) {
            addHole(*front, outer, hole, FastenerType::Handle, handle->model->holeDiameter,
                    front->panel()->thickness());
        }
    }
}

// ---------------------------------------------------------------------------------------------
// Back panel grooves
// ---------------------------------------------------------------------------------------------

void addBackGrooves(core::Carcass& carcass, const ConstructionRules& r)
{
    if (carcass.backMountType() != core::BackMountType::Groove) {
        return;
    }
    const core::Aabb& back = carcass.back().box();
    const Real grooveCentreZ =
        carcass.box().min().z + r.backGrooveOffset + r.backGrooveWidth / 2.0;

    for (PanelComponent* part : carcass.frameParts()) {
        const PanelFrame frame(*part);
        if (frame.widthAxis() != Axis::Z) {
            continue; // grooves run along the length, across the depth (not upright rails)
        }
        const core::Aabb& box = part->box();
        if (grooveCentreZ - r.backGrooveWidth / 2.0 < box.min().z - kTol
            || grooveCentreZ + r.backGrooveWidth / 2.0 > box.max().z + kTol) {
            continue; // the front rail: the groove is not in it
        }
        // Only where the back panel is (plus its expansion gap): an overlay top keeps clean ends.
        const int l = index(frame.lengthAxis());
        const Real from = std::max(part->box().min()[l], back.min()[l] - r.backExpansionGap);
        const Real to = std::min(part->box().max()[l], back.max()[l] + r.backExpansionGap);
        if (to <= from) {
            continue;
        }
        Vec3 start = part->box().min();
        start[l] = from;
        start.z = grooveCentreZ;
        Vec3 end = start;
        end[l] = to;

        const Vec3 a = frame.toLocal(start);
        const Vec3 b = frame.toLocal(end);
        part->panel()->addGroove({frame.faceToward(carcass.box().center()), core::Vec2(a.x, a.y),
                                  core::Vec2(b.x, b.y), r.backGrooveWidth, r.backGrooveDepth});
    }
}

// ---------------------------------------------------------------------------------------------
// Clean-up
// ---------------------------------------------------------------------------------------------

/// Merges identical holes (the same pin hole used for a shelf and a hinge plate) and makes
/// blind holes from opposite faces (shelf supports on both sides of a partition) leave
/// `minWallBetweenHoles` of material between them.
void resolveHoleConflicts(core::Panel& panel, const ConstructionRules& r)
{
    auto& holes = panel.drillings();
    const Real t = panel.thickness();

    for (std::size_t i = 0; i < holes.size(); ++i) {
        for (std::size_t j = holes.size() - 1; j > i; --j) {
            const bool same = holes[i].face == holes[j].face
                           && core::nearlyEqual(holes[i].position, holes[j].position, kTol)
                           && core::nearlyEqual(holes[i].diameter, holes[j].diameter, kTol);
            if (same) {
                holes[i].depth = std::max(holes[i].depth, holes[j].depth);
                holes.erase(holes.begin() + static_cast<std::ptrdiff_t>(j));
            }
        }
    }

    const Real maxBlind = (t - r.minWallBetweenHoles) / 2.0;
    for (std::size_t i = 0; i < holes.size(); ++i) {
        for (std::size_t j = i + 1; j < holes.size(); ++j) {
            core::Drilling& a = holes[i];
            core::Drilling& b = holes[j];
            const bool oppositeFaces = a.isFaceHole() && b.isFaceHole() && a.face != b.face;
            const bool blind = a.depth < t && b.depth < t;
            if (!oppositeFaces || !blind) {
                continue;
            }
            const Real distance = glm::length(core::Vec2(a.position.x - b.position.x,
                                                         a.position.y - b.position.y));
            const bool meet = distance < (a.diameter + b.diameter) / 2.0
                           && a.depth + b.depth > t - r.minWallBetweenHoles;
            if (meet) {
                a.depth = std::min(a.depth, maxBlind);
                b.depth = std::min(b.depth, maxBlind);
            }
        }
    }
}

} // namespace

DetailingReport applyDetailing(core::Product& product)
{
    DetailingReport report;
    const std::vector<PanelComponent*> parts = core::findAll<PanelComponent>(product);
    for (PanelComponent* part : parts) {
        part->panel()->clearMachining();
    }

    const ConstructionRules& r = product.rules();

    // Carcass by carcass: the cabinets of a kitchen touch each other, but they are separate
    // boxes — a joint, a shelf support or a hinge plate never crosses from one to the next.
    for (core::Carcass* carcass : core::findAll<core::Carcass>(product)) {
        std::vector<PanelComponent*> walls;
        for (PanelComponent* part : core::findAll<PanelComponent>(*carcass)) {
            if (isWall(*part)) {
                walls.push_back(part);
            }
        }
        const std::vector<Joint> joints = findJoints(*carcass);
        report.joints += static_cast<int>(joints.size());
        for (const Joint& joint : joints) {
            addJointFasteners(joint, r, report);
        }
        addShelfSupports(*carcass, walls, r, report);
        addHinges(*carcass, walls, carcass->system32OriginY(), r, report);
        addBackGrooves(*carcass, r);
    }
    // L corner cabinets: joints like a carcass (all their parts but the folding door share the
    // back run's coordinates), supports in both end walls, the first door on its wall, the
    // second on the first.
    for (core::CornerCabinet* corner : core::findAll<core::CornerCabinet>(product)) {
        const std::vector<Joint> joints = findJoints(*corner);
        report.joints += static_cast<int>(joints.size());
        for (const Joint& joint : joints) {
            addJointFasteners(joint, r, report);
        }
        addCornerShelfSupports(*corner, r);
        hangDoor(&corner->door(), {&corner->side()}, corner->system32OriginY(), r, report);
        addFoldingHinges(*corner, corner->system32OriginY(), r);
    }
    addHandles(product, r, report);

    for (PanelComponent* part : parts) {
        resolveHoleConflicts(*part->panel(), r);
        report.drillings += static_cast<int>(part->panel()->drillings().size());
        report.grooves += static_cast<int>(part->panel()->grooves().size());
    }
    return report;
}

} // namespace rc::detailing
