#include "readycad/cam/Route.hpp"

#include <glm/geometric.hpp>

#include <algorithm>
#include <limits>

namespace rc::cam {

using core::Real;
using core::Vec2;

Real routeLength(const std::vector<Vec2>& points, const std::vector<std::size_t>& order, Vec2 from)
{
    Real length = 0.0;
    Vec2 at = from;
    for (const std::size_t i : order) {
        length += glm::distance(at, points[i]);
        at = points[i];
    }
    return length;
}

std::vector<std::size_t> shortRoute(const std::vector<Vec2>& points, Vec2 from)
{
    const std::size_t n = points.size();
    std::vector<std::size_t> order;
    order.reserve(n);
    if (n == 0) {
        return order;
    }

    // Nearest neighbour: always the closest point not visited yet.
    std::vector<char> visited(n, 0);
    Vec2 at = from;
    for (std::size_t k = 0; k < n; ++k) {
        std::size_t best = 0;
        Real bestDistance = std::numeric_limits<Real>::max();
        for (std::size_t i = 0; i < n; ++i) {
            if (visited[i] == 0) {
                const Real d = glm::distance(at, points[i]);
                if (d < bestDistance) {
                    bestDistance = d;
                    best = i;
                }
            }
        }
        visited[best] = 1;
        order.push_back(best);
        at = points[best];
    }

    // 2-opt on the open route from `from`: reversing order[i..j] replaces the edges
    // (prev, i) and (j, next) by (prev, j) and (i, next). Keep doing it while it helps.
    const auto point = [&](std::size_t k) { return points[order[k]]; };
    constexpr Real kGain = 1e-9;
    constexpr int kMaxRounds = 50;
    for (int round = 0; round < kMaxRounds; ++round) {
        bool improved = false;
        for (std::size_t i = 0; i + 1 < n; ++i) {
            const Vec2 before = i == 0 ? from : point(i - 1);
            for (std::size_t j = i + 1; j < n; ++j) {
                const Real oldLength = glm::distance(before, point(i))
                                     + (j + 1 < n ? glm::distance(point(j), point(j + 1)) : 0.0);
                const Real newLength = glm::distance(before, point(j))
                                     + (j + 1 < n ? glm::distance(point(i), point(j + 1)) : 0.0);
                if (newLength + kGain < oldLength) {
                    std::reverse(order.begin() + static_cast<std::ptrdiff_t>(i),
                                 order.begin() + static_cast<std::ptrdiff_t>(j) + 1);
                    improved = true;
                }
            }
        }
        if (!improved) {
            break;
        }
    }
    return order;
}

} // namespace rc::cam
