#include "readycad/editor/Document.hpp"

#include "readycad/i18n/Labels.hpp"
#include "readycad/i18n/Localization.hpp"
#include "readycad/storage/Storage.hpp"

#include <algorithm>
#include <utility>

namespace rc::editor {

namespace {

std::string txt(i18n::TextId id)
{
    return std::string(i18n::text(id));
}

/// Throws ConstructionError if the change made a part lose its size or put parts into each
/// other's space (more overlapping pairs than `overlapsBefore`).
void requireBuildable(const core::Product& product, std::size_t overlapsBefore)
{
    for (const core::FurnitureComponent* c : core::collectPanels(product)) {
        const core::Panel& panel = *c->panel();
        if (!(panel.finishLength() > Document::kOverlapTolerance)
            || !(panel.finishWidth() > Document::kOverlapTolerance)) {
            throw ConstructionError(txt(i18n::TextId::ChangeRefused) + ": "
                                    + txt(i18n::TextId::PartWithoutSize) + " ("
                                    + i18n::displayName(*c) + ")");
        }
    }
    const auto overlaps = core::findOverlaps(product, Document::kOverlapTolerance);
    if (overlaps.size() > overlapsBefore) {
        const core::Overlap& o = overlaps.front();
        throw ConstructionError(txt(i18n::TextId::ChangeRefused) + ": "
                                + txt(i18n::TextId::PartsOverlap) + " ("
                                + i18n::displayName(*o.first) + " — "
                                + i18n::displayName(*o.second) + ")");
    }
}

} // namespace

TreePath pathOf(const core::FurnitureComponent& node)
{
    TreePath path;
    for (const core::FurnitureComponent* n = &node; n->parent() != nullptr; n = n->parent()) {
        const auto& siblings = n->parent()->children();
        const auto it = std::find_if(siblings.begin(), siblings.end(),
                                     [n](const auto& child) { return child.get() == n; });
        path.push_back(static_cast<std::size_t>(it - siblings.begin()));
    }
    std::reverse(path.begin(), path.end());
    return path;
}

core::FurnitureComponent* resolve(core::FurnitureComponent& root, const TreePath& path)
{
    core::FurnitureComponent* node = &root;
    for (const std::size_t index : path) {
        if (index >= node->children().size()) {
            return nullptr;
        }
        node = node->children()[index].get();
    }
    return node;
}

Document::Document(std::shared_ptr<core::Product> product) : product_(std::move(product))
{
    refresh();
}

Document Document::fromCatalog(core::CatalogItem item, const core::ConstructionRules& rules)
{
    return Document(core::createProduct(item, rules));
}

Document Document::open(const std::filesystem::path& path)
{
    Document document(storage::loadProduct(storage::readTextFile(path)));
    document.path_ = path;
    return document;
}

core::FurnitureComponent* Document::selected() const
{
    return selection_ ? product_->find(*selection_) : nullptr;
}

void Document::select(core::ComponentId id)
{
    if (product_->find(id) != nullptr) {
        selection_ = id;
    } else {
        selection_.reset();
    }
}

Document::Snapshot Document::snapshot(std::string_view description,
                                      std::string_view mergeKey) const
{
    Snapshot s;
    s.project = storage::saveProduct(*product_);
    if (const core::FurnitureComponent* node = selected()) {
        s.selection = pathOf(*node);
        s.hasSelection = true;
    }
    s.description = description;
    s.mergeKey = mergeKey;
    return s;
}

void Document::restore(const Snapshot& s)
{
    product_ = storage::loadProduct(s.project); // new objects -> new ids
    selection_.reset();
    if (s.hasSelection) {
        if (core::FurnitureComponent* node = resolve(*product_, s.selection)) {
            selection_ = node->id();
        }
    }
}

void Document::edit(std::string_view description,
                    const std::function<void(core::Product&)>& change, std::string_view mergeKey)
{
    const bool merging =
        mergeOpen_ && !mergeKey.empty() && !undo_.empty() && undo_.back().mergeKey == mergeKey;
    Snapshot before = snapshot(description, mergeKey);

    try {
        change(*product_);
        requireBuildable(*product_, overlaps_.size());
    } catch (...) {
        restore(before); // strong guarantee: a failed edit leaves no trace
        refresh();
        throw;
    }

    if (!merging) {
        undo_.push_back(std::move(before));
        if (undo_.size() > kMaxUndoSteps) {
            undo_.erase(undo_.begin());
        }
    }
    mergeOpen_ = !mergeKey.empty();
    redo_.clear();
    dirty_ = true;
    refresh();
}

std::string Document::undoDescription() const
{
    return undo_.empty() ? std::string{} : undo_.back().description;
}

std::string Document::redoDescription() const
{
    return redo_.empty() ? std::string{} : redo_.back().description;
}

void Document::undo()
{
    if (undo_.empty()) {
        return;
    }
    endMerge();
    redo_.push_back(snapshot(undo_.back().description, {}));
    restore(undo_.back());
    undo_.pop_back();
    dirty_ = true;
    refresh();
}

void Document::redo()
{
    if (redo_.empty()) {
        return;
    }
    endMerge();
    undo_.push_back(snapshot(redo_.back().description, {}));
    restore(redo_.back());
    redo_.pop_back();
    dirty_ = true;
    refresh();
}

void Document::save()
{
    if (path_.empty()) {
        throw storage::StorageError("The document has no file name yet");
    }
    storage::writeTextFile(path_, storage::saveProduct(*product_));
    dirty_ = false;
}

void Document::saveAs(const std::filesystem::path& path)
{
    storage::writeTextFile(path, storage::saveProduct(*product_));
    path_ = path;
    dirty_ = false;
}

void Document::refresh()
{
    report_ = detailing::applyDetailing(*product_);
    cutList_ = detailing::buildCutList(*product_, product_->rules());
    edgeUsage_ = detailing::edgeBandUsage(cutList_, product_->rules());
    hardware_ = detailing::hardwareList(*product_);
    overlaps_ = core::findOverlaps(*product_, kOverlapTolerance);
    if (selection_ && product_->find(*selection_) == nullptr) {
        selection_.reset();
    }
    ++revision_;
}

} // namespace rc::editor
