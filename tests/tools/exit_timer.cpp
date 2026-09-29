// glideslope_exit_timer - how long a program took to end after it said
// something, read from the other end of its standard output.
//
//   program ... | glideslope_exit_timer TEXT...
//
// Copies standard input to standard output, line by line. At the first line
// holding each TEXT it starts a clock, and when standard input ends - the
// program has exited, and its end of the pipe with it - it says on standard
// error, for each TEXT,
//
//   glideslope_exit_timer: ended 0.412 s after "TEXT"
//
// or, if no line held it, that it never saw it. The program must flush the
// line when it says it, or it arrives with the end. Exits 0, or 2 on bad
// arguments.

#include <chrono>
#include <csignal>
#include <cstdio>
#include <iostream>
#include <optional>
#include <string>
#include <vector>

int main(int argc, char** argv) {
#ifndef _WIN32
    // It runs in a test's pipeline, whose reader may go first. A write must
    // fail, not end it (src/platform/closed_pipes.hpp).
    std::signal(SIGPIPE, SIG_IGN);
#endif
    if (argc < 2) {
        std::fprintf(stderr, "usage: glideslope_exit_timer TEXT...\n");
        return 2;
    }
    const std::vector<std::string> texts(argv + 1, argv + argc);
    std::vector<std::optional<std::chrono::steady_clock::time_point>> said(texts.size());
    std::string line;
    while (std::getline(std::cin, line)) {
        for (std::size_t i = 0; i < texts.size(); ++i) {
            if (!said[i] && line.find(texts[i]) != std::string::npos) {
                said[i] = std::chrono::steady_clock::now();
            }
        }
        std::cout << line << '\n';
    }
    std::cout.flush();
    const auto end = std::chrono::steady_clock::now();
    for (std::size_t i = 0; i < texts.size(); ++i) {
        if (!said[i]) {
            std::fprintf(stderr, "glideslope_exit_timer: never saw \"%s\"\n",
                         texts[i].c_str());
            continue;
        }
        const double took = std::chrono::duration<double>(end - *said[i]).count();
        std::fprintf(stderr, "glideslope_exit_timer: ended %.3f s after \"%s\"\n", took,
                     texts[i].c_str());
    }
    return 0;
}
