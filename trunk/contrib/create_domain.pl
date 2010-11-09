#!/usr/bin/perl

#############################################################################
#        $Id: create_domain.pl,v 1.1 2005/04/21 17:21:35 bboy Exp $
#        A Perl script to add a minimal functioning domain entry
#
#        Copyright (C) 2005  Gerard de Brieder <smeevil@meladicta.com>
#
#        This program is free software; you can redistribute it and/or modify
#        it under the terms of the GNU General Public License as published by
#        the Free Software Foundation; either version 2 of the License, or
#        (at Your option) any later version.
#
#        This program is distributed in the hope that it will be useful,
#        but WITHOUT ANY WARRANTY; without even the implied warranty of
#        MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
#        GNU General Public License for more details.
#
#        You should have received a copy of the GNU General Public License
#        along with this program; if not, write to the Free Software
#        Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
##############################################################################

########################
# CONFIGURABLE OPTIONS #
########################
$config_file="/etc/mydns.conf";		#location of your mydns config

$refresh=14400; 			#default value , though meaningless to MyDNS
$retry=3600;				#default value , though meaningless to MyDNS
$expire=604800; 			#default value , though meaningless to MyDNS
$minimum=86400;				#The minimum TTL field that should be exported with any RR from a zone.
$ttl=1800;				#The number of seconds that this zone may be cached before the source of the information should again be consulted. Zero values are interpreted to mean that the zone should not be cached.

$mail_ip="127.0.0.1";			#ip of your default mail server to use for MX records
$site_ip="127.0.0.1";			#ip of your default web server to use for WWW and A records

$master_dns="ns1.yourdomain.tld";	#default hostname of your primary dns server
$slave_dns="ns2.yourdomain.tld";	#default hostname of your secondary dns server
$mx_prefix="mail";			#MX record prefix (value now reads as mail.yourdomain.tld); 
$hostmaster="hostmaster.yourdomain.tld";#default email for hostmaster (notice replace @ for . (dot) (so value now reads as hostmaster@yourdomain.tld)
#################################################
#  Start of the script , no more changes needed #
#################################################

use DBI;
use Switch;

$master_dns.=".";
$slave_dns.=".";
$hostmaster.=".";

if (!$ARGV[0]&&!$ARGV[1]){
	print "
#########################################################################
# MyDNS Initial Zone Creator by G.j. De Brieder (smeevil\@meladicta.com) #
#########################################################################

Usage :
create_domain.pl --domain=<domain name> --site=<ip> [OPTIONS] <--test|--create> 

Options :
--minimum	Min. TTL send when exporting a domain (default 86400)
--ttl		# of seconds zone is cached, 0 means no cache (default 1800)
--mx		ip number of the mailserver to use
--site		ip number of the webserver where www record will point to

Example : 
./create.pl --domain=example.tld --ip=127.0.0.1 --test
./create.pl --domain=example.tld --ip=127.0.0.1 --create
\n";

	exit();
}

foreach $arg (@ARGV){
	if ($arg=~/--domain=(.*)/){
		$new_domain=$1.".";
		$new_domain=~s/\s+//;
	}

	if ($arg=~/--site=(\d+.\d+.\d+.\d+)/){
		$site_ip=$1;
	}

	if ($arg=~/--minimum=(\d+)/){
		$minimum=$1;
	}

	if ($arg=~/--ttl=(\d+)/){
		$ttl=$1;
	}

	if ($arg=~/--mx=(\d+.\d+.\d+.\d+)/){
		$mail_ip=$1;
	}
	
	if ($arg=~/--create/){
		print "Creating...\n";
		$action="create";
	}
	if ($arg=~/--test/){
		print "Testing...\n";
		$action="test";
	}
}

if (!$action){
	exit();
}

print "Checking for domain $new_domain\n";
open(FILEHANDLE, $config_file);
	@filedata=<FILEHANDLE>;
close(FILEHANDLE);

