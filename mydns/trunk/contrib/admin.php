<?php
/*****************************************************************************
	$Id: admin.php,v 1.64 2005/04/29 15:24:01 bboy Exp $
	admin.php: Functional and useful example of a web-based DNS
				  administration interface.
	Jorge Bastos <jorge@mydns-ng.com> / Howard Wilkinson <support@cohtech.com>

	Copyright (C) 2002-2005  Don Moore <bboy@bboy.net>
	Copyright (C) 2005-2010  MyDNS Team
	

	This program is free software; you can redistribute it and/or modify
	it under the terms of the GNU General Public License as published by
	the Free Software Foundation; either version 2 of the License, or
	(at Your option) any later version.

	This program is distributed in the hope that it will be useful,
	but WITHOUT ANY WARRANTY; without even the implied warranty of
	MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
	GNU General Public License for more details.

	You should have received a copy of the GNU General Public License
	along with this program; if not, write to the Free Software
	Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
*****************************************************************************/

/*****************************************************************************
	CONFIGURABLE OPTIONS
*****************************************************************************/

/*
**  Set the following four variables to the hostname of your SQL server, the
**  username and password used to access that server, and the name of the
**  database where your MyDNS data resides.
*/
$dbhost = "localhost";
$dbuser = "root";
$dbpass = "password";
$dbname = "mydns";


/*
**  This script uses MySQL by default.  To use PostgreSQL instead, set
**  '$use_pgsql' to '1'.
*/
$use_pgsql = 0;


/*
**  The following two variables tell this script the name of your SOA
**  table and the name of your RR table.
*/
$soa_table_name = "soa";
$rr_table_name = "rr";

/*
**  Define this if you define RR types via foreign keys, not as enumerated
**  value in $rr_table_name table
*/
/*$rrtype_table_name = "rrtype";*/

/* limits on TXT record sizes */
$rr_maxtxtlen = 2048;
$rr_maxtxtelemlen = 255;

/*
**  The following two values configure the number of records shown per page
**  in the zone browser and the resource record editor, respectively.
*/
$zone_group_size = 25;
$rr_group_size = 20;


/*
**  This script can automatically update the serial number for a zone
**  whenever a client modifies any record in that zone.
**  Setting '$auto_update_serial' to '1' will enable this option.
*/
$auto_update_serial = 0;


/*
**  This script can automatically update PTR records when you modify,
**  add, or delete A records.  To enable this functionality, set
**  '$auto_update_ptr' to '1'.  If you enable this, be sure to fill in
**  the values for '$default_ns' and '$default_mbox', below, so that
**  new SOA records will have the correct information.
*/
$auto_update_ptr = 0;

/*
**  This can automatically insert the defaults w/o updating the serial
**  number automatically.  Sometimes you want to change entries but not
**  have the nameserver reload and start serving out new data.
**  -bek@monsterous.com
*/

$auto_defaults = 1;


/*
**  If this option is nonzero, this script will not complain if the
**  TTL for a record is set below the zone minimum.
**
**  Note that if $ttl_min below is nonzero, that value will still be
**  checked.
*/
$ignore_minimum_ttl = 1;


/*
**  The following values are used by this script to enforce minimum values
**  for SOA and RR records.  The script will prevent clients from entering
**  values lower than these numbers.
*/
$ttl_min = 300;
$refresh_min = 300;
$retry_min = 300;
$expire_min = 86400;


/*
**  The following two variables specify the default nameserver for new
**  SOA records, and the default administrator mailbox for new SOA records.
**  These will be filled in automatically whenever a new zone is created.
*/
$default_ns = "";
$default_mbox = "";


/*
**  The following array specifies default records for new SOA records.
**  These get inserted automatically whenever a SOA is inserted.
**  The format of each record is (name, type, aux, data).
*/
$default_records = array(
/*	array("", "NS",  0, "ns.example.com."), */
/*	array("", "MX", 10, "mail.example.com.") */
);


/*
**  The following five values will be used as default values whenever new
**  zones are created.
*/
$default_ttl = 86400;
$default_refresh = 28800;
$default_retry = 7200;
$default_expire = 604800;
$default_minimum_ttl = 86400;


/*
 * Set to the path of the zonenotify program
 */
$zonenotify = "";    # e.g. /usr/sbin/zonenotify

/*
**  The remainder of these variables enable cosmetic changes.
*/
$fontsize = 12;						/* Default font size (pixels) */

$font_color			=	"#663300";	/* Font color */

$page_bgcolor		=	"white";		/* Page background color */
$help_bgcolor		=	"#FFFFCC";	/* Main screen help box background color */
$soa_bgcolor		=	"#FFFF99";	/* SOA editor background color */

$list_bgcolor_1	=	"#FFFFCC";	/* List items #1 background */
$list_bgcolor_2	=	"#FFFFAA";	/* List items #2 background */

$query_bgcolor		=	"#FFFFCC";	/* Search query input background color */
$query_fgcolor		=	"black";		/* Search query input font color */

$input_bgcolor		=	"white";		/* Text input box background color */
$input_fgcolor		=	"black";		/* Text input box font color */

$highlight_color  =  "#E0E0E0";  /* Zone browser highlight color */

/**** End of configurable options *******************************************/






/**************************************************************************************************
	SHORT CONVENIENCE FUNCTIONS
**************************************************************************************************/
function esc($str) {
  global $use_pgsql;
  return ($use_pgsql ? pg_escape_string($str) : mysql_escape_string($str));
}

function nf($num) { return number_format($num); }

function ent($str) { return htmlentities($str); }

function s($num) { return (($num == 1) ? "" : "s"); }

function lastchar($str) { return substr($str, strlen($str)-1, 1); }

function ends_with_dot($str) { return (lastchar($str) == "."); }

function quote($str) { return "&quot;<TT>" . ent($str) . "</TT>&quot;"; }

function FailSQL($str) { ErrSQL($str); }

function getpostvar($name) {
  global $_GET, $_POST;
  if (isset($_POST[$name])) return $_POST[$name];
  if (isset($_GET[$name])) return $_GET[$name]; return '';
}

function postvar($name) {
  global $_POST;
  return (isset($_POST[$name]) ? $_POST[$name] : '');
}
/*-----------------------------------------------------------------------------------------------*/


/**************************************************************************************************
	OPEN_PAGE
	Start HTML output.
**************************************************************************************************/
function open_page() {
  global $soa_table_name, $fontsize, $soa_bgcolor, $page_bgcolor, $font_color, $highlight_color,
    $help_bgcolor, $input_bgcolor, $input_fgcolor, $query_bgcolor, $query_fgcolor;

  /* Color definitions... */
  $manila = "#FFFFEE";
  $pink = "#FF99CC";
  $brown = "#663300";
  $canary = "#FFFF99";
  $eggplant = "#660033";
  $green = "#00FFCC";
  $gray = "#666666";

  $title = "MyDNS";

  /* Get zone origin to use as title */
  if (isset($_POST['query']) && strlen($_POST['query']) && $_POST['action'] == "search")
    $title = $_POST['query'];
  else {
    $zone = getpostvar('zone');
    if ((int)$zone > 0)
      if (($res = sql_query("SELECT origin FROM $soa_table_name WHERE id=" . (int)$zone)))
	if ($row = sql_fetch_row($res))
	  $title = $row[0];
  }

  $query = getpostvar('query');

  ob_start();									/* Buffer output */
/* <!DOCTYPE HTML PUBLIC "-//W3C//DTD HTML 4.01//EN" "http://www.w3.org/TR/html4/strict.dtd"> */
/* <!DOCTYPE HTML PUBLIC "-//W3C//DTD HTML 4.01 Transitional//EN"> */
?>
<!DOCTYPE HTML PUBLIC "-//W3C//DTD HTML 4.01 Transitional//EN">
<HTML lang="en">
<HEAD>
   <META http-equiv="Content-Type" content="text/html; charset=US-ASCII">
   <TITLE><?php echo $title?></TITLE>
   <SCRIPT type="text/javascript">
   <!--
      function imgSwap(name, type) {
        eval("document." + name + ".src = '<?php echo $_SERVER['PHP_SELF']?>?img=" + name + type + ".png'");
      }

      function setFocus() {
        if (document.soaform)
          document.soaform.origin.focus();
        else
          document.searchform.query.focus();
      }
   -->
   </SCRIPT>

   <STYLE type="text/css">
       <!--
          A { color: blue; text-decoration: none; }
          A:hover { color: red; text-decoration: underline; }
          A.light { color: <?php echo $font_color?>; text-decoration: none; }
	  A.light:hover { color: red; text-decoration: none; }

	  TABLE { font: <?php echo $fontsize?>px sans-serif; }
	  UL { padding-left: 1.5em; }
	  IMG.noborder { border: 0px; }
	  TT { font: <?php echo $fontsize?>px monospace; }

	  BODY {
	     background: <?php echo $page_bgcolor?>;
	     color: <?php echo $font_color?>;
	     margin: 1% 3% 3% 3%;
	     text-align: center;
	  }

	  /*
	   **	Form styles
	   */
	  INPUT.mono {
	     background: <?php echo $input_bgcolor?>;
	     color: <?php echo $input_fgcolor?>;
	     border-width: 1px;
	     padding: 2px 4px;
	     font: <?php echo $fontsize?>px monospace;
	  }
	  INPUT.activeBox {
	     background: <?php echo $input_bgcolor?>;
	     color: <?php echo $input_fgcolor?>;
	     vertical-align: middle;
	     border-width: 1px;
	  }
	  INPUT.activeList {
	     background: <?php echo $input_bgcolor?>;
	     color: <?php echo $input_fgcolor?>;
	     vertical-align: middle;
	     border-width: 1px;
	  }
	  INPUT.recursiveBox {
	     background: <?php echo $input_bgcolor?>;
	     color: <?php echo $input_fgcolor?>;
	     vertical-align: middle;
	     border-width: 1px;
	  }
	  SELECT.rrTypes {
	     background: <?php echo $input_bgcolor?>;
	     color: <?php echo $input_fgcolor?>;
	     border-width: 1px;
	  }
	  .formButton {
	     font: bold <?php echo ($fontsize - 1)?>px sans-serif;
	     color: #FFFFCC;
	     background-color: #663300;
	     border-width: 1px;
	     border-style: solid;
	     vertical-align: middle;
	  padding: 2px 2px;
	  }
	  INPUT.hdrQuery {
	     background: <?php echo $query_bgcolor?>;
	     color: <?php echo $query_fgcolor?>;
	     border-width: 1px;
	     font: <?php echo $fontsize?>px monospace;
	     padding: 2px 4px;
	  }

	  /*
	   **	Top-of-page header
	   */
	  TABLE.hdrBox {
	     width: 100%;
	     border-bottom: #663300 solid 2px;
	     vertical-align: middle;
	     /* margin-bottom: 1em; */
	  }
	  TD.hdrInput {
	     padding: 0em 1em 0em 0em;
	     text-align: left;
	  }
	  TD.hdrCell {
	     padding: 0em 1em;
	  }
	  TD.hdrRight {
	     width: 100%;
	  }

	  /*
	   **	ErrBox()
	   */
	  TABLE.errBox {
	     background: #FFFF99;
	     border: black solid 5px;
	     padding: 0px;
	     width: 60%;
	     margin: 1em;
	  }
	  TD.errLeft {
	     vertical-align: top;
	     padding: 0.5em 1em 0.5em 0.5em;
	     border-right: black solid 2px;
	  }
	  TD.errRight {
	     vertical-align: middle;
	     padding: 0.5em 0.5em 0.5em 1em;
	  }


	  /*
	   **	Warn()
	   */
	  TABLE.warnBox {
	     background: #FFFF99;
	     border: black solid 5px;
	     padding: 0px;
	     width: 60%;
	     margin: 1em 0em;
	  }
	  TD.warnLeft {
	     vertical-align: top;
	     padding: 0.5em 1em 0.5em 0.5em;
	     border-right: black solid 2px;
	  }
	  TD.warnRight {
	     vertical-align: middle;
	     padding: 0.5em 0.5em 0.5em 1em;
	  }

	  /*
	   **	Notice()
	   */
	  TABLE.noticeBox {
	     background: #FFFF99;
	     border: black solid 5px;
	     padding: 0px;
	     width: 60%;
	     margin-bottom: 1em;
	  }
	  TD.noticeLeft {
	     vertical-align: top;
	     padding: 0.5em 1em 0.5em 0.5em;
	     border-right: black solid 2px;
	  }
	  TD.noticeRight {
	     vertical-align: middle;
	     padding: 0.5em 0.5em 0.5em 1em;
	  }

	  /*
	   **	RR editor
	   */
	  DIV.rrBox {
	     border: <?php echo $gray?> solid 1px;
	     margin: 1em 0em;
	  }
	  TABLE.rrBox {
	     margin: 0px;
	     padding: 0px;
	     border: 0px;
	     width: 100%;
	  }
	  TD.rrCell {
	     padding: 7px 4px;
	     margin: 0px;
	  }
	  TD.rrCellLeft {
	     padding: 10px;
	  }
	  TD.rrCellRight {
	     padding: 7px 14px;
	     text-align: left;
	     width: 100%;
	  }

	  /*
	   **	SOA editor
	   */
	  TABLE.soaBox {
	     background: <?php echo $soa_bgcolor?>;
	     margin: 0px;
	     padding: 10px 10px 6px 10px;
	     border: <?php echo $gray?> solid 1px;
	  }
	    TABLE.inactivesoaBox {
		background: #FF4500;
		margin: 0px;
		padding: 10px 10px 6px 10px;
		border: <?php echo $gray?> solid 1px;
		}
	  TD.soaFirstRow {
	     vertical-align: middle;
	  }
	  TABLE.soaFields {
	     margin-left: 3em;
	     vertical-align: middle;
	  }
	  TD.soaFields {
	     padding-top: 5px;
	  }

	  /*
	   **	Zone transfer access list
	   */
	  TABLE.xferBox {
	     background: <?php echo $soa_bgcolor?>;
	     padding: 4px 8px;
	     margin: 0px;
	  }

	  /*
	   **	Update access control list
	   */
	  TABLE.updateAclBox {
	     background: <?php echo $soa_bgcolor?>;
	     padding: 4px 8px;
	     margin: 0px;
	  }

	  /*
	   **	Also notify list
	   */
	  TABLE.alsoNotifyBox {
	     background: <?php echo $soa_bgcolor?>;
	     padding: 4px 8px;
	     margin: 0px;
	  }

	  /*
	   **	Zone browser
	   */
	  TABLE.browserBox {
	     width: 80%;
	     background: #FFFFCC;
	     border: #663300 solid 1px;
	  }
	  TD.browserCellLeft {
	     font: <?php echo $fontsize?>px monospace;
	     text-align: left;
	     padding: 2px 5px;
	  }
	  TD.browserCellRight {
	     font: <?php echo $fontsize?>px sans-serif;
	     text-align: right;
	     padding: 2px 5px;
	  }
	  SPAN.highlight {
	     background: <?php echo $highlight_color?>;
	  }

	  /*
	   **	offset_select()
	   */
	  TABLE.offsetBox {
	     margin-bottom: 10px;
	  }
	  TD.offsetLeft {
	     vertical-align: middle;
	     text-align: left;
	     padding-right: 1em;
	  }
	  TD.offsetRight {
	     vertical-align: middle;
	     text-align: right;
	     padding-left: 1em;
	  }
	  TD.offsetCenter {
	     font: <?php echo ($fontsize - 1)?>px sans-serif;
	     vertical-align: middle;
	     text-align: center;
	     padding: 2px 20px;
	  }
	  DIV.offsetTop {
	     font: bold <?php echo $fontsize?>px sans-serif;
	  }

	  /*
	   **	Box displayed by the zone editor if no offset browser is used
	   */
	  .zoneHdr2 {
	     font-weight: bold;
	  }

	  /*
	   **	Help/welcome page
	   */
	  DIV.helpBox {
	     background: <?php echo $help_bgcolor?>;
	     border: #663300 solid 1px;
	     padding: 1em 1em 0em 1em;
	     width: 80%;
	     text-align: left;
	     font-family: sans-serif;
	     margin-top: 3em;
	  }
	  TD.helpBtnLeft {
	     text-align: right;
	     vertical-align: middle;
	  }
	  TD.helpBtnRight {
	     text-align: left;
	     vertical-align: middle;
	  }
	  .revBox {
	     background: <?php echo $page_bgcolor?>;
	     border: #660033 solid 1px;
	     padding: 1em;
	     margin: 1em;
	  }
       -->
    </STYLE>
</HEAD>
<BODY onLoad="setFocus()">

<!-- BEGIN page header (query form, etc) -->
<FORM method=POST action="<?php echo $_SERVER['PHP_SELF']?>" name="searchform">
<DIV><INPUT type=hidden name="action" value="search"></DIV>
	<TABLE class=hdrBox>
		<TR>
<!--			<TD><?php
	if (strlen(ent($query)))
	  echo "<A href=\"{$_SERVER['PHP_SELF']}?action=search&query=" . ent($query) . "\">" .
	    "<IMG name=\"refresh\" src=\"{$_SERVER['PHP_SELF']}?img=Refresh.png\" alt=\"REFRESH\" title=\"Click this to refresh zone\" class=noborder></A>";
else
  echo "&nbsp;";
?></TD> -->
			<TD class=hdrInput><INPUT class=hdrQuery type=TEXT name="query" value="<?php echo ent($query)?>" size=25
				title="Enter zone or FQDN to view/edit DNS records.">

			<TD class=hdrCell><A href="<?php echo $_SERVER['PHP_SELF']?>?action=browse" onMouseOver="imgSwap('browse','Hover')"
				onMouseOut="imgSwap('browse','Reg')"><IMG name="browse" src="<?php echo $_SERVER['PHP_SELF']?>?img=browseReg.png"
				alt="BROWSE" title="Click this to load the zone browser" class=noborder></A>

			<TD class=hdrCell><A href="<?php echo $_SERVER['PHP_SELF']?>?action=new" onMouseOver="imgSwap('add','Hover')"
				onMouseOut="imgSwap('add','Reg')"><IMG name="add" src="<?php echo $_SERVER['PHP_SELF']?>?img=addReg.png"
				alt="NEW" title="Click this to add a new zone" class=noborder></A>

			<TD align=right class=hdrRight><A href="<?php echo $_SERVER['PHP_SELF']?>"><IMG src="<?php echo $_SERVER['PHP_SELF']?>?img=Logo.png"
				alt="HOME" class=noborder></A>
	</TABLE>
</FORM>
<P>
<!-- END page header -->

<?php
}
/*--- open_page() -------------------------------------------------------------------------------*/


/**************************************************************************************************
	CLOSE_PAGE
	Close the HTML document.  Exit.
**************************************************************************************************/
function close_page() {
?>
</BODY>
</HTML>
<?php
  ob_flush();
  exit;
}
/*--- close_page() ------------------------------------------------------------------------------*/



/**************************************************************************************************
	GETBOOL
	Returns 1 for "[Yy](es)", "[Tt](rue)", "1", "[Aa]ctive", or "(o)[Nn]", else 0.
**************************************************************************************************/
function getbool($str) {
  $c1 = strtoupper(substr($str, 0, 1));
  $c2 = strtoupper(substr($str, 1, 1));
  if ($c1 == 'Y' || $c1 == 'T' || $c1 == '1' || $c2 == 'N' || $c1 == 'A')
    return 1;
  return 0;
}
/*--- getbool() ---------------------------------------------------------------------------------*/

/**************************************************************************************************
	GETTRINARY
	Returns 2 for "[Dd](eleted)"
	Returns 1 for "[Yy](es)", "[Tt](rue)", "1", "[Aa]ctive", or "(o)[Nn]", else 0.
**************************************************************************************************/
function gettrinary($str) {
  $c1 = strtoupper(substr($str, 0, 1));
  $c2 = strtoupper(substr($str, 1, 1));
  if ($c1 == 'D') return 2;
  if ($c1 == 'Y' || $c1 == 'T' || $c1 == '1' || $c2 == 'N' || $c1 == 'A')
    return 1;
  return 0;
}
/*--- gettrinary() ---------------------------------------------------------------------------------*/


/**************************************************************************************************
	DURATION
	Given a number of seconds, returns a human-readable representation of the amount of time it
	represents.
**************************************************************************************************/
function duration($seconds) {
  $weeks = floor($seconds / 604800); $seconds -= ($weeks * 604800);
  $days = floor($seconds / 86400); $seconds -= ($days * 86400);
  $hours = floor($seconds / 3600); $seconds -= ($hours * 3600);
  $minutes = floor($seconds / 60); $seconds -= ($minutes * 60);
  $rv = '';
  if ($weeks) $rv .= "$weeks" . "w";
  if ($days) $rv .= "$days" . "d";
  if ($hours) $rv .= "$hours" . "h";
  if ($minutes) $rv .= "$minutes" . "m";
  if ($seconds || !strlen($rv))
    $rv .= "$seconds" . "s";
  return ($rv);
}
/*--- duration() --------------------------------------------------------------------------------*/


