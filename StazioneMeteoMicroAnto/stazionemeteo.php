<?php
$myfile = fopen("stazionemeteo.txt", "w") or die("Unable to open file!");
$txt = $_GET["data"];
fwrite($myfile, $txt);
fclose($myfile);
?>