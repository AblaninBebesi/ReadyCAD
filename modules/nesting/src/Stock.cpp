#include "readycad/nesting/Stock.hpp"

#include "readycad/nesting/Nesting.hpp"

#include <algorithm>
#include <stdexcept>
#include <unordered_set>

namespace rc::nesting {

bool sameMaterial(const core::Material& a, const core::Material& b) noexcept
{
    return a.name == b.name && core::nearlyEqual(a.thickness, b.thickness);
}

bool RemnantRules::isUsable(Real a, Real b) const noexcept
{
    return std::max(a, b) >= minLength - core::kEpsilon
        && std::min(a, b) >= minWidth - core::kEpsilon;
}

StockManager::StockManager(std::vector<Sheet> sheets) : sheets_(std::move(sheets))
{
    std::unordered_set<SheetId> ids;
    for (const Sheet& sheet : sheets_) {
        check(sheet);
        if (sheet.id == 0 || !ids.insert(sheet.id).second) {
            throw std::invalid_argument("stock sheet ids must be unique and non-zero");
        }
        nextId_ = std::max(nextId_, sheet.id + 1);
    }
}

void StockManager::check(const Sheet& sheet)
{
    if (sheet.length <= 0.0 || sheet.width <= 0.0 || sheet.material.thickness <= 0.0) {
        throw std::invalid_argument("sheet sizes and thickness must be positive");
    }
    if (sheet.quantity < 0) {
        throw std::invalid_argument("sheet quantity must not be negative");
    }
}

const Sheet* StockManager::find(SheetId id) const noexcept
{
    const auto it = std::find_if(sheets_.begin(), sheets_.end(),
                                 [id](const Sheet& s) { return s.id == id; });
    return it != sheets_.end() ? &*it : nullptr;
}

SheetId StockManager::add(Sheet sheet)
{
    check(sheet);
    for (Sheet& existing : sheets_) {
        if (sameMaterial(existing.material, sheet.material)
            && existing.material.hasGrain == sheet.material.hasGrain
            && core::nearlyEqual(existing.length, sheet.length)
            && core::nearlyEqual(existing.width, sheet.width)
            && existing.remnant == sheet.remnant && existing.note == sheet.note) {
            existing.quantity += sheet.quantity;
            return existing.id;
        }
    }
    sheet.id = nextId_++;
    sheets_.push_back(std::move(sheet));
    return sheets_.back().id;
}

void StockManager::update(const Sheet& sheet)
{
    check(sheet);
    const auto it = std::find_if(sheets_.begin(), sheets_.end(),
                                 [&](const Sheet& s) { return s.id == sheet.id; });
    if (it == sheets_.end()) {
        throw std::invalid_argument("no such sheet on stock");
    }
    *it = sheet;
}

void StockManager::remove(SheetId id) noexcept
{
    std::erase_if(sheets_, [id](const Sheet& s) { return s.id == id; });
}

std::vector<SheetOffer> StockManager::offers(const core::Material& material) const
{
    std::vector<SheetOffer> remnants;
    std::vector<SheetOffer> full;
    const Sheet* sizeToBuy = nullptr;
    for (const Sheet& sheet : sheets_) {
        if (!sameMaterial(sheet.material, material)) {
            continue;
        }
        if (!sheet.remnant && sizeToBuy == nullptr) {
            sizeToBuy = &sheet;
        }
        if (sheet.quantity > 0) {
            (sheet.remnant ? remnants : full)
                .push_back({sheet, sheet.remnant ? SheetSource::Remnant : SheetSource::Stock,
                            sheet.quantity});
        }
    }
    // Small remnants first: they are worth least as stock and fill up quickest.
    std::stable_sort(remnants.begin(), remnants.end(), [](const SheetOffer& a, const SheetOffer& b) {
        return a.sheet.area() < b.sheet.area();
    });

    std::vector<SheetOffer> out = std::move(remnants);
    out.insert(out.end(), full.begin(), full.end());
    Sheet buy;
    buy.material = material;
    buy.length = sizeToBuy != nullptr ? sizeToBuy->length : standardLength;
    buy.width = sizeToBuy != nullptr ? sizeToBuy->width : standardWidth;
    buy.quantity = 0;
    out.push_back({buy, SheetSource::Purchase, SheetOffer::kUnlimited});
    return out;
}

void StockManager::consume(const NestingResult& result, const std::string& note)
{
    StockManager next = *this; // all or nothing
    for (const SheetLayout& layout : result.layouts) {
        if (layout.source != SheetSource::Purchase) {
            const auto it = std::find_if(next.sheets_.begin(), next.sheets_.end(),
                                         [&](const Sheet& s) { return s.id == layout.sheet.id; });
            if (it == next.sheets_.end() || it->quantity < layout.count) {
                throw std::invalid_argument(
                    "the stock changed: the cutting plan needs sheets it no longer has");
            }
            it->quantity -= layout.count;
            if (it->remnant && it->quantity == 0) {
                next.sheets_.erase(it); // a full sheet entry stays: it tells the size to buy
            }
        }
        for (const Rect& r : layout.remnants) {
            Sheet remnant;
            remnant.material = layout.sheet.material;
            remnant.length = r.width(); // along X, the grain direction of the sheet
            remnant.width = r.height();
            remnant.quantity = layout.count;
            remnant.remnant = true;
            remnant.note = note;
            next.add(std::move(remnant));
        }
    }
    *this = std::move(next);
}

} // namespace rc::nesting
