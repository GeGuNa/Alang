#!/bin/bash
BIN=${1:-./prog}
PORT=18445
TMP=$(mktemp -d)
trap 'kill $PID 2>/dev/null; rm -rf "$TMP"' EXIT

openssl req -x509 -newkey rsa:2048 -keyout "$TMP/key.pem" \
    -out "$TMP/cert.pem" -days 1 -nodes -subj '/CN=localhost' >/dev/null 2>&1

cat > "$TMP/app.alang" <<EOF
var app = new Server();
app->secure("$TMP/cert.pem", "$TMP/key.pem");
app->limit(10);
app->get("/secure", function (req, res) { res->send("tls-ok"); });
app->listen($PORT);
EOF

$BIN "$TMP/app.alang" > "$TMP/server.log" 2>&1 &
PID=$!
sleep 1

result=$(curl -sk https://127.0.0.1:$PORT/secure)
if [ "$result" = "tls-ok" ]; then
    echo "PASS https"
else
    echo "FAIL https: '$result'"
    cat "$TMP/server.log"
    exit 1
fi

code=$(curl -sk -o /dev/null -w '%{http_code}' https://127.0.0.1:$PORT/missing)
if [ "$code" = "404" ]; then
    echo "PASS https 404"
else
    echo "FAIL https 404: $code"
    exit 1
fi
