#pragma once

#include <cstdint>
#include <list>
#include <utility>

#include "cacheflow/order.hpp"

namespace cacheflow {

class PriceLevel {
public:
    using OrderList = std::list<Order>;
    using Iterator = OrderList::iterator;

    explicit PriceLevel(std::int64_t price = 0) : price_(price) {}

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

    std::size_t size() const noexcept {
        return orders_.size();
    }

    const OrderList& orders() const noexcept {
        return orders_;
    }

    OrderList& orders() noexcept {
        return orders_;
    }

private:
    std::int64_t price_;
    OrderList orders_;
};

} // namespace cacheflow