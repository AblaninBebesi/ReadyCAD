#include "readycad/production/Batch.hpp"
#include "readycad/ui/Application.hpp"

#ifdef _WIN32
#ifndef NOMINMAX
#define NOMINMAX
#endif
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#endif

#include <cstdio>
#include <cstdlib>
#include <exception>
#include <iostream>
#include <string>

namespace {

void printUsage()
{
    std::puts("ReadyCAD [options]\n"
              "  --open <file.rcproj>      open a project\n"
              "  --catalog <0..4>          start with a catalogue model (4: kitchen)\n"
              "  --holes                   show drilled holes in the 3D view\n"
              "  --no-fronts               hide doors and drawer fronts\n"
              "  --select <Kind>[:n]       select the first (n-th) part of a kind, e.g. Door:2\n"
              "  --pdf-dialog              open the PDF export settings\n"
              "  --nesting <saw|router>    compute the cutting plan for this machine\n"
              "  --count <n>               copies of the product in the cutting plan\n"
              "  --stock                   show the stock of sheets\n"
              "  --cut-list                show the cut list\n"
              "  --export-pdf <file.pdf>   write the PDF documents and exit\n"
              "  --cutting-settings        open the cutting settings\n"
              "  --data-dir <folder>       stock and settings files from this folder\n"
              "  --export-cnc <folder>     write the CNC programs (with --nesting router) and exit\n"
              "  --cnc-settings            open the CNC settings\n"
              "  --toolpath                show the toolpath in the cutting plan\n"
              "  --screenshot <file.bmp>   save the window as an image and exit\n"
              "  --package <folder>        no window: plan the cutting, write the production\n"
              "                            package into this (new or empty) folder and exit");
}

#ifdef _WIN32
/// The release build is a GUI program without a console of its own. In batch mode its output
/// goes to the console it was started from — unless it is redirected into a file or a pipe.
/// Turkish letters need the UTF-8 code page; the console's own one is restored at exit.
class BatchConsole {
public:
    BatchConsole()
    {
        const HANDLE out = GetStdHandle(STD_OUTPUT_HANDLE);
        if ((out == nullptr || out == INVALID_HANDLE_VALUE) && AttachConsole(ATTACH_PARENT_PROCESS)) {
            std::freopen("CONOUT$", "w", stdout);
            std::freopen("CONOUT$", "w", stderr);
        }
        codePage_ = GetConsoleOutputCP();
        SetConsoleOutputCP(CP_UTF8);
    }
    ~BatchConsole()
    {
        if (codePage_ != 0) {
            SetConsoleOutputCP(codePage_);
        }
    }
    BatchConsole(const BatchConsole&) = delete;
    BatchConsole& operator=(const BatchConsole&) = delete;

private:
    UINT codePage_ = 0;
};
#else
struct BatchConsole {};
#endif

} // namespace

int main(int argc, char** argv)
{
    rc::ui::ApplicationConfig config;
    std::string package;
    for (int i = 1; i < argc; ++i) {
        const std::string arg = argv[i];
        const bool hasValue = i + 1 < argc;
        if (arg == "--open" && hasValue) {
            config.openFile = argv[++i];
        } else if (arg == "--catalog" && hasValue) {
            config.catalogItem = std::atoi(argv[++i]);
        } else if (arg == "--holes") {
            config.showHoles = true;
        } else if (arg == "--no-fronts") {
            config.showFronts = false;
        } else if (arg == "--select" && hasValue) {
            config.selectKind = argv[++i];
        } else if (arg == "--pdf-dialog") {
            config.pdfDialog = true;
        } else if (arg == "--nesting" && hasValue) {
            const std::string machine = argv[++i];
            config.nesting = machine == "router" ? 1 : 0;
        } else if (arg == "--count" && hasValue) {
            config.productCount = std::atoi(argv[++i]);
        } else if (arg == "--stock") {
            config.stockWindow = true;
        } else if (arg == "--cut-list") {
            config.cutListWindow = true;
        } else if (arg == "--export-pdf" && hasValue) {
            config.exportPdf = argv[++i];
        } else if (arg == "--cutting-settings") {
            config.cuttingSettings = true;
        } else if (arg == "--data-dir" && hasValue) {
            config.dataFolder = argv[++i];
        } else if (arg == "--export-cnc" && hasValue) {
            config.exportCnc = argv[++i];
        } else if (arg == "--cnc-settings") {
            config.cncSettings = true;
        } else if (arg == "--toolpath") {
            config.toolpath = true;
        } else if (arg == "--screenshot" && hasValue) {
            config.screenshot = argv[++i];
        } else if (arg == "--package" && hasValue) {
            package = argv[++i];
        } else {
            printUsage();
            return arg == "--help" ? 0 : 2;
        }
    }

    if (!package.empty()) {
        // Batch mode: the same production pipeline as the window, without the window.
        const BatchConsole console;
        rc::production::BatchOptions batch;
        batch.project = config.openFile;
        batch.catalogItem = config.catalogItem;
        batch.quantity = config.productCount;
        if (config.nesting >= 0) {
            batch.machine = config.nesting == 0 ? rc::nesting::Machine::PanelSaw
                                                : rc::nesting::Machine::NestingCenter;
        }
        batch.dataFolder = config.dataFolder.value_or(std::filesystem::path{});
        batch.package = package;
        return rc::production::runBatch(batch, std::cout, std::cerr);
    }

    try {
        rc::ui::Application app(config);
        return app.run();
    } catch (const std::exception& e) {
        std::fprintf(stderr, "ReadyCAD: fatal error: %s\n", e.what());
        return 1;
    }
}
