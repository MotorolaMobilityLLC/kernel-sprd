#!/usr/bin/python3

# Report defconfig and toolchain to Aiaiai for checking

import os
import sys

default = 0
boardconfig_list=[]
def main():
	global toolchain

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
				kernel_defconfig = ""

				#**** set value
				kernel_defconfig = filename
				apath = os.path.join(maindir, filename)
				arch = apath.split("/").pop(1)

				#**** defconfig defined compiler
				if default == 1:
					toolchain = ""
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
					f.close()

				if arch == "arm64" and toolchain == "clang":
					cross_compile = "aarch64-linux-gnu-"
				elif arch == "arm" and toolchain == "clang":
					cross_compile = "arm-linux-androidkernel-"
				elif arch == "arm64" and toolchain == "gcc":
					cross_compile = "aarch64-none-linux-gnu-"
				elif arch == "arm" and toolchain == "gcc":
					cross_compile = "arm-none-linux-gnueabihf-"

				if kernel_defconfig != "" and arch != "" and cross_compile != "" and toolchain != "":
					boardconfig_list.append(kernel_defconfig+","+arch+","+cross_compile+","+toolchain)

	formatList = list(set(boardconfig_list))
	formatList.sort()

	string_output=""
	for boardconfig in formatList:
	        string_output+=boardconfig+" "

	print(string_output)

if __name__ == '__main__':
	if len(sys.argv) == 1:
		toolchain = "clang"
		default=1
	elif len(sys.argv) == 2:
		toolchain=sys.argv[1]
	main()
