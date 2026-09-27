#include "ai/OpeningBook.hpp"
#include <cstdio>
using namespace draughts;
int main(int argc, char** argv) {
    if (argc < 2) { std::printf("usage: book_info <file.bin>\n"); return 1; }
    ai::OpeningBook b;
    if (!b.loadFromFile(argv[1])) { std::printf("load failed\n"); return 1; }
    std::printf("%s\n  positions: %zu\n  total moves: %zu\n  avg moves/pos: %.2f\n",
                argv[1], b.positionCount(), b.moveCount(),
                b.positionCount() ? double(b.moveCount()) / b.positionCount() : 0.0);
    return 0;
}