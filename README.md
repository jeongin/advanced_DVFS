# advanced_DVFS

Linux shell script for advanced DVFS technique.

Set a maximum temperature, # of cores, maximum frequency of your devices. It predict the higest temperature of cores at different frequencies. The highest one is adopted for CPU frequency.

This script must be run with sudo privileges. Only Celsius temperature are supported. This example will limit system temperature to 50 Celsius (System has 8 cores. Maximum frequency is 4.2GHz):
./advanced_DVFS.sh 50 8 4200000
