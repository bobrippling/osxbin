#!/bin/sh

vol_get(){
	# output volume:0, input volume:100, alert volume:100, output muted:false
	echo get volume settings \
		| osascript \
		| cut -d' ' -f2 \
		| cut -d: -f2 \
		| sed 's/,//'
}

vol_set(){
	echo "set volume output volume $1" | osascript
}

interactive(){
	printf 'interactive: j/k to alter volume: '
	stty -echo -icanon; \
	perl -e '
	my $current = `vol`;
	chomp $current;

	while(1){
		print "\e[0K", $current, "%\r";

		$x = "a";
		last if(read(STDIN, $x, 1) < 1);

		if($x eq "k"){
			$current++;
		}elsif($x eq "j"){
			$current--;
		}elsif($x eq "\4"){
			# ctrl-d
			last;
		}elsif($x eq "q"){
			last;
		}else{
			print "ignoring \"$x\"\n";
			next;
		}

		system("vol", $current);
	}
	'
}

usage(){
	echo "Usage: $0 [-i | vol-to-set]" >&2
	exit 1
}

if [ $# -eq 0 ]
then vol_get
elif [ $# -eq 1 ]
then
	case "$1" in
		-i)
			interactive
			;;

		*)
			vol_set "$1"
			;;
	esac
else usage
fi
