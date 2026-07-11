#pragma once

#include <cstdint>
#include <functional>
#include <map>
#include <unordered_map>
#include <string>
#include <vector>

#include "cacheflow/order.hpp"
#include "cacheflow/price_level.hpp"
#include "cacheflow/trade.hpp"

namespace cacheflow {

class OrderBook {
public:
    using BidLevels = std::map<std::int64_t, PriceLevel, std::greater<std::int64_t>>;
    using AskLevels = std::map<std::int64_t, PriceLevel>;

    struct OrderLocation {
        Side side;
        std::int64_t price;
        PriceLevel::Iterator iterator;
    };

    void add_order(Order order);
    bool cancel_order(std::uint64_t order_id);
    bool modify_order(std::uint64_t order_id, std::uint32_t new_quantity);

    std::string to_string() const;

    const std::vector<Trade>& trades() const noexcept {
        return trades_;
    }

    const BidLevels& bids() const noexcept {
        return bids_;
    }

    const AskLevels& asks() const noexcept {
        return asks_;
    }

private:
    using Index = std::unordered_map<std::uint64_t, OrderLocation>;

    BidLevels bids_;
    AskLevels asks_;
    Index order_index_;
    std::vector<Trade> trades_;
};

} // namespace cacheflow