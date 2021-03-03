void fff() {
    static bool first = true;
    if (first) {
        first = false;
        FOO("hello\n");
    }
}
