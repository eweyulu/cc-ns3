#!/bin/bash

for f in outNewReno*run1 
    do
	cwnd=$(echo "$f" | cut -d'-' -f 2 | cut -d'm' -f 1)
        delay=$(echo "$f" | cut -d'-' -f 3 | cut -d'm' -f 1)
        rate=$(echo "$f" | cut -d'-' -f 4 | cut -d'M' -f 1)
        que=$(echo "$f" | cut -d'-' -f 5 | cut -d'_' -f 1)
        #run=$(echo "$f" | cut -d'_' -f 2 | cut -d'n' -f 2)
        for pcap in $f/*.newreno-data-4-3.pcap 
        do
        pkts=$(echo "$pcap" | cut -d'.' -f 1 | cut -d'/' -f 2)
        fct=$(ipsumdump -r "$pcap" -tLSsF -f src="10.3.3.2" | grep -v "!" | grep -v "-" | awk 'NR==1{ts=$1}{print $1-ts, $2, $3, $4, $5}'|awk '{if($5=="FA" || $5=="FPA") print $1, $5 }' | awk 'END {print $1}')
        printf "$pkts $fct\n" >> $pkts.newreno-$cwnd-$delay-$rate-$que.tmp;
    done
    cat *.newreno-$cwnd-$delay-$rate-$que.tmp | sort -n -k 1 > newreno-LSDA-$cwnd-$delay-$rate-$que.txt
    rm *newreno-$cwnd-$delay-$rate-$que.tmp
done
