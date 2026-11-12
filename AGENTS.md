# Chess engine project instructions

- Build this engine with the rigor you would use if the long-term goal were to beat Stockfish. Prioritize correctness and robustness from the first step, especially in the code that enforces the rules of chess.
- Verify behavior with reproducible checks, including rule edge cases. Support claims about speed or playing strength with benchmarks or controlled matches.
- Present project choices and their tradeoffs to Brandon before adopting them. Wait for his explicit confirmation. This quality standard does not decide the language, dependencies, search approach, or whether to use a neural network.

## Confirmed rules-layer decisions

- Write the engine in C++ and implement the rules in this project.
- Use bitboards for the initial standard-chess position and move generator.
- Keep move generation and legality in the rules layer. Search receives legal moves only.
- Keep board state, attack geometry, move generation, and notation modular.
