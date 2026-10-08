#pragma once

// A small sheet job written by hand, so that every expected number can be checked by eye:
// a 1000 x 600 x 18 sheet with
//   part 1 "Raf", 500 x 300 at (20, 20): two Ø5 holes 13 deep, a Ø7 through hole, a Ø35 cup,
//          a 4 x 8 groove, one hole in its left edge and one in its bottom face;
//   part 2 "Kapak", 200 x 150 at (540, 20): only its contour.

#include "readycad/cam/NestingCenter.hpp"

namespace rc::cam::sample {

inline SheetJob testJob()
{
    SheetJob job;
    job.name = "test";
    job.material = "Suntalam 18";
    job.length = 1000.0;
    job.width = 600.0;
    job.thickness = 18.0;
    job.partSpacing = 12.0;

    PartMachining a;
    a.label = "1";
    a.name = "Raf";
    a.thickness = 18.0;
    a.operations = {
        Drilling{Face::Top, {100.0, 100.0, 18.0}, 5.0, 13.0},
        Drilling{Face::Top, {200.0, 100.0, 18.0}, 5.0, 13.0},
        Drilling{Face::Top, {300.0, 200.0, 18.0}, 7.0, 18.0},
        Drilling{Face::Top, {400.0, 150.0, 18.0}, 35.0, 13.0},
        Groove{Face::Top, {20.0, 60.0}, {520.0, 60.0}, 4.0, 8.0},
        Drilling{Face::Left, {20.0, 170.0, 9.0}, 8.0, 30.0},
        Drilling{Face::Bottom, {450.0, 280.0, 0.0}, 5.0, 10.0},
        Contour{nesting::rectangle(nesting::Rect{{20.0, 20.0}, {520.0, 320.0}})},
    };
    PartMachining b;
    b.label = "2";
    b.name = "Kapak";
    b.thickness = 18.0;
    b.operations = {Contour{nesting::rectangle(nesting::Rect{{540.0, 20.0}, {740.0, 170.0}})}};
    job.parts = {a, b};
    return job;
}

/// Outlines of the parts of a job, in sheet coordinates.
inline std::vector<nesting::Polygon> outlines(const SheetJob& job)
{
    std::vector<nesting::Polygon> out;
    for (const PartMachining& p : job.parts) {
        for (const Operation& op : p.operations) {
            if (const auto* c = std::get_if<Contour>(&op)) {
                out.push_back(c->outline);
            }
        }
    }
    return out;
}

} // namespace rc::cam::sample
