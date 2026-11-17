# Alang

PHP-like interpreter written in C++17.

## Build

    g++ -std=c++17 -O2 -Wall -Wextra -Wpedantic -o prog main.cpp lexer.cpp parser.cpp interpreter.cpp

## Run

Run the built-in demo:

    ./prog

Run a source file:

    ./prog program.alang

Dump the tokens for a source file (or the demo when no file is given):

    ./prog --tokens program.alang

## Language

Types: `int`, `float`, `double`, `string` / `str`, `bool`, `char`, `void`.

Declarations and assignment:

    int a = 15;
    float c = 14.3;
    string s = "hello";
    a = 35;

Constants require an initializer and cannot be reassigned:

    const int limit = 100;
    const name = "Alang";

Functions with parameters and return values. Parameters can use C-style types, `$` sigils, or a mix of both:

    int add(int a, int b) { return a + b; }
    function add($a, $b) { return $a + $b; }
    function mixed(int a, $b) { return a + $b; }

`var` infers a fixed type from the initializer. Reassigning a different type is rejected:

    var s = "qq";
    var num = 13.5;
    var fn = function () { return 1; };

A `$` sigil is part of the variable name, so `$name` and `name` are separate variables with separate values:

    $name = "eeeeeeee";
    string name = "aaa";
    println($name);
    println(name);

A `$`-prefixed assignment declares the variable implicitly, with the type fixed from the first value, just like `var`:

    $score = 10;
    $label = "alang";
    $double = function (int n) { return n * 2; };

Control flow: `if` / `else` / `else if`, `while`, `for`, `break`, `continue`.

Loops also accept Rust-style forms. `for { }` and `while { }` are infinite loops that exit with `break`, and conditions do not need parentheses:

    for {
        if (done) { break; }
    }
    while count < 10 {
        count = count + 1;
    }
    for count < 10 {
        count = count + 1;
    }

Operators: `=`, `+`, `-`, `*`, `/`, `%`, `==`, `!=`, `>`, `>=`, `<`, `<=`, `!`, `&&`, `||`.
Strings concatenate with `+` or the PHP-style `.` operator (`"a" . 1`).
String literals support `\n`, `\t`, `\r`, `\\`, `\"` and `\'` escapes.
Source comments use double-slash, hash, or slash-star delimiters.
Built-in output functions: `print`, `echo`, `println`.

See `demo/` for runnable programs that exercise the supported syntax.
