// closed_pipe - does a program carry on when the reader of its output has gone?
//
//   glideslope_closed_pipe EXIT PROGRAM [ARGS...]
//
// The network tests run their programs in one pipeline, each one's standard
// output the next one's standard input, and a program often writes after the
// one it writes to has finished: a client prints what it did after its
// goodbye, and the goodbye is what ended the server. With SIGPIPE at its
// default action that write killed it - on macOS CI, twice, as a client's exit
// code of SIGPIPE in a run that had otherwise gone well.
//
// So PROGRAM is run twice, both times with its standard output and standard
// error into one pipe:
//
// - **once read**, to show it writes something and exits EXIT - or the second
//   run tests nothing;
// - **once with the pipe's reading end closed before it starts**, so that its
//   every write is to a pipe with no reader. Not a race against a reader that
//   might still be there: there is none from the first byte. SIGPIPE is put
//   back to its default action in it, as a shell or ctest might not have left
//   it, so that only what the program does itself decides. It must exit EXIT
//   again, not be killed.
//
// Windows has no SIGPIPE - a write to a pipe nobody reads fails, and that is
// all - so this is not built there.

#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>

#include <csignal>
#include <cstdio>
#include <cstdlib>
#include <cstring>

namespace {

struct Ran {
    bool exited = false;
    int code = 0;   // the exit code, or the signal that killed it
    long wrote = 0; // bytes read from it, when read
};

bool run(char** program, bool closed, Ran& ran) {
    int ends[2];
    if (pipe(ends) != 0) {
        std::perror("closed_pipe: pipe");
        return false;
    }
    if (closed) {
        close(ends[0]);
    }
    const pid_t child = fork();
    if (child < 0) {
        std::perror("closed_pipe: fork");
        return false;
    }
    if (child == 0) {
        std::signal(SIGPIPE, SIG_DFL);
        sigset_t pipe_signal;
        sigemptyset(&pipe_signal);
        sigaddset(&pipe_signal, SIGPIPE);
        sigprocmask(SIG_UNBLOCK, &pipe_signal, nullptr);
        dup2(ends[1], STDOUT_FILENO);
        dup2(ends[1], STDERR_FILENO);
        close(ends[1]);
        if (!closed) {
            close(ends[0]);
        }
        execv(program[0], program);
        _exit(127);
    }
    close(ends[1]);
    if (!closed) {
        char buffer[4096];
        for (ssize_t got; (got = read(ends[0], buffer, sizeof buffer)) != 0;) {
            if (got > 0) {
                ran.wrote += got;
            }
        }
        close(ends[0]);
    }
    int status = 0;
    if (waitpid(child, &status, 0) != child) {
        std::perror("closed_pipe: waitpid");
        return false;
    }
    ran.exited = WIFEXITED(status);
    ran.code = ran.exited ? WEXITSTATUS(status) : WTERMSIG(status);
    return true;
}

void describe(const Ran& ran) {
    if (ran.exited) {
        std::printf("exited %d", ran.code);
    } else {
        std::printf("was killed by signal %d%s", ran.code,
                    ran.code == SIGPIPE ? " (SIGPIPE)" : "");
    }
}

} // namespace

int main(int argc, char** argv) {
    if (argc < 3) {
        std::fprintf(stderr, "usage: glideslope_closed_pipe EXIT PROGRAM [ARGS...]\n");
        return 2;
    }
    const int expected = std::atoi(argv[1]);
    char** program = argv + 2;

    Ran read_run;
    if (!run(program, false, read_run)) {
        return 2;
    }
    std::printf("%s: read, it wrote %ld bytes and ", program[0], read_run.wrote);
    describe(read_run);
    std::printf("\n");
    if (read_run.wrote == 0 || !read_run.exited || read_run.code != expected) {
        std::printf("closed_pipe: it must write something and exit %d, or the run with the "
                    "pipe closed tests nothing\n",
                    expected);
        return 1;
    }

    Ran closed_run;
    if (!run(program, true, closed_run)) {
        return 2;
    }
    std::printf("%s: with nobody reading, it ", program[0]);
    describe(closed_run);
    std::printf("\n");
    if (!closed_run.exited || closed_run.code != expected) {
        std::printf("closed_pipe: it must exit %d with nobody reading, as it did read\n",
                    expected);
        return 1;
    }
    return 0;
}
