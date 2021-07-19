#!/usr/bin/env python
#-*- coding: utf-8 -*-
#!/usr/bin/python3

import os
import sys
import commands

d_gki_diff_config = {}
d_gki_defconfig = {}
d_defconfig = {}
failure_flag = 0
tmp_path_def = "./tmp_config_check/"
gki_diff_path = "Documentation/sprd-gki-diff-config"
clang_version=commands.getoutput("cat ./build.config.common |grep clang-r |awk -F'/' '{print $(NF-1)}'")
clang_path = os.path.abspath("../../toolchain/prebuilts/clang/host/linux-x86/" + clang_version + "/bin")
gcc_path = os.path.abspath("../../toolchain/prebuilts/gcc/linux-x86/aarch64/gcc-linaro-aarch64-linux-gnu-7.4/bin/aarch64-linux-gnu-")

def check_consistency(android_version):
	global failure_flag

	print("======================================" + android_version.split('/')[1] + "======================================")
	for soc in d_defconfig:
		print("======================================" + soc + "======================================")
		for config in d_defconfig[soc]:
			if d_defconfig[soc][config] == "y":
				if ( config not in d_gki_defconfig or d_gki_defconfig[config] != "y" ) and ( config not in d_gki_diff_config or d_gki_diff_config[config] != "y" ):
					if check_gki_choice_config(config, "y"):
						print("\033[1;31;40m ERROR: " + config + " isn't the subset of gki_defconfig + gki_diffconfig.\033[0m")
						failure_flag = 1
			elif d_defconfig[soc][config] == "m":
				if ( config not in d_gki_defconfig or d_gki_defconfig[config] != "m" ) and ( config not in d_gki_diff_config or d_gki_diff_config[config] != "m" ):
					if check_gki_choice_config(config, "m"):
						print("\033[1;31;40m ERROR: " + config + " isn't the subset of gki_defconfig + gki_diffconfig.\033[0m")
						failure_flag = 1
			elif d_defconfig[soc][config] == "n":
				if ( config in d_gki_defconfig and d_gki_defconfig[config] != "n" ) and ( config in d_gki_diff_config and d_gki_diff_config[config] != "n" ):
					if check_gki_choice_config(config, "n"):
						print("\033[1;31;40m ERROR: " + config + " is not set, but is set y or m in the .config compiled by (gki_defconfig + gki_diffconfig).\033[0m")
						failure_flag = 1
					else:
						print("WARNING: " + config + " is not set, and is set y or m in the .config compiled by (gki_defconfig + gki_diffconfig), but exists in sprd-gki-choice-config.")
		print("======================================" + soc + "======================================")

def check_gki_choice_config(config, status):
	f = open("Documentation/sprd-gki-choice-config")
	lines = f.readlines()
	if status == "y":
		for i in range(len(lines)):
			if lines[i].strip() == "ADD:"+config:
				return 0
	elif status == "n":
		for i in range(len(lines)):
			if ( lines[i].strip() == "ADD:"+config or lines[i].strip() == "MOD:"+config ):
				return 0
	elif status == "m":
		for i in range(len(lines)):
			if lines[i].strip() == "MOD:"+config:
				return 0
	return 1

# Only check the user version
# d_gki_diff_config={config_name:y/m/n}
def create_gki_diff_config_dict():
	ret = os.system("make LLVM=1 LLVM_IAS=1 CLANG_TRIPLE=aarch64-linux-gnu- ARCH=arm64 CROSS_COMPILE=" + gcc_path + " gki_defconfig O=" + tmp_path_def)
	ret += os.system("bash scripts/sprd/sprd_create_user_config.sh " + tmp_path_def + "/.config " + gki_diff_path)
	ret += os.system("make LLVM=1 LLVM_IAS=1 CLANG_TRIPLE=aarch64-linux-gnu- ARCH=arm64 CROSS_COMPILE=" + gcc_path + " olddefconfig O=" + tmp_path_def)
	if (ret):
		exit(1)

	f = open(tmp_path_def+"/.config",'r')
	lines = f.readlines()
	for i in range(len(lines)):
		# the config set to value or string same as y
		if "CONFIG_" in lines[i] and '=m' in lines[i]:
			config_name = lines[i].split('=')[0]
			d_gki_diff_config[config_name] = "m"
		elif "CONFIG_" in lines[i] and '=' in lines[i]:
			config_name = lines[i].split('=')[0]
			d_gki_diff_config[config_name] = "y"
		elif "# CONFIG_" in lines[i] and 'is not set' in lines[i]:
			config_name = lines[i].split(' ')[1]
			d_gki_diff_config[config_name] = 'n'
	f.close()

def create_gki_defconfig_dict():
	if (os.system("make LLVM=1 LLVM_IAS=1 CLANG_TRIPLE=aarch64-linux-gnu- ARCH=arm64 CROSS_COMPILE=" + gcc_path + " gki_defconfig O=" + tmp_path_def)):
		exit(1)

	f = open(tmp_path_def+"/.config",'r')
	lines = f.readlines()
	for i in range(len(lines)):
		# the config set to value or string same as y
		if "CONFIG_" in lines[i] and '=m' in lines[i]:
			config_name = lines[i].split('=')[0]
			d_gki_defconfig[config_name] = "m"
		elif "CONFIG_" in lines[i] and '=' in lines[i]:
			config_name = lines[i].split('=')[0]
			d_gki_defconfig[config_name] = "y"
		elif "# CONFIG_" in lines[i] and 'is not set' in lines[i]:
			config_name = lines[i].split(' ')[1]
			d_gki_defconfig[config_name] = 'n'
	f.close()

