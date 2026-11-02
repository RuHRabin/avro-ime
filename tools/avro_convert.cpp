// avro-convert: quick way to try the engine without any input method.
//
//   avro-convert ami bangla likhochhi          (arguments are joined with spaces)
//   echo "ami bangla likhochhi" | avro-convert (or read stdin, line by line)
//   avro-convert --final-o bhalo achi          (word-final o -> ো)

#include <iostream>
#include <string>

#include "avro_core.hpp"

int main(int argc, char **argv) {
    avro::Options opt;
    std::string joined;
    for (int i = 1; i < argc; ++i) {
        std::string a = argv[i];
        if (a == "--final-o") { opt.final_o_kar = true; continue; }
        if (a == "--ascii-digits") { opt.bangla_digits = false; continue; }
        if (!joined.empty()) joined += ' ';
        joined += a;
    }
    if (!joined.empty()) {
        std::cout << avro::convert(joined, opt) << "\n";
        return 0;
    }
    std::string line;
    while (std::getline(std::cin, line)) std::cout << avro::convert(line, opt) << "\n";
    return 0;
}
