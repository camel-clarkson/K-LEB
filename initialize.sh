#!/bin/bash
# Config: Counter, Umask, Timer Delay, Program Name
llc_references="0x4f2e"
llc_misses="0x412e"
ITLB_flushes="0x1ae"

config="$llc_references 0x00 10000 ./meltdown/testnm"
PS3='Run: '
options=("Clean" "Setup" "Start" "Stop" "Stop w/ file" "Quit")
select opt in "${options[@]}"
do
    case $opt in
        "Clean")
            make clean
			sudo rm /dev/kleb
			sudo rmmod kleb			
	    	;;
        "Setup")
            make
			sudo insmod kleb.ko
			maj_num=$(dmesg | tail | grep "The major number for your device is" | sed 's/^.*\([0-9][0-9][0-9]\)$/\1/g')
			echo "Major Number: " $maj_num
			sudo mknod /dev/kleb c $maj_num 0
			;;
        "Start")
			echo $config
			sudo ./ioctl_start $config
			;;
		"Stop")
			lines=$(sudo ./ioctl_stop | grep "\-10,\-10,\-10,\-10,\-10," | wc -l)
			sudo ./ioctl_stop | grep -v "\-10,\-10,\-10,\-10,\-10,"
        	echo $lines "Empty Lines Suppressed"
			if [ $lines -eq 0 ]
			then
				echo "CRITICAL WARNING: Not enough samples allocated, decrease resolution or increase sample size."
			fi
			;;
		"Stop w/ file")
	    	lines=$(sudo ./ioctl_stop | grep "\-10,\-10,\-10,\-10,\-10," | wc -l)
			read -p "Name of File: " output
			sudo ./ioctl_stop | grep -v "\-10,\-10,\-10,\-10,\-10," > $output
	    	cat $output
			echo $lines "Empty Lines Suppressed"
            if [ $lines -eq 0 ]
            then
                 echo "Not enough samples allocated, decrease resolution or increase sample size."
            fi  
			;;
		"Quit")
            break
            ;;
        *) echo "invalid option $REPLY";;
    esac
done
