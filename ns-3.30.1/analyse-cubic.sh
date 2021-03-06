#!/bin/bash

for f in outCubic*run1 
    do
        delay=$(echo "$f" | cut -d'-' -f 2 | cut -d'm' -f 1)
        rate=$(echo "$f" | cut -d'-' -f 3 | cut -d'M' -f 1)
        que=$(echo "$f" | cut -d'-' -f 4 | cut -d'_' -f 1)
        #run=$(echo "$f" | cut -d'_' -f 2 | cut -d'n' -f 2)
        for pcap in $f/*.cubic-data-fct-0-0.pcap 
        do
        pkts=$(echo "$pcap" | cut -d'.' -f 1 | cut -d'/' -f 2)
        fct=$(ipsumdump -r "$pcap" -tLF | grep -v "!" | grep -v "-" | awk 'NR==1{ts=$1}{print $1-ts, $2, $3}'| awk 'END {print $1}')
        printf "$pkts $fct\n" >> $pkts.cubic-$delay-$rate-$que.tmp;
    done
    cat *.cubic-$delay-$rate-$que.tmp | sort -n -k 1 > cubic-$delay-$rate-$que.txt
    rm *cubic-$delay-$rate-$que.tmp
done