/**************************************************************************************************
	GETTIME
	Given a string, translates into seconds.
	Accepts syntax like "20h32m" or "4 days, 3 minutes" or whatnot.
**************************************************************************************************/
function gettime($str) {
  $str = trim($str);

  /* No strings - treat as straight seconds */
  if (strspn($str, "0123456789") == strlen($str))
    return (int)$str;

  for ($n = $secs = 0, $current = ""; $n < strlen($str); $n++) {
    $c = substr($str, $n, 1);
    if ($c < '0' || $c > '9') {
      switch (strtolower($c)) {
      case "w":
	$secs += (int)$current * 604800; $current = "";
	break;
      case "d":
	$secs += (int)$current * 86400; $current = "";
	break;
      case "h":
	$secs += (int)$current * 3600; $current = "";
	break;
      case "m":
	$secs += (int)$current * 60; $current = "";
	break;
      case "s":
	$secs += (int)$current; $current = "";
	break;
      }
    } else
      $current .= $c;
  }
  return $secs;
}
/*--- gettime() ---------------------------------------------------------------------------------*/


/**************************************************************************************************
	T
	Title output.  IE will honor linefeeds in a "title" attribute; Mozilla won't.  Strip linefeeds
	for Mozilla.
**************************************************************************************************/
function t($text) {
  $ua = $_SERVER['HTTP_USER_AGENT'];

  if (!strstr($ua, "MSIE"))
    return str_replace("\n", " ", $text);
  return $text;
}
/*--- t() ---------------------------------------------------------------------------------------*/


/**************************************************************************************************
	NEXT_SERIAL
	Attempts to guess the next serial number based on the current serial number.
**************************************************************************************************/
function next_serial($serial = NULL) {
  $now = time();
  $low = $now - (31449600 * 2);					/* Time 2 years ago */
  $high = $now + (31449600 * 1);				/* Time 1 year hence */
/* date_default_timezone_set avoid warning about It is not safe to rely on the system's timezone settings. */
  date_default_timezone_set('UTC');
  $curyear = (int)strftime("%Y", $now);				/* The current year */
  $timestamp = strftime("%Y%m%d", $now);			/* The current timestamp */

  if (!$serial)
    return $timestamp . "01";

  /* If it looks like a Unix timestamp, return the current timestamp */
  if ($serial >= $low && $serial <= $high) {
    if ($now == $serial) $now++;
    return $now;
  }

  /* If the first four characters look like a year, suggest a YYYYMMDDnn type format */
  $first = substr($serial, 0, 4);
  if ($first >= ($curyear - 10) && $first <= ($curyear + 1)) {
    $compare = substr($serial, 0, 10);
    for ($n = 1; $n < 3650; $n++)
      if ((int)($rv = $timestamp . sprintf("%02d", $n)) > (int)$compare)
	return $rv;
    return $timestamp . "01";
  }

  /*
   * If the current serial is a number that's less than the unix time 10 years ago, assume
   * the serial is a simple number being incremented
   */
  if ($serial < $low)
    return $serial + 1;

  /* Otherwise return the "timestamp of choice" - YYYYMMDDnn */
  return $timestamp . "01";
}
/*--- next_serial() -----------------------------------------------------------------------------*/


/**************************************************************************************************
	AUTO_NEXT_SERIAL
	Returns a good "next" serial number based on '$auto_update_serial'.
**************************************************************************************************/
function auto_next_serial($current) {
  global $auto_update_serial;

  switch ($auto_update_serial) {
  case 1:								/* YYYYMMDDnn (RFC1912) */
    $serial = date("Ymd01", time());
    while ($serial <= $current) $serial++;
    return ($serial);

  case 2:								/* Unix timestamp */
    $serial = time();
    while ($serial <= $current) $serial++;
    return ($serial);

  default:
    return ($current);
  }
}
/*--- auto_next_serial() ------------------------------------------------------------------------*/


/**************************************************************************************************
	VALIDATE_NAME
	Validates a name:
		-	Name must be 255 octets or less.
		-	No label can be longer than 63 octets.
		-	If wildcard is present, it must be the first label.
		-	Labels may not begin or end with a hyphen.
		-	Labels must be alphanumeric (plus hyphens/underscores).

	Returns the number of errors found.
**************************************************************************************************/
function validate_name(&$name, $desc_prefix, &$errors, $wildcard_ok, $origin = NULL,
		       $allow_backslash = 0) {
  /* List of valid characters for a label */
  $valid_chars = "*ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz1234567890-_";

  /* Backslash may be appropriate in the mbox field, for folks with a dot in their username */
  /* For example, "bleach\.boy.bboy.net" would mean "bleach.boy@bboy.net" */
  if ($allow_backslash)
    $valid_chars .= '\\';

  $desc = "$desc_prefix (" . quote($name) . ")";
  $errct = 0;

  /* Name too long */
  if (strlen($name) > 255) {
    $errors[] = "$desc is too long.";
    $errct++;
  }

  $labels = explode(".", $name);
  $which = 0;
  foreach ($labels as $label) {
    $which++;

    /* Label too long */
    if (strlen($label) > 63) {
      $errors[] = "$desc contains a label (&quot;<TT>$label</TT>&quot;) which is too long.";
      $errct++;
    }

    /* Label contains only legal characters */
    if (strspn($label, $valid_chars) != strlen($label)) {
      $errors[] = "$desc contains a label (&quot;<TT>$label</TT>&quot;) with invalid characters.";
      $errct++;
    }

    /* If we're allowing backslashes, they may only appear at the end of a label */
    if ($allow_backslash && (($p = strpos($label, '\\')) !== false) && ($p < (strlen($label) - 1))) {
      $errors[] = "$desc contains a label (&quot;<TT>$label</TT>&quot;) with invalid characters -- backslash must immediately precede a dot.";
      $errct++;
    }

    /* Label may not begin/end with a hyphen */
    if (substr($label, 0, 1) == '-') {
      $errors[] = "$desc contains a label (&quot;<TT>$label</TT>&quot;) that begins with a hyphen.";
      $errct++;
    }
    if (lastchar($label) == '-') {
      $errors[] = "$desc contains a label (&quot;<TT>$label</TT>&quot;) that ends with a hyphen.";
      $errct++;
    }

    /* Check wildcards */
    if (strstr($label, "*")) {
      if (!$wildcard_ok) {
	$errors[] = "$desc may not contain wildcard labels.";
	$errct++;
      }

      if ($which != 1) {
	$errors[] = "$desc contains a wildcard in a non-initial label.";
	$errct++;
      }

      if ($label != "*") {
	$errors[] = "$desc contains a label (&quot;<TT>$label</TT>&quot;) mixing a wildcard character with other data.";
	$errct++;
      }
    }
  }

  /* If an origin was specified, append a dot to '$name' if it ought to be there */
  if ($origin) {
    $origin_without_dot = substr($origin, 0, strlen($origin)-1);
    $end_of_name = substr($name, strlen($name) - strlen($origin_without_dot));

    if (!strcasecmp($origin_without_dot, $end_of_name))
      $name .= ".";
  }
  return $errct;
}
/*--- validate_name() ---------------------------------------------------------------------------*/


/**************************************************************************************************
	CHECK_VALUE32
	Checks a 32-bit unsigned value to make sure it's in range.  Returns the number
	of errors found.
**************************************************************************************************/
function check_value32($num, $desc, &$errors, $minimum_value = 0) {
  $errct = 0;
  if ($num > 4294967295) {
    $errors[] = "$desc (" . quote($num) . ") exceeds the maximum value for an unsigned 32-bit integer.";
    $errct++;
  }
  if ($num < 0) {
    $errors[] = "$desc (" . quote($num) . ") may not be negative.";
    $errct++;
  }
  if ($minimum_value && ($num < $minimum_value)) {
    $errors[] = "$desc (" . quote($num) . ") must be at least $minimum_value.";
    $errct++;
  }
  return $errct;
}
/*--- check_value32() ---------------------------------------------------------------------------*/


