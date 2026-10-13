#pragma once

#include "readycad/core/builders/Catalog.hpp"
#include "readycad/core/model/Product.hpp"
#include "readycad/core/model/Queries.hpp"
#include "readycad/detailing/CutList.hpp"
#include "readycad/detailing/Detailing.hpp"
#include "readycad/detailing/HardwareList.hpp"

#include <cstdint>
#include <filesystem>
#include <functional>
#include <memory>
#include <optional>
#include <stdexcept>
#include <string>
#include <string_view>
#include <vector>

namespace rc::editor {

/// An edit that would make the product impossible to build — parts in each other's space, a
/// part left without size — is refused with this error (a Turkish message for the user); the
/// document stays as it was.
class ConstructionError : public std::logic_error {
public:
    using std::logic_error::logic_error;
};

/// Position of a node in the tree: child indices from the root. Unlike ComponentId it stays
/// valid when the product is rebuilt from a file or an undo snapshot.
using TreePath = std::vector<std::size_t>;

[[nodiscard]] TreePath pathOf(const core::FurnitureComponent& node);
[[nodiscard]] core::FurnitureComponent* resolve(core::FurnitureComponent& root,
                                                const TreePath& path);

/// The open project: one product, its selection, undo history, file and derived data.
///
/// Every change goes through edit(), which
///   1. remembers the state before (Memento pattern: the project serialised to JSON),
///   2. applies the change — if it throws, the model is restored and the error propagates,
///   3. checks that the product can still be built: no new parts in each other's space, no
///      part without size, no new worktop cut-out that cannot be made — otherwise the change is
///      refused (ConstructionError) and undone,
///   4. recomputes the derived data: drilling (detailing), cut list, edge banding usage,
///      hardware list, overlapping parts.
/// So the UI never sees a half-applied change, stale drilling or a product that cannot be
/// built, and every change is undoable.
class Document {
public:
    explicit Document(std::shared_ptr<core::Product> product);

    [[nodiscard]] static Document fromCatalog(core::CatalogItem item,
                                              const core::ConstructionRules& rules = {});
    /// Throws storage::StorageError.
    [[nodiscard]] static Document open(const std::filesystem::path& path);

    [[nodiscard]] core::Product& product() noexcept { return *product_; }
    [[nodiscard]] const core::Product& product() const noexcept { return *product_; }

    // --- Derived data, always up to date ------------------------------------------------------
    [[nodiscard]] const detailing::DetailingReport& detailingReport() const noexcept
    {
        return report_;
    }
    [[nodiscard]] const std::vector<detailing::CutListRow>& cutList() const noexcept
    {
        return cutList_;
    }
    [[nodiscard]] const std::vector<detailing::EdgeBandUsage>& edgeUsage() const noexcept
    {
        return edgeUsage_;
    }
    [[nodiscard]] const std::vector<detailing::HardwareItem>& hardware() const noexcept
    {
        return hardware_;
    }
    /// Pairs of parts in each other's space. An edit never adds one; a project file of an older
    /// version may bring some, and the warnings show them.
    [[nodiscard]] const std::vector<core::Overlap>& overlaps() const noexcept { return overlaps_; }

    /// Base cabinets of a kitchen whose worktop cut-out cannot be made (Kitchen::misplacedCutouts).
    /// Like overlaps(): an edit never adds one, an older file may bring some.
    [[nodiscard]] const std::vector<const core::Carcass*>& misplacedCutouts() const noexcept
    {
        return misplacedCutouts_;
    }

    /// Panels closer than this (mm) do not count as overlapping: rounding of the formulas.
    static constexpr core::Real kOverlapTolerance = 0.01;
    /// Increases after every change (edit, undo, redo): views compare it to rebuild caches.
    [[nodiscard]] std::uint64_t revision() const noexcept { return revision_; }

    // --- Selection ----------------------------------------------------------------------------
    [[nodiscard]] core::FurnitureComponent* selected() const;
    void select(core::ComponentId id);
    void clearSelection() noexcept { selection_.reset(); }

    // --- Editing ------------------------------------------------------------------------------
    /// Applies `change` as one undoable step. Consecutive edits with the same non-empty
    /// `mergeKey` (e.g. dragging a slider) form ONE step until endMerge() is called.
    void edit(std::string_view description, const std::function<void(core::Product&)>& change,
              std::string_view mergeKey = {});
    void endMerge() noexcept { mergeOpen_ = false; }

    [[nodiscard]] bool canUndo() const noexcept { return !undo_.empty(); }
    [[nodiscard]] bool canRedo() const noexcept { return !redo_.empty(); }
    [[nodiscard]] std::string undoDescription() const;
    [[nodiscard]] std::string redoDescription() const;
    void undo();
    void redo();

    // --- File ---------------------------------------------------------------------------------
    [[nodiscard]] const std::filesystem::path& path() const noexcept { return path_; }
    [[nodiscard]] bool isDirty() const noexcept { return dirty_; }
    /// Throws storage::StorageError (also if the document has no path yet).
    void save();
    void saveAs(const std::filesystem::path& path);

    static constexpr std::size_t kMaxUndoSteps = 100;

private:
    struct Snapshot {
        std::string project; // saveProduct() JSON
        TreePath selection;  // empty = nothing selected
        bool hasSelection = false;
        std::string description;
        std::string mergeKey;
    };

    [[nodiscard]] Snapshot snapshot(std::string_view description,
                                    std::string_view mergeKey) const;
    void restore(const Snapshot& snapshot);
    void refresh();

    std::shared_ptr<core::Product> product_;
    std::optional<core::ComponentId> selection_;
    std::vector<Snapshot> undo_;
    std::vector<Snapshot> redo_;
    bool mergeOpen_ = false;
    bool dirty_ = false;
    std::filesystem::path path_;
    std::uint64_t revision_ = 0;

    detailing::DetailingReport report_;
    std::vector<detailing::CutListRow> cutList_;
    std::vector<core::Overlap> overlaps_;
    std::vector<const core::Carcass*> misplacedCutouts_;
    std::vector<detailing::EdgeBandUsage> edgeUsage_;
    std::vector<detailing::HardwareItem> hardware_;
};

} // namespace rc::editor
