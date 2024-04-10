#pragma once
#include "block/chain/fork_range.hpp"
#include "block/chain/worksum.hpp"
#include "block/header/batch.hpp"
#include "general/descriptor.hpp"
#include <chrono>

struct AppendMsg;
struct Descripted {
    // data
    const Descriptor descriptor;
    std::optional<std::chrono::steady_clock::time_point> deprecation_time;

    Descripted(Descriptor descriptor, Height chainLength, Worksum worksum, Grid grid);
    void deprecate();
    void append_throw(const AppendMsg& msg); //throws

    //getters
    [[nodiscard]] bool expired() const;
    [[nodiscard]] Height chain_length() const{return _chainLength;}
    [[nodiscard]] const Worksum& worksum() const {return _worksum;}
    [[nodiscard]] const Grid& grid() const {return _grid;}


    private:
    bool valid();
    Height _chainLength { 0u };
    Worksum _worksum;
    Grid _grid;
};