/**************************************************************************************************
	SEND_IMG
	This sends an image, so that users don't have to install extra graphics.
	The binary data you see here is the PNG images used by this script.
**************************************************************************************************/
function send_img() {
  if (!isset($_GET['img']))
    return;

  switch ($_GET['img']) {
  case "addHover.png":
    $imgdata = "89504e470d0a1a0a0000000d494844520000003c00000011080600000081cecf9a0" .
      "0000006624b474400ff00ff00ffa0bda793000001634944415478daed57c1718330105c65fc" .
      "773a48d20199e10feec01d383f3d6357105241f2d58f12d441c45f0fd281d3015470fec08c8" .
      "7d109217026197b7fdc9dd02db7a7138288704db8c39561e532ca54144cfc51592a47624a00" .
      "3980c789b91c013400129753592a642a1200db814b2b4bb5271fa32c99fe41b8242d53e1d3f" .
      "94659329e980d800240369170054003f860fc4f005e00bc0decef000c802f66ddb3b254cf91" .
      "742953717f21c5198f2fe9943344cea902407b4e3696f04357c1c5d125f7e3219c31768eb00" .
      "eeae100bc46aeab94a53ca0ca3b877dcfc4af99ca3b15b3c2ef2291a9e0645b2b4bfbae2a3b" .
      "86984f7517ad702cd60187995968af6f65a9f9f373b84bb21a096b035ea583e7b00707cfd85" .
      "8aa87fb2a6723fec4236556295309d73349679ef97dfe31b463de0ec9344caff7e3c82c72b5" .
      "54963e0324b7c4786a47089b987320b687b7817d34077ae4326122d6baaf96b7bfa51be1ff8" .
      "b13f7957c14621a46f90000000049454e44ae426082";
    break;

  case "addReg.png":
    $imgdata = "89504e470d0a1a0a0000000d494844520000003c00000011080600000081cecf9a0" .
      "0000006624b474400ff00ff00ffa0bda7930000015e4944415478daed97cd6d834010853f47" .
      "be3b1d24e980680bc0eec01d38a7bdc6a920a482e4cc8912e8204b0148a403a703a8801c0c9" .
      "2857696e5c75122fbdd989965e7316f7696455dd75c126eb8302cadd63c8e84f8034a273d31" .
      "09b006ee07e672004a20b07a958ec8e300d8763c294a178e7c0c4a9bf6616195741ebb74be4" .
      "169e388d90011100e249c0129f02ef81f8027e0b5637f030cf029ac7b44e9628aa413f2f8f6" .
      "4c8a330e5fd028a78bb5a80aa84ec98e257cd754707e1c93fb76100e05bb4438f5ebe17e3c8" .
      "f5c97a1f4daa3ca3b8b7d2fc4af84ca5b15b3e4771190c7926c0b94de3755d909c45caa3b6b" .
      "85c762e571989999f6fa42e9f2efcfe16392594f54e5f1a6d47f0ecb78718c8db97ab8ad72d" .
      "8e30f1c521695329470319174e898dfa71f23b5ccdb2e9952e8f5761c9979ae964a7f78486e" .
      "8ef154f5103663ce81b13dbcf5eca329487b2e1366c45ae16a79fd5bba12feb7f801964062f" .
      "1144f28690000000049454e44ae426082";
    break;

  case "browseHover.png":
    $imgdata = "89504e470d0a1a0a0000000d4948445200000067000000110806000000c03ab2250" .
      "0000006624b474400ff00ff00ffa0bda793000002eb4944415478daed594b6edb30107d2cba" .
      "976f60f504d6827b2b27a86e1075c565d413d8bd81bbd4caca09a29cc0f29e40e41bc837904" .
      "fc06e46002b70a84fdcc0453c80e018d690c379336f6618618cc15d6e53bedc5d70bbf2d5fe" .
      "a2a48801c413d7680054b9368d922204904ed4af49bfedffa0a488002400427afa7a0d8032d" .
      "7a6e9bdef9202400b20ebaf936b532a2952c71e00d0e4da14b47e022062eca92c7fd4b4573a" .
      "0393a23b8fb0694d49b105b09909f40f32ec3043f70220ceb5a92d2717005623f55fc911218" .
      "0b789f61d736d622505c7ef3f49af00108cb4e708603bd3170fb936d5b5696dfb0edda08b3c" .
      "02e66d023000f0ddcaa40bf34ec4b0c29af6e4a405f03201987f436b03f28b3e13c671cb01f" .
      "d131d34620e1a10adee3c6b9c012c18fd25054809e091018793d4b39f8fe67fd3996051dde2" .
      "c3c1c9b5d952642f18709e0796c8726d2ae2ed17e69d8527635e736d12dabf66822121805ce" .
      "0ac3d59c5d5a98aa9435ced2dbbfa498136485d570147495159877401930d44e750049fac28" .
      "e40a3ae8e0258027267b4a007b0f7d6242d69704dc9af9dd65c345499111589cec9414ad278" .
      "8eba9b4b6f6a4763342ffa0a4f015d06400dc96f9bb9fe1ad92e234b166c1933935d936b6e6" .
      "0444cd89e71d9f6d8b3935e768457ee0889ed4d1a64eedd2ea818cab46645f17f1ab117bfb1" .
      "c7ea4f6beb54684b85753d8fa3963247957cd89ad59e8c044433471ff0040a1a488736d5a8a" .
      "fa3343335ba28290ba336e66ea227e33000cd738fc35b7f466b77eb392796c19d52e5fbbe68" .
      "4330381a39a151d3ab55af23d03e47e4c3b4f8d872f332a7a7ce094d6793ba0371318c11720" .
      "3e8a9f35e7ace9597a9cefa3a5cca29dbe3c2a2976e4d86244e7e71c32bbc9ba37b1fbc0619" .
      "ddb51ed4cc97005b9c6107aa2a6600cc7a69e76f689ae50906b930278a0c9ff3230f73c03f8" .
      "d65db13822df090e01791ec89aaef9384fb8a97870d8324bc4ff702b4dbc1f3aeec4da0fb62" .
      "36286cca697b5f834e07c56b9ffcbe086e50f38fa1d985dabe6370000000049454e44ae4260" .
      "82";
    break;

  case "browseReg.png":
    $imgdata = "89504e470d0a1a0a0000000d4948445200000067000000110806000000c03ab2250" .
      "0000006624b474400ff00ff00ffa0bda793000002e64944415478daed99d151db4010863f32" .
      "79973bc0a900cfa8008b0aa20e505ef48a53014e07ceebbd202a40548028e006b903bb03510" .
      "179d08ac8b6f6a4130e43067646633cd6deeddebffbefee71f2fcfccca7bc4ff9f27904ef57" .
      "beee7cb3260222cf35364041986eb0660a249efaa5e85707bf58330362602acfbede06c809d" .
      "3cddefb5d920115b03858274c73ac493af6a8fd0bd34cd68f8199624fd13a8f52f64a466092" .
      "35fe9cecd09a354be06a24d03fc4b0fb11ba4f40449896ad43ce80b381fa77721053e0d1d3b" .
      "e07c234c21a8ddf7f8a5e060403ed79009623cfe29c302d8e4d6bcb57e8062f915703f3e801" .
      "0cc0f756263d29efcc145698cb9e9a54c0ad0730ff88d6dcf24b3e63e5e04e7bf4d7e2e84c7" .
      "134105a5d39d6d8021345ff540224072e147034491cfbb968feb7f8448bea266f0f4e982e25" .
      "b2270a38373d2b2c08d34278fb567967e2c8983bc23496fd4b25186201e8a23343f4acd2ea5" .
      "4a1d421adf6e62ff5b30eb45eea3a0e38d6142d27bb8059f444675f04af5b51a81574c4f11c" .
      "b854b22707ae1df48947d6e702dc5cf9fdb2b37e5ab310b03459614de508e2d297d6e68ed4d" .
      "e0cd0bfc71a57018d7bc0ad94bff733bcc29ab567cdc29139a5d836b4e60442cdb1e31d976d" .
      "933135e7a115f94147f4241d6daa6f9756f6645c3120fb9a883f1bb077e0f4b76eefabd6881" .
      "0edd514bd7efa8f24afaa39516b16ba57a261e6b97f0064581311a69544fd56a199a550c154" .
      "ba336d666a22feaa0718ad71d89d5b7667b7d50105e9b60c6a978f5d73a6230341a39a33713" .
      "a69b5e4d70a90d783daf9baf1706546218f0b9cbce56f03f4950723b802c445f1a3e69cb93c" .
      "a78ec32f9dddda5fdad9970bac59c9c166033abfee21b3b929d89dd85de0e887db50ed38597" .
      "00439c610ba96a66008c7268e76f652ae50204c13e05c26ffa79eb9e706f8f672c57218f9dd" .
      "e0d4406e7bb2a6693eb61e3715e71db68c9293ffe256bae6fd69c79d58f5c676cc942173b39" .
      "7b57c1c703ea87cfecbe01dcb1f4acbfd1f07f073670000000049454e44ae426082";
    break;

  case "EndHover.png":
    $imgdata = "89504e470d0a1a0a0000000d494844520000000e0000000c080600000052808cda0" .
      "0000006624b474400ff00ff00ffa0bda793000000d84944415478da9dd2ad4a44511486e167" .
      "1f4650ef41ac83c9b0c15bf03a2c1bd43b98a4a0c968d956eb18a7192c82c8c1aa62330d862" .
      "91afc655b46190ece1cf58315d6c77a61fd85528affa80329867be45c97bd3620c530c2a01a" .
      "e743f4520cd72986f516f6056bd544b2802e8e520c272986d529e0fb77ab0d2d8d6325c530c" .
      "041aecbb05954cd68a98b2d5ca418b6ff02c23c96b19f62d898f0e73abfd8fc1d0ed19ff03e" .
      "66818f38c64eaecbc38f776ce80997d8cc75b99df90058c41baeb08bd35c97d7294c85ea0b3" .
      "cc7197ab92ecf2d33df60f40993023938ad153c8c0000000049454e44ae426082";
    break;

  case "EndReg.png":
    $imgdata = "89504e470d0a1a0a0000000d494844520000000e0000000c080600000052808cda0" .
      "0000006624b474400ff00ff00ffa0bda793000000cf4944415478da9dd2ad4a44511486e1c7" .
      "cd08ea3d887530093b780b5e876107f50e262968325adcd53ac669068b20c2b6aad84c83618" .
      "a067f399611cf88738efac10aeb63bdb0fe66aaaaf21f7540c9773814d36e2b51f20883304e" .
      "87e829f94ac96b2de83356432d99471759c9c74a5e9902be7db53aa9c5712c2b79807d310db" .
      "f17858696bad8c4b992b7fe02c21c96b0a7e4f59a3fdbf9c5e66f71807ecd7b6f021f70846d" .
      "31ddff7cc7493de2021b62ba697e0016f08a4bece0444c2f539880f0099ee1143d313db5cc7" .
      "c8dd107a68535996937ec830000000049454e44ae426082";
    break;

  case "Error.png":
    $imgdata = "89504e470d0a1a0a0000000d494844520000001e0000001e08060000003b30aea20" .
      "0000006624b474400ff00ff00ffa0bda7930000016b4944415478dae596bd4ac35018869fd4" .
      "34e05cbbeb5807bb88e86271ea0db875d0ba14111dbc14f1163a76eb15d4492188881841eb0" .
      "dd8b5a0b47e2e39e518923649cfc9d2032f9c21e139efc7f707ab769c2c1f0b6c020da0066c" .
      "84ff8f80001838f06eec65026b022d817b0159a04781b680bb2c7447c04f018cea5960372ff" .
      "458609c03aaf42d7092073a5902aaf42b709a25bc630350ddf97e9a44f20d42955e05bc79e0" .
      "9605a8d2f93cf08345702009d02d8b50a5bae2953476a3804e79a82e7a87a901d06c42b76b1" .
      "6d7e940af07b01d07ae02502e43a56216ec79ff1991503b454e27ddf1d7ec369d9aa5c82c9f" .
      "4771e000807e1f5cd796d1202ed47705447890d4409e2cd6f0876879548ab06f2cbabd75409" .
      "21c97055e2cb81d0aac2f9a507be12833059d081ca59dc9ed70889b005f66dd42ce047e9674" .
      "7a9577ef3a1078cb01fd4c1dde39704fe022e5038602d70b13294b7f0e6bb0ae2df455ad0d0" .
      "66173f0134b66e5cf1fa0cf8d6116a7b1ea0000000049454e44ae426082";
    break;

  case "ErrorSmall.png":
    $imgdata = "89504e470d0a1a0a0000000d49484452000000100000001008060000001ff3ff610" .
      "0000006624b474400ff00ff00ffa0bda793000000d44944415478daad933d0ac2401085bfd5" .
      "80de41308285b7b0b010ecf416561e219d07b0f418360aa2883f8577b012143c4284c8b3d01" .
      "589266ed481c7c2c0b73b33fb067e0c134fe876d4813650bba777c0085898b4db0425c14ca0" .
      "042d05e534789f025b1d04fe4bd982b9036cb5d673fb827a06d8aa09e0ddefe890cf83e7b98" .
      "dfe7281286a03535bc1584120e7e8f7259803e4fee203c18062b147a1e0469dcf108643035d" .
      "db42e38b21b6e2dfb8ca006f1577b1c0171c1de093a09ae4c68a60f3e1e5eaa76532dc4cf26" .
      "e9926e6b16f7f8a2b6303fdb29ced81f00000000049454e44ae426082";
    break;

  case "HomeHover.png":
    $imgdata = "89504e470d0a1a0a0000000d494844520000000e0000000c080600000052808cda0" .
      "0000006624b474400ff00ff00ffa0bda793000000cf4944415478da9dd0214b045114c5f1df" .
      "c8b088613469113169d122cb7e02a368114ca26d8b49102c468bd629aff8252c7e8817b62f1" .
      "82c6a31095ae459de04879d61f1c02d87f3bf877b8b9492ffa884f1b0a8b119623ae80b8f87" .
      "c522ceb053666f84f51e60807d5c62179f0df8d3b4cf80b6719f97af65fba3ec6959c5354eb" .
      "194e7ef8d2da0c2092eb08195cee7b4748cbb2ea0d142db08313de006aff89e1bcc708d3dd4" .
      "78991bccf05b88e90a4778c47b17d8d53e09311de21c537ca16cc2cfa8fa9e11627a0a316de" .
      "116cbbf5ddd341bd7fb2a220000000049454e44ae426082";
    break;

  case "HomeReg.png":
    $imgdata = "89504e470d0a1a0a0000000d494844520000000e0000000c080600000052808cda0" .
      "0000006624b474400ff00ff00ffa0bda793000000cf4944415478da9dd2214b436114c6f1df" .
      "e432c4304db38c617265165ff0132c0e2d03d3d0f4269320ac2c5ab4deb4e097b0f8250676c" .
      "1b0a296a58116b986dd2bcc712fc3074e7938fff3700ea7966599ff2801d3498a0321f62bbb" .
      "a7936d5ca09be4d6095a15401d3d5ce3088b02fcfe4d5f873ab8cf87efe7ee3ca9486962842" .
      "176f2fab3e32ad0c039aed0c65ef9715635c05d1950686bcd09f10163bce16b737009a73846" .
      "8ad9e6e0127e17e20dcef0888f32b02cfd5988a7b8c40b3e9114cdaf68547e4d884f423cc42" .
      "d767f00d36928ba8752bfb80000000049454e44ae426082";
    break;

  case "Info.png":
    $imgdata = "89504e470d0a1a0a0000000d494844520000001e0000001e08060000003b30aea20" .
      "0000006624b474400ff00ff00ffa0bda793000001f14944415478dae5963d4fdb4018c77f06" .
      "878581a105b1205623e884044c112f1f842c14982a763e0242adbdd00ef011d8985106d4ccc" .
      "47136985190581828827f073bad63ce2f38e0a17da493cf39dff3bbdcf30aff9b58c53f15c0" .
      "1c508f9e1fa2855b20009ae1d37aabb3a906da025d8294332ea36f6bc34297409d24607d5df" .
      "23c697757b26de301fc706f396803f49054bab6263d3de98f1c1ea6fefb07d06619e8b349a1" .
      "eb6a40aeaf33affe390d3e62802e03dfd31ccff707df3b9d5ce7fd51e0da5533d9343e4647a" .
      "58303e9ea4a3a3b936666721dae6ff32c87d3e7024aca8ead943816401b98371d696303f6f7" .
      "5ffe7e7a0aae5bc871dac0a73ed28e2dcca74101a6a66075d5a0ad5dd86317a2c41324c1f5a" .
      "c5d3737707e1ecea7a7c1714ac568bd0f8edbf75b517b351a7fc3c9f35e65e7afa6709aaca0" .
      "367ccc88e36a240eee55c0eb99c04105e0ae09dcac00dc34817da0f38ed001fd238924e6bd2" .
      "3d88d274a3bb1780c7c015ea407c781f1f1703e3b1b8bc149585c0ce7f7f7d0ed1aa1017092" .
      "57a15640bf92c1df6a29572e2e521b82e502f5d8fa09ecf4abc6b0bd53a8cb6a2517ec94fa7" .
      "d1c718f801ac0de1e4c4c6453eeee065e1f816db04eca1c78051494a8bd41b877b8db1a03ed" .
      "e47526b14e63bb487bfbda867e212a6d4e2ce1f7620dbdff860dfd3f26bf01cfc5dd7f1b25f" .
      "7720000000049454e44ae426082";
    break;

  case "InfoSmall.png":
    $imgdata = "89504e470d0a1a0a0000000d49484452000000100000001008060000001ff3ff610" .
      "0000006624b474400ff00ff00ffa0bda793000001034944415478daa593216e82411046dfa2" .
      "711ca0a82a2448046d7d6b518443c00d089ab497c051d992568045120406c37f00824190bc0" .
      "af8c9427f68a0934cb299ccbcf9766707fe69e177c8003c002fc0fd3e380706c037042ff0bc" .
      "0347a0f9bcf67ada6c2a1c7cb4cb395fbc4c93eb7555dd6c348423c8320362483ba75e2868b" .
      "fafedf65171ac2400e4f68447a09ae24a2568b560b180ed36536e75ff4e07056f71875a4d27" .
      "135dad7436cb5420f81a033eb39286c38b808ff80a375b2e9af3b5368f01831b00efa7631ca" .
      "7f7ab54b4d1d0e9549364772e97b3c718438a6002daedea7a7dec9dcea138b9f41b8bb1920c" .
      "1f9f169f5ba627e0396399befe58a6ebed0734a0ece44e66626b0000000049454e44ae42608" .
      "2";
    break;

  case "JumpNextHover.png":
    $imgdata = "89504e470d0a1a0a0000000d49484452000000150000000c08060000008dcef6c50" .
      "0000006624b474400ff00ff00ffa0bda793000001334944415478daadd3bd4a5d411885e167" .
      "0e0a492736a6090454102b8bb9084b499736cd1436da444844bb63931bd8098856368a76b90" .
      "249223b048460a75829a9fcc37f189b7d02cad9fb586435d3bcdf9a35336b42ced9ff561fa4" .
      "180ef0b52873bb094e316ce20c7345998febb856b5fec57c8a612fc530d9e07b8877f89e629" .
      "8ed657a839718c34a8a6123c530d185bf443fde60a12e44abcbe010de623dc5f039c5f0aa26" .
      "d04015e24b8a6133c530de64dad130a6b1d37454bcc614b6520c4b2986c1be1e0ff9a21a6aa" .
      "7187ad564143318693da321fb58c44a0fee14cb986e4a7a85357cead427c5d08d3bc1363e16" .
      "65defdd7d3273ac78faa8bbf1b36bdc01fcc61bb28f3fda3f25777778b5db4f1ad28f375cd6" .
      "7b9c351c5ad76e33aa6bff0131f8a325f35a42bb185f745992feaa007c8355d7b133a504e00" .
      "00000049454e44ae426082";
    break;

  case "JumpNextReg.png":
    $imgdata = "89504e470d0a1a0a0000000d49484452000000150000000c08060000008dcef6c50" .
      "0000006624b474400ff00ff00ffa0bda7930000011f4944415478daadd3bf2b85511cc7f197" .
      "1bc5260b8b522899d4f34f1865b31a9ec5c24221b66bf1171c25260b5d9bbf407ee4a4946c6" .
      "422931f37bfeb1a3c48dde7b9069fe52cefefe77cce399fd354abd5fcb79a410c175891a4e5" .
      "423a860aee312349aff3b052b6de605e0c6762182eb0bdc418f6c430d5c8f4056d18c09a18b" .
      "6c43054877f440b7ab09017a25467b013a3d814c3b218ba7202b5672182182a62182c32fd52" .
      "2f2670587454746304db62581243477383876ccd86ca626854937e4ca2aff487869c63116b0" .
      "db83bac62a228e9133630f75d9f18ea71b7d8c5ac243df9e9e96f3d603febe271c1a6559c62" .
      "06bb92f4fd77f93fefee1527286347923ee77c96375c65dc7a3deecb34e200d392f4a920dd1" .
      "1b6312e49ab79d0072ce059d3e0b9b6220000000049454e44ae426082";
    break;

  case "JumpPreviousHover.png":
    $imgdata = "89504e470d0a1a0a0000000d49484452000000150000000c08060000008dcef6c50" .
      "0000006624b474400ff00ff00ffa0bda7930000011c4944415478daadd33f2b46511c00e0e7" .
      "ea8610322019303028195e16062c186cca6432bc29992894c5429444ca1df8063e80c137b89" .
      "bc12e0625834949c772d51bd7bd8b339ef3f4ebfcfe452104ff7de23250ad442d38c65d9286" .
      "b302d788550cc605a8154bd8440f6e7196e3624c601723788b73503da6b360c3e8ce9e3e726" .
      "c3ff63185aeecfa25fe81867084b19a607959746203ebf844d3af9a66680bcb19682a29c91a" .
      "fad050d4a873cca3b9a46f8b38447b11aafbfe44d684c7229ca4e12a4bfb05ef85419334bc2" .
      "669d8c62caef15c10f81243b8c0439e89f286bf5a89667082819a925c276958fce146b187f1" .
      "9aeedf47451b55ad442b38401b6e92342cfce1e6708a5e3c45656b5aad441dd92c0e2769982" .
      "cb13b58ff02a3384e4c99f5997b0000000049454e44ae426082";
    break;

  case "JumpPreviousReg.png":
    $imgdata = "89504e470d0a1a0a0000000d49484452000000150000000c08060000008dcef6c50" .
      "0000006624b474400ff00ff00ffa0bda7930000011c4944415478daadd3312b45611807f0df" .
      "d5095d420624030677b825756261c082c1a64c2675a474270a65b11025b929dde21bdc0f60f" .
      "011ee64b08b41dd0c26255d8333dc38ce59bccb5beffbeb5feff3bc4faed168f8ef15648a5a" .
      "a51367b81746e514d78e4d8c0529a80babd8c120ee504e7001a6718071bc0509a8157371581" .
      "103f1cd47821dc11166d11f9fd6831fa880534c368525bda20fdb28e113f9df35fd46bb588b" .
      "413ea3245b18465b5aa32eb1848e8cb6ade0043d69a825de37e2263ca54686d14dfcec3aded" .
      "343c3e85518ed610155bca4045fa3802b3c26915ce2e7af55e6718ed1a6925485d1ca0f3781" .
      "434c3575ff21973a51b5ca3a8ed18d5b61b4fc875bc40586f09ccb1cd35aa537fe8b4561349" .
      "361f751fa021bcd4ca427e8c37a0000000049454e44ae426082";
    break;

  case "Logo.png":
    $imgdata = "89504e470d0a1a0a0000000d49484452000000c3000000240806000000343e8e900" .
      "0000006624b474400ff00ff00ffa0bda7930000053f4944415478daed5ccd71a35810fe70e9" .
      "6e65203290b696bb9908cc4660e6321c2d47602682d11c392d8e60700483ee540d8a60a50c4" .
      "404ec41cd0ea595107aaffb81305dc5c146f57ebafb7bfdfbb0cab2c44823dd12058e3505b0" .
      "68fa4d9495e9b5e35a5ffe840bc0655ceb2acacafd994dd8007cc6b9e2282bb73d11900fc03" .
      "634ddb67adaee9f617d4d72551e3bcacab061cd36e9a64be33f28ae7d47fc02800a2439807d" .
      "1d34139ae89551503980e4ccbb258067c6b9d2da26bb265f43583a202c880f0980e49cc232a" .
      "cafe920d3193b3c71eafba42b332636cd6a633d1ccd07009fa2ac4cef04e4e329be1b498dee" .
      "013c02f81bc03670ac50689e27b20092c0f6e830fdc60884d6640c0c64f266a3ee8a03e3357" .
      "0ac3c70ac85c0f82b92a3041042003fbad49189844002c75a4459991bb00a0b32732abe6ada" .
      "201895186ada2350cc01a424872d33d8924bc1ab62bcf5da35d32642e3563e9f3418be6908e" .
      "08f1380059da83f076225d81517c03c70acb029f0bd520e3680551f18762734ae7b22287ae8" .
      "99b2f81f20ae990bc510af8ad6f3142d09b88305c3fcc8b79452b03781407f6841fe5268dc9" .
      "80e399118f30a2a00ac017c3d7adee8ff6bfa4d676e52b5c995b082c564855482aed9716c43" .
      "009e0f2da816ca02cd88ff1ec338aab403b06848279f72c9ea4f55bcdb4b83c1ad81c1159c2" .
      "7c421adc811db0c35f52bc5ff47e9746b8ba4c5b452e61649932d1aea5292607824347a923e" .
      "6194953109442526f198c050109375accaa7a3bf17a4c48f0c6c5ab45518055a098edd2649f" .
      "04fe0589b0b6bc8e97d0e203d674924c15001c1c4691b422d03f49faba419e427d06cc53891" .
      "ea4d71c8eb7b38e4dfb56238f29da514b2eb00f8d221745c757ec7a1bd24351140d74f5a579" .
      "a13b4295561fb0c2e5228b8b744335150918d91ea5ecbcfc0b156a6c160aaa2186aac51070c" .
      "6f069a0513a680b78ff4dee1dccf75404883c198f9d4b00e332ab4b98641780ded315ceabae" .
      "0f65cd54cee06c65855c58c1581bbee4b0bf9ad121d629f3b5e863f3830685887b961f05d4b" .
      "1c2d15eb1ecb2d2640141d2dc1e300c38e6121dc0c30a5a06b95db549a714d57722a0c01c20" .
      "6f0026063180cf781634d7553ab09f42feb24009e38ad43e0586bc8f74219f175356a28c73c" .
      "d6b12e2e805f0600b127beae6a573b6d5cce8479d0ef1c587080c1d708943790b9a91642b6f" .
      "37447294f4910d8b40f8e8322d6012fd5613e43bdd2af0a8cb696370c1c2bd7050447d14de7" .
      "648f21700fc08075080594bf2e781b7ca9d03529b3b61b43599727c1bd6fe9a92ac6009037f" .
      "51ed5ee49dbbaebe81a0c956591525809ebb023ff969b7a0fdc282b7d4a43cf85f6fe7046e1" .
      "4d5822fd3bd0e42ea804581bc9b4a46655ba0f013a077d1708f23d7497f591a20dc0975a55f" .
      "19f63039be456dc023cd56023028eb292fd2e031d6043ebee8dbb0683b85209588755dbdef9" .
      "1e9c74ae305f5f0602841d2b18145ca58dc1ca2d574c52a02777752fd03b00571ab45156aec" .
      "0d340d8251500bc8a579c15e86b4efad8d46e09741c428b7b6e150a002f51567a06d7b984f9" .
      "021927bfdcfa4dc70933189e0480d34894ddb8746273a46ffb6a152a336fdc858bb2725ffbf" .
      "0d7fd0d01e10dc0f2985f1346c624f4a9c34b4ce176914c7c79e36b8f1af2aa5b5d290eb7b6" .
      "d22e171365e596aae43f34955322655ba7351dc2672dbc75eb5fe1a6429064b5b90060df48e" .
      "07cf344967e8affb7612c5a5af82d7e7735e4387c9c396f33f76464ffe5007c0482514b9377" .
      "35f7ddc8fe46fa2edd8334527f680443831f2b51b41a6904c32dd10ec05f5156fa232b3e168" .
      "d31c3efec4cf54d9dd12dfaa0f42fb24405438cc8bc550000000049454e44ae426082";
    break;

  case "NextHover.png":
    $imgdata = "89504e470d0a1a0a0000000d494844520000000b0000000c0806000000b4a9479e0" .
      "0000006624b474400ff00ff00ffa0bda793000000b84944415478da8dd1214e03401484e16f" .
      "9b1aeaa9ade00a2b4b1004db13e037699a5a041ca117d88021a486a049d020101b0ed013549" .
      "00a822a6231152564dbfec9cb3393f72633a1d6ea50ba906278c33366b9d4754bdcd9ec235c" .
      "63916218ed13afd1c300b72986d714c3594bbc4d1f43dca7181e520c277f3c3718e018e7298" .
      "6396eba7b02e86d668cd3ce0189fde00e17bb2e7fe105d35ceab2e5f91b1fb8caa5beff2b65" .
      "ebdd0a935cea53b3417ce231977ab9cbfc2f10c42ba4cf1b95700000000049454e44ae42608" .
      "2";
    break;

  case "NextReg.png":
    $imgdata = "89504e470d0a1a0a0000000d494844520000000b0000000c0806000000b4a9479e0" .
      "0000006624b474400ff00ff00ffa0bda793000000b34944415478da8dd12d4e035114c5f15f" .
      "9b9a8e075bc1129ea541102c2b403f41486d052c81153c3084d4103449751198c7025801825" .
      "43455533198498094f9f82737d79cdc7b72cea0aa2a7d19819c5ef1825b21964de261bdc7b8" .
      "c6879ccebbc4250a4c7027a7959c4e9ac4bf39c4311ee4f428a7a3bf9eff6782039cca69819" .
      "b514700453d97980e7b24b6c33dceda2e6fb0c44c889f4d9eb778c75c886ffba5fcbc5be34a" .
      "88cfcd0df28527215eb499ff0660ad283ae55ed4840000000049454e44ae426082";
    break;
    
  case "PreviousHover.png":
    $imgdata = "89504e470d0a1a0a0000000d494844520000000b0000000c0806000000b4a9479e0" .
      "0000006624b474400ff00ff00ffa0bda793000000ab4944415478da8dd1316a02611086e167" .
      "641b218516920472058bc05f6863bd04048f9026fc57b010c13216290216dec1a3e41239801" .
      "64961b9365b88e8ba534df1ce30ef7c515595b655b481728a39cae20e34c51a8ff82f6e40af" .
      "58625c83f0575c40bd7ad30c83ab37e7140ff8c0178ee83609ee30a9fbee2d870e6c7faa12d" .
      "f383409c7f99f738a17acf086a70bf637ae8592538cf08921fa8df0d950890d9eb18f3671e7" .
      "140bbc9f00773e27bb1ed68b390000000049454e44ae426082";
    break;

  case "PreviousReg.png":
    $imgdata = "89504e470d0a1a0a0000000d494844520000000b0000000c0806000000b4a9479e0" .
      "0000006624b474400ff00ff00ffa0bda793000000a74944415478da8dd1316a02511087f1df" .
      "0bdb082962212a78058b60639ad48b2078041b3d83451052c6c22290e69d21577987c801b4d" .
      "0c2726d16c465ddec54537c33cc37ff501485b695b5a252dc20cffe81e6d8a18f73f6007ac5" .
      "166f2508a7ac02bd949b16e8d5df9ce23356d8e3824e93e02fdecbbef348e1094cd639be716" .
      "cf20d777f4e71844fcc30a8b07fa1369414a7f8c218dd66f83694e307431c42abb853fcc0f2" .
      "0ae6c72d1323c9f9400000000049454e44ae426082";
    break;

  case "Warn.png":
    $imgdata = "89504e470d0a1a0a0000000d494844520000001e0000001e08060000003b30aea20" .
      "0000006624b474400ff00ff00ffa0bda7930000025a4944415478daedd74f4893711cc7f1f7" .
      "fe888d985982381912c4c0cac01876e812441978510f764a083c0c165e3cd9c15317114928d" .
      "6c18b09764a482f0661cc3c0ce9148430ff04533364248c3128b6b56f87c75fcf7e6e8fe9da" .
      "d6a17ef0bdfc7edfe7f38287e7f9fd9e07fe8fe38d334008780ab8ab094f027250a16aa1578" .
      "16c1e9c05ae541ab501eff250556f2b0ddf5598c381d8ed1ade5b29d405c41414082003031a" .
      "fc09385509784421f5f5483c8eeced2175751afeb0dca817482960620211316a6c4c839380a" .
      "79cf00b15deda8a6c6c207ebf519108e2f369f8f372a1d7819c0a5e5840d6d64c281c46e6e7" .
      "35f80770ed4f513bf05e85767519b7f7302c8274766a78e4e0d52b79dc5761353548346a0da" .
      "fae224ea786df2b1575035f54d0d090f9406d6d99c0ca8a393f38a8c19f81d3a5c0a32aa4b1" .
      "1149244c209d36378f9d1d737e7f1f6968d0f04727452f00df55c0e4a419aeaaa9c9d8bd321" .
      "97d3e14d2e06fc0f993c0afd4c5eded48365b08fbfd487373e17c2683b4b569f8cbe3a2b7f2" .
      "0f80a5a5c27011a4bb1be9e828beb6b8587088dcf81dea043eaa0bfafa8a078b20c120d2db6" .
      "bbdded3a3c11f00c751f003d5ec7221b198757022613c4c56eb9b9b486dad8607acd073c057" .
      "d53832621dbabd8d8c8f1bb5be6edd373cacc171a0be18fc443579bd482a651d180e9b81b3b" .
      "3d67dc924e2f168f8e3c3e86520a31a6666acc34490e565c4ed366a6eeee8dea9290d4e03ad" .
      "e4eda76f80db002d2d303d0d365b794e985c0efafb6177f7d7d46ba00be04e916fa84ad74d3" .
      "b70e92f7c975fb40167816780af4a681408fe7bff403f012fb97beaa47ee99f000000004945" .
      "4e44ae426082";
    break;

  case "WarnSmall.png":
    $imgdata = "89504e470d0a1a0a0000000d49484452000000100000001008060000001ff3ff610" .
      "0000006624b474400ff00ff00ffa0bda793000001444944415478daa5d2cd4a5b711005f05f" .
      "6e85aaf8816e12080aa2b8926e4b1191e233882b41d1f800e2ce0fb0505bb185ec7c03f11dd" .
      "c74e142dc49fb0a2137e8be719371714d2e0a6ad403c37f66fe9c39736078273ebcf0ff0df3" .
      "f8f396e15fd0ba8fcfaf2527b8ecef177d7d021728bc66c00a627f5feced0904969f527a8c4" .
      "11c8c8f333646b198bdf889816ed47f204e4fc5da9a585c1427279d2dbebf449e44736e4eb4" .
      "5aa252114b4b593e3b2bf01f13cf59f895243e56ab140ad9fac5629657ab24895e1c3da5be8" .
      "0585f1711591c1f8bc3c3bc5e5ded58f9fa98dc83bfc3c3224d73c2f9b9383bcbeb7a5d0c0d" .
      "095cb58fb06d610333bbbbd9ca6dd46a341a795d2ab1bd0d3ea1d2ee8fe2667a5adcdee66a1" .
      "16267476c6d3dec359b626a4ae01a2305fcc666a944b9dcdd95d56aa42938ea411dffd2b4d3" .
      "ec1671cf7d1fee00a04a929b850a144b0000000049454e44ae426082";
    break;
    
  case "Refresh.png":
    $imgdata = "89504e470d0a1a0a0000000d494844520000001200000012080600000056ce8e570" .
      "000000467414d410000b18f0bfc610500000006624b474400ff00ff00ffa0bda79300000009" .
      "7048597300000b1000000b1001ad23bd750000000774494d4507d202050f3536b8eba4c4000" .
      "0029d49444154789c7594d14b537114c73ff7bad564b67507d775dd945231124444d2870409" .
      "d683e1cb26a12f4150affd03ad879e855e7aab179f227c51500295bd04ab20b1049181419ac" .
      "cbbdc849c736d63d3d383bbb55de70f2e977b38bfcfeffcbedf73ae826d8988e3e6c35be562" .
      "b154172f1c6e9389e105f2c0897d9f6a87f43eea2bbf7c3e8d3af4072dac5038dc2697dca6b" .
      "00b7a88ac1ea2027880263becdfba31d52ddf0fb6e4fab45ffadf768a1e4280c7c014f00010" .
      "c715acb8510babab482dab7ccd6ce00db831677e9089f104d807e2400c68afe4b89f89811ec" .
      "2045a2d98a316542a97f8b8b7cc55c38b7916ca03eb40aaaa4b163802da33317a81ceea77de" .
      "11887488727a82a84d50815fe97db87646d143bcb30eb1844e2693d96030e805be00a740114" .
      "001dc81c9b6e3372f5eb3bcb7c8caf622954205b7d98b7fb2c0b7a73fc9c4f7275c2ed781c7" .
      "e3f9904ea7fd4ea733502e97b7aa15d73967748ff5c8a79db874bfd2a5ffd95d09ad0d893ee" .
      "2b7c41eefeaea92a5a5250104e8062e3532ac09307cf774799f5894d0eaed3a88d3e9949595" .
      "1599989890482462c1ea5c536cb0567dc46f0264e2fb96638bb3b3b3cccfcfb3b9b94973733" .
      "3c16090b9b939803620cd050d6a0063c098a669a2699a381c0e8946a332303020d16854acb8" .
      "a669e72ab3c33c35cf9dc1c14199999991e1e161eb5a5693468011c06df59154dfd62c1dd54" .
      "037d6d6d6d8dddd45d7750cc320954ae581cfd5bc13a06875768ba228288a9285ffb3242297" .
      "81ecc2c20289448252a9442a95a2baf908f86dc1949a93fd3e9fcf74b95c98a689aaaa84c36" .
      "1464747595d5d65676787f5f57572b9dc13ce3a3d5e537963a1354d939e9e1e4b0fe9e8e8a8" .
      "d566bc91c04a03584bf56a7d80db9673ccd9ec9db3dc0eb203555bce0917fcd8fe027dd712b" .
      "f968b8ffc0000000049454e44ae426082";
    break;

  case "blank.gif":
    $imgdata = "47494638396101000100800000ffffff00000021f90401140000002c00000000010" .
      "001000002024401003b";
    break;

  default:
    return;
  }

  $data = pack("H" . strlen($imgdata), $imgdata);
  header("Content-Type: image/png");

  /* A Last-Modified: header keeps the image from being reloaded every time */
  /* date_default_timezone_set avoid warning about It is not safe to rely on the system's timezone settings. */
  date_default_timezone_set('UTC');
  header("Last-Modified: " . date("r", filectime($_SERVER['SCRIPT_FILENAME'])));
  header("Content-Length: " . strlen($data));
  echo $data;
  exit;
}
/*--- send_img() --------------------------------------------------------------------------------*/


