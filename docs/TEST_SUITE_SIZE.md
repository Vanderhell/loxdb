# Test suite size (measured)

The repository uses CMake/CTest, while most behavioral cases use the in-repo
microtest harness in `tests/microtest.h`.

Current source count:

- 571 `MDB_RUN_TEST(` call sites across `tests/*.c`;
- 61 `tests/test_*.c` files;
- one C++ wrapper test file, `tests/test_cpp_wrapper.cpp`.

The effective CTest entry count varies with the configured build options and
build type. Measure it with `ctest --test-dir <build-tree> -N`.
