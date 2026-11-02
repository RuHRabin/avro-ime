// avro_core.hpp — Avro-style phonetic Bengali conversion engine.
//
// Pure C++17, no dependencies, no I/O, no knowledge of Fcitx/IBus/X11.
// The same core can sit behind any input-method frontend.
//
//   avro::convert("ami bangla likhochhi")  ->  "আমি বাংলা লিখছি"
//
// Design: the frontend keeps the Latin letters of the *current word* in a
// buffer; on every keystroke the whole buffer is re-converted from scratch.
// That makes context rules (vowel vs. kar, conjuncts, word-final 'o', ...)
// trivial and stateless, and makes Backspace correct (remove one Latin key).
#pragma once

#include <string>

namespace avro {

struct Options {
    // Word-final 'o' after a consonant yields ো  (bhalo -> ভালো).
    // false (default): 'o' is the silent inherent vowel, 'O' gives ো.
    bool final_o_kar = false;
    // Type ASCII digits as Bengali digits (০-৯).
    bool bangla_digits = true;
};

// Convert a Latin phonetic string (a word, or several words) to Bengali (UTF-8).
std::string convert(const std::string &latin, const Options &opt = {});

// Keystroke-level session used by input-method frontends.
class Session {
public:
    struct Result {
        std::string commit;   // text to commit to the application (may be empty)
        std::string preedit;  // text to show as underlined preedit ("" = none)
        bool consumed;        // true: swallow the key; false: let the app also see it
    };

    explicit Session(Options opt = {}) : opt_(opt) {}

    void set_options(const Options &opt) { opt_ = opt; }

    // Feed one printable ASCII character.
    Result feed(char c);
    // Backspace: removes one Latin key from the buffer. consumed only if buffer non-empty.
    Result backspace();
    // Commit whatever is in the buffer (focus change, space, Enter, shortcuts...).
    Result flush();
    // Drop the buffer without committing (Escape). consumed only if buffer non-empty.
    Result cancel();

    bool empty() const { return buf_.empty(); }
    const std::string &raw() const { return buf_; }      // what the user typed
    std::string preedit() const { return convert(buf_, opt_); }

private:
    Options opt_;
    std::string buf_;
    bool last_digit_ = false;
};

}  // namespace avro
