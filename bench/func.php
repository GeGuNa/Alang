<?php
function twice($x) { return $x * 2; }
$total = 0;
for ($i = 0; $i < 2000000; $i = $i + 1) {
    $total = $total + twice($i);
}
echo $total, "\n";