#d_defconfig={'project_name':{config_name:y/n},}
def create_defconfig_dict(android_version):
	(status, output)=commands.getstatusoutput("find ./arch/arm64 -name 'sprd_*_defconfig'|grep -v sprd_debian_defconfig")
	print("Checking defconfig as following:\n" + output)

	# defconfig format : ./arch/arm64/configs/sprd_sharkl3_defconfig
	# 1) for each defconfig in arch/arm64
	# 2) get the basic information about defconfig
	# 3) compile firstly to generate expanded .config
	# 4) patch the user_diff_config to .config
	# 5) re-make .config to check the dependency
	# 6) accroding the .config create the dict d_defconfig[soc][config_name]
	for defconfig_path in output.split("\n"):
		arch = defconfig_path.split("/").pop(2)
		soc = defconfig_path.split("/").pop().split("_").pop(1)
		defconfig = defconfig_path.split("/").pop()

		d_defconfig[soc] = {}

		print("Compile first time for " + defconfig)
		ret=os.system("make LLVM=1 LLVM_IAS=1 CLANG_TRIPLE=aarch64-linux-gnu- ARCH=arm64 CROSS_COMPILE=" + gcc_path + " O=" + tmp_path_def + " " + defconfig)

		(status, diffconfig)=commands.getstatusoutput("find " + android_version + "/" + soc + "/" + arch + " -name *user_diff_config")
		if (status == 0 and diffconfig != ""):
			print("Add user diffconfig for " + soc)
			ret += os.system("bash scripts/sprd/sprd_create_user_config.sh " + tmp_path_def + "/.config " + diffconfig)
			print("Re-compile .config for " + defconfig)
			ret += os.system("make LLVM=1 LLVM_IAS=1 CLANG_TRIPLE=aarch64-linux-gnu- ARCH=arm64 CROSS_COMPILE=" + gcc_path + " O=" + tmp_path_def + " olddefconfig")
		else:
			print("WARNING:"+soc+": Cannot patch the user_diff_config to .config")

		if (ret):
			exit(1)

		f_defconfig = open(tmp_path_def + "/.config")
		lines = f_defconfig.readlines()

		for i in range(len(lines)):
			# the config set to value or string same as y
			if "CONFIG_" in lines[i] and '=m' in lines[i]:
				config_name = lines[i].split('=')[0]
				d_defconfig[soc][config_name] = 'm'
			elif "CONFIG_" in lines[i] and '=' in lines[i]:
				config_name = lines[i].split('=')[0]
				d_defconfig[soc][config_name] = 'y'
			elif "# CONFIG_" in lines[i] and 'is not set' in lines[i]:
				config_name = lines[i].split(' ')[1]
				d_defconfig[soc][config_name] = 'n'
		f_defconfig.close()

def pre_env():
	global gcc_path

	if os.path.exists(tmp_path_def):
		os.system("rm -rf " + tmp_path_def)

	if (len(sys.argv) == 1):
		# Using the clang and gcc already export to PATH.
		gcc_path="aarch64-linux-gnu-"
	else:
		# export the clang version to PATH.
		os.environ['PATH']= clang_path + ":" + os.environ['PATH']


def usage():
	print("""
    Usage: ./scripts/sprd/sprd_check-gki_consistency.py [CROSS_COMPILE] [CLANG_PATH] [FLAG]

    Check sprd defconfig for consistency with gki requirement.

    CROSS_COMPILE : Set to the directory of gcc cross compile. (Need absolute path)
                    e.g /home/toolchain/prebuilts/gcc/linux-x86/aarch64/aarch64-linux-android-4.9/bin/aarch64-linux-android-
    CLANG_PATH    : Set to the clang path. (Need absolute path)
                    e.g /home/toolchain/prebuilts/clang/host/linux-x86/clang-r353983c/bin
    FLAG          : Optional
                    modify    : Modified the issue configs to sprd defconfig
                    no-modify : Only print ERROR message, don't modify the code.
                    default is no-modify
    """)

def clean():
	os.system("rm -rf " + tmp_path_def)

# sys.argv[1]	: CROSS_COMPILE
# sys.argv[2]	: CLANG_PATH
# sys.argv[3]	: FLAG
def main():
	if (len(sys.argv) == 1):
		print("Using gcc and clang already export to PATH.")
	elif (len(sys.argv) == 2 and sys.argv[1] == "default"):
		print("Using default gcc and clang version.")
	elif (len(sys.argv) >= 3):
		gcc_path = sys.argv[1]
		clang_path = sys.argv[2]
	else:
		print("Parameters error, please check.")
		usage()
		exit(1)
	pre_env()
	create_gki_diff_config_dict()
	create_gki_defconfig_dict()
	(stat, android_versions) = commands.getstatusoutput("find sprd-diffconfig/ -type d -name \"android*\"")
	for android_version in android_versions.split('\n'):
		d_defconfig = {}
		create_defconfig_dict(android_version)
		check_consistency(android_version)
	clean()
	if (failure_flag != 0):
		sys.exit(1)

if __name__ == '__main__':
	main()

