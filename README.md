# Alang

PHP-like interpreter written in C++17.

## Build

    g++ -std=c++17 -O3 -march=native -flto -Wall -Wextra -Wpedantic -o prog main.cpp lexer.cpp parser.cpp interpreter.cpp vm.cpp

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

PHP-style classes with properties, constants, methods, visibility, constructors, and static members:

    class Point {
        public $x = 0;
        private $y = 0;
        const ORIGIN = "zero";

        function __construct($a, $b) {
            $this->x = $a;
            $this->y = $b;
        }

        public function sum() { return $this->x + $this->y; }
        static function describe() { return "a point"; }
    }

    var p = new Point(3, 4);
    println(p->sum(), Point::ORIGIN, Point::describe());

`__construct` runs on `new`; `__destruct` runs when the object is released (scope exit, or program end):

    class Resource {
        function __construct() { println("open"); }
        function __destruct() { println("close"); }
    }

Inheritance accepts PHP `extends`, C++ `: public Base`, and `extend`. `parent::` calls base methods:

    class Dog extends Animal {
        function speak() { return parent::speak() + " woof"; }
    }

Namespaces with C++-style `using namespace` and `Name::member` lookup:

    namespace stD {
        class Vec { }
        function hello() { return "hi"; }
        const PI = 3;
    }
    using namespace stD;
    var v = new Vec();
    println(stD::PI, hello(), PI);

Concurrency with true OS threads. `async function` calls return a future immediately, `await` unwraps it, and `Thread` runs a function on its own thread:

    async function work(int n) { return n * 2; }
    var future = work(21);
    println(await future);

    function job(int n) { return n * 2; }
    var t = new Thread(job);
    t->start(21);
    println(t->join());

Each thread gets an isolated copy of the interpreter state, so there are no data races on shared variables.

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

## Tests

The regression suite covers literals, variables, operators, control flow, functions, closures, classes, inheritance, namespaces, destructors, threads/async, and error cases, plus randomized fuzz inputs. It checks exact output, exit codes and sanitizer reports:

    g++ -std=c++17 -O1 -g -fsanitize=address,undefined -fsanitize-recover=all \
        -o alang_test main.cpp lexer.cpp parser.cpp interpreter.cpp vm.cpp
    python3 tests/run.py ./alang_test
