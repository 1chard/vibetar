# Contributing to Vibetar

Use issues to report problems and pull requests to propose changes. Describe
the expected behavior, actual behavior, and steps needed to reproduce it.
For hardware issues, include the board, firmware revision, operating system or
console, game, USB mode, connection type, and optional peripherals. Attach the
configuration JSON when it helps reproduce the issue.

## Setup and tests

Initialize submodules and build both UF2 targets as described in the
[README](README.md). C++ `static_assert` checks run during firmware compilation.
Python tests that inspect ELF and UF2 files require the outputs in `build`.

```sh
python -m unittest discover -s tests -p 'test_*.py' -v
node --test tests/*.test.mjs
```

To run authentication tests on the host computer, build the firmware first,
then use a native C/C++ compiler:

```sh
cmake -S tests/xsm3_native -B build-native
cmake --build build-native --config Release
ctest --test-dir build-native -C Release --output-on-failure
```

## Pull requests

- Explain the problem and resulting behavior.
- Run tests relevant to the change.
- Keep HID protocol changes aligned across the firmware and editor.
- Distinguish simulated tests from checks on actual boards, consoles, and games.
- Preserve license notices in dependencies and adapted sources.

Do not include SDK checkouts, build output, private configuration, or credentials.
