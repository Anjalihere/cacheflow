#include <algorithm>
#include <chrono>
#include <cstdint>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <optional>
#include <random>
#include <string>
#include <vector>

#include "benchmark/load_generator.hpp"
#include "cacheflow/direct_indexed_order_book.hpp"
#include "cacheflow/pooled_direct_indexed_order_book.hpp"
#include "cacheflow/order_book.hpp"

namespace cacheflow::benchmark {

struct BenchmarkConfig {
    std::size_t operations{100000};
    std::size_t warmup_operations{5000};
    std::uint64_t seed{42};
    std::string csv_path;
};

struct BenchmarkResult {
    std::string name;
    std::size_t operations{0};
    double seconds{0.0};
    double throughput{0.0};
    double p50_ns{0.0};
    double p95_ns{0.0};
    double p99_ns{0.0};
};

double percentile(std::vector<std::uint64_t> samples, double fraction) {
    if (samples.empty()) {
        return 0.0;
    }

    std::sort(samples.begin(), samples.end());
    const auto index = static_cast<std::size_t>(fraction * static_cast<double>(samples.size() - 1));
    return static_cast<double>(samples[index]);
}

template <typename Fn>
BenchmarkResult run_benchmark(const std::string& name, std::size_t operations, Fn&& fn) {
    std::vector<std::uint64_t> latencies_ns;
    latencies_ns.reserve(operations);

    const auto start = std::chrono::steady_clock::now();
    for (std::size_t i = 0; i < operations; ++i) {
        const auto before = std::chrono::steady_clock::now();
        fn();
        const auto after = std::chrono::steady_clock::now();
        latencies_ns.push_back(static_cast<std::uint64_t>(
            std::chrono::duration_cast<std::chrono::nanoseconds>(after - before).count()));
    }
    const auto end = std::chrono::steady_clock::now();

    BenchmarkResult result;
    result.name = name;
    result.operations = operations;
    result.seconds = std::chrono::duration<double>(end - start).count();
    result.throughput = operations / result.seconds;
    result.p50_ns = percentile(latencies_ns, 0.50);
    result.p95_ns = percentile(latencies_ns, 0.95);
    result.p99_ns = percentile(latencies_ns, 0.99);
    return result;
}

template <typename BookFactory>
BenchmarkResult benchmark_add_workload(const BenchmarkConfig& config, const std::string& name, BookFactory&& make_book) {
    auto book = make_book();
    SyntheticOrderGenerator generator(GeneratorConfig{
        .seed = config.seed,
        .starting_order_id = 1,
        .starting_timestamp = 1,
        .mid_price = 100,
        .price_span = 12,
        .min_quantity = 1,
        .max_quantity = 8,
        .buy_probability = 0.5,
        .cancel_probability = 0.0,
    });

    for (std::size_t i = 0; i < config.warmup_operations; ++i) {
        auto event = generator.next_add_event();
        book.add_order(std::move(event.order));
    }

    return run_benchmark(name, config.operations, [&] {
        auto event = generator.next_add_event();
        book.add_order(std::move(event.order));
    });
}

template <typename BookFactory>
BenchmarkResult benchmark_cancel_workload(const BenchmarkConfig& config, const std::string& name, BookFactory&& make_book) {
    auto book = make_book();
    std::vector<std::uint64_t> ids;
    ids.reserve(config.operations + config.warmup_operations);

    SyntheticOrderGenerator generator(GeneratorConfig{
        .seed = config.seed,
        .starting_order_id = 1,
        .starting_timestamp = 1,
        .mid_price = 100,
        .price_span = 4,
        .min_quantity = 1,
        .max_quantity = 4,
        .buy_probability = 0.5,
        .cancel_probability = 0.0,
    });

    for (std::size_t i = 0; i < config.operations + config.warmup_operations; ++i) {
        auto event = generator.next_add_event();
        ids.push_back(event.order.id);
        book.add_order(std::move(event.order));
    }

    std::mt19937_64 rng(config.seed + 1);
    std::shuffle(ids.begin(), ids.end(), rng);
    std::size_t index = 0;

    for (std::size_t i = 0; i < config.warmup_operations && index < ids.size(); ++i) {
        book.cancel_order(ids[index++]);
    }

    return run_benchmark(name, config.operations, [&] {
        book.cancel_order(ids[index++]);
    });
}

template <typename BookFactory>
BenchmarkResult benchmark_match_workload(const BenchmarkConfig& config, const std::string& name, BookFactory&& make_book) {
    auto book = make_book();
    SyntheticOrderGenerator generator(GeneratorConfig{
        .seed = config.seed,
        .starting_order_id = 1,
        .starting_timestamp = 1,
        .mid_price = 100,
        .price_span = 1,
        .min_quantity = 1,
        .max_quantity = 4,
        .buy_probability = 0.5,
        .cancel_probability = 0.0,
    });

    for (std::size_t i = 0; i < config.warmup_operations; ++i) {
        auto add = generator.next_add_event();
        add.order.side = (i % 2 == 0) ? Side::Sell : Side::Buy;
        add.order.price = (add.order.side == Side::Buy) ? 101 : 99;
        book.add_order(std::move(add.order));
    }

    return run_benchmark(name, config.operations, [&] {
        auto add = generator.next_add_event();
        add.order.side = (add.order.side == Side::Buy) ? Side::Sell : Side::Buy;
        add.order.price = (add.order.side == Side::Buy) ? 101 : 99;
        add.order.quantity = 1;
        book.add_order(std::move(add.order));
    });
}

std::vector<BenchmarkResult> run_baseline_suite(const BenchmarkConfig& config) {
    return {
        benchmark_add_workload(config, "baseline/add-only", [] { return cacheflow::OrderBook{}; }),
        benchmark_cancel_workload(config, "baseline/cancel-only", [] { return cacheflow::OrderBook{}; }),
        benchmark_match_workload(config, "baseline/match-heavy", [] { return cacheflow::OrderBook{}; }),
    };
}

std::vector<BenchmarkResult> run_direct_indexed_suite(const BenchmarkConfig& config) {
    return {
        benchmark_add_workload(config, "direct-indexed/add-only", [] { return cacheflow::DirectIndexedOrderBook{80, 120}; }),
        benchmark_cancel_workload(config, "direct-indexed/cancel-only", [] { return cacheflow::DirectIndexedOrderBook{80, 120}; }),
        benchmark_match_workload(config, "direct-indexed/match-heavy", [] { return cacheflow::DirectIndexedOrderBook{80, 120}; }),
    };
}

std::vector<BenchmarkResult> run_pooled_suite(const BenchmarkConfig& config) {
    return {
        benchmark_add_workload(config, "pooled-direct-indexed/add-only", [] { return cacheflow::PooledDirectIndexedOrderBook{80, 120}; }),
        benchmark_cancel_workload(config, "pooled-direct-indexed/cancel-only", [] { return cacheflow::PooledDirectIndexedOrderBook{80, 120}; }),
        benchmark_match_workload(config, "pooled-direct-indexed/match-heavy", [] { return cacheflow::PooledDirectIndexedOrderBook{80, 120}; }),
    };
}

void print_result(const BenchmarkResult& result) {
    std::cout << std::fixed << std::setprecision(2);
    std::cout << result.name << "\n";
    std::cout << "  operations: " << result.operations << "\n";
    std::cout << "  throughput:  " << result.throughput << " ops/sec\n";
    std::cout << "  p50:         " << result.p50_ns << " ns\n";
    std::cout << "  p95:         " << result.p95_ns << " ns\n";
    std::cout << "  p99:         " << result.p99_ns << " ns\n";
}

void write_csv(const std::string& path, const std::vector<BenchmarkResult>& results) {
    if (path.empty()) {
        return;
    }

    std::ofstream out(path);
    out << "name,operations,seconds,throughput,p50_ns,p95_ns,p99_ns\n";
    for (const auto& result : results) {
        out << result.name << ','
            << result.operations << ','
            << result.seconds << ','
            << result.throughput << ','
            << result.p50_ns << ','
            << result.p95_ns << ','
            << result.p99_ns << '\n';
    }
}

BenchmarkConfig parse_args(int argc, char** argv) {
    BenchmarkConfig config;

    for (int i = 1; i < argc; ++i) {
        const std::string argument = argv[i];
        const auto equals = argument.find('=');
        const auto key = argument.substr(0, equals);
        const auto value = equals == std::string::npos ? std::string{} : argument.substr(equals + 1);

        if (key == "--orders" && !value.empty()) {
            config.operations = static_cast<std::size_t>(std::stoull(value));
        } else if (key == "--warmup" && !value.empty()) {
            config.warmup_operations = static_cast<std::size_t>(std::stoull(value));
        } else if (key == "--seed" && !value.empty()) {
            config.seed = std::stoull(value);
        } else if (key == "--csv" && !value.empty()) {
            config.csv_path = value;
        }
    }

    return config;
}

} // namespace cacheflow::benchmark

int main(int argc, char** argv) {
    using namespace cacheflow::benchmark;

    const auto config = parse_args(argc, argv);

    std::vector<BenchmarkResult> results;
    const auto baseline = run_baseline_suite(config);
    const auto direct_indexed = run_direct_indexed_suite(config);
    const auto pooled = run_pooled_suite(config);
    results.insert(results.end(), baseline.begin(), baseline.end());
    results.insert(results.end(), direct_indexed.begin(), direct_indexed.end());
    results.insert(results.end(), pooled.begin(), pooled.end());

    for (const auto& result : results) {
        print_result(result);
    }

    write_csv(config.csv_path, results);
    return 0;
}