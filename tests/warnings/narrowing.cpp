// Deliberately narrows a double to a float with no cast - the floating origin's
// bug, in one line. The warning set must make this a build failure.
float narrow(double d);

float narrow(double d) {
    return d;
}

int main() {
    return narrow(1.5) > 0.0f ? 0 : 1;
}
