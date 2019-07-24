#!/usr/bin/python3

import os
import sys
import re

defconfig_list=[]
boardconfig_list=[]
def main():
	result=[]
	for maindir,subdir,file_name_list in os.walk("arch"):
		"""
		print("1:",maindir) #current dir
		print("2:",subdir) # all subdirs
		print("3:",file_name_list)  #all subfiles
		"""
		for filename in file_name_list:
			if "sprd" in filename and "defconfig" in filename:
				apath = os.path.join(maindir, filename)
				result.append(apath)

				#apath value example: arch/arm64/configs/sprd_all_defconfig
				arch = (apath.split("/").pop(1))
				name = (apath.split("/").pop(3))
				path = apath

				defconfig_list.append(name+","+arch)

	for maindir,subdir,file_name_list in os.walk("sprd-board-config"):
		toolchain=""
		cross_compile=""
		kernel_defconfig=""
		"""
		print("1:",maindir) #current dir
		print("2:",subdir) # all subdirs
		print("3:",file_name_list)  #all subfiles
		"""
		for filename in file_name_list:
			apath = os.path.join(maindir, filename)
			result.append(apath)

			f=open(apath,'r')
			lines = f.readlines()
			for j in range(len(lines)):
				if '#' in lines[j]:
					continue

				if '"' in lines[j][-2:]:
					tmpline = lines[j][:-2]
				else:
					tmpline = lines[j][:-1]

				matchObj = re.search(r'sprd(.*)defconfig', tmpline)
				if matchObj:
					kernel_defconfig=matchObj.group()

				matchObj = re.search(r'KERNEL_ARCH(.*)=(.*)', tmpline)
				if matchObj:
					arch=matchObj.group(2).strip('"').strip()

				if 'CROSS_COMPILE' in tmpline:
					matchObj = re.search(r'(.*)-' ,tmpline.split("/").pop())
					if matchObj:
						cross_compile=matchObj.group(0)

				if 'BSP_MAKE_EXTRA_ARGS' in tmpline and toolchain != 'clang':
					toolchain="clang"
				elif toolchain=="":
					toolchain="gcc"

		if kernel_defconfig!="" and arch!="" and cross_compile!="" and toolchain!="":
			boardconfig_list.append(kernel_defconfig+","+arch+","+cross_compile+","+toolchain)

	formatList = list(set(boardconfig_list))
	formatList.sort()

	string_output=""
	for boardconfig in formatList:
		for defconfig in defconfig_list:
			two_head_string=boardconfig.split(",").pop(0)+","+boardconfig.split(",").pop(1)
			if defconfig == two_head_string:
				string_output+=boardconfig+" "

	print(string_output)
if __name__ == '__main__':
    main()
