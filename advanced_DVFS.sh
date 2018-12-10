#!/bin/bash

# How to use : ./advanced_DVFS.sh target_temperature, # of cores, maximum frequency (MHz)


if [ $# -ne 3 ]; then
	echo "--------------------------------------------------------------------------"
	echo "You should supply target_temperature, # of cores, maximum frequency (KHz)."
	echo "Example: ${0} 80 8 4200000"
	echo "--------------------------------------------------------------------------"
	exit 2
fi

tempwall=10		# temperature where predicion is started at
target_T=$1		# target temeprature
pred_T=100		# predicted temperature
num_cores=$2 		# number of cores
maximum_freq=$3 	# maximum frequency
target_freq=$3  	# target frequency

#	Correlation coefficient of linear regression model
#	linear regression model was established using the following:
#	
#	EXEC	IPC	FREQ	AFREQ	L3MPI			READ		
#	WRITE	INST	PhysIPC	INSTnom	Proc_Energy_(Joules)	Total_Util	
#	
#	These can vary depending on used variables
#	
coef=( 103.266751 -10.7134869 -22.9760373 -13.4503406 -3.82154816 -2.37912808
	-4.28318653 -0.0331801658 7.61166971 -10.2039915 16.5466676 -1.89475463 )

current_info=( 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 )

float_mul() # $1 * $2
{
	echo "$1*$2" | bc
}

float_add() # $1 + $2
{
	echo "$1+$2" | bc
}

float_sub() # $1 - $2
{
	echo "$1-$2" | bc
}

float_to_int()
{
	echo "$1/1" | bc
}

# predict temperature using linear regression model 
# $1: frequency (GHz) 
pred_temperature() 
{
	pred_T=30
	for i in {0..11};
	do
		result_mul=$(float_mul ${coef[i]} ${current_info[i]})
		pred_T=$(float_add $pred_T $result_mul)
	done
	
	result_mul=$(float_mul "2.77988233" $1)	# constant is coef of frequency
	result_add=$(float_add $pred_T $result_mul)
	pred_T=$(float_to_int $result_add)
	
}

# find max frequency that maintains the temperature below target temperature 
# $1: maximum_freq 
find_freq() 
{
	freq_check=$(float_add $1 0.1)
	while [ $pred_T -gt $target_T ]
	do
		#echo "freq: $freq_check"
		pred_temperature $freq_check		
		freq_check=$(float_add $freq_check -0.1)
	done

	target_freq=$freq_check
	#echo $target_freq
}

# set cpu frequency 
# $1: number of cores 
# $2: target frequency (MHz) 
set_freq()
{
	core=0
	while [ $core -lt $1 ]
	do
		cpufreq-set -f $(float_to_int $2) -c $core
		core=$(($core+1))
	done
}

while [ $# -gt 0 ]
do
	if [ $(($(head -1 /sys/class/hwmon/hwmon0/temp1_input)/1000)) -gt $tempwall ]; then
		current_info=(`cat current_state`)
		find_freq $(float_mul $maximum_freq 0.000001)
#		echo "------------------------------"
#		echo "Temp :$(($(head -1 /sys/class/hwmon/hwmon0/temp1_input)/1000))"
#		echo "Pred :$pred_T"
		set_freq $num_cores $(float_mul $target_freq 1000000)
#		echo "target_F: $target_freq"
		pred_T=100
	fi
	sleep 0.1
done



