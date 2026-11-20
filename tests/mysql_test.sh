#!/bin/bash
BIN=${1:-./prog}
PORT=18451
TMP=$(mktemp -d)
cleanup() {
    if [ -f "$TMP/server.pid" ]; then kill $(cat "$TMP/server.pid") 2>/dev/null; fi
    sleep 0.3
    rm -rf "$TMP"
}
trap cleanup EXIT

if ! command -v mariadb-install-db >/dev/null || ! command -v mariadbd >/dev/null && [ ! -x /usr/sbin/mariadbd ]; then
    echo "SKIP mysql integration (server tools unavailable)"
    exit 0
fi

mariadb-install-db --datadir="$TMP/data" --auth-root-authentication-method=normal \
    --user=$(whoami) >/dev/null 2>&1 || { echo "SKIP mysql integration (init failed)"; exit 0; }

MARIADBD=$(command -v mariadbd || echo /usr/sbin/mariadbd)
$MARIADBD --datadir="$TMP/data" --socket="$TMP/mysql.sock" --port=$PORT \
    --bind-address=127.0.0.1 --skip-grant-tables --user=$(whoami) \
    --pid-file="$TMP/mysql.pid" --log-error="$TMP/mysql.log" >/dev/null 2>&1 &
echo $! > "$TMP/server.pid"
sleep 3

mysql --socket="$TMP/mysql.sock" -u root -e \
    'CREATE DATABASE alangtest; USE alangtest; CREATE TABLE users (id INT AUTO_INCREMENT PRIMARY KEY, name VARCHAR(50), age INT);' \
    >/dev/null 2>&1 || { echo "FAIL mysql setup"; exit 1; }

cat > "$TMP/app.alang" <<EOF
var db = new MySQL();
println("connect:", db->connect("127.0.0.1", "root", "", "alangtest", $PORT));
db->query("INSERT INTO users (name, age) VALUES ('alice', 30)");
var res = db->query("SELECT id, name, age FROM users");
println("cols:", res->columns());
println("rows:", res->rows());
var row = res->fetch();
println("row:", row->get("name"), row->get("age"));
db->close();
EOF

result=$($BIN "$TMP/app.alang" 2>&1)
expected='connect: true
cols: id,name,age
rows: 1
row: alice 30'

if [ "$result" = "$expected" ]; then
    echo "PASS mysql integration"
else
    echo "FAIL mysql integration"
    echo "$result"
    exit 1
fi
