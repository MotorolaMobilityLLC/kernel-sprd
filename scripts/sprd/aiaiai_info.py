#!/usr/bin/python3

# Report defconfig and toolchain to Aiaiai for checking

import os
import sys

boardconfig_list=[]
def main():
	for maindir,subdir,file_name_list in os.walk("arch/arm64"):
		"""
		print("1:",maindir) #current dir
		print("2:",subdir) # all subdirs
		print("3:",file_name_list)  #all subfiles
		"""
		for filename in file_name_list:
			if filename == "gki_defconfig":
				boardconfig_list.append("gki_defconfig,arm64,aarch64-linux-gnu-,clang")
			elif filename[:4] == "sprd" and filename[-9:] == "defconfig":
				#**** init
				toolchain = ""
				cross_compile = ""
				kernel_defconfig = ""

				#**** set value
				kernel_defconfig = filename
				arch = "arm64"
				apath = os.path.join(maindir, filename)
				f=open(apath,'r')
				lines = f.readlines()
				for j in range(len(lines)):
					if "CONFIG_CC_IS_GCC" in lines[j] and "=" in lines[j]:
						toolchain = "gcc"
						break
					elif "CONFIG_CC_IS_CLANG" in lines[j] and "=" in lines[j]:
						toolchain = "clang"
						break
					else:
						continue
				cross_compile = "aarch64-linux-gnu-"

				if kernel_defconfig != "" and arch != "" and cross_compile != "" and toolchain != "":
					boardconfig_list.append(kernel_defconfig+","+arch+","+cross_compile+","+toolchain)

	formatList = list(set(boardconfig_list))
	formatList.sort()

	string_output=""
	for boardconfig in formatList:
	        string_output+=boardconfig+" "

	print(string_output)

if __name__ == '__main__':
    main()