/**************************************************************************************************
	BGCOLOR
	Returns the current background color, alternating between two.
**************************************************************************************************/
function bgcolor($highlight = 0) {
  global $bgcolor, $high_bgcolor, $list_bgcolor_1, $list_bgcolor_2;

  $bgcolors = array($list_bgcolor_1, $list_bgcolor_2);
  $high_bgcolors = array("#00CCFF", "#3399FF");

  if ($highlight) {
    $high_bgcolor = (($high_bgcolor == $high_bgcolors[0]) ? $high_bgcolors[1] : $high_bgcolors[0]);
    return ($high_bgcolor);
  } else {
    $bgcolor = (($bgcolor == $bgcolors[0]) ? $bgcolors[1] : $bgcolors[0]);
    return ($bgcolor);
  }
}
/*--- bgcolor() ---------------------------------------------------------------------------------*/


/**************************************************************************************************
	FORMBUTTON
	Returns HTML for a SUBMIT button with onmouseover syntax.
**************************************************************************************************/
function formbutton($action, $help, $bgcolor = NULL) {
  global $list_bgcolor_1;

  $out = ($bgcolor ? $bgcolor : $list_bgcolor_1);
  return "<INPUT type=submit class=formButton" .
    " style=\"border-color: $out; color: $out;\"" .
    " name=\"action\" value=\"" . ent($action) . "\"" .
    " onmouseover=\"style.border='#663300 solid 1px'\"" .
    " onmouseout=\"style.border='$out solid 1px'\"" .
    " title=\"$help\">";
}
/*--- formbutton() ------------------------------------------------------------------------------*/


/**************************************************************************************************
	ERRBOX
	Outputs the specified text in a box with optional box title.
**************************************************************************************************/
function ErrBox($msg) {
  if ($msg) {
?>
<DIV align=center>
	<TABLE class=errBox>
		<TR>
			<TD class=errLeft><IMG src="<?php echo $_SERVER['PHP_SELF']?>?img=Error.png" alt="ERROR">
			<TD class=errRight width="100%"><?php echo $msg?>
	</TABLE>
</DIV>
<?php
  }
}
/*--- ErrBox() ----------------------------------------------------------------------------------*/


/**************************************************************************************************
	WARN
	Output a box containing a warning message.
**************************************************************************************************/
function Warn($msg) {
  if ($msg) {
?>
<DIV align=center>
	<TABLE class=warnBox>
		<TR>
			<TD class=warnLeft><IMG src="<?php echo $_SERVER['PHP_SELF']?>?img=Warn.png" alt="WARNING">
			<TD class=warnRight width="100%"><?php echo $msg?>
	</TABLE>
</DIV>
<?php
  }
}
/*--- Warn() ------------------------------------------------------------------------------------*/


/**************************************************************************************************
	NOTICE
	Output a box containing a notice to the user.
**************************************************************************************************/
function Notice($msg) {
  if ($msg) {
?>
<DIV align=center>
	<TABLE class=noticeBox>
		<TR>
			<TD class=noticeLeft><IMG src="<?php echo $_SERVER['PHP_SELF']?>?img=Info.png" alt="INFO">
			<TD class=noticeRight width="100%"><?php echo $msg?>
	</TABLE>
</DIV>
<?php
  }
}
/*--- Notice() ----------------------------------------------------------------------------------*/


/**************************************************************************************************
	ERR
	A fatal error has occurred.  Output a simple error message to the screen.
**************************************************************************************************/
function Err($msg) {
  ErrBox($msg);
  close_page();
}
/*--- Err() -------------------------------------------------------------------------------------*/


/**************************************************************************************************
	ERRSQL
	A fatal error has occurred while trying to talk to the database.  Output a simple error
	message to the screen, and output a complete error message via the logging mechanism.
**************************************************************************************************/
function ErrSQL($msg) {
  global $dbconn, $use_pgsql;

  ErrBox("<B>$msg</B><P>For more detail, check the server's error log.");
  if ($dbconn)
    $error_msg = (($use_pgsql) ? pg_last_error($dbconn) : mysql_error($dbconn));
  if (ends_with_dot($msg))
    $msg = substr($msg, 0, strlen($msg)-1);
  error_log($_SERVER['SCRIPT_FILENAME'] . ": $msg" . (strlen($error_msg) ? ": $error_msg" : ""));
  close_page();
}
/*--- ErrSQL() ----------------------------------------------------------------------------------*/


/**************************************************************************************************
	OFFSET_SELECT
	Outputs offset selection navigation bar.  Returns the current offset.
**************************************************************************************************/
function offset_select($page, $total, $group_size, $xtra = "") {
  if (!$group_size || !$total)
    return 0;

  $pages = ceil($total / $group_size);
  $last = $pages - 1;
  $pagesize = ceil($total / $pages);
  $jump = ceil($pages / 10);

  if ($pages == 1)
    return 0;

  if ($page > $last)
    $page = $last;
  if ($page < 0)
    $page = 0;

  $prev_page = ($page > 0 ? $page - 1 : $page);
  $next_page = ($page < $last ? $page + 1 : $page);

  if (($next_jump = $page + $jump) > $last)
    $next_jump = $last;
  if (($prev_jump = $page - $jump) < 0)
    $prev_jump = 0;

  $xtra .= "&amp;";
  $query = getpostvar('query');
  if (strlen($query))
    $xtra .= "query=" . urlencode($query) . "&amp;";

  /* NEXT and END buttons */
  $next_button = "<A href=\"{$_SERVER['PHP_SELF']}?{$xtra}page=$next_page\"" .
    " onMouseOver=\"imgSwap('Next','Hover')\"" .
    " onMouseOut=\"imgSwap('Next','Reg')\">" .
    "<IMG name=\"Next\" src=\"{$_SERVER['PHP_SELF']}?img=NextReg.png\"" .
    " alt=\"Next page\" title=\"Next page\" class=noborder></A>";

  $end_button = "<A href=\"{$_SERVER['PHP_SELF']}?{$xtra}page=$last\"" .
    " onMouseOver=\"imgSwap('End','Hover')\"" .
    " onMouseOut=\"imgSwap('End','Reg')\">" .
    "<IMG name=\"End\" src=\"{$_SERVER['PHP_SELF']}?img=EndReg.png\"" .
    " alt=\"Last page\" title=\"Last page\" class=noborder></A>";

  /* HOME and PREV buttons */
  $home_button = "<A href=\"{$_SERVER['PHP_SELF']}?{$xtra}page=0\"" .
    " onMouseOver=\"imgSwap('Home','Hover')\"" .
    " onMouseOut=\"imgSwap('Home','Reg')\">" .
    "<IMG name=\"Home\" src=\"{$_SERVER['PHP_SELF']}?img=HomeReg.png\"" .
    " alt=\"First page\" title=\"First page\" class=noborder></A>";

  $prev_button = "<A href=\"{$_SERVER['PHP_SELF']}?{$xtra}page=$prev_page\"" .
    " onMouseOver=\"imgSwap('Previous','Hover')\"" .
    " onMouseOut=\"imgSwap('Previous','Reg')\">" .
    "<IMG name=\"Previous\" src=\"{$_SERVER['PHP_SELF']}?img=PreviousReg.png\"" .
    " alt=\"Previous page\" title=\"Previous page\" class=noborder></A>";

  /* JUMP previous/next buttons */
  $n = $page - $prev_jump;
  $desc = "Back $n page" . S($n);
  $jump_prev_button = "<A href=\"{$_SERVER['PHP_SELF']}?{$xtra}page=$prev_jump\"" .
    " onMouseOver=\"imgSwap('JumpPrevious','Hover')\"" .
    " onMouseOut=\"imgSwap('JumpPrevious','Reg')\">" .
    "<IMG name=\"JumpPrevious\" src=\"{$_SERVER['PHP_SELF']}?img=JumpPreviousReg.png\"" .
    " alt=\"$desc\" title=\"$desc\" class=noborder></A>";

  $n = $next_jump;
  if ($page + $next_jump > $last)
    $n = $last - $page;
  $desc = "Forward $n page" . S($n);
  $jump_next_button = "<A href=\"{$_SERVER['PHP_SELF']}?{$xtra}page=$next_jump\"" .
    " onMouseOver=\"imgSwap('JumpNext','Hover')\"" .
    " onMouseOut=\"imgSwap('JumpNext','Reg')\">" .
    "<IMG name=\"JumpNext\" src=\"{$_SERVER['PHP_SELF']}?img=JumpNextReg.png\"" .
    " alt=\"$desc\" title=\"$desc\" class=noborder></A>";

?>
<!-- BEGIN page navigation bar -->
<TABLE class=offsetBox cellspacing=0>
	<TR>
		<TD class=offsetLeft><?php echo $prev_button?>
		<TD class=offsetLeft><?php echo $jump_prev_button?>
		<TD class=offsetLeft><?php echo $home_button?>
		<TD class=offsetCenter>
			<DIV style="font-weight: bold; font-size: 150%"><?php echo nf($page + 1)?> of <?php echo nf($pages)?></DIV>
			<?php echo nf($total)?> record<?php echo S($total)?>
		<TD class=offsetRight><?php echo $end_button?>
		<TD class=offsetRight><?php echo $jump_next_button?>
		<TD class=offsetRight><?php echo $next_button?>
</TABLE>
<!-- END page navigation bar -->

<?php
  return ($page * $pagesize);
}
/*--- offset_select() ---------------------------------------------------------------------------*/


/**************************************************************************************************
	SQL_QUERY
	Issues an SQL query and returns the result.
**************************************************************************************************/
function sql_query($query) {
  global $use_pgsql;

  return ($use_pgsql ? @pg_exec($query) : @mysql_query($query));
}
/*--- sql_query() -------------------------------------------------------------------------------*/


/**************************************************************************************************
	SQL_FETCH_ROW
	Returns the next row in the result set specified by '$result'.
**************************************************************************************************/
function sql_fetch_row($result) {
  global $use_pgsql;

  return ($use_pgsql ? pg_fetch_row($result) : mysql_fetch_row($result));
}
/*--- sql_fetch_row() ---------------------------------------------------------------------------*/


/**************************************************************************************************
	SQL_FETCH_ARRAY
	Returns the next row in the specified result set as an associative array.
**************************************************************************************************/
function sql_fetch_array($result) {
  global $use_pgsql;

  return ($use_pgsql ? pg_fetch_array($result) : mysql_fetch_array($result));
}
/*--- sql_fetch_array() -------------------------------------------------------------------------*/


/**************************************************************************************************
	SQL_NUM_ROWS
	Returns the number of rows in the specified result set.
**************************************************************************************************/
function sql_num_rows($result) {
  global $use_pgsql;

  return ($use_pgsql ? pg_num_rows($result) : mysql_num_rows($result));
}
/*--- sql_num_rows() ----------------------------------------------------------------------------*/


/**************************************************************************************************
	SQL_COUNT
	Issues an SQL query and returns the first column of the first row return as an integer.
**************************************************************************************************/
function sql_count($query, $what) {
  $res = sql_query($query)
    or ErrSQL("Error counting $what.");
  $row = sql_fetch_row($res);
  return ($row[0]);
}
/*--- sql_count() -------------------------------------------------------------------------------*/


/**************************************************************************************************
	SQL_RESULT
	Issues an SQL query and returns the first row of the result as an associative array.
**************************************************************************************************/
function sql_result($query, $what) {
  $res = sql_query($query)
    or ErrSQL("Error loading $what.");
  $row = sql_fetch_array($res);
  return ($row);
}
/*--- sql_result() ------------------------------------------------------------------------------*/


/**************************************************************************************************
	DB_CONNECT
	Establish the database connection.
**************************************************************************************************/
function db_connect() {
  global $dbhost, $dbuser, $dbpass, $dbname, $use_pgsql, $dbconn;

  if ($use_pgsql) {
    $connect = "host=$dbhost user=$dbuser password=$dbpass dbname=$dbname";
    if (!($dbconn = @pg_connect($connect))) {
      open_page();
      ErrSQL("Unable to connect to database.");
    }
  } else {
    if (!($dbconn = @mysql_connect($dbhost, $dbuser, $dbpass))) {
      open_page();
      ErrSQL("Unable to connect to database.");
    }
    if (!(@mysql_select_db($dbname))) {
      open_page();
      ErrSQL("Unable to select database.");
    }
  }
}
/*--- db_connect() ------------------------------------------------------------------------------*/


/**************************************************************************************************
	DB_COLUMN_EXISTS
	If a column named '$column' exists in the table named '$table', returns 1, else 0.
**************************************************************************************************/
function db_column_exists($table, $column) {
  global $use_pgsql;

  if ($use_pgsql) {
    if (sql_count("SELECT COUNT(*) FROM pg_class,pg_attribute WHERE relkind='r'" .
		  " AND relname='$table' AND attrelid=oid AND attname='$column'",
		  "columns named '$column' from table '$table'") > 0)
      return (1);

    return (0);
  } else
    return sql_count("SHOW COLUMNS FROM $table LIKE '$column'",
		     "columns named '$column' from table '$table'");
}
/*--- db_column_exists() ------------------------------------------------------------------------*/

function db_get_column_width ($table, $column) {

/* always return 128 for now because the flowing sql query works only with mysql
  $width = 128;
  $res = sql_query("SELECT character_maximum_length FROM information_schema.columns WHERE table_name='$table' and column_name='$column'",
		   "columns named '$column' from table '$table'");
  if ($res) {
    if ($row = sql_fetch_row($res)) {
      $width = $row[0];
    }
  }
*/
  $width = 128;
  return $width;
}

/* List of available resource record types */
$available_rr_types = array(
			    "A",
			    "AAAA",
			    "ALIAS",
			    "CNAME",
			    "HINFO",
			    "MX",
			    "NAPTR",
			    "NS",
			    "PTR",
			    "RP",
			    "SRV",
			    "TXT"
			    );


/**************************************************************************************************
	DB_GET_KNOWN_TYPES
	Gets allowed ENUM values for the 'type' column.
	XXX: Is this PostgreSQL technique foolproof?
**************************************************************************************************/
function db_get_known_types() {
  global $dbname, $rr_table_name, $use_pgsql, $available_rr_types;
  global $rrtype_table_name;

  $type_values = array();

  if ($rrtype_table_name) {
    $res = sql_query("select type from $rrtype_table_name");
    if (!$res) {
      open_page();
      ErrSQL("Error getting available values for " . quote("type") . " column in RR table.");
    }
 
    while ($row = sql_fetch_row($res)) {
      $val = $row[0];
      foreach ($available_rr_types as $rr_type)
	if (!strcasecmp($val, $rr_type))
	  $type_values[] = $val;
    }
 
  } elseif ($use_pgsql) {
    $res = sql_query("SELECT consrc FROM pg_constraint" .
		     " WHERE conname LIKE '" . esc($rr_table_name) . "%'");
    if (!$res) {
      open_page();
      ErrSQL("Error getting available values for " . quote("type") . " column in RR table.");
    }

    while ($row = sql_fetch_row($res)) {
      $constraints = explode(" OR ", $row[0]);
      foreach ($constraints as $con) {
	$val = substr(strstr($con, "'"), 1);
	$val = substr($val, 0, strpos($val, "'"));

	reset($available_rr_types);
	foreach ($available_rr_types as $rr_type)
	  if (!strcasecmp($val, $rr_type))
	    $type_values[] = $val;
      }
    }
  } else {	/* MySQL */
    $res = sql_query("SHOW COLUMNS FROM $dbname.$rr_table_name");
    if (!$res) {
      open_page();
      ErrSQL("Error getting available values for " . quote("type") . " column in RR table.");
    }

    while ($col = mysql_fetch_array($res))
      if (!strcasecmp($col['Field'], "Type")) {
	$values = explode(",", substr($col['Type'], 5));
	foreach ($values as $value) {
	  $val = substr($value, 1);
	  $val = substr($val, 0, strpos($val, "'"));

	  reset($available_rr_types);
	  foreach ($available_rr_types as $rr_type)
	    if (!strcasecmp($val, $rr_type))
	      $type_values[] = $val;
	}
      }
  }
  return $type_values;
}
/*--- db_get_known_types() -----------------------------------------------------------------------*/


