#pragma once
#include <cstdint>
#include <map>
#include <unordered_map>
#include <list>
#include <iostream>
#include <stdexcept>
#include <limits>

struct Order {	
    uint64_t id;
    double price;
    uint64_t volume;
    bool is_buy;
    bool is_valid;
    std::list<Order*>::iterator iter;

    Order() = delete;
    Order(uint64_t id_, double price_, uint64_t volume_, bool is_buy_)
        : id(id_), price(price_), volume(volume_), is_buy(is_buy_), is_valid(false) {
        if (price <= 0.0) throw std::invalid_argument("invalid price");
        if (volume <= 0) throw std::invalid_argument("invalid volume");
    }
    void set_iterator(std::list<Order*>::iterator iter_){
        iter = iter_;
        is_valid = true;
    }
};

inline std::ostream& operator<<(std::ostream& os, const Order& o) {
    os << "Order{id=" << o.id
       << ", price=" << o.price
       << ", volume=" << o.volume
       << ", side=" << (o.is_buy ? "BUY" : "SELL")
       << ", valid=" << (o.is_valid ? "true" : "false")
       << "}";
    return os;
}


class OrderBook {
private:

    uint64_t id_generator;
    std::unordered_map<uint64_t, Order> masterbook;
    std::map<double, std::list<Order*>, std::less<double>> askbook;
    std::map<double, std::list<Order*>, std::greater<double>> bidbook;

    template<typename Book>
    uint64_t match(Book& book, const double& order_price, uint64_t order_volume, const bool is_buy_order) noexcept{
        
        if (book.empty()) return order_volume;

        int8_t multiplier = is_buy_order ? 1 : -1;

        while (order_volume > 0 && !book.empty()) {
            auto best_it = book.begin();
            double best_price = best_it->first;
            if (!(best_price * multiplier <= order_price * multiplier)) break;

            std::list<Order*>& lvl_list = best_it->second;

            while (order_volume > 0 && !lvl_list.empty()) {
                Order* matched_order = lvl_list.front();
                if (matched_order->volume <= order_volume) {
                    order_volume -= matched_order->volume;
                    bool need_new_price = lvl_list.size() == 1 ? true: false;
                    remove(matched_order->id);
                    if (need_new_price) break;
                } else {
                    matched_order->volume -= order_volume;
                    order_volume = 0;
                    return 0;
                }
            }
        }
        return order_volume;
    }

    template<typename Book>
    std::list<Order*>& get_price_linked_list(Book& book, double price) noexcept{
        auto res = book.emplace(price, std::list<Order*>{});
        return res.first->second;
    }

    template<typename Book>
    void remove_from_book(Book& book, Order& ord) noexcept {
        double price = ord.price;
        auto pit = book.find(price);
        if (pit == book.end()) return; // nothing to do

        std::list<Order*>& lvl_list = pit->second;

        ord.is_valid = false;
        lvl_list.erase(ord.iter);
        ord.iter = std::list<Order*>::iterator();

        if (lvl_list.empty()) {
            book.erase(pit);
        }
    }


    template<typename Book>
    uint64_t market_consume(Book& book, uint64_t volume_) {
        while (volume_ > 0 && !book.empty()) {
            std::list<Order*>& best_lvl = book.begin()->second;

            while (volume_ > 0) {
                Order* o = best_lvl.front();

                if (o->volume <= volume_) {
                    volume_ -= o->volume;
                    bool lvl_empty = best_lvl.size() ==1 ? true : false;
                    remove(o->id);
                    if(lvl_empty) break;
                } else {
                    o->volume -= volume_;
                    volume_ = 0;
                    return 0;
                }
            }
        }
        return volume_; 
    }

public:
    // prevent copying
    OrderBook(const OrderBook&) = delete;
    OrderBook& operator=(const OrderBook&) = delete;
    // allow moving
    OrderBook(OrderBook&&) = default;
    OrderBook& operator=(OrderBook&&) = default;

    OrderBook():id_generator(1){
        masterbook.reserve(10000);
    }


    uint64_t add(double price_, uint64_t volume_, bool is_buy_){
        if(price_<=0) return 0;
        //attempts matching and gets updated volume;
        volume_ = (is_buy_) ? match(askbook, price_, volume_, is_buy_) : match(bidbook, price_, volume_, is_buy_);
        if(volume_ == 0) return 0;

        //if not completely matched
        uint64_t id_ = id_generator++;
        auto [mb_it, inserted] = masterbook.emplace(std::piecewise_construct,
                                                   std::forward_as_tuple(id_),
                                                   std::forward_as_tuple(id_, price_, volume_, is_buy_));
        if(!inserted) return 0;
        std::list<Order*>& ll = is_buy_ ? get_price_linked_list(bidbook, price_) : get_price_linked_list(askbook, price_);
        std::list<Order*>::iterator iter = ll.insert(ll.end(), &(mb_it->second));
        mb_it->second.set_iterator(iter);

        return id_;
    }

    bool remove(uint64_t id) {
        auto it = masterbook.find(id);
        if (it == masterbook.end()) return false;
        Order& ord = it->second;
        if (!ord.is_valid) {
            // not in book (maybe already removed)
            masterbook.erase(it);
            return true;
        }
        // remove from correct book
        ord.is_buy ? remove_from_book(bidbook, ord) : remove_from_book(askbook, ord);
        //erase from masterbook
        masterbook.erase(it);
        return true;
    }

    bool modify_volume(uint64_t id, uint64_t new_volm) {
        auto it = masterbook.find(id);
        if (it == masterbook.end()) return false;
        if (new_volm == 0) {
            return remove(id);
        }
        it->second.volume = new_volm;
        return true;
    }

    // market order: consume best prices until volume is exhausted or book empty
    // returns : unfulfilled volume 
    uint64_t market_order(bool is_buy, uint64_t volume_) {
        if (volume_ == 0) return 0;
        return is_buy ? market_consume(askbook, volume_) : market_consume(bidbook, volume_);
    }

    // for debugging: print top N levels
    void print_books(size_t max_levels = 5) const {
        std::cout << "BIDS (top " << max_levels << "):\n";
        size_t cnt = 0;
        for (auto it = bidbook.begin(); it != bidbook.end() && cnt < max_levels; ++it, ++cnt) {
            std::cout << " price=" << it->first << "\n";
            uint64_t total = 0;
            for (auto p : it->second)std::cout <<"\t"<< *p << "\n";
        }
        std::cout << "ASKS (top " << max_levels << "):\n";
        cnt = 0;
        for (auto it = askbook.begin(); it != askbook.end() && cnt < max_levels; ++it, ++cnt) {
            std::cout << " price=" << it->first << "\n";
            uint64_t total = 0;
            for (auto p : it->second) std::cout <<"\t"<< *p << "\n";
        }
    }

        // extra helper for debug: print all resting orders with ids
    void print_all_orders() const {
        std::cout << "All resting orders (id: price vol side valid):\n";
        for (const auto& kv : masterbook) {
            std::cout << kv.second << "\n";
        }
    }
};
