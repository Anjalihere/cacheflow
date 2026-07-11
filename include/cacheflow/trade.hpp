#pragma once

#include <cstdint>

namespace cacheflow {

struct Trade {
    std::uint64_t buy_order_id{0};
    std::uint64_t sell_order_id{0};
    std::int64_t price{0};
    std::uint32_t quantity{0};
    std::uint64_t timestamp{0};
};

} // namespace cacheflow