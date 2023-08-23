#!/bin/bash
# Config: Event Counters, Timer Delay, Log path, Program Name

declare -a counter
num_event=0

config_cfg="./perf.cfg"
init_log_path=$(pwd)

# Read events from configuration file
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
printf -v joined '%s,' "${counter[@]}"
events="${joined%,}"
echo "Event: $events"
# Run script

		while true ; do
			echo "K-LEB program script"
			PS3='Select your option: '
			options=("Start monitoring" "Setup the kernel module" "Unload the kernel module" "Exit")
			select opt in "${options[@]}"
			do	
				case $opt in
									"Start monitoring")
										read -p "Enter hrtimer (in ms): " hrtimer
										[ -z "$hrtimer" ] && hrtimer=1
										read -p "Enter program to monitor with parameters: " target_path
										log_path=$init_log_path"/Output.csv"
										#config="${counter[@]} $hrtimer $log_path $target_path"
										config="-e $events -t $hrtimer -o $log_path $target_path"
										echo $config
										> $log_path
										sudo ./ioctl_start $config
										echo "-------------------------------------------"
										break
										;;
									"Unload the kernel module")
										make clean
										sudo rm /dev/kleb
										sudo rmmod kleb
										echo "-------------------------------------------"
										break			
										;;
									"Setup the kernel module")
										make
										sudo insmod kleb.ko
										maj_num=$(dmesg | tail -n 5 | grep "The major number for your device is" | sed 's/^.*\([0-9][0-9][0-9]\)$/\1/g')
										sudo mknod /dev/kleb c $maj_num 0
										echo "-------------------------------------------"
										break
										;;
									"Exit")
										exit
									break 
									;;
									*) echo "invalid option $REPLY" 
									
				esac
				
			done
		done