/**************************************************************************************************
	DB_GET_ACTIVE_TYPES
	Examines the 'active' table in a column and attempts to deduce the legal values for that
	field.
**************************************************************************************************/
function db_get_active_types($table, $deleted) {
  $active = NULL;

  $row = sql_result("SELECT DISTINCT(active) FROM $table LIMIT 1",
		    "legal value for $table.active");
  $entry = strtolower($row[0]);
  switch ($entry) {
  case "yes": case "no": case "deleted":
    $active = array('No', 'Yes');
    if ($deleted || $entry == "deleted") { $active[] = 'Deleted'; }
    break;
  case "y": case "n": case "d":
    $active = array('N', 'Y');
    if ($deleted || $entry == "d") { $active[] = 'D'; }
    break;
  case "true": case "false":
    $active = array('False', 'True');
    if ($deleted || $entry == 'deleted') { $active[] = 'Deleted'; }
    break;
  case "t": case "f":
    $active = array('F', 'T');
    if ($deleted) { $active[] = 'D'; }
    break;
  case "0": case "1": case "2":
    $active = array('0', '1');
    if ($deleted || $entry == '2') { $active[] = '2'; }
    break;
  case "active": case "inactive":
    $active = array('Inactive', 'Active');
    if ($deleted) { $active[] = 'Deleted'; }
    break;
  case "a": case "i":
    $active = array('I', 'A');
    if ($deleted) { $active[] = 'D'; }
    break;
  case "on": case "off":
    $active = array('off', 'on');
    if ($deleted) { $active[] = 'deleted'; }
    break;
  }
  if (!$active) {
    $active = array("N", "Y");					/* Default to Y/N */
    if ($deleted) { $active[] = "D"; }
  }
  return $active;
}
/*--- db_get_active_types() ---------------------------------------------------------------------*/


/**************************************************************************************************
	DB_GET_RECURSIVE_TYPES
	Examines the 'recursive' table in a column and attempts to deduce the legal values for that
	field.
**************************************************************************************************/
function db_get_recursive_types($table) {
  $recursive = array("N", "Y");					/* Default to Y/N */

  $row = sql_result("SELECT DISTINCT(recursive) FROM $table LIMIT 1",
		    "legal value for $table.recursive");
  switch (strtolower($row[0])) {
  case "yes": case "no": $recursive = array('No', 'Yes'); break;
  case "y": case "n": $recursive = array('N', 'Y'); break;
  case "true": case "false": $recursive = array('False', 'True'); break;
  case "t": case "f": $recursive = array('F', 'T'); break;
  case "0": case "1": $recursive = array('0', '1'); break;
  case "recursive": case "direct": $recursive = array('Direct', 'Recursive'); break;
  case "r": case "d": $recursive = array('I', 'A'); break;
  case "o": $recursive = array('off', 'on'); break;
  }
  return $recursive;
}
/*--- db_get_recursive_types() ---------------------------------------------------------------------*/


/**************************************************************************************************
	DB_GET_SETTINGS
	Sets global variables based on optional fields.
**************************************************************************************************/
function db_get_settings() {
  global $soa_use_active, $rr_use_active;
  global $soa_use_recursive;
  global $rr_extended_data, $rr_datalen;
  global $soa_use_xfer;
  global $soa_use_update_acl;
  global $soa_use_also_notify;
  global $soa_table_name, $rr_table_name;
  global $soa_active_types, $rr_active_types;
  global $soa_recursive_types;
  global $db_valid_types;
  global $allow_ixfr;

  $db_valid_types = db_get_known_types();

  $soa_use_active = db_column_exists($soa_table_name, "active");
  $soa_use_recursive = db_column_exists($soa_table_name, "recursive");
  $soa_use_xfer = db_column_exists($soa_table_name, "xfer");
  $soa_use_update_acl = db_column_exists($soa_table_name, "update_acl");
  $soa_use_also_notify = db_column_exists($soa_table_name, "also_notify");

  $rr_extended_data = db_column_exists($rr_table_name, "edata");
  $rr_datalen = db_get_column_width($rr_table_name, 'data');

  $rr_use_active = db_column_exists($rr_table_name, "active");

  $allow_ixfr = ($rr_use_active &&
		 db_column_exists($rr_table_name, "stamp") &&
		 db_column_exists($rr_table_name, "serial"));

  /* Get possible column values for 'active' column */
  if ($soa_use_active)
    $soa_active_types = db_get_active_types($soa_table_name, 0);
  if ($soa_use_recursive)
    $soa_recursive_types = db_get_recursive_types($soa_table_name);
  if ($rr_use_active)
    $rr_active_types = db_get_active_types($rr_table_name, 1);
}
/*--- db_get_settings() -------------------------------------------------------------------------*/


/**************************************************************************************************
	DB_VALID_TYPE
	If the specified RR type is valid according to the database, returns 1, else 0.
**************************************************************************************************/
function db_valid_type($type) {
  global $db_valid_types;

  reset($db_valid_types);
  foreach ($db_valid_types as $db_type)
    if (!strcasecmp($db_type, $type))
      return 1;
  return 0;
}
/*--- db_valid_type() ---------------------------------------------------------------------------*/


/**************************************************************************************************
	SOA_SELECT
	Returns a SELECT statemnt for loading a SOA record, including the correct table name and
	optional field values.
**************************************************************************************************/
function soa_select() {
  global $soa_table_name, $soa_use_active,
    $soa_use_recursive, $soa_use_xfer, $soa_use_update_acl, $soa_use_also_notify;

  $select = "SELECT id,origin,ns,mbox,serial,refresh,retry,expire,minimum,ttl";
  if ($soa_use_active)
    $select.= ",active";
  if ($soa_use_recursive)
    $select.= ",recursive";
  if ($soa_use_xfer)
    $select.= ",xfer";
  if ($soa_use_update_acl)
    $select.= ",update_acl";
  if ($soa_use_also_notify)
    $select.= ",also_notify";

  $select .= " FROM $soa_table_name ";

  return ($select);
}
/*--- soa_select() ------------------------------------------------------------------------------*/


/**************************************************************************************************
	RR_SELECT
	Returns a SELECT statemnt for loading a RR record, including the correct table name and
	optional field values.
**************************************************************************************************/
function rr_select() {
  global $rr_table_name, $rr_use_active, $allow_ixfr, $rr_extended_data;;

  $select = "SELECT id,name,type,data,aux,ttl";
  if ($rr_extended_data)
    $select .= ",edata,edatakey";
  if ($rr_use_active)
    $select.= ",active";
  if ($allow_ixfr)
    $select .= ",stamp,serial";

  $select .= " FROM $rr_table_name ";

  return ($select);
}
/*--- rr_select() -------------------------------------------------------------------------------*/


/**************************************************************************************************
	ZONE_EXISTS
	Check to make sure the specified zone exists.  Returns 1 if so, 0 if not.
**************************************************************************************************/
function zone_exists($zone_id) {
  global $soa_table_name;

  return sql_count("SELECT COUNT(*) FROM $soa_table_name WHERE id=" . esc($zone_id),
		   "zone existence");
}
/*--- zone_exists() -----------------------------------------------------------------------------*/


/**************************************************************************************************
	HELP_SCREEN
	Output general instructions explaining how to use this interface.
**************************************************************************************************/
function help_screen() {
?>

<!-- BEGIN help box -->
<DIV align=center>
<DIV class=helpBox>
    <TABLE>
       <TR>
          <TD>
              Enter a search query in the input box located at the upper left corner of this page
              to edit DNS records.  Acceptable forms of input include:

              <P>

              <UL>
                  <LI><B>A fully qualified domain name (FQDN)</B><BR>
                      Display and edit all DNS data exactly matching the FQDN specified.
                      A FQDN is a full hostname, like &quot;<TT>speedy.example.com</TT>&quot;.<P>

		   <LI><B>A zone name</B><BR>
			  Display and edit all DNS data for the zone name specified.
			  A zone name is often a domain name, like &quot;<TT>example.com</TT>&quot;,
			  or an &quot;<TT>in-addr.arpa</TT>&quot; zone for reverse DNS, such as
						&quot;<TT>2.1.10.in-addr.arpa</TT>&quot;.<P>

		    <LI><B>A partial zone name</B><BR>
			   Displays all matches for the partial zone.  For example, you could enter
			   &quot;<TT>arpa</TT>&quot; to list all &quot;<TT>in-addr.arpa</TT>&quot; zones,
			   or &quot;<TT>com</TT>&quot; to list all &quot;<TT>.com</TT>&quot; zones.<P>
	       </UL>

		<P>

		Your mouse pointer can be hovered over almost any item on the screen
		for context-sensitive help, if your browser supports it.

		<DIV class=revBox>
		     <TABLE>
			 <TR>
			     <TD class=helpBtnLeft><IMG src="<?php echo $_SERVER['PHP_SELF']?>?img=browseReg.png" alt="BROWSE">
			     <TD class=helpBtnRight>activates the zone browser, which lists all the zones in your database.
			 <TR>
			     <TD class=helpBtnLeft><IMG src="<?php echo $_SERVER['PHP_SELF']?>?img=addReg.png" alt="NEW">
			     <TD class=helpBtnRight>allows you to add a new zone to the database from scratch.
		      </TABLE>
		</DIV>
	</TABLE>
</DIV>
</DIV>
<!-- END help box -->

<?php
  close_page();
}
/*--- help_screen() -----------------------------------------------------------------------------*/


/**************************************************************************************************
	SOA_EDITOR
	Outputs the top part of a zone edit page; the SOA editing form.
**************************************************************************************************/
function soa_editor($soa = NULL, $error_message = NULL) {
  global $soa_use_active, $soa_use_recursive, $soa_use_xfer, $soa_use_update_acl, $soa_use_also_notify;
  global $rr_table_name, $soa_active_types, $soa_recursive_types;
  global $default_refresh, $default_retry, $default_expire, $default_minimum_ttl, $default_ttl;
  global $default_ns, $default_mbox, $auto_defaults;
  global $soa_bgcolor, $zonenotify;

  $delete_confirm = 0;

  if ($soa) {							/* Editing SOA record */
    $update_button = formbutton("Update SOA",
				"Click this button to save any changes made to the fields above.",
				$soa_bgcolor);
    if ($zonenotify != "")
      $notify_button = formbutton("Notify Slaves",
				  "Click this button to notify slaves that the zone has changed",
				  $soa_bgcolor);
    else
      $notify_button = "";
		
    switch (strtolower(postvar('action'))) {
    case "delete zone":
      if (($rrct = zone_numrecs($soa['id'])) == 0)
	soa_delete();
      $delete_confirm = 1;
      $delete_button = formbutton("Really delete zone",
				  "Click this button to confirm deletion of this zone and all" .
				  " related resource records.", $soa_bgcolor);
      break;

    default:
      $delete_button = formbutton("Delete zone",
				  "Click this button to delete this zone and all related resource records.",
				  $soa_bgcolor);
      break;
    }
    $buttons = "$update_button $delete_button $notify_button";
    $new_soa = 0;
    $values = isset($_POST['origin']) ? $_POST : $soa;
  } else {							/* Adding new SOA */
    $create_button = formbutton("Add new SOA",
				"Click this button to add a SOA record for the fields specified above.",
				$soa_bgcolor);
    $buttons = "$create_button";

    /* Set default values for new SOA */
    if (isset($_POST['done']) && $_POST['done'] != "1")	{
      if ($soa_use_active)
	$soa['active'] = $soa_active_types[1];
      if ($soa_use_recursive)
	$soa['recursive'] = $soa_recursive_types[0];
      if (($soa['serial'] = auto_next_serial(1)) == 1)
        date_default_timezone_set('UTC');
	$soa['serial'] = date("Ymd01", time());		/* Use RFC1912 by default */
      $soa['refresh'] = $default_refresh;
      $soa['retry'] = $default_retry;
      $soa['expire'] = $default_expire;
      $soa['minimum'] = $default_minimum_ttl;
      $soa['ttl'] = $default_ttl;
      $soa['ns'] = trim($default_ns);
      $soa['mbox'] = trim($default_mbox);
      $values = $soa;
    } else
      $soa = $values = $_POST;
    $new_soa = 1;

       if ($auto_defaults == 1) {
                        date_default_timezone_set('UTC');
                        $soa['serial'] = date("Ymd01", time());
                        $soa['refresh'] = $default_refresh;
                        $soa['retry'] = $default_retry;
                        $soa['expire'] = $default_expire;
                        $soa['minimum'] = $default_minimum_ttl;
                        $soa['ttl'] = $default_ttl;
                        $soa['ns'] = trim($default_ns);
                        $soa['mbox'] = trim($default_mbox);
                        $values = $soa;
                        }
  }

  /* Set 'values' vars to avoid 'undefined' errors */
  foreach (array('origin', 'ttl', 'ns', 'mbox', 'serial', 'refresh', 'retry', 'expire', 'minimum') as $n)
    if (!isset($values[$n]))
      $values[$n] = '';
?>
<!-- BEGIN SOA editor -->
<FORM method=POST action="<?php echo $_SERVER['PHP_SELF']?>" name="soaform">
<DIV>
<?php
   if ($new_soa)
     echo "<INPUT type=hidden name=\"done\" value=\"1\">\n";
   else
     echo "<INPUT type=hidden name=\"zone\" value=\"" 
       . (isset($_POST['zone']) ? $_POST['zone'] : $soa['id']) . "\">\n";
  $query = getpostvar('query');
  if (strlen($query))
    echo "<INPUT type=hidden name=\"query\" value=\"" . ent($query) . "\">\n";
  $page = getpostvar('page');
  if (strlen($page))
    echo "<INPUT type=hidden name=\"page\" value=\"$page\">\n";
?>
</DIV>
<DIV align=center>
<?php
// Ensure that we notice INACTIVE zones
if ($soa_use_active){
	if (isset($values['active']) && getbool($values['active'])){
	echo "<TABLE class=soaBox>";
	}else{
	echo "<TABLE class=inactivesoaBox>";
	}
}
?>
<!---TABLE class=soaBox--->
<TR><TD class=soaFirstRow nowrap>
<?php
    if ($soa_use_active) {
      echo "<INPUT class=activeBox type=checkbox name=\"active\" value=\"";
	echo $soa_active_types[1] . "\"";
      if (isset($values['active']) && getbool($values['active'])) {
	echo " checked=\"checked\"";
      }
      echo " title=\"Uncheck this box to deactivate this zone.\">&nbsp;";
    }
?>
<INPUT class=mono type=text name="origin" value="<?php echo ent($values['origin'])?>" maxlength=78 size=30
	title="The name of this zone.">&nbsp;
<INPUT class=mono type=text name="ttl" value="<?php echo ent($values['ttl'])?>" maxlength=15 size=6
	title="The cache expiration time (TTL) for this zone.">&nbsp;
<TT>IN SOA</TT>&nbsp;
<INPUT class=mono type=text name="ns" value="<?php echo ent($values['ns'])?>" maxlength=255 size=30
	title="The primary authoritative name server for this zone.">&nbsp;
<INPUT class=mono type=text name="mbox" value="<?php echo ent($values['mbox'])?>" maxlength=255 size=35
	title="The email address of the person responsible for this zone, with the '@' replaced by a dot.">&nbsp;
<TT><B>(</B></TT>

<TABLE class=soaFields>
	<TR title="The serial number for this zone.">
		<TD class="soaFields"><INPUT class=mono type=text name="serial" value="<?php echo ent($values['serial'])?>" size=20>
		<TD>&nbsp;
                <TD nowrap><TT>; <B>Serial</B> (next is <?=next_serial(isset($soa['serial']) ? $soa['serial'] : '');?>)</TT>
                <TD><input name="updateserial" class=formButton style="border-color: #FFFF99; color: #FFFF99;" type="button" value="Update Serial" onclick="javascript:document.soaform.serial.value
=<?=next_serial(isset($soa['serial']) ? $soa['serial'] : '');?>">&nbsp;
	<TR title="The number of seconds slave nameservers will wait before updating their zone data for this zone.">
		<TD class=soaFields><INPUT class=mono type=text name="refresh" value="<?php echo ent($values['refresh'])?>" size=10>
		<TD>&nbsp;
		<TD nowrap><TT>; <B>Refresh</B> (currently <?php echo duration(isset($soa['refresh']) ? $soa['refresh'] : '')?>)</TT>
		<TD>&nbsp;

	<TR title="The number of seconds slave nameservers will wait before retrying a zone transfer if the last one failed.">
		<TD class=soaFields><INPUT class=mono type=text name="retry" value="<?php echo ent($values['retry'])?>" size=10>
		<TD>&nbsp;
		<TD nowrap><TT>; <B>Retry</B> (currently <?php echo duration(isset($soa['retry']) ? $soa['retry'] : '')?>)</TT>
		<TD>&nbsp;

	<TR title="The number of seconds after which slave nameservers will give up trying to transfer authoritative data if the master server cannot be reached.">
		<TD class=soaFields><INPUT class=mono type=text name="expire" value="<?php echo ent($values['expire'])?>" size=10>
		<TD>&nbsp;
		<TD nowrap><TT>; <B>Expire</B> (currently <?php echo duration(isset($soa['expire']) ? $soa['expire'] : 0)?>)</TT>
		<TD>&nbsp;

	<TR title="The minimum TTL (cache timeout) value to list for any resource records in this zone.">
		<TD class=soaFields><INPUT class=mono type=text name="minimum" value="<?php echo ent($values['minimum'])?>" size=10>
		<TD><TT><B>)</B></TT>
		<TD nowrap><TT>; <B>Minimum TTL</B> (currently <?php echo duration(isset($soa['minimum']) ? $soa['minimum'] : '')?>)</TT>
		<TD align=right width="100%" valign=bottom><?php echo $buttons?>

<?php
    if ($soa_use_recursive) {
      echo "<TR title=\"Record is a recursive marker only.\"><TD class=soaFields nowrap>";
      echo "<INPUT class=recursiveBox type=checkbox name=\"recursive\" value=\"";
      if (isset($values['recursive']) && getbool($values['recursive'])) {
	echo $soa_recursive_types[1] . "\"";
	echo " checked";
      } else {
	echo $soa_recursive_types[0] . "\"";
      }
      echo " title=\"Check this box to make the zone recursive only.\">&nbsp;";
      echo "<TD nowrap><TT> <B>Recurse for real data</B></TT>";
    }
?>

</TABLE>
</TABLE>
</DIV>
</FORM>
<!-- END SOA editor -->

<?php
    /* Output error message, if any */
    if ($error_message)
      ErrBox($error_message);

  /* If there's an 'xfer' column in the soa table, allow it */
  if ($soa_use_xfer && !$new_soa) {
?>
<FORM method=POST action="<?php echo $_SERVER['PHP_SELF']?>">
	<INPUT type=hidden name="zone" value="<?php echo ent($values['id'])?>">
	<DIV align=center>
		<TABLE class=xferBox>
			<TR title="Comma-separated list of IP addresses allowed to transfer this zone via AXFR.  Wildcards are OK.">
				<TD><B>Zone transfer access list:</B>
				<TD>&nbsp; &nbsp;
				<TD><INPUT class=mono type=text name="xfer" value="<?php echo ent($values['xfer'])?>" size=60>
				<TD>&nbsp; &nbsp;
				<TD><?php echo formbutton("Update list",
					"Click this button to save changes to the zone transfer access list.", $soa_bgcolor)?>
		</TABLE>
	</DIV>
</FORM>

<?php
    }

    /* If there's an 'update_acl' column in the soa table, allow it */
    if ($soa_use_update_acl && !$new_soa) {
?>
<FORM method=POST action="<?php echo $_SERVER['PHP_SELF']?>">
	<INPUT type=hidden name="zone" value="<?php echo ent($values['id'])?>">
	<DIV align=center>
		<TABLE class=updateAclBox>
			<TR title="Comma-separated list of IP addresses allowed to update this zone.  Wildcards are OK.">
				<TD><B>Update access list:</B>
				<TD>&nbsp; &nbsp;
				<TD><INPUT class=mono type=text name="update_acl" value="<?php echo ent($values['update_acl'])?>" size=60>
				<TD>&nbsp; &nbsp;
				<TD><?php echo formbutton("Update ACL for updates",
					"Click this button to save changes to the update access list.", $soa_bgcolor)?>
		</TABLE>
	</DIV>
</FORM>

<?php
    }

    /* If there's an 'also_notify' column in the soa table, allow it */
    if ($soa_use_also_notify && !$new_soa) {
?>
<FORM method=POST action="<?php echo $_SERVER['PHP_SELF']?>">
	<INPUT type=hidden name="zone" value="<?php echo ent($values['id'])?>">
	<DIV align=center>
		<TABLE class=alsoNotifyBox>
			<TR title="Comma-separated list of IP addresses to be notified in addition to listed NS records.  '*' indicates just NS entries">
				<TD><B>Also notify list:</B>
				<TD>&nbsp; &nbsp;
				<TD><INPUT class=mono type=text name="also_notify" value="<?php echo ent($values['also_notify'])?>" size=60>
				<TD>&nbsp; &nbsp;
				<TD><?php echo formbutton("Update Also Notify",
					"Click this button to save changes to the also notify list.", $soa_bgcolor)?>
		</TABLE>
	</DIV>
</FORM>

<?php
    }

    /* If the user's trying to delete a zone, let them know that everything'll be deleted */
    if ($delete_confirm)
      Warn(
	   "<B>Deleting this zone will also delete " . nf($rrct) .
	   " resource record" . S($rrct) . " associated with this zone.</B>\n" .
	   "<P>" .
	   "To confirm deletion of this zone, click the <B>Really delete zone</B> button above."
	   );
}
/*--- soa_editor() ------------------------------------------------------------------------------*/


