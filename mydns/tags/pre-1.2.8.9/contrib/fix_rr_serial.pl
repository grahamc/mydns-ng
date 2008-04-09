#! /usr/bin/perl -w
#
#

use Getopt::Long;

use MyDNS;

my $dbhost = "localhost";
my $dbname = "mydns";
my $dbuser = "mydnsuser";
my $dbpass = "mydnspass";

my $options = GetOptions("dbhost=s" => \$dbhost,
			 "dbname=s" => \$dbname,
			 "dbuser=s" => \$dbuser,
			 "dbpass=s" => \$dbpass);

print STDERR "Trying to connect to $dbname at $dbhost using $dbuser $dbpass\n";

my $mydns = MyDNS->new("dbi:mysql:database=$dbname;host=$dbhost", $dbuser, $dbpass);

unless (defined($mydns)) {
  print STDERR "Cannot connect to Database\n";
  exit;
}

my $zones = $mydns->get_all_soas();

printf STDERR "Retrieved %d zones\n", scalar(@{$zones});

foreach my $zone ( @{$zones} ) {
  my $origin = $zone->{origin};
  printf STDERR "Processing %s zone\n", $origin;
  my $rrs = $mydns->get_all_rr($origin);
  printf STDERR "Retrieved %d resource records\n", scalar(@{$rrs});
  foreach my $rr ( @{$rrs} ) {
    my $name = $rr->{name};
    my $type = $rr->{type};
    my $data = $rr->{data};
    my $aux = $rr->{aux};
    my $ttl = $rr->{ttl};
    my $active = $rr->{active};
    my $stamp = $rr->{stamp};
    my $serial = $rr->{serial};
    if ($name eq $origin) {
      # This is allowed
    } elsif ($name eq "") {
      $name = $origin;
    } elsif ($name =~ m/.+\.$origin$/i) {
      # A fully qualified item
    } else {
      # partially qualified so fully qualify it
      $name .= ".$origin";
    }
    printf STDERR "Processing %s %s %s %s %s %s %s %s\n",
      $name, $type, $data, $aux, $ttl, $active, $stamp, defined($serial)?$serial:"-";
    if (defined($serial) && ($serial ne "")) { next; }
    $serial = $zone->{serial};
    # Make relevative if not the origin
    $name =~ s/\.$origin$//i;
    if ($name =~ m/^$origin$/i) { $name = undef; }
    my $args = {
		origin     => $origin,
		type       => $type,
		data       => $data,
		aux        => $aux,
		ttl        => $ttl,
		active     => $active,
		serial     => $serial
	       };
    if (defined($name)) { $args->{name} = $name; }
    $mydns->put_rr( $args );
  }
}

exit;
