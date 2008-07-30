#!/usr/local/bin/php -q
<?php
/**************************************************************************************************
	$Id: stats.php,v 1.5 2005/03/22 19:34:10 bboy Exp $

	This PHP script demonstrates how one might write a script to process the query log lines
	output by MyDNS when started with the --verbose (-v) option.

	1. Add a line "log = stdout" to your mydns.conf.
	2. Start MyDNS with a command like "mydns --verbose | stats.php"

**************************************************************************************************/

if (!($fp = fopen("php://stdin", "r")))
	die("Unable to open input stream.\n");

while ($line = trim(fgets($fp, 1024)))
{
	$s = explode(" ", $line);

	if (!isset($s[16]) || $s[16] != "LOG")
		continue;

	/*
	**  Construct "stats" array based on input line.
	*/
	$stats = array();
	$stats['date'] = $s[1];
	$stats['time'] = substr($s[2], 0, 8);
	$stats['time_ms'] = substr($s[2], 9);
	$stats['query_number'] = substr($s[3], 1);
	$stats['client_id'] = $s[4];
	$stats['protocol'] = $s[5];	/* Will be "UDP" or "TCP" */
	$stats['client_ip'] = $s[6];
	$stats['qclass'] = $s[7];		/* Always "IN" */
	$stats['qtype'] = $s[8];		/* "A", "MX", etc.. */
	$stats['name'] = $s[9];
	$stats['result'] = $s[10];	/* "NOERROR", "NXDOMAIN", etc. */
	$stats['success'] = ($s[10] == "NOERROR" ? 1 : 0);
	$stats['reason'] = str_replace("_", " ", $s[11]);
	$stats['query_count'] = $s[12];
	$stats['answer_count'] = $s[13];
	$stats['authority_count'] = $s[14];
	$stats['additional_count'] = $s[15];
	$stats['log_word'] = $s[16];
	$stats['cached'] = $s[17];
	$stats['opcode'] = $s[18];

	/* Get UPDATE description -- this field is quoted, so may appear as multiple fields */
	$stats['update_desc'] = $s[19];
	for ($x = 20; $stats['update_desc']{0} == '"' && $stats['update_desc']{strlen($stats['update_desc'])-1} != '"'; $x++)
		$stats['update_desc'] .= " " . $s[$x];

	/*
	**  Here's where you would do something with $stats
	*/
	foreach ($stats as $name => $value)
	{
		printf("%16s = %s\n", $name, $value);
	}

	echo "\n";
}

fclose($fp);

/* vi:set ts=3: */ ?>
