#pragma once

#include <array>
#include <cstdint>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace chess {

enum class Color { white, black };
enum class Piece { pawn, knight, bishop, rook, queen, king, none };

struct Move {
    int from; // a1 = 0, h8 = 63
    int to;
    Piece promotion = Piece::none;
    bool operator==(const Move&) const = default;
};

std::optional<Move> parse_move(std::string_view text);
std::string format_move(Move move);

class Position {
public:
    static Position starting();
    // Checks syntax and local position invariants, not reachability from the start.
    static std::optional<Position> from_fen(std::string_view fen);
    std::string fen() const;

    bool in_check() const;
    // Reuses caller storage and replaces its contents with legal moves.
    void legal_moves(std::vector<Move>& moves) const;
    bool play(Move move);
    bool undo();
    // Counts legal move sequences without game-history draw adjudication.
    std::uint64_t perft(unsigned depth) const;

private:
    using Mask = std::uint64_t;
    Position() = default;

    struct Undo {
        Move move;
        Piece moving;
        Piece captured;
        int capture_square;
        unsigned castling;
        int en_passant;
        std::uint64_t halfmove;
        std::uint64_t fullmove;
    };

    std::array<Mask, 6> by_piece_{};
    std::array<Mask, 2> by_color_{};
    Color turn_ = Color::white;
    unsigned castling_ = 0;
    int en_passant_ = -1;
    std::uint64_t halfmove_ = 0;
    std::uint64_t fullmove_ = 1;
    std::vector<Undo> history_;

    Mask pieces(Color side, Piece piece) const;
    Mask occupied() const;
    Piece piece_at(int square) const;
    Mask leaper_attackers(int square, Color side, Mask available) const;
    bool attacked(int square, Color side, Mask occupancy, Mask removed = 0) const;
    void add_piece(Color side, Piece piece, int square);
    void remove_piece(Color side, Piece piece, int square);
    Undo prepare_move(Move move) const;
    void make(const Undo& state);
    void unmake(const Undo& state);
    std::uint64_t count_paths(unsigned depth, std::vector<std::vector<Move>>& buffers);
};

} // namespace chess
