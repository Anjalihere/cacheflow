# CacheFlow

A C++ order matching engine implementing price-time priority matching, O(1) order cancellation, and a benchmark-driven optimization pass from a balanced-tree order book to a direct-indexed, pool-allocated one.

[![CI](https://github.com/Anjalihere/cacheflow/actions/workflows/ci.yml/badge.svg)](https://github.com/Anjalihere/cacheflow/actions)
[![License: MIT](https://img.shields.io/badge/License-MIT-yellow.svg)](https://opensource.org/licenses/MIT)

---

## Table of Contents
- [Overview](#overview)
- [Architecture](#architecture)
- [Features](#features)
- [Getting Started](#getting-started)
- [Usage](#usage)
- [Benchmarks](#benchmarks)
- [Testing](#testing)
- [Project Structure](#project-structure)
- [Design Decisions](#design-decisions)
- [License](#license)

---

## Overview

CacheFlow is a from-scratch C++ implementation of the core of a limit order book and matching engine: the part of a trading system responsible for taking incoming buy/sell orders and matching them against resting orders by **price-time priority**.

This project was built to explore two things in depth:
1. **Data structure tradeoffs** under a matching workload (balanced trees vs. direct-indexed arrays, linked lists vs. contiguous storage, hash-indexed lookups for O(1) cancellation).
2. **Measured performance optimization** — every claim about speed in this README is backed by the benchmark harness in `benchmark/`, run before and after each optimization.


## Architecture

```
                     ┌─────────────────────┐
   Incoming Order ──▶│      OrderBook       │
                     │  ┌───────────────┐   │
                     │  │  Bids (desc)  │   │◀── Order index (unordered_map<id, location>)
                     │  ├───────────────┤   │        for O(1) cancel/modify
                     │  │  Asks (asc)   │   │
                     │  └───────────────┘   │
                     └─────────┬────────────┘
                               │ crosses spread?
                               ▼
                     ┌─────────────────────┐
                     │   Matching Engine    │──▶ Trade log (executed fills)
                     └─────────────────────┘
```

Each price level holds a FIFO queue of resting orders. Matching walks the opposite side's best price level(s) front-to-back until the incoming order is filled or no longer crosses.

## Features

- [x] Price-time priority matching with partial fill support
- [x] O(1) order cancellation via hash-indexed lookup
- [x] Order modification (reduce / cancel-replace)
- [x] Synthetic order-flow generator with configurable price distribution, side skew, and cancel rate
- [x] Benchmark harness reporting throughput and p50/p95/p99 latency
- [x] Optimized order-book storage (direct-indexed array vs. balanced tree) — see [Benchmarks](#benchmarks)
- [x] Pooled allocation for order objects
- [ ] Lock-free single-producer/single-consumer ingestion (stretch goal)

## Getting Started

### Prerequisites
- CMake ≥ 3.16
- A C++17 (or later) compiler
- GoogleTest (fetched automatically via CMake, or install locally)

### Build

```bash
git clone https://github.com/Anjalihere/cacheflow.git
cd cacheflow
mkdir build && cd build
cmake ..
cmake --build .
```

### Run tests

```bash
ctest --output-on-failure
```

### Run the benchmark suite

```bash
./build/cacheflow_bench --orders=20000 --warmup=2000 --csv=benchmark.csv
```

### Run the demo script

```bash
./demo.sh
```

The benchmark harness is synthetic by design. It reports throughput and p50/p95/p99 latency for add-only, cancel-only, and match-heavy workloads, and compares the baseline `std::map` book against a direct-indexed price-level book.

## Usage

CacheFlow ships with a small CLI for interactive use:

```bash
./build/cacheflow_cli
> add SELL 100 50
Order #1 added: SELL 100 @ 50
> add BUY 105 50
Trade executed: 50 @ 100 (buy #2, sell #1)
Order #2 added: BUY 105 @ 50
> book
ASKS:
BIDS:
> cancel 1
Order #1 not found
```

## Benchmarks

All numbers below are produced by `benchmark/benchmark_main.cpp` against a synthetic order stream (not real market data). Reproduce with `./build/cacheflow_bench --orders=10000 --warmup=1000 --csv=benchmark.csv`.

Representative add-only workload results:

| Version | Throughput (orders/sec) | p50 latency | p99 latency |
|---|---|---|---|
| Baseline (`std::map` + `std::list`) | `742879.96` | `1125 ns` | `3333 ns` |
| Direct-indexed price levels | `829654.58` | `1000 ns` | `2666 ns` |
| Pooled direct-indexed price levels | `1127072.84` | `750 ns` | `1792 ns` |

The benchmark binary also prints cancel-only and match-heavy results, and can write the full dataset to CSV.

## Testing

Correctness is validated with GoogleTest, covering:
- Full and partial matches
- FIFO ordering within a price level
- No-match cases (non-crossing orders)
- O(1) cancellation correctness across arbitrary book positions

Run with `ctest --output-on-failure` from the build directory.

## Project Structure

```
cacheflow/
├── CMakeLists.txt
├── include/cacheflow/        # public headers
├── src/                       # implementation
├── benchmark/                  # synthetic load generator + benchmark harness
├── cli/                        # demo CLI
└── tests/                      # GoogleTest suite
```

## Design Decisions

- **`std::list` for per-price-level FIFO queues:** erasing an element doesn't invalidate other iterators, which is what makes O(1) cancellation possible without rebuilding index pointers.
- **Integer tick prices, never floats:** avoids floating-point comparison/rounding issues when matching on exact price equality.
- **Direct-indexed price levels over a balanced tree:** order flow in practice clusters near the mid-price, so a bounded array indexed by tick offset gives O(1) level access instead of O(log n) — at the cost of needing a defined price range.
- **Pooled allocation for `Order` objects:** avoids per-order `malloc`/`free` overhead on the hot path.


## License

MIT — see `LICENSE`.
