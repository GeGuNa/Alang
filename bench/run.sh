#!/bin/sh
cd "$(dirname "$0")/.." || exit 1
for name in io for while elseif func concat class rec; do
    echo "=== $name ==="
    for lang in alang php; do
        if [ "$lang" = alang ]; then
            cmd="./prog bench/$name.alang"
        else
            cmd="php bench/$name.php"
        fi
        printf '%-6s ' "$lang"
        for run in 1 2 3; do
            /usr/bin/time -f '%e' sh -c "$cmd > /dev/null" 2>&1 | tr '\n' ' '
        done
        echo
    done
done
