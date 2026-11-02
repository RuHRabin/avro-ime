// avro_core.cpp — see avro_core.hpp
//
// HOW THE CONVERSION WORKS
// ------------------------
// 1. Tokenise the Latin input with greedy longest-match (max 3 chars) against
//    the table below. Unknown upper-case letters fall back to their lower-case
//    entry (so "Ami" works like "ami").
// 2. Walk the tokens keeping one bit of context: what did we just emit?
//        START : word start / after space or punctuation
//        CONS  : a consonant
//        VOWEL : a vowel or vowel sign
//        OTHER : after a sign (ং ঁ ঃ ৎ), after the silent inherent 'o',
//                or after the ` separator
//    and apply the Avro rules:
//        vowel  after CONS          -> kar (া ি ী ু ূ ে ৈ ো ৌ ৃ)
//        vowel  elsewhere           -> independent form (আ ই ঈ ...)
//        cons   after CONS          -> hasanta + cons (automatic conjunct)
//        'o'    after CONS          -> nothing (inherent vowel). It also
//                                      *blocks* the conjunct:  likhochhi -> লিখছি
//        'o'    at START            -> অ        'o' after a vowel -> ও
//        'O'                        -> ও / ো  (always explicit)
//
// Everything here is data + one small loop; tweak the table to taste.

#include "avro_core.hpp"

#include <cctype>
#include <map>
#include <vector>