foreach $line (@filedata){
	if ($line=~/db-host = (\w+)/){
		$db_host=$1;
	}
	if ($line=~/db-user = (\w+)/){
		$db_user=$1;
	}
	if ($line=~/db-password = (\w+)/){
		$db_pass=$1;
	}
	if ($line=~/database = (\w+)/){
		$db_name=$1;
	}
}

$dbc = DBI->connect("dbi:mysql:database=$db_name;host=$db_host;user=$db_user;password=$db_pass");
$query="SELECT * FROM soa WHERE origin='$new_domain'";
$result = $dbc->prepare($query);
$result->execute();
$rows=$result->rows();
if ($rows>0){
	while (@r=$result->fetchrow_array()){
		print "Found in db : ".$r[1]."\n";
		print "Bailing out\n";
	}
}else{
	print "No hit found on $new_domain\n";
	if ($action eq "test"){
		print "test info :

Master Dns : $master_dns
Slave Dns : $slave_dns

refresh=$refresh
retry=$retry
expire=$expire

minimum=$minimum
ttl=$ttl

mail_ip=$mail_ip
site_ip=$site_ip
\n";
	}
	if ($action eq "create"){
		print "Proceding with creation\n";
		$query="INSERT INTO soa (origin,ns,mbox,serial,refresh,retry,expire,minimum,ttl)VALUES('$new_domain','$master_dns','$hostmaster','1','$refresh','$retry','$expire','$minimum','$ttl')";

		$result = $dbc->prepare($query);
		$result->execute();

		$query="SELECT id FROM soa WHERE origin='$new_domain'";
		$result = $dbc->prepare($query);
		$result->execute();
		while (@r=$result->fetchrow_array()){
			print "Added $new_domain and got id : ".$r[0]."\n";
			$id=$r[0];
		}

		print "Adding NS $master_dns\n";
		$query="INSERT INTO rr (zone,name,type,data,aux,ttl)VALUES('$id','$new_domain','NS','$master_dns','0','$ttl')";
		$result = $dbc->prepare($query);
		$result->execute();
	
		print "Adding NS $slave_dns\n";
		$query="INSERT INTO rr (zone,name,type,data,aux,ttl)VALUES('$id','$new_domain','NS','$slave_dns','0','$ttl')";
		$result = $dbc->prepare($query);
		$result->execute();
	
		print "Adding MX to $mail_ip\n";
		$mx_record=$mx_prefix.".".$new_domain;
		$query="INSERT INTO rr (zone,name,type,data,aux,ttl)VALUES('$id','$new_domain','MX','$mx_record','10','$ttl')";
		$result = $dbc->prepare($query);
		$result->execute();
	
		print "Adding localhost\n";
		$query="INSERT INTO rr (zone,name,type,data,aux,ttl)VALUES('$id','localhost','A','127.0.0.1','0','$ttl')";
		$result = $dbc->prepare($query);
		$result->execute();

		print "Adding A record\n";
		$query="INSERT INTO rr (zone,name,type,data,aux,ttl)VALUES('$id','$new_domain','A','$site_ip','0','$ttl')";
		$result = $dbc->prepare($query);
		$result->execute();
	
		print "Adding mail record\n";
		$query="INSERT INTO rr (zone,name,type,data,aux,ttl)VALUES('$id','$mx_prefix','A','$mail_ip','0','$ttl')";
		$result = $dbc->prepare($query);
		$result->execute();


		print "Adding www record\n";
		$query="INSERT INTO rr (zone,name,type,data,aux,ttl)VALUES('$id','www','A','$site_ip','0','$ttl')";
		$result = $dbc->prepare($query);
		$result->execute();
	
		print "Adding Catch all record\n";
		$query="INSERT INTO rr (zone,name,type,data,aux,ttl)VALUES('$id','*','A','$site_ip','0','$ttl')";
		$result = $dbc->prepare($query);
		$result->execute();
	}
	print "\nAll done\n";
}

