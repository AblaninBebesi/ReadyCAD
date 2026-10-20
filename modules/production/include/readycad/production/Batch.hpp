#pragma once

#include "readycad/nesting/Nesting.hpp"

#include <filesystem>
#include <iosfwd>
#include <optional>

/// The command line without a window: `ReadyCAD --catalog 1 --nesting router --package out`.
/// For a workshop PC that makes the files of a saved project without a click, and for the
/// end-to-end test that runs the real program.
namespace rc::production {

struct BatchOptions {
    std::optional<std::filesystem::path> project; ///< a project file; otherwise the catalogue model
    int catalogItem = 0;
    int quantity = 1;
    /// Machine of the cutting plan; nullopt: the one of the settings file.
    std::optional<nesting::Machine> machine;
    /// Whether the cutting plan may take remnants from stock; nullopt: as in the settings file.
    std::optional<bool> useRemnants;
    /// Folder of the stock and settings files; empty: the user's own (storage::userDataFolder).
    std::filesystem::path dataFolder;
    std::filesystem::path package; ///< where the package goes: must not exist or be empty
};

/// Opens the project, plans the cutting with the workshop's stock and settings, writes the
/// production package and prints its summary on `out`. The stock file is only read: taking the
/// sheets out of the stock stays a decision of the user in the window.
/// Returns the exit code of the program: 0 done, 1 failed (the reason on `err`).
int runBatch(const BatchOptions& options, std::ostream& out, std::ostream& err);

} // namespace rc::production
