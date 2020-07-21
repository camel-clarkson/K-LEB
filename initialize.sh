#!/bin/bash
# Config: Counters, Umask, Timer Delay, Log path, Program Name

declare -a counter
num_event=0

config_cfg="./perf.cfg"
init_log_path=$(pwd)


while IFS= read -r line
do
	if [[ $line =~ ^#.* ]]
	then
		fconfig=""
	else
		fconfig=$line
	fi
	if [ ! -z $fconfig ]
	then
		let "num_event+=1"
         
		counter[$num_event]=$fconfig
	 
	fi 
done < "$config_cfg"

if [ $num_event == 4 ]
then
		while true ; do
		
		PS3='Choose Program to run: '
		options=("Start" "Setup" "Clean" "Quit")
		select opt in "${options[@]}"
		do	
			case $opt in
								"Start")
									read -p "Enter hrtimer (default 1ms): " hrtimer
									[ -z "$hrtimer" ] && hrtimer=1
									read -p "Enter program to monitor: " target_path
									log_path=$init_log_path"/Output.csv"
									config="${counter[@]} $hrtimer $log_path $target_path"
									#echo "Config: $config"
									#Clear Log
									> $log_path
									#echo $log_path
									sudo ./ioctl_start $config
									echo "-------------------------------------------"
									break
									;;
								"Clean")
									make clean
									sudo rm /dev/kleb
									sudo rmmod kleb
									break			
									;;
								"Setup")
									make
									sudo insmod kleb.ko
									maj_num=$(dmesg | tail | grep "The major number for your device is" | sed 's/^.*\([0-9][0-9][0-9]\)$/\1/g')
									#echo "Major Number: " $maj_num
									sudo mknod /dev/kleb c $maj_num 0
									break
									;;
								"Quit")
									exit
								break 
								;;
								*) echo "invalid option $REPLY" 
								
			esac
			
		done
		done
else
	echo "Invalid number of events $numcounter"
fi
