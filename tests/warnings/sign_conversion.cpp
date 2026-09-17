// Deliberately converts a signed int to unsigned with no cast. The warning set
// must make this a build failure on GCC and Clang.
unsigned to_unsigned(int i);

unsigned to_unsigned(int i) {
    return i;
}

int main(int argc, char**) {
    return to_unsigned(argc) > 0u ? 0 : 1;
}
