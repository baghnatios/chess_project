# chess_project

A C++ chess engine project. The first milestone implements standard chess
position handling and legal move generation from scratch.

- `include/chess/position.hpp` exposes positions, legal moves, and make/undo.
- `src/bitboards.hpp` handles attack masks and sliding rays.
- `src/movegen.cpp` generates legal moves using checks, pins, and king safety.
- `src/position.cpp` applies and undoes moves.
- `src/fen.cpp` parses and writes FEN and coordinate move notation.
- `tests/rules_tests.cpp` checks rule edge cases and known perft counts.

Use a C++20 compiler to build and run the standalone suite:

```sh
mkdir -p build
c++ -std=c++20 -O2 -Wall -Wextra -Wpedantic -Iinclude src/*.cpp tests/rules_tests.cpp -o build/rules_tests
./build/rules_tests
```

FEN parsing validates syntax and local board invariants. It does not prove that
a position can be reached from the starting position. The current milestone
covers move legality. Game history adjudication and search are future work.
