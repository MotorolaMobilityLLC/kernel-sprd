#!/usr/bin/python3
# -*- coding: UTF-8 -*-

'''
脚本检查最近一次提交的修改的c文件是否在目标目录下生成对应的.o文件

使用方法：
1.脚本可以带参数：搜索.o文件的绝对路径目录
指令如：python3 scripts/sprd/sprd_check_compile.py [find target dir] [multiple check flag]
multiple check flag: 0:只检查一次; 1:多次检查,最终结果需综合多次检查的结果
或不带参数，脚本默认在kernel目录下的out_abi文件中进行搜索
指令如：python3 scripts/sprd/sprd_check_compile.py

2.如果修改的c文件生成了对应的.o文件就认为参与了编译，该检查pass
'''

import os
import sys
import subprocess

GET_PATCH_MODIFY_FILE_INFO = ' --pretty="format:" --name-only --diff-filter=AMCR'

#需要忽略编译检查的文件或目录请在Documentation/sprd_check_compile_ignore文件中添加。
#  每一行一个忽略项。
#  忽略具体文件时带上后缀名".c"
#  忽略到目录时请带上目录后的"/"
IGNORE_LIST_NAME = "Documentation/sprd_check_compile_ignore"
NO_COMPILE_LIST_NAME = "no_compile_list_temp"

def save_append(path, file_name, contents):
     save_file = path + '/' + file_name
     f = open(save_file, 'a')
     f.write(contents)
     f.close()

def read_line(path, file_name):
    read_file = path + '/' + file_name
    f = open(read_file, 'r')
    lines = f.readlines()
    f.close()
    return lines

def check_compile(modify_file_list, find_out_dir):
    not_compile_file_list = []
    ignore_list = []
    ignore_flag = 0

#get ignore list
    if os.path.exists(MAIN_PATH + '/../../' + IGNORE_LIST_NAME):
        ignore_list = read_line(MAIN_PATH + '/../../',IGNORE_LIST_NAME)

    for modify_file in modify_file_list:
        if len(modify_file) < 3:
            continue

        if modify_file.endswith('.c'):
            ignore_flag = 0
            find_name_path = modify_file.replace('.c', '.o')
            find_name = os.path.basename(os.path.abspath(find_name_path))
            find_status, find_output = subprocess.getstatusoutput('find ' + \
                    find_out_dir + ' -name ' + find_name + " | grep " + find_name_path)
            if len(find_output) < len(find_name):
                for ignore_path in ignore_list:
                    if ignore_path.strip() in modify_file:
                        ignore_flag = 1
                        break
                if ignore_flag == 0:
                    not_compile_file_list.append(modify_file)
            elif "No such file or directory" in find_output:
                print("ERROR: %s No such file or directory." % find_out_dir)
                not_compile_file_list.append(modify_file)

    return not_compile_file_list

if __name__ == '__main__':
    MAIN_PATH = os.path.dirname(os.path.abspath(sys.argv[0]))
    patch_modify_file_list = []
    multiple_check_flag = 0
    no_compile_list = []
    no_compile_list_temp = []

    if len(sys.argv) > 1:
        find_target_dir = sys.argv[1]
    else:
        find_target_dir = MAIN_PATH + '/../../../out_abi'
    #print("find target dir: %s" % find_target_dir)

    if len(sys.argv) > 2:
        multiple_check_flag = int(sys.argv[2])

    os.chdir(MAIN_PATH)

# get commit id
    status,output = subprocess.getstatusoutput("git log -1 --pretty=format:%H")
    commit_id = output

# get patch 
    status,output = subprocess.getstatusoutput("git show " + commit_id + GET_PATCH_MODIFY_FILE_INFO)
    patch_modify_file_list = output.split('\n')

    print("\nModified file(.c) list:")
    for x in patch_modify_file_list:
        if x.endswith('.c'):
            print("%s" % x)

    ret_info = check_compile(patch_modify_file_list, find_target_dir)

    if len(ret_info) > 0:
        if multiple_check_flag == 1:
            if os.path.exists(find_target_dir + '/' + NO_COMPILE_LIST_NAME):
                no_compile_list = read_line(find_target_dir, NO_COMPILE_LIST_NAME)
                os.remove(find_target_dir + '/' + NO_COMPILE_LIST_NAME)
                if len(no_compile_list) > 0:
                    for no_compile_item in no_compile_list:
                        if no_compile_item in ret_info:
                            no_compile_list_temp.append(no_compile_item)
                    ret_info = no_compile_list_temp
                else:
                    print("\ncheck compile ----------------------------------------- PASS\n")
                    sys.exit(0)
        if len(ret_info) > 0:
            if multiple_check_flag == 1:
                print("\nWARNING: Modified files did not participate in compilation!")
            else:
                print("\nWARNING: Modified files did not participate in compilation!")
            print("\nPlease add config to Documentation/sprd-gki-diff-config!")
            print("\nList of files not involved in compilation:")
            for x in ret_info:
                print("%s" % x)
                if multiple_check_flag == 1:
                    save_append(find_target_dir, NO_COMPILE_LIST_NAME, x)
            sys.exit(1)

    print("\ncheck compile ----------------------------------------- PASS\n")
    sys.exit(0)