/**************************************************************************************************
	SOA_VALIDATE
	Validates the values for user-provided SOA data.  Calls zone_editor() if errors are found.
**************************************************************************************************/
function soa_validate($soa, $new = 0) {
  global $refresh_min, $retry_min, $expire_min, $ttl_min, $ignore_minimum_ttl;

  $errors = array();

  /* Validate 'origin' */
  if (!strlen($soa['origin']))
    $errors[] = "You must specify an origin for this zone.";
  if (!ends_with_dot($soa['origin']))
    $errors[] = "Zone origin (" . quote($soa['origin']) . ") must end with a dot.";
  validate_name($soa['origin'], "Zone origin", $errors, 0);

  /* Validate 'ttl' */
  check_value32($soa['ttl'], "TTL", $errors, $ttl_min);
  if (!$ignore_minimum_ttl && ($soa['ttl'] < $soa['minimum']))
    $errors[] = "Cache expiration time (TTL) for SOA is less than the minimum allowed for this zone.";

  /* Validate 'ns' */
  if (!strlen($soa['ns']))
    $errors[] = "You must specify a primary nameserver for this zone.";
  validate_name($soa['ns'], "Primary nameserver", $errors, 0);

  /* Validate 'mbox' */
  if (!strlen($soa['mbox']))
    $errors[] = "You must specify an administrator mailbox address for this zone.";
  validate_name($soa['mbox'], "Administrator mailbox address", $errors, 0, NULL, 1);

  /* Validate 'serial' */
  check_value32($soa['serial'], "Serial", $errors);

  /* Validate 'refresh' */
  check_value32($soa['refresh'], "Refresh", $errors, $refresh_min);

  /* Validate 'retry' */
  check_value32($soa['retry'], "Retry", $errors, $retry_min);

  /* Validate 'expire' */
  check_value32($soa['expire'], "Expire", $errors, $expire_min);

  /* Validate 'minimum' */
  check_value32($soa['minimum'], "Minimum TTL", $errors, $ttl_min);

  /* Output errors, if any were found. */
  if (count($errors)) {
    if (count($errors) == 1)
      $msg = "<B>The following error was detected:</B>";
    else
      $msg = "<B>The following errors were detected:</B>";
    $msg .= "<UL>\n";
    foreach ($errors as $error)
      $msg .= "\t<LI>$error\n";
    $msg .= "</UL>\n";

    if (count($errors) == 1)
      $msg .= "Please correct this error and try again.";
    else
      $msg .= "Please correct these errors and try again.";

    if ($new)
      soa_editor(NULL, $msg);
    else
      zone_editor($soa['id'], $msg);
    exit;
  }
}
/*--- soa_validate() ----------------------------------------------------------------------------*/


/**************************************************************************************************
	SOA_POST_VARS
	Given the post vars, fills in and returns an appropriate array.
**************************************************************************************************/
function soa_post_vars() {
  global $soa_active_types, $soa_recursive_types;

  $soa = array();

  $soa['id'] = (int)postvar('zone');
  $soa['active'] = $soa_active_types[getbool(postvar('active'))];
  $soa['recursive'] = $soa_recursive_types[getbool(postvar('recursive'))];
  $soa['ttl'] = gettime(postvar('ttl'));
  $soa['origin'] = trim(postvar('origin'));
  $soa['ns'] = trim(postvar('ns'));
  $soa['mbox'] = trim(postvar('mbox'));
  $soa['serial'] = auto_next_serial((int)postvar('serial'));
  $soa['refresh'] = gettime(postvar('refresh'));
  $soa['retry'] = gettime(postvar('retry'));
  $soa['expire'] = gettime(postvar('expire'));
  $soa['minimum'] = gettime(postvar('minimum'));

  return ($soa);
}
/*--- soa_post_vars() ---------------------------------------------------------------------------*/


/**************************************************************************************************
	SOA_UPDATE
	User clicked the "Update SOA" button; update the SOA record.
**************************************************************************************************/
function soa_update() {
  global $soa_table_name, $soa_use_active, $soa_use_recursive;

  $soa = soa_post_vars();
  soa_validate($soa, 0);

  $query = "UPDATE $soa_table_name SET" .
    " origin='" . esc($soa['origin']) . "'" .
    ",ns='" . esc($soa['ns']) . "'" .
    ",mbox='" . esc($soa['mbox']) . "'" .
    ",serial=" . (int)$soa['serial'] .
    ",refresh=" . (int)$soa['refresh'] .
    ",retry=" . (int)$soa['retry'] .
    ",expire=" . (int)$soa['expire'] .
    ",minimum=" . (int)$soa['minimum'] .
    ",ttl=" . (int)$soa['ttl'];
  
  if ($soa_use_active)
    $query .= ",active='" . esc($soa['active']) . "'";

  if ($soa_use_recursive)
    $query .= ",recursive='" . esc($soa['recursive']) . "'";

  $query .= " WHERE id=" . (int)$soa['id'];
  
  sql_query($query)
    or ErrSQL("Error updating SOA record for zone " . (int)$soa['id'] . ".");
  
  zone_redirect($soa['id']);
}
/*--- soa_update() ------------------------------------------------------------------------------*/


/**************************************************************************************************
	NOTIFY_SLAVES
	User clicked the "Notify Slaves" button; send notify message to all slaves.
**************************************************************************************************/
function notify_slaves() {
  global $soa_table_name, $soa_use_active, $soa_use_recursive;
  global $rr_table_name, $rr_use_active, $rr_active_types;
  global $soa_use_also_notify, $zonenotify;

  $soa = soa_post_vars();
  soa_validate($soa, 0);

  $nce = array();
  $nce[] = $zonenotify;
  $nce[] = $soa['origin'];

  $query = rr_select() . "WHERE zone=" . (int)$soa['id']
    . " AND type='NS'";

  if ($rr_use_active)
    $query .= " AND active='" . $rr_active_types[1] . "'";

  $res = sql_query($query)
    or ErrSQL("Error retrieving NS records for notify from zone " . (int)$soa['id'] . ".");

  if (sql_num_rows($res)) {
    while ($rr = sql_fetch_array($res)) {
      $data = $rr['data'];
      if (!strchr($data, '.')) $data .= "." . $soa['origin'];
      $nce[] = $data;
    }
  }
  
  if ($soa_use_also_notify) {
    $query = soa_select() . "WHERE id=" . (int)$soa['id'];

    $res = sql_query($query)
      or ErrSQL("Error retrieving Also Notify record from zone ". (int)$soa['id'] . ".");

    if (sql_num_rows($res)) {
      $also_notifiers = sql_fetch_array($res);
      $also_notify = $also_notifiers['also_notify'];

      if ($also_notify != "") {
	$servers = explode(",", $also_notify);
	foreach ($servers as $server) {
	  $nce[] = $server;
	}
      }
    }
  }

  $shellcmd = "nice " . implode(" ", $nce) . " > /dev/null 2>&1 &";
  print $shellcmd . "\n";

  exec($shellcmd);

  zone_redirect($soa['id']);
}
/*--- notify_slaves() ------------------------------------------------------------------------------*/


/**************************************************************************************************
	SOA_XFER_UPDATE
	User clicked the "Update list" button; update the 'xfer' value for SOA.
**************************************************************************************************/
function soa_xfer_update() {
  global $soa_table_name;

  sql_query("UPDATE $soa_table_name SET xfer='" . esc($_POST['xfer']) .
	    "' WHERE id=" . esc($_POST['zone']))
    or ErrSQL("Error updating zone transfer access for zone " . (int)$soa['id'] . ".");

  zone_redirect($_POST['zone']);
}
/*--- soa_xfer_update() -------------------------------------------------------------------------*/


/**************************************************************************************************
	SOA_UPDATE_ACL_UPDATE
	User clicked the "Update ACL for updates" button; update the 'update_acl' value for SOA.
**************************************************************************************************/
function soa_update_acl_update() {
  global $soa_table_name;

  sql_query("UPDATE $soa_table_name SET update_acl='" . esc($_POST['update_acl']) .
	    "' WHERE id=" . esc($_POST['zone']))
    or ErrSQL("Error updating update access for zone " . (int)$soa['id'] . ".");

  zone_redirect($_POST['zone']);
}
/*--- soa_update_acl_update() -------------------------------------------------------------------------*/


/**************************************************************************************************
	SOA_ALSO_NOTIFY_UPDATE
	User clicked the "Update Also Notify" button; update the 'also_notify' value for SOA.
**************************************************************************************************/
function soa_also_notify_update() {
  global $soa_table_name;

  sql_query("UPDATE $soa_table_name SET also_notify='" . esc($_POST['also_notify']) .
	    "' WHERE id=" . esc($_POST['zone']))
    or ErrSQL("Error updating also notify list for zone " . (int)$soa['id'] . ".");

  zone_redirect($_POST['zone']);
}
/*--- soa_also_notify_update() -------------------------------------------------------------------------*/


/**************************************************************************************************
	SOA_DELETE
	Delete zone and all related resource records.
**************************************************************************************************/
function soa_delete() {
  global $soa_table_name, $rr_table_name;

  sql_query("DELETE FROM $rr_table_name WHERE zone=" . (int)$_POST['zone'])
    or ErrSQL("Error deleting resource records for zone " . (int)$_POST['zone'] . ".");
  sql_query("DELETE FROM $soa_table_name WHERE id=" . (int)$_POST['zone'])
    or ErrSQL("Error deleting SOA record for zone " . (int)$_POST['zone'] . ".");

  Notice("<B>Zone " . nf((int)$_POST['zone']) . " deleted.</B>");
  close_page();
}
/*--- soa_delete() ------------------------------------------------------------------------------*/


/**************************************************************************************************
	SOA_ADD
	Add a new SOA record.
**************************************************************************************************/
function soa_add() {
  global $soa_table_name, $soa_use_active, $soa_use_recursive;

  $soa = soa_post_vars();

  /* Check to see if this zone already exists */
  if (($id = sql_count("SELECT id FROM $soa_table_name WHERE origin='" . esc($soa['origin']) . "'",
		       "existing SOA record count"))) {
    ErrBox("A zone with that origin (" . quote($soa['origin']) . ") " .
	   "<A href=\"{$_SERVER['PHP_SELF']}?zone=$id\">already exists</A>.");
    soa_editor(NULL);
    exit;
  }

  soa_validate($soa, 1);
  
  $active = ($soa_use_active ? ",active" : "");
  $recursive = ($soa_use_recursive ? ",recursive" : "");

  $query = "INSERT INTO $soa_table_name" .
    " (origin,ns,mbox,serial,refresh,retry,expire,minimum,ttl$active) VALUES (";

  $query .= "'" . esc($soa['origin']) . "'";
  $query .= ",'" . esc($soa['ns']) . "'";
  $query .= ",'" . esc($soa['mbox']) . "'";
  $query .= ",'" . esc($soa['serial']) . "'";
  $query .= ",'" . esc($soa['refresh']) . "'";
  $query .= ",'" . esc($soa['retry']) . "'";
  $query .= ",'" . esc($soa['expire']) . "'";
  $query .= ",'" . esc($soa['minimum']) . "'";
  $query .= ",'" . esc($soa['minimum']) . "'";
  if ($soa_use_active)
    $query .= ",'" . esc($soa['active']) . "'";
  if ($soa_use_recursive)
    $query .= ",'" . esc($soa['recursive']) . "'";

  $query .= ")";

  sql_query($query)
    or ErrSQL("Error creating new SOA record.");

  $new_soa = sql_result(soa_select() . "WHERE origin='" . esc($soa['origin']) . "'",
			"SOA record for origin " . $soa['origin']);

  add_default_rr($new_soa);
  
  zone_editor($new_soa['id']);
}
/*--- soa_add() ---------------------------------------------------------------------------------*/


/**************************************************************************************************
	ADD_DEFAULT_RR
	Add default resource records for the specified zone.
**************************************************************************************************/
function add_default_rr($soa) {
  global $default_records, $rr_table_name;

  foreach ($default_records as $rr) {
    $query = "INSERT INTO $rr_table_name" .
      " (zone,name,type,data,aux,ttl) VALUES (";
    $query .= (int)$soa['id'];
    $query .= ",'" . esc($rr[0]) . "'";
    $query .= ",'" . esc($rr[1]) . "'";
    $query .= ",'" . esc($rr[3]) . "'";
    $query .= "," . (int)$rr[2];
    $query .= "," . (int)$soa['ttl'];
    $query .= ")";
    sql_query($query) or ErrSQL("Error adding new resource record to zone {$soa['id']}.");
  }
}
/*--- add_default_rr() --------------------------------------------------------------------------*/


/**************************************************************************************************
	ZONE_REDIRECT
	Redirects browser to zone editor for the specified zone.
**************************************************************************************************/
function zone_redirect($zone_id) {
  ob_clean();

  if (($page = (int)($_POST['page'] ? $_POST['page'] : $_GET['page'])))
    $xtra = "&page=$page";
  if (($query = $_POST['query'] ? $_POST['query'] : $_GET['query']))
    $xtra .= "&query=" . urlencode($query);
  
  header("Location: {$_SERVER['PHP_SELF']}?zone=$zone_id$xtra");
}
/*--- zone_redirect() ---------------------------------------------------------------------------*/


/**************************************************************************************************
	ZONE_NUMRECS
	Returns the number of resource records in the specified zone.
**************************************************************************************************/
function zone_numrecs($zone_id) {
  global $rr_table_name;

  return sql_count("SELECT COUNT(*) FROM $rr_table_name WHERE zone=" . (int)$zone_id,
		   "number of resource records for zone $zone_id");
}
/*--- zone_numrecs() ----------------------------------------------------------------------------*/


/**************************************************************************************************
	ZONE_BROWSER
	Displays a list of known zones, allowing the user to navigate the list and click on a zone
	to edit it.
**************************************************************************************************/
function zone_browser($like = NULL, $highlight = NULL) {
  global $zone_group_size, $soa_table_name, $rr_table_name, $use_pgsql;

  echo "<DIV align=center>\n";

  if ($zone_group_size == 0) {
    $res = sql_query(soa_select() . " ORDER BY origin")
      or ErrSQL("Error loading SOA record(s).");
  } else {
    /* Get current offset and total number of zones */
    $page = getpostvar('page');
    $total = sql_count("SELECT COUNT(*) FROM $soa_table_name $like", "number of SOA records");

    $offset = offset_select($page, $total, $zone_group_size, "action=browse");
    $query = soa_select() . " $like ORDER BY origin ";

    if ($use_pgsql)
      $query .= "LIMIT $zone_group_size OFFSET $offset";
    else
      $query .= "LIMIT $offset,$zone_group_size";

    $res = sql_query($query) or ErrSQL("Error loading SOA record(s).");
  }
?>
<TABLE class=browserBox cellspacing=0>

<?php

   while ($soa = sql_fetch_array($res)) {
     $record_count = sql_count("SELECT COUNT(*) FROM $rr_table_name WHERE zone={$soa['id']}",
			       "number of resource records in zone {$soa['id']}");

     $output_origin = $soa['origin'];
     if ($highlight)
       $output_origin = str_replace($highlight, "<span class=highlight>$highlight</span>",
				    $output_origin);
?>
	<TR bgcolor="<?php echo bgcolor();?>">
		<TD class=browserCellLeft><A href="<?php echo $_SERVER['PHP_SELF']?>?zone=<?php echo $soa['id']?>"
			title="Edit zone <?php echo $soa['id']?>"><?php echo $output_origin?></A>
		<TD class=browserCellRight><?php echo nf($record_count);?> record(s)
<?php
   }

  echo "</TABLE>\n";
  echo "</DIV>\n";
}
/*--- zone_browser() ----------------------------------------------------------------------------*/


/**************************************************************************************************
	ZONE_EDITOR
	Output zone editor for the specified zone.
**************************************************************************************************/
function zone_editor($zone, $soa_error_message = NULL, $rr_error_message = NULL) {
  $soa = sql_result(soa_select() . "WHERE id=$zone", "SOA record for zone $zone");
  rr_editor($soa, $soa_error_message, $rr_error_message);
  close_page();
}
/*--- zone_editor() -----------------------------------------------------------------------------*/


/**************************************************************************************************
	RR_TYPE_OPTIONS
	Returns a list of resource record types.
**************************************************************************************************/
function _rr_type_option($type, $which) {
  return "<OPTION" . (!strcasecmp($type, $which) ? " selected" : "") . ">$which";
}

function rr_type_options($type) {
  global $db_valid_types;

  reset($db_valid_types);
  $rv = '';
  foreach ($db_valid_types as $which)
    $rv .= _rr_type_option($type, $which);
  return $rv;
}
/*--- rr_type_options() -------------------------------------------------------------------------*/


/**************************************************************************************************
	RR_POST_VARS
	Given the POST vars, fills in and returns an appropriate array.
**************************************************************************************************/
function rr_post_vars() {
  global $soa, $rr_active_types;

  $rr = array();

  $rr['id'] = (int)postvar('id');
  $rr['active'] = $rr_active_types[gettrinary(postvar('active'))];
  $rr['serial'] = postvar('serial');
  $rr['stamp'] = postvar('stamp');
  $rr['name'] = trim(postvar('name'));
  $rr['ttl'] = gettime(postvar('ttl'));
  $rr['type'] = strtoupper(trim(postvar('type')));
  $rr['aux'] = (int)postvar('aux');
  $rr['data'] = trim(postvar('data'));

  if (!strlen($rr['name']))
    $rr['name'] = $soa['origin'];

  return $rr;
}
/*--- rr_post_vars() ----------------------------------------------------------------------------*/


/**************************************************************************************************
	RR_VALIDATE_IP4
	Given an IPv4 address, verifies that it is in correct numbers-and-dots format.
	Returns 1 if an error was found, else 0.
**************************************************************************************************/
function rr_validate_ip4(&$errors, &$ip) {
  $quad = explode(".", $ip);
  if (count($quad) != 4) {
    $errors[] = "Format of IP address (in resource record data) is invalid.";
    return 1;
  }
  for ($n = 0; $n < 4; $n++) {
    $q = $quad[$n];
    if ((int)$q < 0 || (int)$q > 255) {
      $errors[] = "Format of IP address (in resource record data) is invalid.";
      return 1;
    }
  }
  $ip = (int)$quad[0] . "." . (int)$quad[1] . "." . (int)$quad[2] . "." . (int)$quad[3];
  return 0;
}
/*--- rr_validate_ip4() -------------------------------------------------------------------------*/


/**************************************************************************************************
	RR_VALIDATE_IP6
	Given an IPv6 address, verifies that it is in correct format.
	Returns 1 if an error was found, else 0.
	TODO: Improve this to validate the actual value.
**************************************************************************************************/
function rr_validate_ip6(&$errors, &$ip) {
  /* List of valid characters for an IPv6 address */
  $valid_chars = "ABCDEFabcdef1234567890:";

  /* Make sure all characters are valid */
  if (strspn($ip, $valid_chars) != strlen($ip)) {
    $errors[] = "Format of IPv6 address (in resource record data) is invalid.";
    return 1;
  }
  return 0;
}
/*--- rr_validate_ip6() -------------------------------------------------------------------------*/


/**************************************************************************************************
	RR_VALIDATE_SRV_DATA
	Given SRV record data, validates the data.
	Should be in the format:  WEIGHT <sp> PORT <sp> TARGET
	Returns 1 if an error was found, else 0.
**************************************************************************************************/
function rr_validate_srv_data(&$errors, &$data, $origin) {
  $fields = explode(" ", $data);
  if (count($fields) != 3) {
    $errors[] = "SRV record data has invalid format.  The correct format is the " .
      "<B>weight</B> (0-65535), then a space, then the <B>port</B> (0-65535), then a space," .
      " then the <B>target</B>.";
    return 1;
  }
  $weight = $fields[0];
  $port = $fields[1];
  $target = $fields[2];
  if ($weight < 0 || $weight > 65535) {
    $errors[] = "Weight (" . quote($weight) . ") for SRV record is out of range.";
    return 1;
  }
  if ($port < 0 || $port > 65535) {
    $errors[] = "Port (" . quote($port) . ") for SRV record is out of range.";
    return 1;
  }

  if (validate_name($target, "SRV target", $errors, 0, $origin))
    return 1;

  $data = (int)$weight . " " . (int)$port . " " . $target;
  return 0;
}
/*--- rr_validate_srv_data() --------------------------------------------------------------------*/


