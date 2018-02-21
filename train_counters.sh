#! /bin/sh

for i in {1..10002}; do
	sudo ./ioctl_start
	sudo ./ioctl_stop
done

cd ../LR_v13_tfdeploy
sed -i 's/,$//' input.csv
cat input.csv > input2.csv
cat input.csv >> input2.csv

echo "# Type,Fname,HPC1,HPC2,HPC3,HPC4" > input.csv
cat input2.csv >> input.csv


python3 lr_script_OC_training.py

