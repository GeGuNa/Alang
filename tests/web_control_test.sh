#!/bin/bash
BIN=${1:-./prog}
PORT=18447
$BIN tests/web_app2.alang > /tmp/alang_web2.log 2>&1 &
PID=$!
trap 'kill $PID 2>/dev/null' EXIT
sleep 1

fail=0
check() {
    local name="$1"; shift
    local expected="${@: -1}"
    local result
    result=$("$@" 2>/dev/null)
    if [ "$result" = "$expected" ]; then
        echo "PASS $name"
    else
        echo "FAIL $name: got '$result' expected '$expected'"
        fail=1
    fi
}

check all_get curl -s http://127.0.0.1:$PORT/method GET
check all_post curl -s -X POST http://127.0.0.1:$PORT/method POST
check all_put curl -s -X PUT http://127.0.0.1:$PORT/method PUT
check headers curl -s http://127.0.0.1:$PORT/hdr 'a:'
check response_cookie curl -s http://127.0.0.1:$PORT/ck abc
check missing_file curl -s http://127.0.0.1:$PORT/missingfile fallback
check stats curl -s http://127.0.0.1:$PORT/stats '1/6'

for i in 1 2 3; do
    curl -s -o /tmp/alang_limit_$i -w '%{http_code}\n' http://127.0.0.1:$PORT/slow > /tmp/alang_code_$i &
done
PIDS=$(jobs -p | grep -v $PID | tr '\n' ' ')
for p in $PIDS; do wait $p 2>/dev/null; done
codes=$(cat /tmp/alang_code_1 /tmp/alang_code_2 /tmp/alang_code_3 | sort | tr '\n' ' ')
if [ "$codes" = "200 503 503 " ]; then
    echo "PASS limit"
else
    echo "FAIL limit: $codes"
    fail=1
fi

exit $fail
