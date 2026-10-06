#include "readycad/core/ConstructionRules.hpp"

namespace rc::core {

const ConstructionRules& ConstructionRules::defaults()
{
    static const ConstructionRules instance{};
    return instance;
}

} // namespace rc::core
