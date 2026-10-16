### What GCC cannot see, now seen, 2026-09-22 — tail done

**Both Linux builds are GCC, and GCC does not warn about an unused constant
at namespace scope in C++** - not under `-Wall -Wextra`, and not under
`-Wunused-const-variable` at any level, which it honours for C alone. Clang
does, so that class was invisible here and reddened macOS and Windows
clang-cl together on a commit both Linux jobs had passed.

`tests/cmake/clang_warnings.cmake` closes it by re-running the compile
commands CMake already exports - under clang, syntax only, with the same
flags the build used and the unused-constant warning added. Same flags, same
files, so a warning here is a warning the other compiler would give. 93
first-party sources in 100 seconds; `ext/` is somebody else's and is left
alone, and where clang is not installed it reports itself skipped.

**Watched both ways**, which is the point of it: a constant nobody uses put
into `checklist.cpp`, and GCC built the whole project **without a word**,
while the test failed with "unused variable 'nobody_uses_this'
[-Werror,-Wunused-const-variable]".

**What it does not compile**, with its reason: flags GCC has and clang has
not - its module mapper, its dependency format, and a suppression named for a
GCC warning - are dropped; `-o` and its argument go, and the dependency
files; Objective-C++ is macOS's own. Getting `-c` wrong is how the first run
failed: `-c` takes no argument, the source merely follows it, and dropping
what followed dropped the input, so clang said "no input files" 93 times.
