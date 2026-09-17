// The same conversions as the other two probes, written with explicit casts.
// It must build: if it does not, the warning set is failing everything, and the
// other probes failing proves nothing.
float narrow(double d);
unsigned to_unsigned(int i);

float narrow(double d) {
    return static_cast<float>(d);
}

unsigned to_unsigned(int i) {
    return static_cast<unsigned>(i);
}

int main(int argc, char**) {
    return narrow(1.5) > 0.0f && to_unsigned(argc) > 0u ? 0 : 1;
}
