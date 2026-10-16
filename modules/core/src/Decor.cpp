#include "readycad/core/Decor.hpp"

namespace rc::core {

std::vector<Decor> defaultDecorCatalog()
{
    return {
        {"white", "Beyaz", "Plastic_White.jpg", 238, 234, 233, false},
        {"light-grey", "Açık gri", "", 196, 196, 192, false},
        {"anthracite", "Antrasit", "", 62, 63, 66, false},
        {"wood-beige", "Bej ahşap", "Wood_Beige.jpg", 167, 131, 100, true},
        {"wood-gold", "Altın ahşap", "Wood_Gold.jpg", 176, 115, 63, true},
        {"wood-honey", "Bal rengi ahşap", "Wood_Honey_Brown.jpg", 89, 52, 19, true},
        {"wood-dark", "Koyu kahve ahşap", "Wood_Dark_Brown.jpg", 65, 52, 46, true},
        {"marble-white", "Beyaz mermer", "Marble_White.jpg", 244, 242, 240, false},
        {"marble-grey", "Gri mermer", "Marble_Textured.jpg", 172, 173, 182, false},
        {"marble-black", "Siyah mermer", "Marble_Black.jpg", 61, 63, 62, false},
        {"chipboard", "Yonga levha", "Chipboard.jpg", 210, 167, 115, false},
    };
}

const Decor* findDecor(const std::vector<Decor>& catalog, const std::string& id) noexcept
{
    if (id.empty()) {
        return nullptr;
    }
    for (const Decor& decor : catalog) {
        if (decor.id == id) {
            return &decor;
        }
    }
    return nullptr;
}

Material withDecor(const Material& material, const Decor* decor, const std::vector<Decor>& catalog)
{
    Material result = material;
    // The material's own name: without the name of the decor it had.
    if (const Decor* previous = findDecor(catalog, material.decor)) {
        const std::string suffix = " " + previous->name;
        if (result.name.size() > suffix.size()
            && result.name.compare(result.name.size() - suffix.size(), suffix.size(), suffix) == 0) {
            result.name.erase(result.name.size() - suffix.size());
        }
    }
    if (decor == nullptr) {
        result.decor.clear();
        return result;
    }
    result.name += " " + decor->name;
    result.decor = decor->id;
    result.hasGrain = decor->grain;
    return result;
}

} // namespace rc::core
