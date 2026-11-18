<?php
$total = 0;
for ($i = 0; $i < 2000000; $i = $i + 1) {
    if ($i % 5 == 0) { $total = $total + 1; }
    elseif ($i % 5 == 1) { $total = $total + 2; }
    elseif ($i % 5 == 2) { $total = $total + 3; }
    elseif ($i % 5 == 3) { $total = $total + 4; }
    else { $total = $total + 5; }
}
echo $total, "\n";
