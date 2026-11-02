#include "harness.hpp"

// **A line written to a stream made whole-lined goes out in one write**
// (src/platform/whole_lines.hpp). Impair's summary, 146 bytes, went out as a
// write of 128 and one of 18 - glibc's printf to an unbuffered stream - and the
// client's line about a tile fetch came between them, on CI's linux-release
// (run 41397416052): the test that read the summary did not find it.
//
// Waiting for two programs' writes to meet would only sometimes test the rule.
// This counts the writes instead: the stream is one whose every write the C
// library makes is handed to the test (fopencookie on glibc, funopen on the
// BSDs and macOS), so a line in pieces is seen as pieces whoever else is
// writing. Every length from one byte to the buffer's last is written, from
// one thread and then from four at once, and each must arrive as one write of
// exactly one whole line.
//
// Windows' C runtime takes line buffering as full buffering, so the helper
// leaves its streams be (whole_lines.hpp says why), and it has no stream whose
// writes can be counted: there the test says it is skipped, never passed.

#include "platform/whole_lines.hpp"

#include <cstddef>
#include <cstdio>
#include <mutex>
#include <string>
#include <thread>
#include <vector>

#if !defined(_WIN32)
#include <sys/types.h>
#endif

namespace {

#if !defined(_WIN32)

// What the C library wrote, a write at a time.
struct Writes {
    std::mutex lock;
    std::vector<std::string> each;
};

#if defined(__APPLE__) || defined(__FreeBSD__)
int counted_write(void* cookie, const char* data, int size) {
    auto& writes = *static_cast<Writes*>(cookie);
    const std::lock_guard guard(writes.lock);
    writes.each.emplace_back(data, static_cast<std::size_t>(size));
    return size;
}

std::FILE* counted_stream(Writes& writes) {
    return funopen(&writes, nullptr, counted_write, nullptr, nullptr);
}
#else
ssize_t counted_write(void* cookie, const char* data, std::size_t size) {
    auto& writes = *static_cast<Writes*>(cookie);
    const std::lock_guard guard(writes.lock);
    writes.each.emplace_back(data, size);
    return static_cast<ssize_t>(size);
}

std::FILE* counted_stream(Writes& writes) {
    cookie_io_functions_t io{};
    io.write = counted_write;
    return fopencookie(&writes, "w", io);
}
#endif

// A line of `size` bytes, its newline the last, saying who wrote it.
std::string line_of(std::size_t size, char who) {
    std::string line(size - 1, who);
    line += '\n';
    return line;
}

// Written as the programs write theirs: formatted from more than one piece.
// glibc writes a single string conversion of its own straight through, so
// "%s" alone would go out in one write even unbuffered; its first character
// and then the rest, as "impair: not an address: %s" is a literal and then
// the rest, is staged and written 128 bytes at a time.
void write_formatted(std::FILE* stream, const std::string& line) {
    std::fprintf(stream, "%.*s%s", 1, line.c_str(), line.c_str() + 1);
}

#endif

} // namespace

GLIDESLOPE_TEST(a_line_written_to_a_stream_made_whole_lined_goes_out_in_one_write) {
#if defined(_WIN32)
    glideslope::test::skip(
        "Windows' C runtime takes line buffering as full buffering, so "
        "the helper leaves its streams be, and no stream of its counts writes");
#else
    using glideslope::test::check;
    using glideslope::test::fail;
    constexpr std::size_t buffer_size = 4096;

    // From one thread: every length the buffer holds, newline included.
    {
        Writes writes;
        std::FILE* stream = counted_stream(writes);
        check(stream != nullptr, "a stream whose writes are counted");
        static char buffer[buffer_size];
        check(glideslope::platform::whole_lines(stream, buffer, buffer_size),
              "the C library took the buffer");
        std::size_t written = 0;
        for (std::size_t size = 1; size <= buffer_size; ++size) {
            const std::string line = line_of(size, 'a');
            write_formatted(stream, line);
            ++written;
            const std::lock_guard guard(writes.lock);
            if (writes.each.size() != written || writes.each.back() != line) {
                fail("a line of " + std::to_string(size) + " bytes went out as " +
                     std::to_string(writes.each.size() - (written - 1)) +
                     " writes, or not as itself");
            }
        }
        std::fclose(stream);
        // **Coverage**: every length from 1 to the buffer's size.
        check(written == buffer_size, "every length written");
    }

    // From four threads at once: each write one thread's whole line.
    {
        Writes writes;
        std::FILE* stream = counted_stream(writes);
        check(stream != nullptr, "a stream whose writes are counted");
        static char buffer[buffer_size];
        check(glideslope::platform::whole_lines(stream, buffer, buffer_size),
              "the C library took the buffer");
        constexpr int threads = 4;
        constexpr std::size_t lines_each = 500;
        std::vector<std::thread> writers;
        for (int t = 0; t < threads; ++t) {
            writers.emplace_back([stream, t] {
                const char who = static_cast<char>('a' + t);
                for (std::size_t i = 0; i < lines_each; ++i) {
                    // 129 to 1129 bytes: past glibc's 128-byte stage, each one.
                    const std::string line = line_of(
                        129 + (i * 7 + static_cast<std::size_t>(t)) % 1000, who);
                    write_formatted(stream, line);
                }
            });
        }
        for (auto& w : writers) {
            w.join();
        }
        std::fclose(stream);
        const std::size_t expected = static_cast<std::size_t>(threads) * lines_each;
        if (writes.each.size() != expected) {
            fail(std::to_string(expected) + " lines went out as " +
                 std::to_string(writes.each.size()) + " writes");
        }
        for (const std::string& w : writes.each) {
            const bool one_writer = w.find_first_not_of(w.front()) == w.size() - 1;
            if (w.back() != '\n' || !one_writer) {
                fail("a write that is not one thread's whole line: " + w.substr(0, 40));
            }
        }
    }
#endif
}
