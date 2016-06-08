#! /bin/bash

#LC1860_PHONE
#LC1860_EVB
#LC1860_EVB2
#A310T_V1_0
#A310T_V1_1

list="
LC1860_PHONE
LC1860_EVB
LC1860_EVB2
A310T_V1_0
A310T_V1_1
LTE26007_V1_0
LTE26007_V1_1
"
C_ERR="\033[1;31;40m"	#red
C_OK="\033[1;32;40m"	#green
C_WARN="\033[1;34;40m"	#blue
C_DEF="\033[0m"
rm -rf log
mkdir log
for prj in $list; do
	echo -e "\ncompiling $prj..."
	./make_zImage.sh $prj > log/build-$prj.log 2>log/build-$prj.log
	error_s=`grep -rni error log/build-$prj.log`
	warn_s=`grep -rni warning log/build-$prj.log`
	if [[ ! -z "$error_s" ]]; then
		echo -e "$prj	$C_ERR something wrong $C_DEF"
		echo "$error_s"
	elif [[ ! -z "$warn_s" ]]; then
		echo -e "$prj	$C_WARN warning $C_DEF"
		echo "$warn_s"
	else
		echo -e "$prj	$C_OK ok $C_DEF"
	fi
done
echo "finish!"
