<?php
$sum = 0;
for ($i = 0; $i < 10000000; $i = $i + 1) {
    $sum = $sum + $i;
}
echo $sum, "\n";
