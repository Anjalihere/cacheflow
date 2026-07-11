#include <gtest/gtest.h>

#include <string>

#include "cacheflow/order_book.hpp"

using cacheflow::Order;
using cacheflow::OrderBook;
using cacheflow::Side;

TEST(OrderBookPhase0, SortsAsksAscendingAndBidsDescending) {
    OrderBook book;

    book.add_order(Order{1, Side::Sell, 101, 10, 1});
    book.add_order(Order{2, Side::Sell, 100, 5, 2});
    book.add_order(Order{3, Side::Buy, 99, 7, 3});
    book.add_order(Order{4, Side::Buy, 98, 4, 4});

    ASSERT_EQ(book.asks().size(), 2u);
    ASSERT_EQ(book.bids().size(), 2u);

    EXPECT_EQ(book.asks().begin()->first, 100);
    EXPECT_EQ(book.asks().rbegin()->first, 101);
    EXPECT_EQ(book.bids().begin()->first, 99);
    EXPECT_EQ(book.bids().rbegin()->first, 98);
}

TEST(OrderBookPhase0, PreservesFifoWithinPriceLevel) {
    OrderBook book;

    book.add_order(Order{10, Side::Sell, 100, 3, 1});
    book.add_order(Order{11, Side::Sell, 100, 4, 2});
    book.add_order(Order{12, Side::Sell, 100, 5, 3});

    const auto& level = book.asks().at(100);
    ASSERT_EQ(level.size(), 3u);

    auto it = level.orders().begin();
    EXPECT_EQ(it->id, 10u);
    ++it;
    EXPECT_EQ(it->id, 11u);
    ++it;
    EXPECT_EQ(it->id, 12u);
}

TEST(OrderBookPhase0, RendersDepthForVisualInspection) {
    OrderBook book;

    book.add_order(Order{1, Side::Sell, 100, 3, 1});
    book.add_order(Order{2, Side::Buy, 99, 4, 2});

    const std::string depth = book.to_string();
    EXPECT_NE(depth.find("ASKS:"), std::string::npos);
    EXPECT_NE(depth.find("BIDS:"), std::string::npos);
    EXPECT_NE(depth.find("#1 x3"), std::string::npos);
    EXPECT_NE(depth.find("#2 x4"), std::string::npos);
}

TEST(OrderBookPhase1, FullyMatchesCrossingBuyOrder) {
    OrderBook book;

    book.add_order(Order{1, Side::Sell, 100, 5, 1});
    book.add_order(Order{2, Side::Buy, 101, 5, 2});

    ASSERT_EQ(book.trades().size(), 1u);
    EXPECT_EQ(book.trades().front().buy_order_id, 2u);
    EXPECT_EQ(book.trades().front().sell_order_id, 1u);
    EXPECT_EQ(book.trades().front().price, 100);
    EXPECT_EQ(book.trades().front().quantity, 5u);
    EXPECT_TRUE(book.asks().empty());
    EXPECT_TRUE(book.bids().empty());
}

TEST(OrderBookPhase1, PartiallyFillsAndRestsIncomingOrder) {
    OrderBook book;

    book.add_order(Order{1, Side::Sell, 100, 4, 1});
    book.add_order(Order{2, Side::Buy, 101, 10, 2});

    ASSERT_EQ(book.trades().size(), 1u);
    EXPECT_EQ(book.trades().front().quantity, 4u);
    ASSERT_EQ(book.bids().size(), 1u);
    const auto& resting_buy = book.bids().at(101).orders().front();
    EXPECT_EQ(resting_buy.id, 2u);
    EXPECT_EQ(resting_buy.quantity, 6u);
    EXPECT_TRUE(book.asks().empty());
}

