#include "chess/position.hpp"
#include "bitboards.hpp"
#include "rules_state.hpp"

namespace chess {
namespace {

using bits::Mask;
using bits::square;
using namespace rules;

constexpr std::array<Piece, 4> promotions{Piece::queen, Piece::rook, Piece::bishop, Piece::knight};
constexpr std::array<Piece, 4> other_pieces{Piece::knight, Piece::bishop, Piece::rook, Piece::queen};
const auto& pawn_attacks(Color side) {
    return side == Color::white ? bits::pawns_white : bits::pawns_black;
}

void append_moves(std::vector<Move>& moves, int from, Mask destinations) {
    while (destinations) moves.push_back({from, bits::pop_square(destinations)});
}

void append_pawn_moves(std::vector<Move>& moves, int from, Mask destinations, Mask back_rank) {
    while (destinations) {
        const int to = bits::pop_square(destinations);
        if (square(to) & back_rank) {
            for (Piece piece : promotions) moves.push_back({from, to, piece});
        } else {
            moves.push_back({from, to});
        }
    }
}

} // namespace

Position::Mask Position::leaper_attackers(int at, Color side, Mask available) const {
    return available & ((pawn_attacks(opposite(side))[at] & by_piece_[index(Piece::pawn)])
                     | (bits::knight[at] & by_piece_[index(Piece::knight)])
                     | (bits::king[at] & by_piece_[index(Piece::king)]));
}

bool Position::attacked(int at, Color side, Mask occupancy, Mask removed) const {
    const Mask available = by_color_[index(side)] & ~removed;
    if (leaper_attackers(at, side, available)) return true;
    const Mask queens = by_piece_[index(Piece::queen)];
    const Mask diagonal = available & (by_piece_[index(Piece::bishop)] | queens);
    if (diagonal && (diagonal & bits::sliding(at, occupancy, true))) return true;
    const Mask straight = available & (by_piece_[index(Piece::rook)] | queens);
    return straight && (straight & bits::sliding(at, occupancy, false));
}

bool Position::in_check() const {
    return attacked(std::countr_zero(pieces(turn_, Piece::king)), opposite(turn_), occupied());
}

void Position::legal_moves(std::vector<Move>& moves) const {
    moves.clear();
    const Color us = turn_, them = opposite(us);
    const Mask friendly = by_color_[index(us)], enemy = by_color_[index(them)];
    const Mask occupancy = friendly | enemy;
    const Mask capturable = enemy & ~pieces(them, Piece::king);
    const Mask targets = ~occupancy | capturable;
    const int king = std::countr_zero(pieces(us, Piece::king));
    const Mask without_king = occupancy & ~square(king);
    Mask checkers = leaper_attackers(king, them, enemy);
    Mask evasion = checkers;
    Mask pinned = 0;
    std::array<Mask, 64> pin_line;
    const Mask enemy_queens = pieces(them, Piece::queen);
    const Mask enemy_bishops = pieces(them, Piece::bishop) | enemy_queens;
    const Mask enemy_rooks = pieces(them, Piece::rook) | enemy_queens;

    // A single pass from the king finds sliding checks and absolute pins.
    for (int dir = 0; dir < 8; ++dir) {
        const Mask ray = bits::rays[dir][king];
        const Mask sliders = ray & ((dir % 4 >= 2) ? enemy_bishops : enemy_rooks);
        if (!sliders) continue;
        const int first = bits::nearest(ray & occupancy, dir);
        const Mask first_bit = square(first);
        if (first_bit & sliders) {
            checkers |= first_bit;
            evasion |= ray ^ bits::rays[dir][first];
        } else if (first_bit & friendly) {
            const Mask beyond = bits::rays[dir][first] & occupancy;
            if (!beyond) continue;
            const int second = bits::nearest(beyond, dir);
            if (!(square(second) & sliders)) continue;
            pinned |= first_bit;
            pin_line[first] = ray ^ bits::rays[dir][second];
        }
    }

    Mask king_targets = bits::king[king] & targets;
    while (king_targets) {
        const int to = bits::pop_square(king_targets);
        const Mask destination = square(to);
        if (!attacked(to, them, without_king | destination, destination))
            moves.push_back({king, to});
    }

    if (!checkers && (castling_ & (kingside(us) | queenside(us)))) {
        for (int step : {1, -1}) {
            const unsigned right = step == 1 ? kingside(us) : queenside(us);
            if (!(castling_ & right)) continue;
            const int transit = king + step, to = transit + step;
            const int rook = king + (step == 1 ? 3 : -4);
            const Mask between = square(transit) | square(to) | (step == -1 ? square(to - 1) : 0);
            if (occupancy & between) continue;
            if (attacked(transit, them, without_king | square(transit))) continue;
            const Mask final_occupancy = (without_king & ~square(rook)) | square(transit) | square(to);
            if (!attacked(to, them, final_occupancy)) moves.push_back({king, to});
        }
    }

    // En passant changes three squares, so check the resulting occupancy directly.
    const int forward = us == Color::white ? 8 : -8;
    const Mask pawns = pieces(us, Piece::pawn);
    if (en_passant_ >= 0) {
        Mask sources = pawns & pawn_attacks(them)[en_passant_];
        const Mask captured = square(en_passant_ - forward);
        while (sources) {
            const int from = bits::pop_square(sources);
            const Mask after = (occupancy & ~(square(from) | captured)) | square(en_passant_);
            if (!attacked(king, them, after, captured)) moves.push_back({from, en_passant_});
        }
    }

    if (checkers & (checkers - 1)) return;
    if (!checkers) evasion = ~Mask{0};

    const auto allowed = [&](int from) {
        return evasion & ((square(from) & pinned) ? pin_line[from] : ~Mask{0});
    };
    const Mask back_rank = us == Color::white ? bits::last_rank : bits::first_rank;
    const int home_rank = us == Color::white ? 1 : 6;
    Mask remaining_pawns = pawns;
    while (remaining_pawns) {
        const int from = bits::pop_square(remaining_pawns);
        const int one = from + forward; // FEN and promotion keep pawns off the back ranks.
        Mask destinations = pawn_attacks(us)[from] & capturable;
        if (!(occupancy & square(one))) {
            destinations |= square(one);
            if (from / 8 == home_rank) destinations |= square(one + forward) & ~occupancy;
        }
        append_pawn_moves(moves, from, destinations & allowed(from), back_rank);
    }

    for (Piece piece : other_pieces) {
        Mask remaining = pieces(us, piece);
        if (piece == Piece::knight) remaining &= ~pinned;
        while (remaining) {
            const int from = bits::pop_square(remaining);
            Mask destinations = 0;
            if (piece == Piece::knight) destinations = bits::knight[from];
            if (piece == Piece::bishop || piece == Piece::queen)
                destinations |= bits::sliding(from, occupancy, true);
            if (piece == Piece::rook || piece == Piece::queen)
                destinations |= bits::sliding(from, occupancy, false);
            append_moves(moves, from, destinations & targets & allowed(from));
        }
    }
}

} // namespace chess
