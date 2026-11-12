#pragma once

#include <array>
#include <bit>
#include <cstdint>

namespace chess::bits {

using Mask = std::uint64_t;
constexpr Mask square(int index) { return Mask{1} << index; }
constexpr Mask first_rank = 0xff;
constexpr Mask last_rank = first_rank << 56;

struct Offset { int file; int rank; };
constexpr std::array<Offset, 8> directions{{
    {1,0}, {0,1}, {1,1}, {-1,1}, {-1,0}, {0,-1}, {-1,-1}, {1,-1}
}};
constexpr std::array<Offset, 8> knight_steps{{
    {1,2}, {2,1}, {-1,2}, {-2,1}, {1,-2}, {2,-1}, {-1,-2}, {-2,-1}
}};

constexpr bool on_board(int file, int rank) {
    return file >= 0 && file < 8 && rank >= 0 && rank < 8;
}

constexpr auto step_table(const auto& offsets) {
    std::array<Mask, 64> table{};
    for (int from = 0; from < 64; ++from) {
        for (auto [df, dr] : offsets) {
            const int file = from % 8 + df, rank = from / 8 + dr;
            if (on_board(file, rank)) table[from] |= square(rank * 8 + file);
        }
    }
    return table;
}

constexpr auto ray_table() {
    std::array<std::array<Mask, 64>, 8> table{};
    for (int direction = 0; direction < 8; ++direction) {
        auto [df, dr] = directions[direction];
        for (int from = 0; from < 64; ++from) {
            for (int f = from % 8 + df, r = from / 8 + dr; on_board(f, r); f += df, r += dr)
                table[direction][from] |= square(r * 8 + f);
        }
    }
    return table;
}

inline constexpr auto knight = step_table(knight_steps);
inline constexpr auto king = step_table(directions);
inline constexpr auto pawns_white = step_table(std::array<Offset, 2>{{{-1,1}, {1,1}}});
inline constexpr auto pawns_black = step_table(std::array<Offset, 2>{{{-1,-1}, {1,-1}}});
inline constexpr auto rays = ray_table();

inline int pop_square(Mask& mask) {
    const int index = std::countr_zero(mask);
    mask &= mask - 1;
    return index;
}

// The first four directions have increasing square indices.
inline int nearest(Mask blockers, int direction) {
    return direction < 4 ? std::countr_zero(blockers) : 63 - std::countl_zero(blockers);
}

inline Mask ray_attack(int from, int direction, Mask occupied) {
    const Mask ray = rays[direction][from];
    const Mask blockers = ray & occupied;
    return blockers ? ray ^ rays[direction][nearest(blockers, direction)] : ray;
}

inline Mask sliding(int from, Mask occupied, bool diagonal) {
    const int first = diagonal ? 2 : 0;
    return ray_attack(from, first, occupied) | ray_attack(from, first + 1, occupied)
         | ray_attack(from, first + 4, occupied) | ray_attack(from, first + 5, occupied);
}

} // namespace chess::bits
