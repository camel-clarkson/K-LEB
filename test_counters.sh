#! /bin/sh
echo "" > ../LR_v13_tfdeploy/input.csv

sudo ./ioctl_start
sudo ./ioctl_stop

cd ../LR_v13_tfdeploy
tail -n -1 input.csv | sed 's/^1,foo,//' | sed 's/,/\n/g' > input.txt
python3 lr_script_OC_testing_only_2.py

