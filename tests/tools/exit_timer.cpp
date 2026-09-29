// glideslope_exit_timer - how long a program took to end after it said
// something, read from the other end of its standard output.
//
//   program ... | glideslope_exit_timer TEXT
//
// Copies standard input to standard output, line by line. At the first line
// holding TEXT it starts a clock, and when standard input ends - the program
// has exited, and its end of the pipe with it - it says on standard error
//
//   glideslope_exit_timer: ended 0.412 s after "TEXT"
//
// or, if no line held TEXT, that it never saw it. The program must flush the
// line when it says it, or it arrives with the end. Exits 0, or 2 on bad
// arguments.

#include <chrono>
#include <cstdio>
#include <iostream>
#include <optional>
#include <string>

int main(int argc, char** argv) {
    if (argc != 2) {
        std::fprintf(stderr, "usage: glideslope_exit_timer TEXT\n");
        return 2;
    }
    const std::string text = argv[1];
    std::optional<std::chrono::steady_clock::time_point> said;
    std::string line;
    while (std::getline(std::cin, line)) {
        if (!said && line.find(text) != std::string::npos) {
            said = std::chrono::steady_clock::now();
        }
        std::cout << line << '\n';
    }
    std::cout.flush();
    if (!said) {
        std::fprintf(stderr, "glideslope_exit_timer: never saw \"%s\"\n", text.c_str());
        return 0;
    }
    const double took =
        std::chrono::duration<double>(std::chrono::steady_clock::now() - *said).count();
    std::fprintf(stderr, "glideslope_exit_timer: ended %.3f s after \"%s\"\n", took,
                 text.c_str());
    return 0;
}
