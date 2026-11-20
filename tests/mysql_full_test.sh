#!/bin/bash
BIN=${1:-./prog}
PORT=18452
TMP=$(mktemp -d)
cleanup() {
    if [ -f "$TMP/server.pid" ]; then kill $(cat "$TMP/server.pid") 2>/dev/null; fi
    sleep 0.3
    rm -rf "$TMP"
}
trap cleanup EXIT

command -v mariadb-install-db >/dev/null || { echo "SKIP mysql full (no install tool)"; exit 0; }
MARIADBD=$(command -v mariadbd || echo /usr/sbin/mariadbd)
[ -x "$MARIADBD" ] || { echo "SKIP mysql full (no server)"; exit 0; }

mariadb-install-db --datadir="$TMP/data" --auth-root-authentication-method=normal \
    --user=$(whoami) >/dev/null 2>&1 || { echo "SKIP mysql full (init failed)"; exit 0; }

$MARIADBD --datadir="$TMP/data" --socket="$TMP/mysql.sock" --port=$PORT \
    --bind-address=127.0.0.1 --skip-grant-tables --user=$(whoami) \
    --pid-file="$TMP/mysql.pid" --log-error="$TMP/mysql.log" >/dev/null 2>&1 &
echo $! > "$TMP/server.pid"
sleep 3

mysql --socket="$TMP/mysql.sock" -u root -e \
    'CREATE DATABASE alang_full_test; USE alang_full_test; CREATE TABLE items (id INT AUTO_INCREMENT PRIMARY KEY, name VARCHAR(50), qty INT);' \
    >/dev/null 2>&1 || { echo "FAIL mysql setup"; exit 1; }

cat > "$TMP/app.alang" <<EOF
var db = new MySQL();
println("connect:", db->connect("127.0.0.1", "root", "", "alang_full_test", $PORT));
println("ping:", db->ping());

println("create:", db->query("CREATE TABLE logs (id INT AUTO_INCREMENT PRIMARY KEY, msg VARCHAR(100))") != null);
db->query("INSERT INTO items (name, qty) VALUES ('alpha', 3)");
println("insertId:", db->insertId());
db->query("INSERT INTO items (name, qty) VALUES ('beta', 5)");

var res = db->query("SELECT id, name, qty FROM items ORDER BY id");
println("cols:", res->columns());
println("rows:", res->rows());
var r1 = res->fetch();
println("r1:", r1->get("name"), r1->get(2), r1->size());
var r2 = res->fetch();
println("r2:", r2->get(1), r2->get("qty"));
println("eof:", res->fetch());

var upd = db->query("UPDATE items SET qty = 9 WHERE name = 'alpha'");
println("affected:", db->affectedRows());

var esc = db->escape("O'Brien");
println("escape:", esc);

var empty = db->query("SELECT * FROM items WHERE name = 'missing'");
println("emptyRows:", empty->rows());
println("emptyFetch:", empty->fetch());

var eachtest = db->query("SELECT name FROM items ORDER BY id");
eachtest->each(function (row) { println("each:", row->get("name")); });

var bad = db->query("SELECT * FROM nonexistent_table");
println("badQuery:", bad);
println("hasError:", db->lastError() != "");

println("close:");
db->close();
println("done");
EOF

result=$($BIN "$TMP/app.alang" 2>&1)
expected="connect: true
ping: true
create: true
insertId: 1
cols: id,name,qty
rows: 2
r1: alpha 3 3
r2: beta 5
eof: void
affected: 1
escape: O\\'Brien
emptyRows: 0
emptyFetch: void
each: alpha
each: beta
badQuery: void
hasError: true
close:
done"

if [ "$result" = "$expected" ]; then
    echo "PASS mysql full"
else
    echo "FAIL mysql full"
    echo "--- got ---"
    echo "$result"
    echo "--- expected ---"
    echo "$expected"
    exit 1
fi
