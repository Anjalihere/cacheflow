#include <cstdint>
#include <cstdlib>
#include <iostream>
#include <sstream>
#include <string>

#include "cacheflow/order_book.hpp"

namespace {

void print_help() {
    std::cout << "Commands:\n"
              << "  add BUY|SELL <price> <quantity>\n"
              << "  cancel <order_id>\n"
              << "  book\n"
              << "  trades\n"
              << "  help\n"
              << "  quit\n";
}

void print_new_trades(const cacheflow::OrderBook& book, std::size_t from_index) {
    const auto& trades = book.trades();
    for (std::size_t index = from_index; index < trades.size(); ++index) {
        const auto& trade = trades[index];
        std::cout << "Trade executed: " << trade.quantity << " @ " << trade.price
                  << " (buy #" << trade.buy_order_id << ", sell #" << trade.sell_order_id << ")\n";
    }
}

void print_all_trades(const cacheflow::OrderBook& book) {
    const auto& trades = book.trades();
    if (trades.empty()) {
        std::cout << "No trades\n";
        return;
    }

    for (const auto& trade : trades) {
        std::cout << "Trade executed: " << trade.quantity << " @ " << trade.price
                  << " (buy #" << trade.buy_order_id << ", sell #" << trade.sell_order_id << ")\n";
    }
}

bool parse_side(const std::string& value, cacheflow::Side& side) {
    if (value == "BUY") {
        side = cacheflow::Side::Buy;
        return true;
    }

    if (value == "SELL") {
        side = cacheflow::Side::Sell;
        return true;
    }

    return false;
}

} // namespace

int main() {
    cacheflow::OrderBook book;
    std::uint64_t next_order_id = 1;
    std::uint64_t next_timestamp = 1;

    std::cout << "CacheFlow CLI\n";
    print_help();

    std::string line;
    while (std::cout << "> " && std::getline(std::cin, line)) {
        std::istringstream input(line);
        std::string command;
        input >> command;

        if (command.empty()) {
            continue;
        }

        if (command == "quit" || command == "exit") {
            break;
        }

        if (command == "help") {
            print_help();
            continue;
        }

        if (command == "book") {
            std::cout << book.to_string();
            continue;
        }

        if (command == "trades") {
            print_all_trades(book);
            continue;
        }

        if (command == "cancel") {
            std::uint64_t order_id{0};
            if (!(input >> order_id)) {
                std::cout << "Usage: cancel <order_id>\n";
                continue;
            }

            if (book.cancel_order(order_id)) {
                std::cout << "Order #" << order_id << " cancelled\n";
            } else {
                std::cout << "Order #" << order_id << " not found\n";
            }
            continue;
        }

        if (command == "add") {
            std::string side_text;
            std::int64_t price{0};
            std::uint32_t quantity{0};
            if (!(input >> side_text >> price >> quantity)) {
                std::cout << "Usage: add BUY|SELL <price> <quantity>\n";
                continue;
            }

            cacheflow::Side side;
            if (!parse_side(side_text, side)) {
                std::cout << "Usage: add BUY|SELL <price> <quantity>\n";
                continue;
            }

            const auto before_trades = book.trades().size();
            book.add_order(cacheflow::Order{next_order_id, side, price, quantity, next_timestamp});
            print_new_trades(book, before_trades);
            std::cout << "Order #" << next_order_id << " added: " << side_text
                      << ' ' << price << " @ " << quantity << "\n";

            ++next_order_id;
            ++next_timestamp;
            continue;
        }

        std::cout << "Unknown command: " << command << "\n";
    }

    return 0;
}