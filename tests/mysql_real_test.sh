#!/bin/bash
BIN=${1:-./prog}
USER=fantom
PASS=123456
HOST=127.0.0.1
PORT=3306
DB="alang_real_test_$(date +%s)_$$"

MYSQL="mysql -h $HOST -P $PORT -u $USER -p$PASS"

cleanup() {
    if [ "${CREATED:-0}" = "1" ]; then
        $MYSQL -e "DROP DATABASE IF EXISTS \`$DB\`;" >/dev/null 2>&1
    fi
    rm -f "$TMPAPP"
}
trap cleanup EXIT

if ! $MYSQL -e 'SELECT 1;' >/dev/null 2>&1; then
    echo "SKIP real mysql (credentials not valid)"
    exit 0
fi

if [ -n "$($MYSQL -N -e "SHOW DATABASES LIKE '$DB';" 2>/dev/null)" ]; then
    echo "FAIL: database name collision $DB"
    exit 1
fi

$MYSQL -e "CREATE DATABASE \`$DB\`;" >/dev/null 2>&1 || { echo "FAIL create db"; exit 1; }
CREATED=1

TMPAPP=$(mktemp)
cat > "$TMPAPP" <<EOF
var db = new MySQL();
println("connect:", db->connect("$HOST", "$USER", "$PASS", "$DB", $PORT));
db->query("CREATE TABLE items (id INT AUTO_INCREMENT PRIMARY KEY, name VARCHAR(50), qty INT)");
db->query("INSERT INTO items (name, qty) VALUES ('alpha', 3)");
println("insertId:", db->insertId());
var res = db->query("SELECT id, name, qty FROM items");
println("cols:", res->columns());
println("rows:", res->rows());
var row = res->fetch();
println("row:", row->get("name"), row->get("qty"));
db->close();
println("done");
EOF

result=$($BIN "$TMPAPP" 2>&1)
expected="connect: true
insertId: 1
cols: id,name,qty
rows: 1
row: alpha 3
done"

if [ "$result" = "$expected" ]; then
    echo "PASS real mysql"
else
    echo "FAIL real mysql"
    echo "$result"
    exit 1
fi
