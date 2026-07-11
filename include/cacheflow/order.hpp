#pragma once

#include <cstdint>
#include <string>

namespace cacheflow {

enum class Side {
    Buy,
    Sell,
};

inline std::string to_string(Side side) {
    switch (side) {
    case Side::Buy:
        return "BUY";
    case Side::Sell:
        return "SELL";
    }

    return "UNKNOWN";
}

struct Order {
    std::uint64_t id{0};
    Side side{Side::Buy};
    std::int64_t price{0};
    std::uint32_t quantity{0};
    std::uint64_t timestamp{0};
};

} // namespace cacheflow