namespace avro {
namespace {

enum class Kind {
    Cons,    // consonant (a = text)
    Phala,   // consonant that attaches as a "-phala" after another consonant
             //   a = text after a consonant, b = text otherwise
    Vowel,   // a = independent form, b = kar (dependent sign)
    VowelO,  // the special lower-case 'o'
    Sign,    // standalone sign (a = text)
    Block,   // ` : emits nothing, separates letters
    Raw      // anything else, copied through (a = text)
};

struct Entry {
    Kind kind;
    std::string a;
    std::string b;
    bool fb_vowel = false;  // Phala: is the fallback (b) a vowel?
};

const std::map<std::string, Entry> &table() {
    static const std::map<std::string, Entry> t = [] {
        std::map<std::string, Entry> m;
        auto cons = [&](const char *k, const char *v) { m[k] = Entry{Kind::Cons, v, "", false}; };
        auto vow = [&](const char *k, const char *indep, const char *kar) {
            m[k] = Entry{Kind::Vowel, indep, kar, false};
        };
        auto sign = [&](const char *k, const char *v) { m[k] = Entry{Kind::Sign, v, "", false}; };
        auto raw = [&](const char *k, const char *v) { m[k] = Entry{Kind::Raw, v, "", false}; };

        // ---- vowels (independent, kar) ----
        vow("a", "আ", "া");  vow("aa", "আ", "া");  vow("A", "আ", "া");
        vow("i", "ই", "ি");
        vow("I", "ঈ", "ী");  vow("ii", "ঈ", "ী");
        vow("u", "উ", "ু");
        vow("U", "ঊ", "ূ");  vow("uu", "ঊ", "ূ");
        vow("e", "এ", "ে");
        vow("oi", "ঐ", "ৈ");  vow("OI", "ঐ", "ৈ");
        vow("ou", "ঔ", "ৌ");  vow("OU", "ঔ", "ৌ");
        vow("O", "ও", "ো");
        vow("rri", "ঋ", "ৃ");
        m["o"] = Entry{Kind::VowelO, "", "", false};

        // ---- consonants ----
        cons("k", "ক");   cons("kh", "খ");
        cons("g", "গ");   cons("gh", "ঘ");
        cons("Ng", "ঙ");
        cons("c", "চ");   cons("ch", "চ");   cons("chh", "ছ");  cons("Ch", "ছ");
        cons("j", "জ");   cons("jh", "ঝ");
        cons("NG", "ঞ");
        cons("T", "ট");   cons("Th", "ঠ");
        cons("D", "ড");   cons("Dh", "ঢ");
        cons("N", "ণ");
        cons("t", "ত");   cons("th", "থ");
        cons("d", "দ");   cons("dh", "ধ");
        cons("n", "ন");
        cons("p", "প");   cons("ph", "ফ");   cons("f", "ফ");
        cons("b", "ব");   cons("bh", "ভ");   cons("v", "ভ");
        cons("m", "ম");
        cons("z", "য");
        cons("r", "র");
        cons("l", "ল");
        cons("sh", "শ");  cons("S", "শ");    cons("Sh", "ষ");
        cons("s", "স");
        cons("h", "হ");
        cons("R", "ড়");  cons("Rh", "ঢ়");
        cons("y", "য়");
        cons("q", "ক");
        // frequently used ready-made clusters
        cons("kkh", "ক্ষ");
        cons("GG", "জ্ঞ");
        cons("x", "ক্স");

        // -phala:  Z = য-ফলা (bakZo -> বাক্য),  w = ব-ফলা (bishwo -> বিশ্ব)
        m["Z"] = Entry{Kind::Phala, "্য", "য", false};
        m["w"] = Entry{Kind::Phala, "্ব", "ও", true};

        // ---- signs ----
        sign("ng", "ং");   // becomes ঙ before k/g, see tokenize()
        sign(":", "ঃ");
        sign("^", "ঁ");
        sign("t``", "ৎ");

        // ---- punctuation ----
        raw(".", "।");
        raw("..", ".");    // literal full stop
        raw("::", ":");    // literal colon
        raw("$", "৳");

        m["`"] = Entry{Kind::Block, "", "", false};
        return m;
    }();
    return t;
}

const char *const kBanglaDigits[10] = {"০", "১", "২", "৩", "৪", "৫", "৬", "৭", "৮", "৯"};
const char *const kHasanta = "্";
const char *const kOKar = "ো";
const char *const kOInitial = "অ";
const char *const kOAfterVowel = "ও";

std::vector<Entry> tokenize(const std::string &in, const Options &opt) {
    const auto &t = table();
    std::vector<Entry> toks;
    size_t i = 0;
    while (i < in.size()) {
        const Entry *e = nullptr;
        size_t len = 0;

        for (int L = 3; L >= 1; --L) {
            if (i + static_cast<size_t>(L) > in.size()) continue;
            auto it = t.find(in.substr(i, static_cast<size_t>(L)));
            if (it != t.end()) {
                e = &it->second;
                len = static_cast<size_t>(L);
                break;
            }
        }
        if (!e && std::isupper(static_cast<unsigned char>(in[i]))) {
            std::string lower(1, static_cast<char>(std::tolower(static_cast<unsigned char>(in[i]))));
            auto it = t.find(lower);
            if (it != t.end()) {
                e = &it->second;
                len = 1;
            }
        }

        if (!e) {
            unsigned char c = static_cast<unsigned char>(in[i]);
            Entry r{Kind::Raw, std::string(1, in[i]), "", false};
            if (opt.bangla_digits && c >= '0' && c <= '9') r.a = kBanglaDigits[c - '0'];
            toks.push_back(r);
            ++i;
            continue;
        }

        // "ng" before k or g is the consonant ঙ (songge -> সঙ্গে), otherwise anusvara ং.
        if (len == 2 && in.compare(i, 2, "ng") == 0 && i + 2 < in.size() &&
            (in[i + 2] == 'k' || in[i + 2] == 'g')) {
            toks.push_back(Entry{Kind::Cons, "ঙ", "", false});
        } else {
            toks.push_back(*e);
        }
        i += len;
    }
    return toks;
}

enum class State { Start, Cons, Vowel, Other };

}  // namespace

std::string convert(const std::string &latin, const Options &opt) {
    const std::vector<Entry> toks = tokenize(latin, opt);
    std::string out;
    State st = State::Start;

    for (size_t i = 0; i < toks.size(); ++i) {
        const Entry &t = toks[i];
        switch (t.kind) {
        case Kind::Cons:
            if (st == State::Cons) out += kHasanta;
            out += t.a;
            st = State::Cons;
            break;

        case Kind::Phala:
            if (st == State::Cons) {
                out += t.a;
                st = State::Cons;
            } else {
                out += t.b;
                st = t.fb_vowel ? State::Vowel : State::Cons;
            }
            break;

        case Kind::Vowel:
            out += (st == State::Cons) ? t.b : t.a;
            st = State::Vowel;
            break;

        case Kind::VowelO:
            if (st == State::Cons) {
                const bool word_final = (i + 1 == toks.size()) || toks[i + 1].kind == Kind::Raw;
                if (opt.final_o_kar && word_final) {
                    out += kOKar;
                    st = State::Vowel;
                } else {
                    st = State::Other;  // silent inherent vowel; blocks the conjunct
                }
            } else if (st == State::Vowel) {
                out += kOAfterVowel;
                st = State::Vowel;
            } else {
                out += kOInitial;
                st = State::Vowel;
            }
            break;

        case Kind::Sign:
            out += t.a;
            st = State::Other;
            break;

        case Kind::Block:
            st = State::Other;
            break;

        case Kind::Raw:
            out += t.a;
            st = State::Start;
            break;
        }
    }
    return out;
}

// ---------------------------------------------------------------- Session

Session::Result Session::feed(char c) {
    const unsigned char uc = static_cast<unsigned char>(c);
    const bool alpha = uc < 128 && std::isalpha(uc);
    const bool buffered = alpha || c == '`' || c == '^' ||
                          (c == ':' && !buf_.empty()) ||
                          (c == '.' && !(buf_.empty() && last_digit_));  // keep 3.5 as 3.5 (digits stay literal)

    if (buffered) {
        buf_.push_back(c);
        last_digit_ = false;
        return {"", preedit(), true};
    }

    // Terminator (space, digit, punctuation, ...): commit the word first.
    std::string commit = convert(buf_, opt_);
    buf_.clear();

    if (c == '.') {  // only reachable right after a digit: keep it a literal '.'
        last_digit_ = false;
        return {commit, "", false};
    }

    const std::string one(1, c);
    const std::string mapped = convert(one, opt_);
    const bool consumed = (mapped != one);
    if (consumed) commit += mapped;
    last_digit_ = (c >= '0' && c <= '9');
    return {commit, "", consumed};
}

Session::Result Session::backspace() {
    if (buf_.empty()) return {"", "", false};
    buf_.pop_back();
    return {"", preedit(), true};
}

Session::Result Session::flush() {
    std::string commit = convert(buf_, opt_);
    buf_.clear();
    return {commit, "", false};
}

Session::Result Session::cancel() {
    const bool had = !buf_.empty();
    buf_.clear();
    return {"", "", had};
}

}  // namespace avro
