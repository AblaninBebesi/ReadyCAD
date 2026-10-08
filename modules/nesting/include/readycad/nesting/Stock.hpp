#pragma once

#include "readycad/core/Material.hpp"
#include "readycad/nesting/Geometry.hpp"

#include <cstdint>
#include <string>
#include <vector>

namespace rc::nesting {

using SheetId = std::uint64_t;

/// A board on stock: a full sheet as bought, or a remnant (offcut) kept from an earlier job.
/// The length runs along X: the grain direction of decors with a texture.
struct Sheet {
    SheetId id = 0; ///< given by StockManager, stable in the stock file; 0 = not on stock
    core::Material material;
    Real length = 2800.0;
    Real width = 2100.0;
    int quantity = 1; ///< on stock; 0 = none left (the entry still tells the size to buy)
    bool remnant = false;
    std::string note; ///< remnants: where they come from

    [[nodiscard]] Real area() const noexcept { return length * width; }
};

/// Parts are cut from sheets of the same material: same name and thickness.
[[nodiscard]] bool sameMaterial(const core::Material& a, const core::Material& b) noexcept;

/// Which offcuts are worth keeping: smaller ones are waste.
struct RemnantRules {
    Real minLength = 300.0; ///< the longer side at least
    Real minWidth = 200.0;  ///< the shorter side at least

    [[nodiscard]] bool isUsable(Real a, Real b) const noexcept;
};

enum class SheetSource {
    Remnant,  ///< a remnant on stock
    Stock,    ///< a full sheet on stock
    Purchase, ///< not on stock: has to be bought
};

/// A kind of sheet the optimiser may cut from.
struct SheetOffer {
    Sheet sheet;
    SheetSource source = SheetSource::Stock;
    int available = 0; ///< how many sheets; kUnlimited for sheets to buy

    static constexpr int kUnlimited = -1;
};

struct NestingResult; // Nesting.hpp

/// Sheets and remnants the workshop has (chipboard, MDF, HDF...). The UI keeps it in a file
/// between sessions (storage::saveStock), so remnants of one job are used by the next ones.
class StockManager {
public:
    StockManager() = default;
    /// Restores a saved stock. Ids must be unique and non-zero; throws std::invalid_argument.
    explicit StockManager(std::vector<Sheet> sheets);

    /// Size of the sheets to buy for a material with no full sheet entry on stock.
    Real standardLength = 2800.0;
    Real standardWidth = 2100.0;
    RemnantRules remnantRules;

    [[nodiscard]] const std::vector<Sheet>& sheets() const noexcept { return sheets_; }
    [[nodiscard]] const Sheet* find(SheetId id) const noexcept;

    /// Adds sheets and returns the id of their entry. Sheets equal to an entry on stock
    /// (material, size, kind and note) only increase its quantity. Throws std::invalid_argument
    /// for sizes or a thickness <= 0, or a negative quantity.
    SheetId add(Sheet sheet);
    /// Replaces the entry with the same id. Throws std::invalid_argument if there is none or the
    /// values are invalid.
    void update(const Sheet& sheet);
    void remove(SheetId id) noexcept;

    /// What a job may cut one material from, in the order the optimiser tries it: remnants from
    /// the smallest, full sheets on stock, then unlimited sheets to buy — of the size of this
    /// material's first full sheet entry, or the standard size.
    [[nodiscard]] std::vector<SheetOffer> offers(const core::Material& material) const;

    /// After cutting: takes the sheets the result uses out of the stock and adds its remnants,
    /// noted with `note`. Throws std::invalid_argument (and changes nothing) if the result needs
    /// sheets the stock no longer has: it was changed after the optimisation.
    void consume(const NestingResult& result, const std::string& note);

private:
    static void check(const Sheet& sheet);

    std::vector<Sheet> sheets_;
    SheetId nextId_ = 1;
};

} // namespace rc::nesting
