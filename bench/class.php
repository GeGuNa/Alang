<?php
class Counter {
    private $n = 0;
    function add($x) { $this->n = $this->n + $x; return $this->n; }
}
$c = new Counter();
$total = 0;
for ($i = 0; $i < 1000000; $i = $i + 1) {
    $total = $c->add(1);
}
echo $total, "\n";
