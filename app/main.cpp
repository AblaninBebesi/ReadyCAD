#include "readycad/ui/Application.hpp"

#include <cstdio>
#include <cstdlib>
#include <exception>
#include <string>

namespace {

void printUsage()
{
    std::puts("ReadyCAD [options]\n"
              "  --open <file.rcproj>      open a project\n"
              "  --catalog <0..3>          start with a catalogue model\n"
              "  --holes                   show drilled holes in the 3D view\n"
              "  --no-fronts               hide doors and drawer fronts\n"
              "  --select <Kind>           select the first part of a kind, e.g. Shelf, Door\n"
              "  --pdf-dialog              open the PDF export settings\n"
              "  --nesting <saw|router>    compute the cutting plan for this machine\n"
              "  --stock                   show the stock of sheets\n"
              "  --export-pdf <file.pdf>   write the PDF documents and exit\n"
              "  --cutting-settings        open the cutting settings\n"
              "  --data-dir <folder>       stock and settings files from this folder\n"
              "  --export-cnc <folder>     write the CNC programs (with --nesting router) and exit\n"
              "  --cnc-settings            open the CNC settings\n"
              "  --toolpath                show the toolpath in the cutting plan\n"
              "  --screenshot <file.bmp>   save the window as an image and exit");
}

} // namespace

int main(int argc, char** argv)
{
    rc::ui::ApplicationConfig config;
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
        } else if (arg == "--stock") {
            config.stockWindow = true;
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
        } else {
            printUsage();
            return arg == "--help" ? 0 : 2;
        }
    }

    try {
        rc::ui::Application app(config);
        return app.run();
    } catch (const std::exception& e) {
        std::fprintf(stderr, "ReadyCAD: fatal error: %s\n", e.what());
        return 1;
    }
}
