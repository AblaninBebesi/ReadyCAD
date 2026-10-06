#include "readycad/core/Version.hpp"

// Generated into the build tree by configure_file(); see modules/core/CMakeLists.txt.
#include "BuildInfo.hpp"

namespace rc::core {

Version version() noexcept
{
    return {build::kVersionMajor, build::kVersionMinor, build::kVersionPatch};
}

std::string_view versionString() noexcept
{
    return build::kVersionString;
}

} // namespace rc::core
