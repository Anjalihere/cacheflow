#pragma once

#include <algorithm>
#include <cstdint>
#include <deque>
#include <random>
#include <vector>

#include "cacheflow/order_book.hpp"

namespace cacheflow::benchmark {

enum class BenchmarkEventType {
    Add,
    Cancel,
};

struct BenchmarkEvent {
    BenchmarkEventType type{BenchmarkEventType::Add};
    Order order{};
    std::uint64_t cancel_order_id{0};
};

struct GeneratorConfig {
    std::uint64_t seed{42};
    std::uint64_t starting_order_id{1};
    std::uint64_t starting_timestamp{1};
    std::int64_t mid_price{100};
    std::int64_t price_span{8};
    std::uint32_t min_quantity{1};
    std::uint32_t max_quantity{10};
    double buy_probability{0.5};
    double cancel_probability{0.15};
};

class SyntheticOrderGenerator {
public:
    explicit SyntheticOrderGenerator(GeneratorConfig config)
        : config_(config),
          rng_(config.seed),
          side_distribution_(0.0, 1.0),
          price_offset_distribution_(-config.price_span, config.price_span),
          quantity_distribution_(config.min_quantity, config.max_quantity) {}

    BenchmarkEvent next_add_event() {
        const auto side_roll = side_distribution_(rng_);
        const auto side = side_roll < config_.buy_probability ? Side::Buy : Side::Sell;
        const auto offset = price_offset_distribution_(rng_);
        const auto quantity = quantity_distribution_(rng_);

        Order order{
            config_.starting_order_id++,
            side,
            config_.mid_price + offset,
            quantity,
            config_.starting_timestamp++,
        };

        active_order_ids_.push_back(order.id);
        return BenchmarkEvent{BenchmarkEventType::Add, order, 0};
    }

    BenchmarkEvent next_cancel_event() {
        if (active_order_ids_.empty()) {
            return next_add_event();
        }

        std::uniform_int_distribution<std::size_t> index_distribution(0, active_order_ids_.size() - 1);
        const auto index = index_distribution(rng_);
        const auto order_id = active_order_ids_[index];

        active_order_ids_.erase(active_order_ids_.begin() + static_cast<std::ptrdiff_t>(index));
        return BenchmarkEvent{BenchmarkEventType::Cancel, Order{}, order_id};
    }

    BenchmarkEvent next_event() {
        const auto roll = side_distribution_(rng_);
        if (roll < config_.cancel_probability) {
            return next_cancel_event();
        }

        return next_add_event();
    }

    void mark_cancelled(std::uint64_t order_id) {
        auto it = std::find(active_order_ids_.begin(), active_order_ids_.end(), order_id);
        if (it != active_order_ids_.end()) {
            active_order_ids_.erase(it);
        }
    }

    const std::vector<std::uint64_t>& active_order_ids() const noexcept {
        return active_order_ids_;
    }

private:
    GeneratorConfig config_;
    std::mt19937_64 rng_;
    std::uniform_real_distribution<double> side_distribution_;
    std::uniform_int_distribution<std::int64_t> price_offset_distribution_;
    std::uniform_int_distribution<std::uint32_t> quantity_distribution_;
    std::vector<std::uint64_t> active_order_ids_;
};

} // namespace cacheflow::benchmark