/**************************************************************************************************
	RR_VALIDATE_RP_DATA
	Given RP record data, validates the data.
	Should be in the format:  MBOX <sp> TXTREC
	Returns 1 if an error was found, else 0.
**************************************************************************************************/
function rr_validate_rp_data(&$errors, &$data, $origin) {
  $fields = explode(" ", $data);
  if (count($fields) != 2) {
    $errors[] = "RP record data has invalid format.  The correct format is the " .
      "<B>mbox</B> (a DNS-encoded email address), then a space, then the <B>txtref</B>, which " .
      "should contain either a host for TXT lookup or a dot.";
    return 1;
  }
  $mbox = $fields[0];
  $txtref = $fields[1];

  if (validate_name($mbox, "RP mbox", $errors, 0, $origin))
    return 1;
  if (validate_name($txtref, "RP txtref", $errors, 0, $origin))
    return 1;

  $data = $mbox . " " . $txtref;
  return 0;
}
/*--- rr_validate_rp_data() ---------------------------------------------------------------------*/

function rr_validate_txt_data(&$errors, &$data, $origin) {
  global $rr_maxtxtlen, $rr_maxtxtelemlen;
  if (strlen($data) > $rr_maxtxtlen) {
    $errors[] = "TXT record length is greater than that allowed in the current implementation ".
      strlen($data) . " > " . $rr_maxtxtlen;
    return 1;
  }
  foreach (explode('\0', $data) as $elem) {
    if (strlen($elem) > $rr_maxtxtelemlen) {
      $errors[] = "TXT record element length is greater than allowed ".
	strlen($elem) . " > " . $rr_maxtxtelemlen;
      return 1;
    }
  }
  /* Check each element as well */
  return 0;
}
/**************************************************************************************************
	RR_VALIDATE_TYPE
	Type-specific RR data validation.
**************************************************************************************************/
function rr_validate_type($soa, &$rr, &$errors) {
  switch ($rr['type']) {
  case "A":
    rr_validate_ip4($errors, $rr['data']);
    break;

  case "AAAA":
    rr_validate_ip6($errors, $rr['data']);
    break;

  case "ALIAS":
    validate_name($rr['data'], "ALIAS data", $errors, 0, $soa['origin']);
    break;

  case "CNAME":
    validate_name($rr['data'], "CNAME data", $errors, 0, $soa['origin']);
    break;

  case "HINFO":
    if (!strchr($rr['data'], ' '))
      $errors[] = "Invalid HINFO data.  Data should contain the CPU type, then a space, then the OS type.";
    break;

  case "MX":
    validate_name($rr['data'], "MX data", $errors, 0, $soa['origin']);
    break;

  case "NS":
    validate_name($rr['data'], "NS data", $errors, 0, $soa['origin']);
    break;

  case "PTR":
    validate_name($rr['data'], "PTR data", $errors, 0, $soa['origin']);
    if (!ends_with_dot($rr['data']))
      $errors[] = "Invalid PTR data.  Data must specify a fully qualified domain name, ending with a dot.";
    break;

  case "RP":
    rr_validate_rp_data($errors, $rr['data'], $soa['origin']);
    break;

  case "SRV":
    rr_validate_srv_data($errors, $rr['data'], $soa['origin']);
    break;

  case "TXT":
    rr_validate_txt_data($errors, $rr['data'], $soa['origin']);
    break;
  }
  return (count($errors));
}
/*--- rr_validate_type() ------------------------------------------------------------------------*/


/**************************************************************************************************
	RR_VALIDATE
	Validates the values for user-provided RR data.
**************************************************************************************************/
function rr_validate($soa, &$rr, $new = 0) {
  global $ttl_min, $ignore_minimum_ttl;

  $errors = array();

  /* Validate 'name' */
  validate_name($rr['name'], "Name", $errors, 1, $soa['origin']);

  /* Validate 'ttl' */
  check_value32($rr['ttl'], "TTL", $errors, $ttl_min);
  if (!$ignore_minimum_ttl && ($rr['ttl'] < $soa['minimum']))
    $errors[] = "Cache expiration time (TTL) for RR is less than the minimum allowed for this zone.";

  /* Validate 'type' */
  if (!db_valid_type($rr['type']))
    $errors[] = "Resource record type specified (" . quote($rr['type']) . ") is unknown.";

  /* Validate 'data' */
  if (!strlen($rr['data']))
    $errors[] = "You must specify some data for this resource record.";

  /* Type-specific data checks */
  rr_validate_type($soa, $rr, $errors);

  /* Output errors, if any were found. */
  if (count($errors)) {
    if (count($errors) == 1)
      $msg = "<B>The following error was detected:</B>";
    else
      $msg = "<B>The following errors were detected:</B>";
    $msg .= "<UL>\n";
    foreach ($errors as $error)
      $msg .= "\t<LI>$error\n";
    $msg .= "</UL>\n";

    if (count($errors) == 1)
      $msg .= "Please correct this error and try again.";
    else
      $msg .= "Please correct these errors and try again.";

    zone_editor($soa['id'], NULL, $msg);
    exit;
  }
}
/*--- rr_validate() -----------------------------------------------------------------------------*/


/**************************************************************************************************
	SOA_UPDATE_SERIAL
	Updates serial number for the provided zone if '$auto_update_serial' is set.
**************************************************************************************************/
function soa_update_serial($soa) {
  global $soa_table_name, $auto_update_serial;

  if (!$auto_update_serial)
    return;

  /* Update the SOA record */
  $query = "UPDATE $soa_table_name SET serial=" . (int)next_serial($soa['serial']);
  $query .= " WHERE id=" . (int)$soa['id'];
  sql_query($query)
    or ErrSQL("Error updating SOA record for zone " . (int)$soa['id'] . ".");
}
/*--- soa_update_serial() -----------------------------------------------------------------------*/


/**************************************************************************************************
	PTR_CREATE_SOA
	For automatic PTR maintenance.
	Find a SOA record that could contain the IP address provided, or create one if it doesn't
	exist.  Returns the SOA data.  Sets '$name' to the name part of the relevant resource record.
**************************************************************************************************/
function ptr_create_soa($ip, &$name) {
  global $soa_table_name, $soa_use_active, $soa_use_recursive, $default_ns, $default_mbox;
  global $soa_active_types, $soa_recursive_types;
  global $default_refresh, $default_retry, $default_expire, $default_minimum_ttl, $default_ttl;

  /* Make sure default_ns and default_mbox have at least a slightly reasonable value */
  if (!strlen($default_ns))
    $default_ns = $_SERVER['HTTP_HOST'] . ".";
  if (!strlen($default_mbox))
    $default_mbox = "hostmaster." . $default_ns;

  $quad = explode('.', $ip);

  /* Look for 0.in-addr.arpa. */
  $origin = (int)$quad[0] . ".in-addr.arpa.";
  $name = (int)$quad[3] . "." . (int)$quad[2] . "." . (int)$quad[1];
  if ($soa = sql_result(soa_select() . "WHERE origin='".esc($origin)."'", "SOA record for PTR data"))
    return $soa;

  /* Look for 1.0.in-addr.arpa. */
  $origin = (int)$quad[1] . "." . $origin;
  $name = (int)$quad[3] . "." . (int)$quad[2];
  if ($soa = sql_result(soa_select() . "WHERE origin='".esc($origin)."'", "SOA record for PTR data"))
    return $soa;

  /* Look for 2.1.0.in-addr.arpa. */
  $origin = (int)$quad[2] . "." . $origin;
  $name = (int)$quad[3];
  if ($soa = sql_result(soa_select() . "WHERE origin='".esc($origin)."'", "SOA record for PTR data"))
    return $soa;

  /* No matching SOA - add new SOA */
  $active = ($soa_use_active ? ",active" : "");
  $recursive = ($soa_use_recursive ? ",recursive" : "");
  $query = "INSERT INTO $soa_table_name" .
    " (origin,ns,mbox,serial,refresh,retry,expire,minimum,ttl$active$recursive) VALUES (";
  $query .= "'" . esc($origin) . "'";
  $query .= ",'" . esc($default_ns) . "'";
  $query .= ",'" . esc($default_mbox) . "'";
  $query .= "," . (int)next_serial();
  $query .= "," . (int)$default_refresh;
  $query .= "," . (int)$default_retry;
  $query .= "," . (int)$default_expire;
  $query .= "," . (int)$default_minimum_ttl;
  $query .= "," . (int)$default_ttl;
  if ($soa_use_active)
    $query .= ",'" . esc($soa_active_types[1]) . "'";
  if ($soa_use_recursive)
    $query .= ",'" . esc($soa_recursive_types[0]) . "'";
  $query .= ")";
  sql_query($query)
    or ErrSQL("Error creating new SOA record for " . quote($origin) . ".");
  
  /* Get new SOA */
  $soa = sql_result(soa_select() . "WHERE origin='".esc($origin)."'", "SOA record for PTR data");

  /* Add default records */
  add_default_rr($soa);
  
  return $soa;
}
/*--- ptr_create_soa() --------------------------------------------------------------------------*/


/**************************************************************************************************
	PTR_CREATE_RR
	For automatic PTR maintenance.
	Creates (or updates) resource record for '$name' within '$arpazone'.
**************************************************************************************************/
function ptr_create_rr($arpazone, $name, $data, $origin) {
  global $rr_table_name, $rr_use_active, $default_ttl, $rr_active_types, $allow_ixfr;

  if (!strlen($data))
    $data = $origin;
  if (!ends_with_dot($data))				/* Make sure '$data' ends with origin */
    $data .= "." . $origin;

  /* See if a record already exists */
  $longname = $name . "." . $arpazone['origin'];
  $exists = sql_count("SELECT COUNT(*) FROM $rr_table_name" .
		      " WHERE zone=" . (int)$arpazone['id'] . " AND type='PTR'" .
		      " AND (name='" .esc($name) . "' OR name='" . esc($longname) . "')",
		      "matching records in " . quote($arpazone['origin']) . " for automatic PTR update.");

  if ($exists)	{					/* Update existing record */
    $query = "UPDATE $rr_table_name SET data='" . esc($data) . "'" .
      " WHERE zone=" . (int)$arpazone['id'] . " AND type='PTR'" .
      " AND (name='" .esc($name) . "' OR name='" . esc($longname) . "')";

    sql_query($query)
      or ErrSQL("Error updating automatic PTR record " . quote($longname));
  } else {
    $active = ($rr_use_active ? ",active" : "");
    $serial = ($allow_ixfr ? ",serial" : "");
    $query = "INSERT INTO $rr_table_name" .
      " (zone,name,type,data,ttl$active$serial) VALUES (";
    $query .= (int)$arpazone['id'];
    $query .= ",'" . esc($name) . "'";
    $query .= ",'PTR'";
    $query .= ",'" . esc($data) . "'";
    $query .= "," . (int)$default_ttl;
    if ($rr_use_active)
      $query .= ",'" . esc($rr_active_types[1]) . "'";
    if ($allow_ixfr)
      $query .= "," . (int)$arpazone['serial'];
    $query .= ")";
    
    sql_query($query)
      or ErrSQL("Error adding automatic PTR record " . quote($longname));
  }
}
/*--- ptr_create_rr() ---------------------------------------------------------------------------*/


/**************************************************************************************************
	PTR_DELETE_RR
	For automatic PTR maintenance.
	Deletes resource record for '$name' within '$arpazone' if data matches '$data'.
**************************************************************************************************/
function ptr_delete_rr($arpazone, $name, $data, $origin, $delete_if_empty = 1) {
  global $soa_table_name, $rr_table_name, $rr_use_active, $default_ttl, $allow_ixfr;

  $longname = $data . "." . $origin;

  $where = " WHERE zone=" . (int)$arpazone['id'] . " AND type='PTR'" .
    " AND (name='" .esc($name) . "' OR name='" . esc($longname) . "')" .
    " AND (data='" . esc($data) . "' OR data='" . esc($longname) . "')";
  if ($allow_ixfr) {
    $query = "UPDATE $rr_table_name SET active='" . $rr_active_types[2] . "',serial=" .
      (int)$arpazone['serial'] . $where;
  } else {
    /* Delete the record */
    $query = "DELETE FROM $rr_table_name" . $where;
  }
  sql_query($query)
    or ErrSQL("Error deleting automatic PTR record " . quote($longname));

  /* Delete the zone, too, if it's empty */
  if ($delete_if_empty) {
    $zonerecs = sql_count("SELECT COUNT(*) FROM $rr_table_name" .
			  " WHERE zone=" . (int)$arpazone['id'],
			  "number of records in " . quote($arpazone['origin']));
    if ($zonerecs == 0) {
      sql_query("DELETE FROM $soa_table_name WHERE id=" . (int)$arpazone['id']);
    }
  }
}
/*--- ptr_delete_rr() ---------------------------------------------------------------------------*/

function rr_extended_data($data, &$edata) {
  global $rr_datalen;
  $edata = NULL;
  if (strlen($data) > $rr_datalen) {
    $xdata = str_split($data, $rr_datalen);
    $edata = implode('', array_slice($xdata, 1));
    $data = $xdata[0];
  }
  return $data;
}

/**************************************************************************************************
	RR_ADD
	Add a new resource record.
**************************************************************************************************/
function rr_add() {
  global $rr_table_name, $rr_use_active, $rr_extended_data, $auto_update_ptr, $allow_ixfr;

  /* Load the SOA */
  $soa = sql_result(soa_select() . "WHERE id=" . (int)$_POST['zone'],
		    "SOA record for zone {$_POST['zone']}");

  /* Get POST vars */
  $rr = rr_post_vars($soa);

  $dataval = rr_extended_data($rr['data'], $edataval);

  /* Check to see if this RR already exists */
  if (($id = sql_count("SELECT id FROM $rr_table_name WHERE" .
		       " zone=" . (int)$soa['id'] . "" .
		       " AND name='" . esc($rr['name']) . "'" .
		       " AND type='" . esc($rr['type']) . "'" .
		       " AND data='" . esc($dataval) . "'" .
		       (($rr_extended_data && $edataval)
			? " AND edatakey=md5('" . esc($edataval) . "')"
			: ""),
		       "existing resource record count"))) {
    zone_editor($soa['id'], "The resource record specified already exists.");
    exit;
  }

  /* Validate POST vars */
  rr_validate($soa, $rr, 1);

  /* Insert the record */
  $edata = ($rr_extended_data && $edataval ? ",edata,edatakey" : "");
  $active = ($rr_use_active ? ",active" : "");
  $serial = ($allow_ixfr ? ",serial" : "");
  $query = "INSERT INTO $rr_table_name" .
    " (zone,name,type,data$edata,aux,ttl$active$serial) VALUES (";
  $query .= (int)$soa['id'];
  $query .= ",'" . esc($rr['name']) . "'";
  $query .= ",'" . esc($rr['type']) . "'";
  $query .= ",'" . esc($dataval) . "'";
  $query .= (($rr_extended_data && $edataval)
	     ? ",'" . esc($edataval) . "',md5('" . esc($edataval) . "')"
	     : "");
  $query .= "," . (int)$rr['aux'];
  $query .= "," . (int)$rr['ttl'];
  if ($rr_use_active)
    $query .= ",'" . esc($rr['active']) . "'";
  if ($allow_ixfr)
    $query .= "," . (int)next_serial($soa['serial']);	/* This is a bodge keep in step with below */
  $query .= ")";
  sql_query($query) or ErrSQL("Error adding new resource record to zone " . (int)$soa['id'] . ".");
  
  /* Update serial number for zone if configured to do so */
  soa_update_serial($soa);				/* Keep in step with next_serial above */

  /* Do PTR record update if configured to do so */
  if ($auto_update_ptr) {
    if ($rr['type'] == "A")						/* Add for "A" records only */
      {
	$arpazone = ptr_create_soa($rr['data'], $name);
	ptr_create_rr($arpazone, $name, $rr['name'], $soa['origin']);
      }
  }

  zone_redirect($soa['id']);
}
/*--- rr_add() ----------------------------------------------------------------------------------*/


/**************************************************************************************************
	RR_UPDATE
	Update a resource record.
**************************************************************************************************/
function rr_update() {
  global $rr_table_name, $rr_use_active, $auto_update_ptr, $allow_ixfr;

  /* Load the SOA for this zone */
  $soa = sql_result(soa_select() . "WHERE id=" . (int)$_POST['zone'],
		    "SOA record for zone {$_POST['zone']}");

  /* Load the old resource record for comparison (if $auto_update_ptr is on) */
  if ($auto_update_ptr)
    $old_rr = sql_result(rr_select() . "WHERE id=" . (int)$_POST['id'],
			 "RR record for ID {$_POST['id']}");

  /* Get and validate form vars */
  $rr = rr_post_vars($soa);
  rr_validate($soa, $rr, 0);

  $dataval = rr_extended_data($rr['data'], $edataval);

  /* Update the resource record */
  $query = "UPDATE $rr_table_name SET" .
    " name='" . esc($rr['name']) . "'" .
    ",type='" . esc($rr['type']) . "'" .
    ",data='" . esc($dataval) . "'" .
  (($rr_extended_data && $edataval)
   ? ",edata='" . esc($edataval) . "',edatakey=md5('" . esc($edataval) . "'"
   : "") .
    ",aux=" . (int)$rr['aux'] .
    ",ttl=" . (int)$rr['ttl'];
  if ($rr_use_active)
    $query .= ",active='" . esc($rr['active']) . "'";
  if ($allow_ixfr)
    $query .= ",serial=" . (int)next_serial($soa['serial']);
  $query .= " WHERE id=" . (int)$rr['id'];
  sql_query($query)
    or ErrSQL("Error updating record " . (int)$rr['id'] . " in zone " . (int)$soa['id'] . ".");

  /* Update serial number for zone if configured to do so */
  soa_update_serial($soa);

  /* Do PTR record update if configured to do so */
  if ($auto_update_ptr) {
    /* "A" record changed to non-"A" - delete obsolete PTR record */
    if ($old_rr['type'] == "A" && $rr['type'] != "A") {
      $arpazone = ptr_create_soa($old_rr['data'], $name);
      ptr_delete_rr($arpazone, $name, $old_rr['name'], $soa['origin']);
    }

    /* Delete old PTR record (in case IP address changed) and add new */
    if ($rr['type'] == "A") {
      $arpazone = ptr_create_soa($old_rr['data'], $name);
      ptr_delete_rr($arpazone, $name, $old_rr['name'], $soa['origin'], 0);
      $arpazone = ptr_create_soa($rr['data'], $name);
      ptr_create_rr($arpazone, $name, $rr['name'], $soa['origin']);
    }
  }

  zone_redirect($soa['id']);
}
/*--- rr_update() -------------------------------------------------------------------------------*/


/**************************************************************************************************
	RR_DELETE
	Delete a resource record.
**************************************************************************************************/
function rr_delete() {
  global $rr_table_name, $auto_update_ptr, $rr_active_types, $allow_ixfr;

  /* Load the SOA for this zone */
  $soa = sql_result(soa_select() . "WHERE id=" . (int)$_POST['zone'],
		    "SOA record for zone {$_POST['zone']}");

  /* Get form vars */
  $rr = rr_post_vars($soa);

  if ($allow_ixfr) {
    sql_query("UPDATE $rr_table_name SET active='" . $rr_active_types[2] . "',serial="
	      . (int)next_serial($soa['serial']) . " WHERE id=" . (int)$rr['id'])
      or ErrSQL("Error marking record deleted " . (int)$rr['id'] . " from zone " . (int)$soa['id'] . ".");
  } else {
    /* Delete the resource record */
    sql_query("DELETE FROM $rr_table_name WHERE id=" . (int)$rr['id'])
      or ErrSQL("Error deleting record " . (int)$rr['id'] . " from zone " . (int)$soa['id'] . ".");
  }

  /* Update serial number for zone if configured to do so */
  soa_update_serial($soa);

  /* Do PTR record update if configured to do so */
  if ($auto_update_ptr) {
    if ($rr['type'] == "A") {
      $arpazone = ptr_create_soa($rr['data'], $name);
      ptr_delete_rr($arpazone, $name, $rr['name'], $soa['origin']);
    }
  }

  zone_redirect($soa['id']);
}
/*--- rr_delete() -------------------------------------------------------------------------------*/


/**************************************************************************************************
	RR_LOOKS_INCOMPLETE
	If the name provided ends in a common TLD but no dot, the user probably meant to put a dot
	at the end.
**************************************************************************************************/
function rr_looks_incomplete($name) {
  if (!strchr($name, '.') || ends_with_dot($name))
    return 0;
  $tld = substr(strrchr($name, '.'), 1);

  /* The top 50 or so TLDs worldwide */
  $common_tld_list = array(
			   "com", "de", "net", "uk", "org", "info", "it", "biz", "nl", "cc", "tv", "ar",
			   "ch", "br", "kr", "dk", "us", "ca", "au", "ws", "jp", "be", "cz", "no", "at",
			   "fr", "pl", "ru", "nz", "cn", "se", "tw", "cl", "to", "mx", "il", "hk", "sk",
			   "es", "fi"
			   );

  foreach ($common_tld_list as $common)
    if (!strcasecmp($tld, $common))
      return 1;
  return 0;
}
/*--- rr_looks_incomplete() ---------------------------------------------------------------------*/


