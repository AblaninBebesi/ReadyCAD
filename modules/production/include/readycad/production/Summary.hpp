#pragma once

#include "readycad/cam/NestingCenter.hpp"
#include "readycad/production/Pipeline.hpp"

#include <string>
#include <vector>

/// Texts about the results of the pipeline, for people: the package summary (OZET.txt), the
/// messages of the UI and of the command line. Turkish, like the whole interface.
namespace rc::production {

/// 252 s -> "4 dk 12 sn".
[[nodiscard]] std::string durationText(double seconds);

/// What the nesting centre leaves for other machines, one line per kind ("62 kenar deliği
/// (yatay delme makinesinde)"); empty if nothing.
[[nodiscard]] std::string leftoverText(const std::vector<cam::SheetProgram>& programs);

/// Machining time of all programs, each as often as its sheet is in the plan (stacks of
/// identical sheets run the same program again).
[[nodiscard]] double totalMachiningSeconds(const nesting::NestingResult& plan,
                                           const std::vector<cam::SheetProgram>& programs);

/// What the workshop must look at before cutting: parts in each other's space (from an older
/// project file), warnings of the drilling. One line each; empty if all is well.
[[nodiscard]] std::vector<std::string> attentionLines(const editor::Document& document);

/// The text of OZET.txt: the order, the parts, the cutting plan per material, the CNC programs
/// with their times, the work left for other machines, the parts not cut here, the files.
/// File names are relative to the package folder.
[[nodiscard]] std::string summaryText(const Order& order, const nesting::NestingResult* plan,
                                      const Package& package);

/// One short paragraph for a message box: "1 PDF, 14 DXF, 2 levha, 2 CNC programı" and the
/// warnings (parts that do not fit, work left for other machines).
[[nodiscard]] std::string packageMessage(const Order& order, const nesting::NestingResult* plan,
                                         const Package& package);

} // namespace rc::production
