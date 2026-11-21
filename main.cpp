#include <exception>
#include <fstream>
#include <iostream>
#include <memory>
#include <sstream>
#include <string>
#include <vector>

#include "interpreter.h"
#include "lexer.h"
#include "parser.h"
#include "token.hpp"

const char* demoSource =
    "int add(int a, int b) { return a + b; }\n"
    "int fact(int n) { if (n <= 1) { return 1; } return n * fact(n - 1); }\n"
    "int a = 15;\n"
    "int b = 125;\n"
    "float c = 14.3;\n"
    "string greeting = \"hello\";\n"
    "println(\"a =\", a);\n"
    "println(\"b =\", b);\n"
    "println(\"c =\", c);\n"
    "println(\"sum =\", add(a, b));\n"
    "println(\"fact(5) =\", fact(5));\n"
    "println(greeting, \"world\");\n"
    "if (a < b) { println(\"a is less than b\"); } else { println(\"a is not less than b\"); }\n"
    "int i = 0;\n"
    "while (i < 3) { println(\"i =\", i); i = i + 1; }\n"
    "for (int j = 0; j < 3; j = j + 1) { println(\"j =\", j); }\n"
    "println(10 > 5, 2 == 2, !false);\n";

void printTokens(const std::vector<Token>& tokens) {
    for (const Token& token : tokens) {
        std::cout << token.line << " " << token.type << " " << token.text << '\n';
    }
}

int main(int argc, char** argv) {
    std::string source;

    if (argc > 1) {
        std::string arg = argv[1];
        if (arg == "--tokens") {
            if (argc > 2) {
                std::ifstream file(argv[2]);
                if (!file) {
                    std::cerr << "Cannot open file: " << argv[2] << '\n';
                    return 1;
                }
                std::ostringstream stream;
                stream << file.rdbuf();
                source = stream.str();
            } else {
                source = demoSource;
            }
            Lexer lexer(source);
            printTokens(lexer.tokenize());
            return 0;
        }

        std::ifstream file(argv[1]);
        if (!file) {
            std::cerr << "Cannot open file: " << argv[1] << '\n';
            return 1;
        }
        std::ostringstream stream;
        stream << file.rdbuf();
        source = stream.str();
    } else {
        source = demoSource;
    }

    try {
        Lexer lexer(source);
        Parser parser(lexer.tokenize());
        std::vector<StmtPtr> program = parser.parse();
        auto interpreter = std::make_shared<Interpreter>();
        interpreter->compileProgram(program);
        interpreter->runProgram();
        interpreter->shutdown();
    } catch (const ThrownException& thrown) {
        std::cerr << "Uncaught: " << valueToString(thrown.value) << '\n';
        return 1;
    } catch (const LangError& error) {
        std::cerr << "Error: " << error.what() << '\n';
        return 1;
    } catch (const std::exception& error) {
        std::cerr << "Error: " << error.what() << '\n';
        return 1;
    }

    return 0;
}
