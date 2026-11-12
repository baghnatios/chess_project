#pragma once

#include "chess/position.hpp"

namespace chess::rules {

constexpr int index(Color c) { return static_cast<int>(c); }
constexpr int index(Piece p) { return static_cast<int>(p); }
constexpr Color opposite(Color c) { return c == Color::white ? Color::black : Color::white; }

constexpr unsigned kingside(Color side) { return 1u << (2 * index(side)); }
constexpr unsigned queenside(Color side) { return 2u << (2 * index(side)); }

} // namespace chess::rules