TEST(OrderBookPhase1, SweepsMultipleRestingOrdersAtSamePrice) {
    OrderBook book;

    book.add_order(Order{1, Side::Sell, 100, 3, 1});
    book.add_order(Order{2, Side::Sell, 100, 4, 2});
    book.add_order(Order{3, Side::Buy, 100, 6, 3});

    ASSERT_EQ(book.trades().size(), 2u);
    EXPECT_EQ(book.trades()[0].sell_order_id, 1u);
    EXPECT_EQ(book.trades()[0].quantity, 3u);
    EXPECT_EQ(book.trades()[1].sell_order_id, 2u);
    EXPECT_EQ(book.trades()[1].quantity, 3u);

    ASSERT_EQ(book.asks().size(), 1u);
    const auto& remaining_sell = book.asks().at(100).orders().front();
    EXPECT_EQ(remaining_sell.id, 2u);
    EXPECT_EQ(remaining_sell.quantity, 1u);
    EXPECT_TRUE(book.bids().empty());
}

TEST(OrderBookPhase1, LeavesNonCrossingOrderResting) {
    OrderBook book;

    book.add_order(Order{1, Side::Sell, 105, 5, 1});
    book.add_order(Order{2, Side::Buy, 100, 5, 2});

    EXPECT_TRUE(book.trades().empty());
    ASSERT_EQ(book.asks().size(), 1u);
    ASSERT_EQ(book.bids().size(), 1u);
    EXPECT_EQ(book.asks().begin()->first, 105);
    EXPECT_EQ(book.bids().begin()->first, 100);
}

TEST(OrderBookPhase1, RespectsFifoWhenMatchingSamePriceLevel) {
    OrderBook book;

    book.add_order(Order{1, Side::Sell, 100, 2, 1});
    book.add_order(Order{2, Side::Sell, 100, 2, 2});
    book.add_order(Order{3, Side::Buy, 100, 3, 3});

    ASSERT_EQ(book.trades().size(), 2u);
    EXPECT_EQ(book.trades()[0].sell_order_id, 1u);
    EXPECT_EQ(book.trades()[1].sell_order_id, 2u);
    EXPECT_EQ(book.trades()[0].quantity, 2u);
    EXPECT_EQ(book.trades()[1].quantity, 1u);
}

TEST(OrderBookPhase2, CancelsAnOrderByIdWithoutScanningTheBook) {
    OrderBook book;

    book.add_order(Order{1, Side::Sell, 101, 2, 1});
    book.add_order(Order{2, Side::Sell, 100, 3, 2});
    book.add_order(Order{3, Side::Buy, 99, 4, 3});

    ASSERT_TRUE(book.cancel_order(2));
    EXPECT_FALSE(book.cancel_order(2));
    EXPECT_TRUE(book.asks().count(100) == 0);
    ASSERT_EQ(book.asks().size(), 1u);
    EXPECT_EQ(book.asks().begin()->first, 101);
}

TEST(OrderBookPhase2, ModifyOrderReQueuesAtBackOfSamePriceLevel) {
    OrderBook book;

    book.add_order(Order{1, Side::Sell, 100, 2, 1});
    book.add_order(Order{2, Side::Sell, 100, 3, 2});

    ASSERT_TRUE(book.modify_order(1, 5));

    const auto& level = book.asks().at(100);
    ASSERT_EQ(level.size(), 2u);

    auto it = level.orders().begin();
    EXPECT_EQ(it->id, 2u);
    ++it;
    EXPECT_EQ(it->id, 1u);
    EXPECT_EQ(it->quantity, 5u);
}

TEST(OrderBookPhase2, ModifyOrderCanReEnterMatchingFlow) {
    OrderBook book;

    book.add_order(Order{1, Side::Sell, 105, 4, 1});
    ASSERT_TRUE(book.modify_order(1, 4));
    book.add_order(Order{2, Side::Buy, 106, 4, 2});

    ASSERT_EQ(book.trades().size(), 1u);
    EXPECT_EQ(book.trades().front().buy_order_id, 2u);
    EXPECT_EQ(book.trades().front().sell_order_id, 1u);
    EXPECT_TRUE(book.asks().empty());
    EXPECT_TRUE(book.bids().empty());
}