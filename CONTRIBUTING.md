# Contributing

Bug reports and pull requests are welcome.

## Reporting a typing problem

Please include:

* what you typed (the Latin keys) and what you expected,
* what you got,
* the application (Kate, Firefox, Konsole, ...), desktop (e.g. KDE Plasma 6 / Wayland) and distro.

## Changing a mapping or a rule

All mappings live in the table at the top of `core/avro_core.cpp`; the conversion rules are
the small `convert()` loop in the same file. Please add a line to `tests/test_core.cpp`
for every behaviour you change, then:

```bash
cmake -S . -B build
cmake --build build -j
ctest --test-dir build --output-on-failure
```

You can try the engine without any input method:

```bash
./build/avro-convert ami bangla likhochhi
```

## Scope

* `core/` must stay dependency-free (no Fcitx, no I/O) so that other frontends can reuse it.
* `fcitx5/` should only translate between Fcitx5 events and `avro::Session`.
