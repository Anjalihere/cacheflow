#include "cacheflow/order_book.hpp"

#include <algorithm>
#include <sstream>
#include <utility>

namespace {

template <typename LevelMap>
bool is_crossed_buy(const LevelMap& asks, std::int64_t buy_price) {
    return !asks.empty() && asks.begin()->first <= buy_price;
}

template <typename LevelMap>
bool is_crossed_sell(const LevelMap& bids, std::int64_t sell_price) {
    return !bids.empty() && bids.begin()->first >= sell_price;
}

} // namespace

namespace cacheflow {

namespace {

template <typename Levels>
auto find_level(Levels& levels, std::int64_t price) {
    return levels.find(price);
}

} // namespace

void OrderBook::add_order(Order order) {
    if (order.side == Side::Buy) {
        while (order.quantity > 0 && is_crossed_buy(asks_, order.price)) {
            auto best_ask_it = asks_.begin();
            auto& resting_orders = best_ask_it->second.orders();

            while (order.quantity > 0 && !resting_orders.empty()) {
                auto resting_it = resting_orders.begin();
                const auto matched_quantity = static_cast<std::uint32_t>(
                    std::min<std::uint64_t>(order.quantity, resting_it->quantity));

                trades_.push_back(Trade{
                    order.id,
                    resting_it->id,
                    best_ask_it->first,
                    matched_quantity,
                    order.timestamp,
                });

                order.quantity -= matched_quantity;
                resting_it->quantity -= matched_quantity;

                if (resting_it->quantity == 0) {
                    order_index_.erase(resting_it->id);
                    resting_orders.erase(resting_it);
                }
            }

            if (resting_orders.empty()) {
                asks_.erase(best_ask_it);
            }
        }

        if (order.quantity > 0) {
            auto [level_it, inserted] = bids_.try_emplace(order.price, order.price);
            (void)inserted;
            auto iterator = level_it->second.add_order(std::move(order));
            order_index_[iterator->id] = OrderLocation{Side::Buy, level_it->first, iterator};
        }
        return;
    }

    while (order.quantity > 0 && is_crossed_sell(bids_, order.price)) {
        auto best_bid_it = bids_.begin();
        auto& resting_orders = best_bid_it->second.orders();

        while (order.quantity > 0 && !resting_orders.empty()) {
            auto resting_it = resting_orders.begin();
            const auto matched_quantity = static_cast<std::uint32_t>(
                std::min<std::uint64_t>(order.quantity, resting_it->quantity));

            trades_.push_back(Trade{
                resting_it->id,
                order.id,
                best_bid_it->first,
                matched_quantity,
                order.timestamp,
            });

            order.quantity -= matched_quantity;
            resting_it->quantity -= matched_quantity;

            if (resting_it->quantity == 0) {
                order_index_.erase(resting_it->id);
                resting_orders.erase(resting_it);
            }
        }

        if (resting_orders.empty()) {
            bids_.erase(best_bid_it);
        }
    }

    if (order.quantity > 0) {
        auto [level_it, inserted] = asks_.try_emplace(order.price, order.price);
        (void)inserted;
        auto iterator = level_it->second.add_order(std::move(order));
        order_index_[iterator->id] = OrderLocation{Side::Sell, level_it->first, iterator};
    }
}

bool OrderBook::cancel_order(std::uint64_t order_id) {
    auto index_it = order_index_.find(order_id);
    if (index_it == order_index_.end()) {
        return false;
    }

    const auto location = index_it->second;
    if (location.side == Side::Buy) {
        auto level_it = bids_.find(location.price);
        if (level_it == bids_.end()) {
            return false;
        }

        level_it->second.orders().erase(location.iterator);
        if (level_it->second.empty()) {
            bids_.erase(level_it);
        }
    } else {
        auto level_it = asks_.find(location.price);
        if (level_it == asks_.end()) {
            return false;
        }

        level_it->second.orders().erase(location.iterator);
        if (level_it->second.empty()) {
            asks_.erase(level_it);
        }
    }

    order_index_.erase(index_it);
    return true;
}

bool OrderBook::modify_order(std::uint64_t order_id, std::uint32_t new_quantity) {
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

std::string OrderBook::to_string() const {
    std::ostringstream out;

    out << "ASKS:\n";
    for (const auto& [price, level] : asks_) {
        out << "  " << price << " | ";
        bool first = true;
        for (const auto& order : level.orders()) {
            if (!first) {
                out << ", ";
            }
            out << "#" << order.id << " x" << order.quantity;
            first = false;
        }
        out << '\n';
    }

    out << "BIDS:\n";
    for (const auto& [price, level] : bids_) {
        out << "  " << price << " | ";
        bool first = true;
        for (const auto& order : level.orders()) {
            if (!first) {
                out << ", ";
            }
            out << "#" << order.id << " x" << order.quantity;
            first = false;
        }
        out << '\n';
    }

    return out.str();
}

} // namespace cacheflow