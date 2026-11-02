// Plain-assert style tests, no framework needed.  Run:  ctest  (or ./test_core)

#include <iostream>
#include <string>
#include <vector>

#include "avro_core.hpp"

static int failures = 0;

static void expect(const std::string &input, const std::string &want,
                   const avro::Options &opt = {}) {
    const std::string got = avro::convert(input, opt);
    if (got != want) {
        ++failures;
        std::cerr << "FAIL  convert(\"" << input << "\")\n      want: " << want
                  << "\n      got : " << got << "\n";
    }
}

static void expect_eq(const std::string &what, const std::string &got, const std::string &want) {
    if (got != want) {
        ++failures;
        std::cerr << "FAIL  " << what << "\n      want: \"" << want << "\"\n      got : \"" << got << "\"\n";
    }
}

int main() {
    // ---- the sentence that started it all ----
    expect("ami", "আমি");
    expect("Ami", "আমি");
    expect("bangla", "বাংলা");
    expect("likhochhi", "লিখছি");           // 'o' blocks the conjunct, chh = ছ
    expect("ami bangla likhochhi", "আমি বাংলা লিখছি");

    // ---- vowels: independent vs kar ----
    expect("a", "আ");
    expect("ka", "কা");
    expect("ki", "কি");
    expect("kI", "কী");
    expect("ku", "কু");
    expect("kU", "কূ");
    expect("ke", "কে");
    expect("koi", "কৈ");
    expect("kou", "কৌ");
    expect("kO", "কো");
    expect("krri", "কৃ");
    expect("rrishi", "ঋশি");
    expect("dao", "দাও");                   // o after a vowel -> ও
    expect("omol", "অমল");                  // o at start -> অ
    expect("Ondho", "ওন্ধ");                // O at start -> ও

    // ---- inherent 'o' ----
    expect("kolom", "কলম");
    expect("ghor", "ঘর");
    expect("kOthay", "কোথায়");
    expect("kothay", "কথায়");

    // ---- consonants & aspirates ----
    expect("khabar", "খাবার");
    expect("Dhaka", "ঢাকা");
    expect("dhaka", "ধাকা");
    expect("chhobi", "ছবি");
    expect("shundor", "শুন্দর");
    expect("Shokal", "ষকাল");
    expect("rasta", "রাস্তা");
    expect("ca^d", "চাঁদ");
    expect("bangladesh", "বাংলাদেশ");
    expect("songge", "সঙ্গে");              // ng before g -> ঙ
    expect("Ngo", "ঙ");                     // explicit ঙ ; trailing o is inherent

    // ---- conjuncts (automatic hasanta between consecutive consonants) ----
    expect("bhokti", "ভক্তি");
    expect("bondhu", "বন্ধু");
    expect("shikkha", "শিক্ষা");
    expect("prem", "প্রেম");
    expect("bakZo", "বাক্য");               // Z = য-ফলা
    expect("bishwo", "বিশ্ব");              // w = ব-ফলা
    expect("dhormo", "ধর্ম");
    expect("gganO", "গ্গানো");
    expect("GGan", "জ্ঞান");
    expect("rasstro", "রাস্স্ত্র");         // no magic: s+s+t+r just chain

    // ---- special characters ----
    expect("ut``", "উৎ");
    expect("`", "");
    expect("k`k", "কক");                    // ` separates, no conjunct
    expect("aa", "আ");
    expect("w", "ও");
    expect("Z", "য");
    expect("Rh", "ঢ়");

    // ---- digits & punctuation ----
    expect("2024", "২০২৪");
    expect("2024", "2024", avro::Options{false, false});
    expect("ami.", "আমি।");
    expect("ami..", "আমি.");
    expect("ami, tumi", "আমি, তুমি");
    expect("$100", "৳১০০");
    expect("naam::", "নাম:");

    // ---- options ----
    expect("bhalo", "ভাল");
    expect("bhalo", "ভালো", avro::Options{true, true});
    expect("bhalo achhi", "ভালো আছি", avro::Options{true, true});
    expect("bhalO", "ভালো");
    expect("kolom", "কলম", avro::Options{true, true});   // mid-word 'o' stays silent

    // ---- Session (keystroke-level behaviour) ----
    {
        avro::Session s;
        std::string committed;
        auto type = [&](const std::string &keys) {
            avro::Session::Result r{"", "", false};
            for (char c : keys) {
                r = s.feed(c);
                committed += r.commit;
            }
            return r;
        };

        auto r = type("ami");
        expect_eq("preedit after 'ami'", r.preedit, "আমি");
        expect_eq("nothing committed yet", committed, "");
        expect_eq("key consumed while buffering", r.consumed ? "1" : "0", "1");

        r = type(" ");                       // space: commit word, let the space through
        expect_eq("commit on space", committed, "আমি");
        expect_eq("space is not consumed", r.consumed ? "1" : "0", "0");
        expect_eq("preedit cleared", r.preedit, "");

        type("bangla");
        r = s.backspace();                   // removes one Latin key: 'a'  -> "bangl"
        expect_eq("backspace preedit", r.preedit, "বাংল");
        expect_eq("backspace consumed", r.consumed ? "1" : "0", "1");
        type("a ");
        expect_eq("committed so far (spaces are inserted by the app)", committed, "আমিবাংলা");

        r = s.backspace();                   // empty buffer: leave it to the app
        expect_eq("backspace on empty buffer passes through", r.consumed ? "1" : "0", "0");

        type("tumi");
        r = s.cancel();                      // Escape
        expect_eq("cancel consumed", r.consumed ? "1" : "0", "1");
        expect_eq("cancel drops text", s.empty() ? "1" : "0", "1");
        r = s.cancel();
        expect_eq("cancel on empty passes through", r.consumed ? "1" : "0", "0");

        type("kemon");
        r = s.flush();                       // focus lost / shortcut
        expect_eq("flush commits", r.commit, "কেমন");

        // digits: committed as Bengali digits, consumed (we insert them ourselves)
        r = type("7");
        expect_eq("digit consumed", r.consumed ? "1" : "0", "1");
        expect_eq("digit text", r.commit, "৭");
        // ... and a following '.' stays a literal full stop (3.5 -> ৩.৫)
        r = type(".");
        expect_eq("dot after digit passes through", r.consumed ? "1" : "0", "0");
        expect_eq("dot after digit commits nothing", r.commit, "");

        // a '.' after a word turns into daari when the word ends
        s.cancel();
        type("ami.");
        r = type(" ");
        expect_eq("daari at end of sentence", r.commit, "আমি।");
    }

    if (failures == 0) {
        std::cout << "All tests passed.\n";
        return 0;
    }
    std::cerr << failures << " test(s) failed.\n";
    return 1;
}
