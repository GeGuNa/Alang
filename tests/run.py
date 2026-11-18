#!/usr/bin/env python3
import os
import subprocess
import sys

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
CASES = os.path.join(ROOT, "tests", "cases")
BINARY = sys.argv[1] if len(sys.argv) > 1 else os.path.join(ROOT, "prog")
ENV = dict(os.environ, ASAN_OPTIONS="detect_leaks=1")

passed = []
failed = []

for name in sorted(os.listdir(CASES)):
    if not name.endswith(".alang"):
        continue
    path = os.path.join(CASES, name)
    expected_path = path[:-6] + ".expected"
    with open(expected_path) as handle:
        expected = handle.read()

    try:
        result = subprocess.run([BINARY, path], capture_output=True, text=True, env=ENV, timeout=30)
    except subprocess.TimeoutExpired:
        failed.append((name, "TIMEOUT", "", ""))
        continue

    stderr = result.stderr.strip()
    stdout = result.stdout
    is_error = expected.startswith("Error:")

    if is_error:
        wanted = expected.strip()
        if result.returncode == 1 and stderr == wanted:
            passed.append(name)
        else:
            failed.append((name, "exit=%d stderr=%r expected=%r" % (result.returncode, stderr, wanted), stdout, stderr))
    else:
        if result.returncode == 0 and stdout == expected and not stderr:
            passed.append(name)
        else:
            failed.append((name, "exit=%d" % result.returncode, stdout, stderr))

def fuzz():
    import random
    random.seed(1234)
    words = ["int", "float", "string", "const", "var", "function", "class", "extends",
             "namespace", "using", "parent", "new", "this", "null", "public", "private",
             "static", "return", "if", "else", "while", "for", "break", "continue",
             "true", "false", "println", "thread", "async", "await", "Thread",
             "__construct", "__destruct", "start", "join"]
    chars = list("abcxyz01239.+-*/%=<>!&|;,(){}[]$:\"'# \n\t") + words
    path = os.path.join(ROOT, "tests", "_fuzz.alang")
    for i in range(200):
        source = "".join(random.choice(chars) for _ in range(random.randint(1, 160)))
        with open(path, "w") as handle:
            handle.write(source)
        try:
            result = subprocess.run([BINARY, path], capture_output=True, text=True, env=ENV, timeout=20)
        except subprocess.TimeoutExpired:
            failed.append(("fuzz-%d" % i, "TIMEOUT", "", ""))
            continue
        if result.returncode not in (0, 1) or "Sanitizer" in result.stderr or "runtime error" in result.stderr:
            failed.append(("fuzz-%d" % i, "exit=%d" % result.returncode, result.stdout[:100], result.stderr[:200]))
    os.remove(path)

fuzz()

print("PASS: %d" % len(passed))
if failed:
    print("FAIL: %d" % len(failed))
    for name, reason, stdout, stderr in failed:
        print("--- %s --- %s" % (name, reason))
        print("STDOUT:", repr(stdout[:300]))
        print("STDERR:", repr(stderr[:300]))
    sys.exit(1)
print("ALL TESTS PASSED")
