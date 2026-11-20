# Alang demo programs

Run any example from the repository root:

    ./prog demo/all_syntax.alang
    ./prog demo/hello.alang
    ./prog demo/variables.alang
    ./prog demo/operators.alang
    ./prog demo/concat.alang
    ./prog demo/functions.alang
    ./prog demo/function_expressions.alang
    ./prog demo/parameters.alang
    ./prog demo/classes.alang
    ./prog demo/threads.alang
    ./prog demo/async_await.alang
    ./prog demo/concurrency_stress.alang
    ./prog demo/inheritance.alang
    ./prog demo/namespaces.alang
    ./prog demo/destructors.alang
    ./prog demo/control_flow.alang
    ./prog demo/rust_loops.alang
    ./prog demo/else_if.alang
    ./prog demo/const.alang
    ./prog demo/var_inference.alang
    ./prog demo/php_variables.alang
    ./prog demo/showcase.alang

`mysql.alang` shows the MySQL client (requires a running server).

`web/basic.alang`, `web/full.alang` and `web/secure.alang` demonstrate the built-in Express-style HTTP/HTTPS server (run one, then use curl).

`all_syntax.alang` is the single combined program that exercises every supported feature. The other files focus on one area each.

To inspect how a file is tokenized:

    ./prog --tokens demo/all_syntax.alang

Each file prints section headers at runtime, so you can see which syntax is being exercised.
