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
