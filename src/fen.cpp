#include "chess/position.hpp"
#include "bitboards.hpp"

#include <bit>
#include <charconv>
#include <sstream>

namespace chess {
namespace {

constexpr std::string_view symbols = "PNBRQKpnbrqk";
constexpr std::string_view rights = "KQkq";

int parse_square(std::string_view value) {
    if (value.size() != 2 || value[0] < 'a' || value[0] > 'h'
        || value[1] < '1' || value[1] > '8') return -1;
    return (value[1] - '1') * 8 + (value[0] - 'a');
}

std::string format_square(int square) {
    return {static_cast<char>('a' + square % 8), static_cast<char>('1' + square / 8)};
}

bool parse_counter(const std::string& text, std::uint64_t& value) {
    const auto [end, error] = std::from_chars(text.data(), text.data() + text.size(), value);
    return error == std::errc{} && end == text.data() + text.size();
}

} // namespace

std::optional<Move> parse_move(std::string_view text) {
    if (text.size() != 4 && text.size() != 5) return std::nullopt;
    const int from = parse_square(text.substr(0, 2));
    const int to = parse_square(text.substr(2, 2));
    if (from < 0 || to < 0 || from == to) return std::nullopt;
    Move move{from, to};
    if (text.size() == 5) {
        const auto piece = symbols.find(text[4]);
        if (piece < 7 || piece > 10) return std::nullopt;
        move.promotion = static_cast<Piece>(piece - 6);
    }
    return move;
}

std::string format_move(Move move) {
    if (move.from < 0 || move.from >= 64 || move.to < 0 || move.to >= 64 || move.from == move.to)
        return {};
    auto text = format_square(move.from) + format_square(move.to);
    if (move.promotion != Piece::none) {
        const int piece = static_cast<int>(move.promotion);
        if (piece < 1 || piece > 4) return {};
        text += symbols[6 + piece];
    }
    return text;
}

Position Position::starting() {
    return *from_fen("rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1");
}

std::optional<Position> Position::from_fen(std::string_view fen) {
    std::istringstream input{std::string(fen)};
    std::string placement, side, castling, ep, halfmove, fullmove, extra;
    if (!(input >> placement >> side >> castling >> ep >> halfmove >> fullmove) || (input >> extra))
        return std::nullopt;

    Position pos;
    int rank = 7, file = 0;
    bool previous_empty = false;
    for (char token : placement) {
        if (token == '/') {
            if (file != 8 || rank == 0) return std::nullopt;
            --rank;
            file = 0;
            previous_empty = false;
        } else if (token >= '1' && token <= '8') {
            if (previous_empty) return std::nullopt;
            file += token - '0';
            previous_empty = true;
        } else {
            const auto piece = symbols.find(token);
            if (piece == std::string_view::npos || file >= 8) return std::nullopt;
            pos.add_piece(piece < 6 ? Color::white : Color::black, static_cast<Piece>(piece % 6),
                          rank * 8 + file);
            ++file;
            previous_empty = false;
        }
        if (file > 8) return std::nullopt;
    }
    if (rank != 0 || file != 8) return std::nullopt;

    if (side != "w" && side != "b") return std::nullopt;
    pos.turn_ = side == "w" ? Color::white : Color::black;
    if (!parse_counter(halfmove, pos.halfmove_) || !parse_counter(fullmove, pos.fullmove_)
        || pos.fullmove_ == 0) return std::nullopt;

    const Mask occupied = pos.occupied();
    for (Color color : {Color::white, Color::black}) {
        const Mask pawns = pos.pieces(color, Piece::pawn);
        if (!std::has_single_bit(pos.pieces(color, Piece::king))
            || std::popcount(pos.by_color_[static_cast<int>(color)]) > 16
            || std::popcount(pawns) > 8 || (pawns & (bits::first_rank | bits::last_rank)))
            return std::nullopt;
    }
    const Color previous = pos.turn_ == Color::white ? Color::black : Color::white;
    const int previous_king = std::countr_zero(pos.pieces(previous, Piece::king));
    if (pos.attacked(previous_king, pos.turn_, occupied)) return std::nullopt;

    if (castling != "-") {
        for (char right : castling) {
            const auto i = rights.find(right);
            if (i == std::string_view::npos || (pos.castling_ & (1u << i))) return std::nullopt;
            const Color owner = i < 2 ? Color::white : Color::black;
            const int home = i < 2 ? 0 : 56;
            if (!(pos.pieces(owner, Piece::king) & bits::square(home + 4))
                || !(pos.pieces(owner, Piece::rook) & bits::square(home + (i % 2 == 0 ? 7 : 0))))
                return std::nullopt;
            pos.castling_ |= 1u << i;
        }
    }

    if (ep != "-") {
        const int target = parse_square(ep);
        const bool white = pos.turn_ == Color::white;
        if (target < 0 || target / 8 != (white ? 5 : 2)) return std::nullopt;
        const int forward = white ? 8 : -8;
        if (pos.halfmove_ != 0 || (occupied & (bits::square(target) | bits::square(target + forward)))
            || !(pos.pieces(previous, Piece::pawn) & bits::square(target - forward)))
            return std::nullopt;
        pos.en_passant_ = target;
    }
    return pos;
}

std::string Position::fen() const {
    std::string result;
    for (int rank = 7; rank >= 0; --rank) {
        int empty = 0;
        for (int file = 0; file < 8; ++file) {
            const int at = rank * 8 + file;
            const Piece piece = piece_at(at);
            if (piece == Piece::none) {
                ++empty;
                continue;
            }
            if (empty) result += static_cast<char>('0' + empty);
            empty = 0;
            const int offset = (by_color_[0] & bits::square(at)) ? 0 : 6;
            result += symbols[offset + static_cast<int>(piece)];
        }
        if (empty) result += static_cast<char>('0' + empty);
        if (rank) result += '/';
    }
    result += turn_ == Color::white ? " w " : " b ";
    if (!castling_) result += '-';
    for (unsigned i = 0; i < rights.size(); ++i)
        if (castling_ & (1u << i)) result += rights[i];
    result += ' ';
    result += en_passant_ < 0 ? "-" : format_square(en_passant_);
    result += ' ';
    result += std::to_string(halfmove_) + ' ' + std::to_string(fullmove_);
    return result;
}

} // namespace chess
