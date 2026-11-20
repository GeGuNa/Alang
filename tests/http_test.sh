#!/bin/bash
BIN=${1:-./prog}
PORT=18083
$BIN tests/web_app.alang > /tmp/alang_web_test.log 2>&1 &
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
        echo "FAIL $name: got '$result'"
        fail=1
    fi
}

check json curl -s http://127.0.0.1:$PORT/json '{"ok":true}'
check header curl -s -H 'X-Test: abc' http://127.0.0.1:$PORT/header 'abc'
check put curl -s -X PUT -d 'hello' http://127.0.0.1:$PORT/put 'put:hello'
check form curl -s -X POST -d 'name=foo&value=bar' http://127.0.0.1:$PORT/form 'foo=bar'
check file curl -s http://127.0.0.1:$PORT/file 'Hello from a served file!'

headers=$(curl -s -i http://127.0.0.1:$PORT/cookie)
echo "$headers" | grep -q 'Set-Cookie: visited=yes' && echo "PASS setcookie" || { echo "FAIL setcookie"; fail=1; }

uploaded=$(curl -s -F 'payload=@demo/web/data.txt' http://127.0.0.1:$PORT/upload)
echo "$uploaded" | grep -q 'data.txt:26:true' && echo "PASS upload" || { echo "FAIL upload: $uploaded"; fail=1; }

redirect=$(curl -s -i http://127.0.0.1:$PORT/redirect)
echo "$redirect" | grep -q '302' && echo "PASS redirect" || { echo "FAIL redirect"; fail=1; }

missing=$(curl -s -o /dev/null -w '%{http_code}' http://127.0.0.1:$PORT/missing)
[ "$missing" = "404" ] && echo "PASS 404" || { echo "FAIL 404: $missing"; fail=1; }

exit $fail
