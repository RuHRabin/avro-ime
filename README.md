# avro-ime — an Avro-style phonetic Bengali input method for Fcitx5

`ami bangla likhochhi` → `আমি বাংলা লিখছি`

Real phonetic typing (no AltGr/Shift gymnastics), fully local: no network, no logging,
nothing but a normal Fcitx5 addon. The linguistics live in a small dependency-free
C++ core; the Fcitx5 part is a thin frontend.

## Features

* Phonetic typing: `ami` → আমি, `kh` → খ, automatic conjuncts (`bondhu` → বন্ধু).
* Underlined Bengali preedit in the text, with the Latin keys you typed shown above the cursor.
* Backspace removes one Latin key, Esc cancels the word, Space/Enter commit it.
* Bengali digits and daari (`.` → ।), switchable.
* Per-window state, local only (no network, no logging, no dictionary files read).
* Dependency-free core engine with unit tests; frontend is a thin Fcitx5 addon.

```
core/avro_core.{hpp,cpp}   conversion engine  (pure C++17, no dependencies)
fcitx5/avroime.cpp         Fcitx5 frontend    (preedit, Backspace, Esc, commit)
fcitx5/data/*.conf         addon + input-method descriptors
tools/avro_convert.cpp     CLI to try the engine without any IME
tests/test_core.cpp        unit tests (ctest)
```

## Build & install

Debian / Ubuntu:

```bash
git clone https://github.com/RuHRabin/avro-ime.git && cd avro-ime

sudo apt install build-essential cmake libfcitx5core-dev libfcitx5utils-dev libfcitx5config-dev \
                 fcitx5 fcitx5-config-qt

cmake -S . -B build
cmake --build build -j
(cd build && ctest --output-on-failure)     # engine tests
sudo cmake --install build                  # installs to /usr (where Fcitx5 looks)
```

Then:

1. Log out and back in (or `fcitx5 -r`).
2. KDE Plasma Wayland: System Settings → Keyboard → Virtual Keyboard → **Fcitx 5**.
   Other desktops: `im-config -n fcitx5`.
3. `fcitx5-configtool` → add **Avro Phonetic (avro-ime)** (search "Avro", untick "Only show current language").
4. Global Options → set a switch shortcut (e.g. `Ctrl+Space`).

Tested on Debian 13 (trixie) with KDE Plasma, and compile-tested against Fcitx5 5.1.7
(Ubuntu 24.04). Older Fcitx5 releases (e.g. 5.0.x) may need a small API tweak — please open an
issue with the compiler error.

Uninstall: `sudo rm /usr/lib/x86_64-linux-gnu/fcitx5/libavroime.so /usr/share/fcitx5/addon/avroime.conf /usr/share/fcitx5/inputmethod/avroime.conf`

## Try the engine without installing anything

```bash
./build/avro-convert ami bangla likhochhi
./build/avro-convert --final-o bhalo achhi
```

## Typing guide

| Type | Get | Notes |
|---|---|---|
| `a` `i` `I` `u` `U` `e` `O` | আ ই ঈ উ ঊ এ ও | after a consonant they become kar: `ka`→কা `ki`→কি `kI`→কী `kO`→কো |
| `oi` `ou` | ঐ ঔ / ৈ ৌ | `koi`→কৈ |
| `rri` | ঋ / ৃ | `krri`→কৃ |
| `o` | see below | silent inherent vowel |
| `k kh g gh` | ক খ গ ঘ | |
| `c`/`ch` `chh`/`Ch` `j jh` | চ ছ জ ঝ | `chh` is ছ |
| `T Th D Dh N` | ট ঠ ড ঢ ণ | capitals = retroflex |
| `t th d dh n` | ত থ দ ধ ন | |
| `p ph f` `b bh v` `m` | প ফ ফ ব ভ ভ ম | |
| `z` `y` `r` `l` | য য় র ল | |
| `sh`/`S` `Sh` `s` `h` | শ ষ স হ | |
| `R` `Rh` | ড় ঢ় | |
| `ng` `Ng` `NG` | ং ঙ ঞ | `ng` before `k`/`g` is ঙ: `songge`→সঙ্গে |
| `^` `:` and `t` + two backticks | ঁ ঃ ৎ | `ca^d`→চাঁদ |
| `Z` `w` | ্য ্ব | `bakZo`→বাক্য, `bishwo`→বিশ্ব |
| `kkh` `GG` `x` | ক্ষ জ্ঞ ক্স | `shikkha`→শিক্ষা, `GGan`→জ্ঞান |
| `.` `..` | । . | `..` gives a literal full stop; a `.` right after a digit stays `.` |
| `0-9` | ০-৯ | switchable |
| a single backtick | (nothing) | separator, blocks a conjunct: `k` `` ` `` `k` gives কক |

### The two rules that matter

**1. Consonants join automatically.** `bhokti`→ভক্তি, `bondhu`→বন্ধু, `prem`→প্রেম.

**2. `o` is the silent inherent vowel, and it breaks a conjunct.**
`kolom`→কলম, `likhochhi`→লিখছি (without the `o`, `khchh` would try to join).
At word start `o` is অ (`omol`→অমল), after a vowel it is ও (`dao`→দাও).
For an explicit ো use capital `O`: `kOthay`→কোথায়, `sOnar`→সোনার, `tOmar`→তোমার.

If you want `bhalo`→ভালো (word-final `o` after a consonant = ো), turn on **FinalOKar** in the addon's
settings (or edit `~/.config/fcitx5/conf/avroime.conf`: `FinalOKar=True`).

### Keys while typing

| Key | Effect |
|---|---|
| Backspace | removes one Latin key (not a whole Bengali character) |
| Esc | throws the current word away |
| Space / Enter / Tab / arrows | commits the word, then the key works as usual |
| Ctrl/Alt/Super shortcuts | commit the word, shortcut works as usual |

The Latin letters you typed are shown above the cursor while the Bengali preedit is underlined in the text.

## Honest notes

* The rules are **my reconstruction of Avro Phonetic**, not a port of OmicronLab's tables, so
  some words will differ from real Avro. Everything is data in the table at the top of
  `core/avro_core.cpp`; changing a mapping is a one-line edit, and `tests/test_core.cpp` shows
  the expected behaviour. There is no dictionary / auto-correct / suggestion list.
* There is **no suggestion/prediction list** yet: the preedit is a direct conversion of what you
  typed. See "Ideas for later".
* Tested: the engine (unit tests; a GitHub Actions workflow is included) and live typing on Debian 13 + KDE Plasma. Other
  desktops and applications have had little testing — issues with the app and the keys you typed
  are very welcome.
* Pending text is dropped (not committed) when focus moves to another window.

## Ideas for later

* Dictionary-based suggestions (candidate list) and auto-correct.
* Reph / ya-phala special cases the way Avro does them.
* An IBus frontend reusing the same core.

## Related projects & credits

* The typing scheme is *inspired by* **Avro Phonetic** by OmicronLab. This project is a
  from-scratch reimplementation of the idea, not a port of their code or tables, and is not
  affiliated with OmicronLab.
* If you want the full-featured, dictionary-backed experience today, use
  [OpenBangla Keyboard](https://github.com/OpenBangla/OpenBangla-Keyboard).
* Its sibling, [xkb-avro](https://github.com/RuHRabin/xkb-avro), is a daemon-free static XKB layout
  (no phonetic sequences, by XKB's design).

## License

MIT — see [LICENSE](LICENSE).
