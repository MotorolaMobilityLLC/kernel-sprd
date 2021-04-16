#!/bin/bash

if [[ `git cat-file -p HEAD | grep parent | wc -l` == 1 ]]; then
	echo "The latest commit isn't kernel update, please check."
	exit
fi

tmp_path_def="./tmp_config_check/"

clang_version=`cat ./build.config.common | grep clang-r | awk -F"/" {'print $(NF-1)'} | tail -n 1`
clang_path=$(readlink -f "../../toolchain/prebuilts/clang/host/linux-x86/$clang_version/bin")
gcc_path=$(readlink -f "../../toolchain/prebuilts/gcc/linux-x86/aarch64/gcc-linaro-aarch64-linux-gnu-7.4/bin/aarch64-linux-gnu-")

# Save old PATH for restore
OLD_PATH=$OLDPWD
# Add clang and gcc absolute path to env
PATH=$clang_path:$PATH
PATH=$gcc_path:$PATH

# parent_1 is unisoc, parent_2 is upstream
parent_1=`git log -n 1 | head -n 2 | tail -n 1 | awk -F" " {'print $2'}`
parent_2=`git log -n 1 | head -n 2 | tail -n 1 | awk -F" " {'print $3'}`
curr_commit=`git log --oneline | head -n 1 | awk -F" " {'print $1'}`

# compile the patched gki_defconfig, named new_gki_defconfig
make CC=clang LD=ld.lld CLANG_TRIPLE=aarch64-linux-gnu- ARCH=arm64 CROSS_COMPILE=$gcc_path gki_defconfig O=$tmp_path_def
cp $tmp_path_def/.config $tmp_path_def/new_gki_defconfig

# compile the original gki_defconfig, named old_gki_defconfig
git checkout $parent_1
make CC=clang LD=ld.lld CLANG_TRIPLE=aarch64-linux-gnu- ARCH=arm64 CROSS_COMPILE=$gcc_path gki_defconfig O=$tmp_path_def
cp $tmp_path_def/.config $tmp_path_def/old_gki_defconfig

git checkout $curr_commit

# diff result: < is left, > is right
diff $tmp_path_def/new_gki_defconfig $tmp_path_def/old_gki_defconfig | grep CONFIG > $tmp_path_def/gki_diff

# compile the sprd_*_defconfig, named sprd_<SoC>_defconfig, same to arch/arm64/config
sprd_defconfig_list=`find ./arch/arm64 -name "sprd_*_defconfig"  | grep -v debian`
for sprd_defconfig in $sprd_defconfig_list;
do
	defconfig_name=`echo $sprd_defconfig | awk -F"/" {'print $NF'}`
	make CC=clang LD=ld.lld CLANG_TRIPLE=aarch64-linux-gnu- ARCH=arm64 CROSS_COMPILE=$gcc_path $defconfig_name O=$tmp_path_def
	cp $tmp_path_def/.config $tmp_path_def/generated_$defconfig_name

	while read line
	do
		if [[ `echo $line | grep "<"` ]]; then
			if [[ `echo $line | grep =` ]]; then
				config=`echo $line | awk -F"=" {'print $1'} | awk -F" " {'print $2'}`
				if [[ `echo $line | grep =m` ]]; then
					gki_state="=m"
				elif [[ `echo $line | grep =y` ]]; then
					gki_state="=y"
				else
					gki_state=`echo $line | awk -F"=" {'print $2'}`
				fi
			else
				config=`echo $line | awk -F" " {'print $3'}`
				gki_state=" is not set"
			fi
		fi

		if [[ $gki_state = " is not set" ]]; then
			gki_diff_line=`grep "$config" -w Documentation/sprd-gki-diff-config`
			if [[ $gki_diff_line == "" ]]; then
				echo "# $config$gki_state" >> $tmp_path_def/generated_$defconfig_name
			fi
		elif [[ $gki_state == "=y" ]]; then
			echo "$config$gki_state" >> $tmp_path_def/generated_$defconfig_name
		elif [[ $gki_state == "=m" ]]; then
			echo "$config$gki_state" >> $tmp_path_def/generated_$defconfig_name
		else
			echo "$config=$gki_state" >> $tmp_path_def/generated_$defconfig_name
		fi
	done < $tmp_path_def/gki_diff

	cp $tmp_path_def/generated_$defconfig_name $tmp_path_def/.config
	make CC=clang LD=ld.lld CLANG_TRIPLE=aarch64-linux-gnu- ARCH=arm64 CROSS_COMPILE=$gcc_path olddefconfig O=$tmp_path_def
	cp $tmp_path_def/.config $sprd_defconfig
done

rm -rf $tmp_path_def
cd $OLD_PATH

