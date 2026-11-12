#include "chess/position.hpp"

#include <algorithm>
#include <cstdlib>
#include <iostream>
#include <stdexcept>
#include <string_view>

using chess::Position;

namespace {

void require(bool value, std::string_view message) {
    if (!value) throw std::runtime_error(std::string(message));
}

Position load(std::string_view fen) {
    auto position = Position::from_fen(fen);
    require(position.has_value(), fen);
    return *position;
}

bool has_move(const Position& pos, std::string_view text) {
    std::vector<chess::Move> moves;
    pos.legal_moves(moves);
    const auto move = chess::parse_move(text);
    return move && std::find(moves.begin(), moves.end(), *move) != moves.end();
}

void round_trip(Position pos, std::string_view move_text, std::string_view expected) {
    const auto before = pos.fen();
    require(pos.play(chess::parse_move(move_text).value()), move_text);
    require(pos.fen() == expected, pos.fen());
    require(pos.undo(), "Undo returned false");
    require(pos.fen() == before, "Undo did not restore FEN");
    require(!pos.undo(), "Undo unexpectedly succeeded with empty history");
}

void check_perft(Position pos, std::initializer_list<std::uint64_t> counts) {
    unsigned depth = 1;
    const auto before = pos.fen();
    require(pos.perft(0) == 1, "perft(0)");
    for (auto expected : counts) {
        const auto actual = pos.perft(depth);
        if (actual != expected)
            throw std::runtime_error("perft " + std::to_string(depth) + ": expected "
                                     + std::to_string(expected) + ", got " + std::to_string(actual)
                                     + " for " + pos.fen());
        ++depth;
    }
    require(pos.fen() == before, "Perft changed the original position");
}

void test_moves() {
    auto start = Position::starting();
    round_trip(start, "e2e4", "rnbqkbnr/pppppppp/8/8/4P3/8/PPPP1PPP/RNBQKBNR b KQkq e3 0 1");
    const auto before = start.fen();
    require(!start.play(chess::parse_move("e2e5").value()), "Accepted illegal pawn jump");
    require(!start.play({-1, 64}), "Accepted out-of-range squares");
    require(start.fen() == before && !start.undo(), "Rejected move changed state");

    auto castles = load("r3k2r/8/8/8/8/8/8/R3K2R w KQkq - 9 4");
    round_trip(castles, "e1g1", "r3k2r/8/8/8/8/8/8/R4RK1 b kq - 10 4");
    round_trip(castles, "e1c1", "r3k2r/8/8/8/8/8/8/2KR3R b kq - 10 4");
    round_trip(castles, "a1a8", "R3k2r/8/8/8/8/8/8/4K2R b Kk - 0 4");
    require(!has_move(load("4kr2/8/8/8/8/8/8/4K2R w K - 0 1"), "e1g1"),
            "Castling through check");
    require(!has_move(load("k3r3/8/8/8/8/8/8/4K2R w K - 0 1"), "e1g1"),
            "Castling out of check");

    auto ep = load("4k3/8/8/3pP3/8/8/8/4K3 w - d6 0 2");
    round_trip(ep, "e5d6", "4k3/8/3P4/8/8/8/8/4K3 b - - 0 2");
    require(!has_move(load("4k3/8/8/r4pPK/8/8/8/8 w - f6 0 2"), "g5f6"),
            "En passant exposed king to rook");
    auto ep_check = load("k7/8/8/3pP3/4K3/8/8/8 w - d6 0 2");
    require(ep_check.in_check() && has_move(ep_check, "e5d6"), "En passant should remove checker");

    auto promote = load("4k3/P7/8/8/8/8/8/4K3 w - - 0 1");
    for (auto move : {"a7a8q", "a7a8r", "a7a8b", "a7a8n"})
        require(has_move(promote, move), move);
    require(!has_move(promote, "a7a8"), "Allowed promotion without piece");
    round_trip(promote, "a7a8n", "N3k3/8/8/8/8/8/8/4K3 b - - 0 1");

    auto pin = load("4r1k1/8/8/8/8/8/4R3/4K3 w - - 0 1");
    require(!has_move(pin, "e2f2") && has_move(pin, "e2e8"), "Incorrect pin handling");
    auto double_check = load("4r1k1/8/8/8/1b6/8/8/R3K3 w - - 0 1");
    std::vector<chess::Move> moves;
    double_check.legal_moves(moves);
    require(double_check.in_check() && !moves.empty(), "Incorrect double-check fixture");
    for (const auto move : moves) require(move.from == 4, "Non-king move under double check");
    require(!has_move(load("4k3/8/8/8/8/8/8/r2K4 w - - 0 1"), "d1e1"),
            "King's old square hid sliding attack");
}

void test_fen() {
    for (auto fen : {
        "8/8/8/8/8/8/8/8 w - - 0 1",
        "8/8/8/8/8/8/3k4/4K3 w - - 0 1",
        "4k3/8/8/8/8/8/8/4K3 w K - 0 1",
        "4k3/8/8/8/8/8/8/4K3 w - d6 0 1",
        "4k3/8/8/8/8/8/8/4K3 w - - -1 1",
        "4k3/8/8/8/8/8/8/4K3 w - - 0 0",
        "4k3/8/8/8/8/8/8/4K3 w - - 0 1 extra",
        "4k3/8/8/8/8/8/8/P3K3 w - - 0 1",
        "4k3/8/8/8/8/8/8/4K3 w - - 18446744073709551616 1",
    }) require(!Position::from_fen(fen), fen);
}

} // namespace

int main() {
    try {
        test_fen();
        test_moves();
        check_perft(Position::starting(), {20, 400, 8902, 197281, 4865609});
        check_perft(load("r3k2r/p1ppqpb1/bn2pnp1/3PN3/1p2P3/2N2Q1p/PPPBBPPP/R3K2R w KQkq - 0 1"),
                    {48, 2039, 97862, 4085603});
        check_perft(load("8/2p5/3p4/KP5r/1R3p1k/8/4P1P1/8 w - - 0 1"),
                    {14, 191, 2812, 43238, 674624});
        check_perft(load("r3k2r/Pppp1ppp/1b3nbN/nP6/BBP1P3/q4N2/Pp1P2PP/R2Q1RK1 w kq - 0 1"),
                    {6, 264, 9467, 422333});
        check_perft(load("rnbq1k1r/pp1Pbppp/2p5/8/2B5/8/PPP1NnPP/RNBQK2R w KQ - 1 8"),
                    {44, 1486, 62379, 2103487});
        check_perft(load("r4rk1/1pp1qppp/p1np1n2/2b1p1B1/2B1P1b1/P1NP1N2/1PP1QPPP/R4RK1 w - - 0 10"),
                    {46, 2079, 89890, 3894594});
        std::cout << "Rules and perft tests passed\n";
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return EXIT_FAILURE;
    }
}
