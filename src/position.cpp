#include "chess/position.hpp"
#include "bitboards.hpp"
#include "rules_state.hpp"

#include <algorithm>
#include <limits>

namespace chess {
namespace {

using bits::Mask;
using bits::square;

using namespace rules;

unsigned rook_right(int at) {
    switch (at) {
        case 0: return queenside(Color::white);
        case 7: return kingside(Color::white);
        case 56: return queenside(Color::black);
        case 63: return kingside(Color::black);
        default: return 0;
    }
}

} // namespace

Position::Mask Position::pieces(Color side, Piece piece) const {
    return by_color_[index(side)] & by_piece_[index(piece)];
}

Position::Mask Position::occupied() const { return by_color_[0] | by_color_[1]; }

Piece Position::piece_at(int at) const {
    const Mask bit = square(at);
    if (!(occupied() & bit)) return Piece::none;
    for (int piece = 0; piece < index(Piece::none); ++piece)
        if (by_piece_[piece] & bit) return static_cast<Piece>(piece);
    return Piece::none;
}

void Position::add_piece(Color side, Piece piece, int at) {
    by_color_[index(side)] |= square(at);
    by_piece_[index(piece)] |= square(at);
}

void Position::remove_piece(Color side, Piece piece, int at) {
    by_color_[index(side)] &= ~square(at);
    by_piece_[index(piece)] &= ~square(at);
}

Position::Undo Position::prepare_move(Move move) const {
    const Piece moving = piece_at(move.from);
    int capture_square = move.to;
    if (moving == Piece::pawn && move.to == en_passant_)
        capture_square += turn_ == Color::white ? -8 : 8;
    return {move, moving, piece_at(capture_square), capture_square,
            castling_, en_passant_, halfmove_, fullmove_};
}

void Position::make(const Undo& state) {
    const Move move = state.move;
    if (state.captured != Piece::none) {
        remove_piece(opposite(turn_), state.captured, state.capture_square);
        if (state.captured == Piece::rook) castling_ &= ~rook_right(state.capture_square);
    }
    remove_piece(turn_, state.moving, move.from);
    add_piece(turn_, move.promotion == Piece::none ? state.moving : move.promotion, move.to);
    const int delta = move.to - move.from;
    if (state.moving == Piece::king) {
        castling_ &= ~(kingside(turn_) | queenside(turn_));
        if (delta == 2 || delta == -2) {
            remove_piece(turn_, Piece::rook, move.from + (delta > 0 ? 3 : -4));
            add_piece(turn_, Piece::rook, move.from + delta / 2);
        }
    } else if (state.moving == Piece::rook) {
        castling_ &= ~rook_right(move.from);
    }
    en_passant_ = state.moving == Piece::pawn && (delta == 16 || delta == -16)
                ? move.from + delta / 2 : -1;
    if (state.moving == Piece::pawn || state.captured != Piece::none) halfmove_ = 0;
    else if (halfmove_ < std::numeric_limits<std::uint64_t>::max()) ++halfmove_;
    if (turn_ == Color::black && fullmove_ < std::numeric_limits<std::uint64_t>::max()) ++fullmove_;
    turn_ = opposite(turn_);
}

void Position::unmake(const Undo& state) {
    turn_ = opposite(turn_);
    const Move move = state.move;
    remove_piece(turn_, move.promotion == Piece::none ? state.moving : move.promotion, move.to);
    add_piece(turn_, state.moving, move.from);
    const int delta = move.to - move.from;
    if (state.moving == Piece::king && (delta == 2 || delta == -2)) {
        remove_piece(turn_, Piece::rook, move.from + delta / 2);
        add_piece(turn_, Piece::rook, move.from + (delta > 0 ? 3 : -4));
    }
    if (state.captured != Piece::none)
        add_piece(opposite(turn_), state.captured, state.capture_square);
    castling_ = state.castling;
    en_passant_ = state.en_passant;
    halfmove_ = state.halfmove;
    fullmove_ = state.fullmove;
}

bool Position::play(Move move) {
    std::vector<Move> moves;
    legal_moves(moves);
    if (std::find(moves.begin(), moves.end(), move) == moves.end()) return false;
    history_.push_back(prepare_move(move));
    make(history_.back());
    return true;
}

bool Position::undo() {
    if (history_.empty()) return false;
    unmake(history_.back());
    history_.pop_back();
    return true;
}

std::uint64_t Position::perft(unsigned depth) const {
    if (depth == 0) return 1;
    Position work = *this;
    std::vector<std::vector<Move>> buffers(depth);
    return work.count_paths(depth, buffers);
}

std::uint64_t Position::count_paths(unsigned depth, std::vector<std::vector<Move>>& buffers) {
    auto& moves = buffers[depth - 1];
    legal_moves(moves);
    if (depth == 1) return moves.size();
    std::uint64_t nodes = 0;
    for (Move move : moves) {
        const auto state = prepare_move(move);
        make(state);
        nodes += count_paths(depth - 1, buffers);
        unmake(state);
    }
    return nodes;
}

} // namespace chess
