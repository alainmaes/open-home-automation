<?php

error_reporting(0);
$max_result_lines = 1000;
header('Content-Type: application/xml; charset=utf-8');

$sock = fsockopen("127.0.0.1", 1028, $errno, $errstr, 30);
if (!$sock)
		$result = "<response status='error'>Unable to connect to linknx</response>\n";
else {
	fwrite($sock, file_get_contents("php://input") . chr(4));
	$result = '';
	$cnt = 0;
	while ($cnt < $max_result_lines && $sock && !feof($sock)) {
		$result .= fgets($sock, 128);
		$c = fgetc($sock);
		if ($c == "\4")	break;
		$result .= $c;
		$cnt++;
	}
	fclose($sock);
}
print($result);
?>
