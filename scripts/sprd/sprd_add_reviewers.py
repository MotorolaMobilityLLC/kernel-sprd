#!/usr/bin/python
# -*- coding: UTF-8 -*-

'''
' 本脚本根据最新一笔patch中的tags和reviewers的base文件中的tags进行匹配,
' 输出所有匹配的reviewer;对带有K的对patch中的修改内存进行关键字匹配,
' 输出匹配的reviewer。
'
' 规则:
'  1.输出最新一笔patch中tags和base中tags匹配对应的reviewer
‘  2.对base中带有K:后的字符串和patch的修改内存进行匹配，输出匹配的reviewer
'  3.如果tags和关键字都匹配不上输出默认reviewer，默认reviewer只有一人，可使用D:进行修改
'  4.对于属性标签[:PRIVATE)之间的，输出默认reviewer，不在进行后面tags和关键字匹配
'
' 输出:
'  Patch title:    Bug #1407851 scripts: sprd: auto add reviewers
'
'  Add reviewers automatically:
'  nianfu.bai@unisoc.com ruifeng.zhang1@unisoc.com
'''

import os
import sys
import commands

MAIN_PATH = ''
TAGS_FILE_NAME = '../../Documentation/sprd-tags.txt'
REIWERS_FILE_NAME = '../../SPRD-REVIEWERS'

GET_PATCH_INFO_COMMANDS = 'git log -1 -p'

ATTRIBUTE_TAGS  = []
REVIEWERS_BASE = []
DEFAULT_REVIEWER = "orson.zhai"
DEFAULT_MAIL = "@unisoc.com"

def read_line(path, file_name):
    read_file = path + '/' + file_name
    f = open(read_file, 'rb')
    lines = f.readlines()
    f.close()
    return lines

def get_attribute_tags():
    global ATTRIBUTE_TAGS
    get_tags_flag = 0

    read_tags_list = read_line(MAIN_PATH, TAGS_FILE_NAME)

    for x in read_tags_list:
        if "\n" in x:
            x = x.strip("\n")

        if "[info]" in x and get_tags_flag == 0:
            get_tags_flag = 1
            continue
        elif get_tags_flag == 0:
            continue

        if "," in x and get_tags_flag == 1:
            ATTRIBUTE_TAGS = x.split(",")
#            print("attribute tags:%s" % ATTRIBUTE_TAGS)

def get_reviewers():
    global REVIEWERS_BASE
    global DEFAULT_REVIEWER
    reviewer = ''
    power = ''
    key_words_list = []
    tags_list = []

    read_reviewers_list = read_line(MAIN_PATH, REIWERS_FILE_NAME)

    for x in read_reviewers_list:
        if "\n" in x:
            x = x.strip("\n")

        if "[" in x :
            if len(reviewer) > 0:
                REVIEWERS_BASE.append([reviewer, power, key_words_list, tags_list])
                power = ''
                key_words_list = []
                tags_list = []
            reviewer = x.strip("[").strip("]")
        elif "R: " in x:
            power = x.strip("R: ")
        elif "K: " in x:
            key_words_list.append(x.strip("K: ").strip('"'))
        elif "T: " in x:
            tags_list.append(x.split("T: ")[1])
        elif "D: " in x:
            DEFAULT_REVIEWER = x.strip("D: ")

    if len(reviewer) > 0:
        REVIEWERS_BASE.append([reviewer, power, key_words_list, tags_list])

#    print("reviewer list:")
#    for x in REVIEWERS_BASE:
#        print("%s" % x)

def find_last_char(string, p):
    index = 0
    i = 0
    for x in string:
        if p == x:
            index = i
        i += 1

    return index

def add_reviewers_automatically(msg_list, diff_str):
    global check_tags_flag
    reviewers_list = []
    tags_list = []
    start_num = 0
    add_flag = 0
    key_words_sub_list = []
    diff_str_temp = ''

    for x in msg_list:
        if "Bug #" in x:
            print("Patch title:%s\n" % x)

            tags_list = x[x.index("Bug #") + len("Bug #"):find_last_char(x, ":")].split(' ')[1:]
            # 删除标签中的空格
            tags_strings = x[x.index("Bug #") + len("Bug #"):find_last_char(x, ":") + 1].replace(" ","")
#            print("tags list:%s" % tags_list)

            # 如果属性标签是不需要检查字标签的，将reviewer设置成default reviewer，直接返回
            if tags_list[start_num].strip(":") in ATTRIBUTE_TAGS[0:ATTRIBUTE_TAGS.index("PRIVATE")]:
                reviewers_list.append(DEFAULT_REVIEWER)
                print("attribute set default reviewer")
                break

            for x in REVIEWERS_BASE:
                add_flag = 0
                for tag in x[3]:
                    if tag in tags_strings:
                        reviewers_list.append(x[0])
                        add_flag = 1
                        break
                # 如果key words不为空则在diff中搜索关键字来判断是否需要将reviewer人员增加到list中
                if len(x[2]) > 0 and add_flag == 0:
                    for key_words in x[2]:
                        diff_str_temp = diff_str
                        # 在diff str中搜索关键字
                        if '*' in key_words:
                            key_words_sub_list = key_words.split('*')
                            # 设置默认增加reviewer标志为增加
                            add_flag = 1
                            for key_sub_words in key_words_sub_list:
                                if key_sub_words not in diff_str_temp:
                                    add_flag = 0
                                    break
                                else:
                                    diff_str_temp = diff_str_temp[diff_str_temp.index(key_sub_words):]
                            if add_flag == 1:
                                reviewers_list.append(x[0])
                                break
                        elif key_words in diff_str:
                            reviewers_list.append(x[0])
                            break

    return reviewers_list

if __name__ == '__main__':
    ret_info = []
    patch_msg_info_list = []
    patch_diff_info_str = ''
    get_reviewers_list = []

    MAIN_PATH = os.path.dirname(os.path.abspath(sys.argv[0]))

#    print "main path: %s\n" % MAIN_PATH

    status,output=commands.getstatusoutput(GET_PATCH_INFO_COMMANDS)

    patch_msg_info_list = output[:output.find('diff --git')].split('\n')
    patch_diff_info_str = output[output.find('diff --git'):]

#    print("patch msg info:")
#    for x in patch_msg_info_list:
#        print("%s" % x)
#    print("\npatch diff info:")
#    for x in patch_diff_info_list:
#        print("%s" % x)
    get_attribute_tags()
    get_reviewers()
    get_reviewers_list = add_reviewers_automatically(patch_msg_info_list, patch_diff_info_str)

    if len(get_reviewers_list) == 0:
#        print("not same tags set default reviewer")
        get_reviewers_list.append(DEFAULT_REVIEWER)

    print("Add reviewers automatically:")
    for x in get_reviewers_list:
        print("%s" % (x + DEFAULT_MAIL)),
