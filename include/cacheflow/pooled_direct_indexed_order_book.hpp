#pragma once

#include <algorithm>
#include <cstdint>
#include <list>
#include <optional>
#include <unordered_map>
#include <utility>
#include <vector>

#include "cacheflow/order.hpp"
#include "cacheflow/pool_allocator.hpp"
#include "cacheflow/trade.hpp"

namespace cacheflow {

class PooledPriceLevel {
public:
    using OrderList = std::list<Order, PoolAllocator<Order>>;
    using Iterator = OrderList::iterator;

    PooledPriceLevel(std::int64_t price = 0, PoolArena* arena = nullptr)
        : price_(price), orders_(PoolAllocator<Order>(arena)) {}

    std::int64_t price() const noexcept {
        return price_;
    }

    Iterator add_order(Order order) {
        orders_.push_back(std::move(order));
        auto iterator = orders_.end();
        --iterator;
        return iterator;
    }

    bool empty() const noexcept {
        return orders_.empty();
    }

    OrderList& orders() noexcept {
        return orders_;
    }

private:
    std::int64_t price_;
    OrderList orders_;
};

class PooledDirectIndexedOrderBook {
public:
    struct OrderLocation {
        Side side;
        std::size_t offset;
        PooledPriceLevel::Iterator iterator;
    };

    PooledDirectIndexedOrderBook(std::int64_t min_price, std::int64_t max_price)
        : min_price_(min_price),
          max_price_(max_price),
          width_(static_cast<std::size_t>(max_price_ - min_price_ + 1)),
          asks_(width_),
          bids_(width_) {}

    void add_order(Order order) {
        if (order.side == Side::Buy) {
            match_buy(order);
        } else {
            match_sell(order);
        }
    }

    bool cancel_order(std::uint64_t order_id) {
        auto index_it = order_index_.find(order_id);
        if (index_it == order_index_.end()) {
            return false;
        }

        const auto location = index_it->second;
        auto& slots = location.side == Side::Buy ? bids_ : asks_;
        auto& slot = slots[location.offset];
        if (!slot.has_value()) {
            return false;
        }

        slot->orders().erase(location.iterator);
        if (slot->empty()) {
            slot.reset();
        }

        order_index_.erase(index_it);
        return true;
    }

    bool modify_order(std::uint64_t order_id, std::uint32_t new_quantity) {
        auto index_it = order_index_.find(order_id);
        if (index_it == order_index_.end()) {
            return false;
        }

        const auto location = index_it->second;
        Order modified_order = *location.iterator;
        modified_order.quantity = new_quantity;

        if (!cancel_order(order_id)) {
            return false;
        }

        add_order(std::move(modified_order));
        return true;
    }

    const std::vector<Trade>& trades() const noexcept {
        return trades_;
    }

private:
    using Slot = std::optional<PooledPriceLevel>;
    using SlotVector = std::vector<Slot>;

    std::optional<std::size_t> best_ask_offset() const {
        for (std::size_t offset = 0; offset < asks_.size(); ++offset) {
            if (asks_[offset].has_value()) {
                return offset;
            }
        }

        return std::nullopt;
    }

    std::optional<std::size_t> best_bid_offset() const {
        for (std::size_t offset = bids_.size(); offset > 0; --offset) {
            const auto candidate = offset - 1;
            if (bids_[candidate].has_value()) {
                return candidate;
            }
        }

        return std::nullopt;
    }

    std::size_t offset_for_price(std::int64_t price) const {
        return static_cast<std::size_t>(price - min_price_);
    }

    bool in_range(std::int64_t price) const {
        return price >= min_price_ && price <= max_price_;
    }

    void match_buy(Order& order) {
        while (order.quantity > 0) {
            const auto best_offset = best_ask_offset();
            if (!best_offset.has_value()) {
                break;
            }

            const auto best_price = min_price_ + static_cast<std::int64_t>(*best_offset);
            if (best_price > order.price) {
                break;
            }

            auto& slot = asks_[*best_offset];
            auto& resting_orders = slot->orders();

            while (order.quantity > 0 && !resting_orders.empty()) {
                auto resting_it = resting_orders.begin();
                const auto matched_quantity = static_cast<std::uint32_t>(
                    std::min<std::uint64_t>(order.quantity, resting_it->quantity));

                trades_.push_back(Trade{order.id, resting_it->id, best_price, matched_quantity, order.timestamp});

                order.quantity -= matched_quantity;
                resting_it->quantity -= matched_quantity;

                if (resting_it->quantity == 0) {
                    order_index_.erase(resting_it->id);
                    resting_orders.erase(resting_it);
                }
            }

            if (resting_orders.empty()) {
                slot.reset();
            }
        }

        if (order.quantity > 0 && in_range(order.price)) {
            const auto offset = offset_for_price(order.price);
            auto& slot = bids_[offset];
            if (!slot.has_value()) {
                slot.emplace(order.price, &pool_);
            }

            auto iterator = slot->add_order(std::move(order));
            order_index_[iterator->id] = OrderLocation{Side::Buy, offset, iterator};
        }
    }

    void match_sell(Order& order) {
        while (order.quantity > 0) {
            const auto best_offset = best_bid_offset();
            if (!best_offset.has_value()) {
                break;
            }

            const auto best_price = min_price_ + static_cast<std::int64_t>(*best_offset);
            if (best_price < order.price) {
                break;
            }

            auto& slot = bids_[*best_offset];
            auto& resting_orders = slot->orders();

            while (order.quantity > 0 && !resting_orders.empty()) {
                auto resting_it = resting_orders.begin();
                const auto matched_quantity = static_cast<std::uint32_t>(
                    std::min<std::uint64_t>(order.quantity, resting_it->quantity));

                trades_.push_back(Trade{resting_it->id, order.id, best_price, matched_quantity, order.timestamp});

                order.quantity -= matched_quantity;
                resting_it->quantity -= matched_quantity;

                if (resting_it->quantity == 0) {
                    order_index_.erase(resting_it->id);
                    resting_orders.erase(resting_it);
                }
            }

            if (resting_orders.empty()) {
                slot.reset();
            }
        }

        if (order.quantity > 0 && in_range(order.price)) {
            const auto offset = offset_for_price(order.price);
            auto& slot = asks_[offset];
            if (!slot.has_value()) {
                slot.emplace(order.price, &pool_);
            }

            auto iterator = slot->add_order(std::move(order));
            order_index_[iterator->id] = OrderLocation{Side::Sell, offset, iterator};
        }
    }

    std::int64_t min_price_{0};
    std::int64_t max_price_{0};
    std::size_t width_{0};
    PoolArena pool_{};
    SlotVector asks_;
    SlotVector bids_;
    std::unordered_map<std::uint64_t, OrderLocation> order_index_;
    std::vector<Trade> trades_;
};

} // namespace cacheflow