/**************************************************************************************************
	RR_SET_WARNINGS
	Set the widget icon if this RR looks like it might have problems, etc.
**************************************************************************************************/
function rr_set_warnings($soa, $rr, &$widget, &$widget_text) {
  global $rr_table_name, $ignore_minimum_ttl;

  $glue = NULL;

  if ($widget)
    return;
  $errors = array();

  /* Make sure the RR has some data */
  if (!strlen($rr['data'])) {
    $widget = "ErrorSmall.png";
    $widget_text = "This resource record does not have any data associated with it.";
    return;
  }

  /* Check 'name' data */
  if (validate_name($rr['name'], "Name", $errors, 1, $soa['origin'])) {
    $widget = "ErrorSmall.png";
    $widget_text = $errors[0];
    return;
  }

  /* Check for "probably a missing dot" on data and name. */
  if (rr_looks_incomplete($rr['data'])) {
    $widget = "WarnSmall.png";
    $widget_text = "The data associated with this resource record looks incomplete." .
      "  Does the data need a dot at the end?";
  }
  if (rr_looks_incomplete($rr['name'])) {
    $widget = "WarnSmall.png";
    $widget_text = "The name associated with this resource record looks incomplete." .
      "  Does the name need a dot at the end?";
  }

  /* Set $fqdn_name and $fqdn_data */
  $fqdn_name = !strlen($rr['name'])
    ? $soa['origin']
    : (ends_with_dot($rr['name'])
       ? $rr['name']
       : $rr['name'] . "." . $soa['origin']);
  $fqdn_data = !strlen($rr['data'])
    ? $soa['origin']
    : (ends_with_dot($rr['data'])
       ? $rr['data']
       : $rr['data'] . "." . $soa['origin']);

  /* Is the 'name' out of zone? */
  if (strcasecmp($soa['origin'], substr($fqdn_name, strlen($fqdn_name) - strlen($soa['origin'])))) {
    /* See if this is DEFINITELY glue from a delegation */
    if (sql_count("SELECT COUNT(*) FROM $rr_table_name WHERE zone=" . (int)$soa['id'] .
		  " AND type='NS' AND data='" .esc($fqdn_name) . "'",
		  "delegation records for glue detection")) {
      $glue = 1;
    } else {
      $widget = "WarnSmall.png";
      $widget_text = "Record contains out-of-zone name.";
      return;
    }
  }

  /* Warn against bad TTL */
  if (!$ignore_minimum_ttl && !$widget && ($rr['ttl'] < $soa['minimum'])) {
    $widget = "WarnSmall.png";
    $widget_text = "TTL for this record is below the zone's minimum.";
    return;
  }

  /* Type-specific data checks */
  if (rr_validate_type($soa, $rr, $errors)) {
    $widget = "ErrorSmall.png";
    $widget_text = $errors[0];
    return;
  }

  /* If this record was glue, report it as info */
  if ($glue) {
    $widget = "InfoSmall.png";
    $widget_text = "This record appears to be glue.";
    return;
  }
}
/*--- rr_set_warnings() -------------------------------------------------------------------------*/


/**************************************************************************************************
	RR_ROW
	Outputs one row for editing an RR.
**************************************************************************************************/
function rr_row($soa, $rr = NULL) {
  global $rr_use_active, $rr_active_types, $query_host, $query_origin, $page, $query, $allow_ixfr;

  $values = $rr;
  $widget = NULL;

  if (isset($values['edata'])) {
	if (strlen($values['edata']))
	$values['data'] .= $values['edata'];
  }
	
	
  if (!strlen($values['name']))
    $values['name'] = $soa['origin'];

  /* Build '$match', a name to highlight if we find it */
  $match = (strlen($query_host) ? $query_host . "." . $query_origin : NULL);

  /* Set background color - highlighted if this name matches $match */
  $bgcolor = bgcolor();
  if ($match) {
    $name = $values['name'];
    if (!ends_with_dot($name))
      $name = $name . "." . $soa['origin'];
    if (!strcasecmp($match, $name))
      $bgcolor = bgcolor(1);
  }

  if ($rr == NULL) {					/* Empty (new) resource record */
    $new_rr = 1;

    if (isset($_POST['done']) && $_POST['done'] == "1" && isset($_POST['zone'])) {
      $values = rr_post_vars();

      $widget = "ErrorSmall.png";
      $widget_text = "Error";
    } else {
      $rr = array();
      $rr['active'] = $rr_active_types[1];
      $rr['ttl'] = $soa['ttl'];
      $rr['type'] = "A";
      $values = $rr;
    }

    $add_button = formbutton("Add new RR",
			     "Click this button to add these fields as a new resource record.", $bgcolor);
    $buttons = $add_button;
  } else {						/* Existing resource record */
    $new_rr = 0;

    if (isset($_POST['id']) && ($rr['id'] == $_POST['id'])) {
      $values['serial'] = $_POST['serial'];
      $values['stamp'] = $_POST['stamp'];
      $values['active'] = $rr_active_types[gettrinary($_POST['active'])];
      $values['name'] = $_POST['name'];
      $values['ttl'] = $_POST['ttl'];
      $values['aux'] = $_POST['aux'];
      $values['data'] = $_POST['data'];

      $widget = "ErrorSmall.png";
      $widget_text = "Error";
    }

    $update_button = formbutton("Update",
				"Click this button to update this resource record.", $bgcolor);
    $delete_button = formbutton("Delete",
				"Click this button to permanently remove this resource record.",
				$bgcolor);
    $buttons = "$update_button $delete_button";
  }

  /* Set widget (the icon displayed in the first column) if applicable */
  if (!$new_rr)
    rr_set_warnings($soa, $rr, $widget, $widget_text);
  if (!$widget) {				/* Set default widget if none was set previously */
    $widget = "blank.gif";
    $widget_text = "";
  }

  /* Make sure 'values' are set to avoid "Undefined index" errors */
  foreach (array('name', 'ttl', 'type', 'aux', 'data') as $n)
    if (!isset($values[$n]))
      $values[$n] = '';

?>
<TABLE class=rrBox cellspacing=0>
<FORM action="<?php echo $_SERVER['PHP_SELF']?>" method=POST>
<TR bgcolor="<?php echo $bgcolor;?>">
<TD class=rrCellLeft>
<?php
   /* Output widget icon */
   echo "<IMG src=\"{$_SERVER['PHP_SELF']}?img=$widget\" width=16 height=16 alt=\"$widget_text\" title=\"$widget_text\">\n";
?>
</TD>
<?php
   /* Output "active" checkbox/selectbox if supported */
   if ($rr_use_active) {
     if ($allow_ixfr) {
       echo "<TD class=rrCell title=\"Record status selection.\">\n";
       echo "<TABLE><TR>";
       foreach ($rr_active_types as $rat) {
	 echo "<TD>";
	 echo "\t<INPUT class=activeList ";
	 if($rat == $values['active']) { echo " checked"; } 
	 echo " type=radio name=\"active\" value=\"" . $rat . "\">";
	 echo $rat . "</INPUT>\n";
	 echo "</TD>";
       }
       echo "</TR></TABLE>";
     } else {
       echo "<TD class=rrCell title=\"Uncheck this box to deactivate this resource record.\">\n";
       echo "\t<INPUT class=activeBox type=checkbox name=\"active\" value=\""
	 . $rr_active_types[1] . "\"";
       if (gettrinary($values['active'])) { echo " checked"; }
       echo ">";
     }
     echo "</TD>\n";
   }
?>
<TD class=rrCell title="The name (hostname or FQDN) with which this resource record is associated.">
<INPUT type=hidden name="zone" value="<?php echo $soa['id']?>">
<?php
   if ($new_rr) {
     echo "<INPUT type=hidden name=\"done\" value=\"1\">\n";
   } else {
     echo "<INPUT type=hidden name=\"id\" value=\"{$rr['id']}\">\n";
     echo "<A name=\"" . (int)$rr['id'] . "\"></A>\n";
   }
   if ($page)
     echo "<INPUT type=hidden name=\"page\" value=\"$page\">\n";
   if ($query)
     echo "<INPUT type=hidden name=\"query\" value=\"" . ent($query) . "\">\n";
?>
<INPUT class=mono type=text name="name" maxlength=255 size=25 value="<?php echo ent($values['name'])?>">
</TD>
<TD class=rrCell title="The cache expiration time (TTL) for this resource record.">
<INPUT class=mono type=text name="ttl" maxlength=15 size=6 value="<?php echo ent($values['ttl'])?>">
</TD>
<TD class=rrCell><TT>IN</TT></TD>
<TD class=rrCell title="The resource record type.">
<SELECT class=rrTypes name="type" size=1><?php echo rr_type_options($values['type']);?></SELECT>
</TD>
<TD class=rrCell title="Auxillary data for this resource record.">
<INPUT class=mono type=text name="aux" maxlength=15 size=5 value="<?php echo ent($values['aux'])?>">
</TD>
<TD class=rrCell title="The data associated with this resource record.">
<INPUT class=mono type=text name="data" maxlength=65535 size=40 value="<?php echo ent($values['data'])?>">
</TD>
<?php
  if ($allow_ixfr) {
    echo "<TD class=rrCell title\"Timestamp of last record change\">" . ent($values['stamp']) . "</TD>\n";
    echo "<TD class=rrCell title=\"Serial number of soa when last changed\">" . ent($values['serial']) . "</TD>\n"; 
   }
?>
<TD class=rrCellRight><?php echo $buttons?>
</TR>
</FORM>
</TABLE>

<?php
}
/*--- rr_row() ----------------------------------------------------------------------------------*/


/**************************************************************************************************
	RR_OPEN
	Opens the RR table.
**************************************************************************************************/
function rr_open($title) {
?>
<!-- BEGIN RR editor -->
<DIV class=rrBox>
<?php
}
/*--- rr_open() ---------------------------------------------------------------------------------*/


/**************************************************************************************************
	RR_CLOSE
	Closes the RR table.
**************************************************************************************************/
function rr_close() {
?>
</DIV>
<!-- END RR editor -->

<?php
}
/*--- rr_close() --------------------------------------------------------------------------------*/


/**************************************************************************************************
	RR_OPEN_INFO_ROW
	Opens an empty row for displaying error message(s), etc.
**************************************************************************************************/
function rr_open_info_row() {
  global $rr_use_active, $allow_ixfr;

  $colspan = 7;
  if ($rr_use_active)
    $colspan++;
  if ($allow_ixfr)
    $colspan += 2;
  echo "<TR><TD colspan=$colspan>\n";
}
/*--- rr_open_info_row() ------------------------------------------------------------------------*/


/**************************************************************************************************
	RR_EDITOR
	Outputs the resource record editor for the current zone.
**************************************************************************************************/
function rr_editor($soa, $soa_error_message = NULL, $rr_error_message = NULL) {
  global $rr_table_name, $page, $query_host, $query_origin, $rr_group_size;
  global $use_pgsql;

  $add_new_text = "Add a new resource record";
  $top_level_text = "Top-level resource records";
  $other_level_text = "Other resource records";
  $matching_text = "Resource records matching your query";

  /* Show only the results matching the query host (group size is ignored) */
  if (strlen($query_host)) {
    soa_editor($soa);							/* The SOA editor */
    if ($soa_error_message) {
      ErrBox($soa_error_message);
      echo "<P>\n";
    }

    rr_open($add_new_text);
    rr_row($soa, NULL);
    rr_close();
    ErrBox($rr_error_message);

    /* First output base-level records ("" or the zone origin) */
    $short = $query_host;
    $long = $query_host . "." . $query_origin;
    $query = rr_select() . " WHERE zone=" . (int)$soa['id'] .
      " AND (name='" . esc($short) . "' OR name='" .esc($long) . "') ORDER BY type,aux,data";
    $res = sql_query($query)
      or ErrSQL("Error loading resource records for zone {$soa['id']} matching" .
		" host " . quote($query_host) . ".");
    if (sql_num_rows($res)) {
      rr_open($matching_text);
      while ($rr = sql_fetch_array($res))
	rr_row($soa, $rr);
      rr_close();
    }
  } else {								/* Normal display */
    if ($rr_group_size == 0) {						/* All records on one screen */
      soa_editor($soa);							/* The SOA editor */
      if ($soa_error_message) {
	ErrBox($soa_error_message);
	echo "<P>\n";
      }

      rr_open($add_new_text);
      rr_row($soa, NULL);
      rr_close();
      ErrBox($rr_error_message);

      /* First output base-level records ("" or the zone origin) */
      $query = rr_select() . " WHERE zone=" . (int)$soa['id'] .
	" AND (name='' OR name='" .esc($soa['origin']) . "') ORDER BY type,aux,data";
      $res = sql_query($query) or ErrSQL("Error loading base resource records for zone {$soa['id']}.");
      if (sql_num_rows($res)) {
	rr_open($top_level_text);
	while ($rr = sql_fetch_array($res))
	  rr_row($soa, $rr, $rr_error_message);
	rr_close();
      }

      /* Then output non base-level records */
      $query = rr_select() . " WHERE zone=" . (int)$soa['id'] .
	" AND (name != '' AND name != '" .esc($soa['origin']) . "') ORDER BY name,type,aux,data";
      $res = sql_query($query) or ErrSQL("Error loading base resource records for zone {$soa['id']}.");
      if (sql_num_rows($res)) {
	rr_open($other_level_text);
	while ($rr = sql_fetch_array($res))
	  rr_row($soa, $rr, $rr_error_message);
	rr_close();
      }
    } else {
      /* Count number of base-level and non base-level records for this zone */
      $num_base = sql_count("SELECT COUNT(*) FROM $rr_table_name WHERE zone=" . (int)$soa['id'] .
			    " AND (name='' OR name='" .esc($soa['origin']) . "')", "Base-level records");
      $num_total = sql_count("SELECT COUNT(*) FROM $rr_table_name WHERE zone=" . (int)$soa['id'],
			     "total number of records for zone {$soa['id']}.");
      $num_nonbase = $num_total - $num_base;

      echo "<DIV align=center>\n";
      $offset = offset_select($page, $num_total, $rr_group_size, "zone=" . (int)$soa['id']);
      echo "</DIV>\n";

      if ($page == 0)
	soa_editor($soa);						/* The SOA editor */
      if ($soa_error_message) {
	ErrBox($soa_error_message);
	echo "<P>\n";
      }

      rr_open($add_new_text);
      rr_row($soa, NULL);
      rr_close();
      ErrBox($rr_error_message);

      /* Display any relevant base-level records */
      if ($offset < $num_base) {
	$query = rr_select() . " WHERE zone=" . (int)$soa['id'] .
	  " AND (name='' OR name='" .esc($soa['origin']) . "') ORDER BY type,aux,data ";
	
	if ($use_pgsql)
	  $query .= "LIMIT $rr_group_size OFFSET $offset";
	else
	  $query .= "LIMIT $offset,$rr_group_size";

	$res = sql_query($query) or ErrSQL("Error loading base resource records for zone {$soa['id']}.");
	$found = sql_num_rows($res);
	$remaining = $rr_group_size - $found;
	$offset = 0;
	if ($found) {
	  rr_open($top_level_text);
	  while ($rr = sql_fetch_array($res))
	    rr_row($soa, $rr, $rr_error_message);
	  rr_close();
	}
      } else {
	$remaining = $rr_group_size;
	$offset -= $num_base;
      }

      /* Display any relevant non-base-level records */
      if ($remaining) {
	/* Then output non base-level records */
	$query = rr_select() . " WHERE zone=" . (int)$soa['id'] .
	  " AND (name != '' AND name != '" .esc($soa['origin']) . "') ORDER BY name,type,aux,data ";

	if ($use_pgsql)
	  $query .= "LIMIT $remaining OFFSET $offset";
	else
	  $query .= "LIMIT $offset,$remaining";

	$res = sql_query($query) or ErrSQL("Error loading base resource records for zone {$soa['id']}.");
	if (sql_num_rows($res)) {
	  rr_open($other_level_text);
	  while ($rr = sql_fetch_array($res)) {
	    rr_row($soa, $rr, $rr_error_message);
	  }
	  rr_close();
	}
      }
    }
  }
}
/*--- rr_editor() -------------------------------------------------------------------------------*/


/**************************************************************************************************
	QUERY_GET_ZONE
	Attempts to look up a zone matching the specified query.  Returns the zone ID; fatal if no
	matches are found.  Sets global variables '$search_query', '$query_host', and '$query_origin'.
**************************************************************************************************/
function query_get_zone($query, $fail_if_missing = 0) {
  global $soa_table_name, $search_query, $query_host, $query_origin;

  if (!strlen($query))
    return;

  if (!ends_with_dot($query))
    $query .= ".";

  /* Search left-to-right at label boundaries for exact match */
  $labels = explode(".", $query);
  $host = array();
  do {
    if ($label = implode(".", $labels)) {
      /* Search for exact match */
      $res = sql_query("SELECT id FROM $soa_table_name WHERE origin='" . esc($label) . "'")
	or ErrSQL("Error searching for zone called " . quote($label) . ".");
      if ($res && ($row = sql_fetch_row($res))) {
	$query_origin = $label;
	$query_host = implode(".", $host);
	$search_query = $query;
	return (int)$row[0];
      }
      $host[] = array_shift($labels);
    }
  } while ($label);

  /* Search left-to-right at label boundaries for fuzzy match */
  $labels = explode(".", $query);
  $host = array();
  do {
    if ($label = implode(".", $labels)) {
      /* Search for similar match */
      $res = sql_query("SELECT id FROM $soa_table_name WHERE origin LIKE '%" . esc($label) . "'")
	or ErrSQL("Error searching for zone like " . quote($label) . ".");
      if ($res && sql_num_rows($res)) {
	zone_browser(" WHERE origin LIKE '%" . esc($label) . "'");
	exit;
      }
    }
  } while ($dummy = array_shift($labels));

  /* Try generic LIKE query */
  if (ends_with_dot($query))
    $query = substr($query, 0, strlen($query) - 1);
  $sql_where = "WHERE origin LIKE '%" . esc($query) . "%'";
  $res = sql_query("SELECT id FROM $soa_table_name $sql_where")
    or ErrSQL("Error searching for zone like " . quote($query) . ".");
  if ($res && sql_num_rows($res)) {
    zone_browser($sql_where, $query);
    exit;
  }

  /* No match - fail */
  if ($fail_if_missing) {
    Notice("<B>No zones found matching your search query.</B>");
    help_screen();
  }
}
/*--- query_get_zone() --------------------------------------------------------------------------*/


/**************************************************************************************************
	MAIN
**************************************************************************************************/

send_img();						/* Output an image if requested */
db_connect();						/* Connect to the database */
open_page();						/* Start the page (always) */
db_get_settings();					/* Set variables based on database fields */

$page = getpostvar('page');
$zone = getpostvar('zone');
$action = getpostvar('action');
if (!strlen($action))
  $action = NULL;
$query = getpostvar('query');
if (!strlen($query))
  $query = NULL;

query_get_zone($query);

/* If there's a query, but no 'zone' or 'action', set action to 'search' */
if ($query && !$zone && !$action)
  $action = "search";

/* If 'zone' was provided, verify that it exists */
if ($zone && !zone_exists($zone))
  Err("<B>Zone $zone does not exist.</B>");

/* Handle the specified action (if any) */
switch (strtolower($action)) {
 case "search":						/* Execute top-of-page search query */
   if (!strlen($query))
     help_screen();
   zone_editor(query_get_zone($query, 1));
   break;

 case "browse":						/* Launch the zone browser */
   zone_browser();
   break;

 case "edit":						/* Edit a specific zone */
   zone_editor($zone);
   break;

 case "new":						/* Add a new zone */
   soa_editor();
   break;

 case "add new soa":					/* Really add new zone */
   soa_add();
   break;

 case "update soa":					/* Update SOA record */
   soa_update();
   break;

 case "notify slaves":
   notify_slaves();
   break;

 case "really delete zone":				/* Delete zone */
   soa_delete();
   break;

 case "update list":					/* Update zone transfer list */
   soa_xfer_update();
   break;

 case "update acl for updates":				/* Update zone transfer list */
   soa_update_acl_update();
   break;

 case "update also notify":				/* Update zone transfer list */
   soa_also_notify_update();
   break;

 case "add new rr":					/* Add a new resource record */
   rr_add();
   break;

 case "update":						/* Update resource record */
   rr_update();
   break;

 case "delete":						/* Delete resource record */
   rr_delete();
   break;

 default:
   /* If no action was specified but a zone was specified, default to the zone editor */
   if ($zone)
     zone_editor($zone);
   else
     help_screen();
   break;
}

/*-----------------------------------------------------------------------------------------------*/


/* vi:set ts=3: */ ?>
