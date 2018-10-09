#!/usr/bin/python3

import os
import sys

os.environ["BUILD_FROM_KERNEL"]='yes'

dict_project={
        #sharkl2

        #pike2
        'sp7731e_1h10_native-userdebug'         : { 'path' : 'sprd-board-config/pike2/sp7731e_1h10/', },
        'sp7731e_1h20_native-userdebug'         : { 'path' : 'sprd-board-config/pike2/sp7731e_1h20/', },

	#sharkle
        'sp9832e_1h10_native-userdebug'		: { 'path' : 'sprd-board-config/sharkle/sp9832e_1h10/', },
        'sp9832e_10c20_native-userdebug'		: { 'path' : 'sprd-board-config/sharkle/sp9832e_10c20/', },
        'sp9832e_1h10_gofu-userdebug'		: { 'path' : 'sprd-board-config/sharkle/sp9832e_1h10_go/', },

	#sharkl3
        's9863a1h10_Natv-userdebug'		: { 'path' : 'sprd-board-config/sharkl3/sp9863a_1h10/', },
        's9863a1h10_Tsc-userdebug'		: { 'path' : 'sprd-board-config/sharkl3/sp9863a_1h10/', },

	#sharkl5
	'ums510_haps_native-userdebug'		: { 'path' : 'sprd-board-config/sharkl5/ums510_haps/', },
	'ums510_1h10_native-userdebug'		: { 'path' : 'sprd-board-config/sharkl5/ums510_1h10/', },

}

print_head="""
Lunch menu... pick a combo:

You're building on Linux
Pick a number:
choice a project
"""

def transform_dict_to_list():
    global list_project
    list_project=list(dict_project)
    list_project.sort()

def print_boardname():
    print(print_head, end='')
    for i in range(len(list_project)):
        print('     {}. {}'.format(i + 1, list_project[i]))
    print('Which would you like? [aosp_arm-eng] ')

def judge_parameters():
    global board_name

    if len(sys.argv) == 1:
        get_line=input()
    elif (len(sys.argv) == 2):
        get_line=sys.argv[1]
    else:
        print('The num of parameter is error.')
        return -1

    if get_line.isdigit():
        if (int(get_line) <= 0 or int(get_line) > len(list_project)):
            print('The num you input is out of range, please check.')
            return -1
        board_name=list_project[int(get_line)-1][:-10]
    else:
        if get_line in list_project:
            board_name=get_line[:-10]
            return 0

        for i in range(len(list_project)):
            if (get_line == list_project[i][:-5]):
                os.environ["TARGET_BUILD_VARIANT"]='user'
                board_name=get_line[:-5]
                return 0
        print('The board name was error, please check.')
        return -1

def get_board_attribute():
    path=dict_project[board_name+'-userdebug']['path']+board_name
    print('Get board attrubute from: ', path)
    os.environ["BOARD_PATH"]=path
    os.environ["TARGET_BOARD"]=board_name

def main():
    os.system('rm -rf .config')
    transform_dict_to_list()
    print_boardname()
    if (judge_parameters() == -1):
        return
    get_board_attribute()
    os.system('make -f AndroidKernel.mk config')

if __name__ == '__main__':
    main()

