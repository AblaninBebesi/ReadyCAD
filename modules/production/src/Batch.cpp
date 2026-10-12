#include "readycad/production/Batch.hpp"

#include "readycad/core/builders/Catalog.hpp"
#include "readycad/editor/Document.hpp"
#include "readycad/export/Pdf.hpp"
#include "readycad/i18n/Labels.hpp"
#include "readycad/i18n/Localization.hpp"
#include "readycad/i18n/Utf8.hpp"
#include "readycad/production/Pipeline.hpp"
#include "readycad/production/Summary.hpp"
#include "readycad/storage/Storage.hpp"

#include <ostream>
#include <stdexcept>

namespace rc::production {

namespace {

editor::Document openDocument(const BatchOptions& options)
{
    if (options.project) {
        return editor::Document::open(*options.project);
    }
    const auto items = core::catalogItems();
    if (options.catalogItem < 0 || options.catalogItem >= static_cast<int>(items.size())) {
        throw std::invalid_argument("There is no catalogue model " + std::to_string(options.catalogItem));
    }
    const core::CatalogItem item = items[static_cast<std::size_t>(options.catalogItem)];
    editor::Document document = editor::Document::fromCatalog(item);
    // Without a project file there is no other name for the files than the model's.
    document.product().setName(std::string(i18n::text(i18n::catalogText(item))));
    return document;
}

} // namespace

int runBatch(const BatchOptions& options, std::ostream& out, std::ostream& err)
{
    try {
        if (options.package.empty()) {
            throw std::invalid_argument("No folder for the package");
        }
        const editor::Document document = openDocument(options);
        const Order order{document, options.quantity};
        if (order.quantity < 1) {
            throw std::invalid_argument("The quantity must be at least 1");
        }

        const storage::WorkshopFiles files = storage::WorkshopFiles::in(options.dataFolder);
        const nesting::StockManager stock = storage::loadStockFile(files.stock);
        storage::AppSettings settings = storage::loadSettingsFile(files.settings);
        if (options.machine) {
            settings.nesting.machine = *options.machine;
        }

        out << i18n::text(i18n::TextId::SummaryCutting) << ": "
            << i18n::text(i18n::TextId::Computing) << std::endl;
        const nesting::NestingResult plan = planCutting(order, stock, settings.nesting);
        const Package package = writePackage(order, &plan, settings.cnc, PackageOptions{},
                                             exporting::findPdfFonts(), options.package);
        out << "\n" << package.summaryText << "\n" << i18n::pathToUtf8(package.folder) << std::endl;
        if (!plan.unplaced.empty()) {
            err << "ReadyCAD: " << i18n::text(i18n::TextId::Unplaced) << std::endl;
        }
        for (const std::string& line : attentionLines(document)) {
            err << "ReadyCAD: " << line << std::endl;
        }
        return 0;
    } catch (const std::exception& e) {
        err << "ReadyCAD: " << e.what() << std::endl;
        return 1;
    }
}

} // namespace rc::production
