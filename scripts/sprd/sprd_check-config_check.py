#!/usr/bin/python3

import os
import sys

kernel_path="../../"
d_sprdconfig={}
d_defconfig={}
not_defined=0
config_path=kernel_path+"Documentation/sprd-configs.txt"
file_name='need_completed.txt'
list_configs=[]
flag_modify=0
tool_name=sys.argv[0][2:-3]

d_defconfig_path={
        'pike2':{'defconfig':kernel_path+'arch/arm/configs/sprd_pike2_defconfig', 'diffconfig':kernel_path+'sprd-diffconfig/pike2'},
        'sharkle32':{'defconfig':kernel_path+'arch/arm/configs/sprd_sharkle_defconfig', 'diffconfig':kernel_path+'sprd-diffconfig/sharkle'},
        'sharkl3':{'defconfig':kernel_path+'arch/arm64/configs/sprd_sharkl3_defconfig', 'diffconfig':kernel_path+'sprd-diffconfig/sharkl3'},
        'sharkle':{'defconfig':kernel_path+'arch/arm64/configs/sprd_sharkle_defconfig', 'diffconfig':kernel_path+'sprd-diffconfig/sharkle'},
        'sharklefp':{'defconfig':kernel_path+'arch/arm/configs/sprd_sharkle_fp_defconfig', 'diffconfig':kernel_path+'sprd-diffconfig/sharkle'},
        'roc1':{'defconfig':kernel_path+'arch/arm64/configs/sprd_roc1_defconfig', 'diffconfig':kernel_path+'sprd-diffconfig/roc1'},
}

d_diffconfig={}
def add_diffconfig_to_dictconfig():
    result=[]
    global d_sprdconfig

    for i in range(len(sys.argv)-2):
        if len(sys.argv) > 1 and sys.argv[i+2] in d_defconfig_path:
            for maindir,subdir,file_name_list in os.walk(d_defconfig_path[sys.argv[i+2]]['diffconfig']):
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
                        if 'ADD' in lines[j]:
                            d_diffconfig[lines[j][11:-1]]={'arch':'','plat':'','field':'','subsys':'','must':'','function':''}
                    f.close

def create_sprdconfigs_dict():
    f_sprdconfig = open(config_path)
    lines = f_sprdconfig.readlines()
    for i in range(len(lines)):
        if "\t" not in lines[i]:
            config_name=lines[i][:-1]
            d_sprdconfig[config_name]={'arch':'','plat':'','field':'','subsys':'','must':'','function':''}
            if i+6 < len(lines):
                if "[arch]" in lines[i+1]:
                    d_sprdconfig[config_name]['arch']=lines[i+1][8:-1]
                if "[plat]" in lines[i+2]:
                    d_sprdconfig[config_name]['plat']=lines[i+2][8:-1]
                if "[field]" in lines[i+3]:
                    d_sprdconfig[config_name]['field']=lines[i+3][9:-1]
                if "[subsys]" in lines[i+4]:
                    d_sprdconfig[config_name]['subsys']=lines[i+4][10:-1]
                if "[must]" in lines[i+5]:
                    d_sprdconfig[config_name]['must']=lines[i+5][8:-1]
                if "[function]" in lines[i+6]:
                    d_sprdconfig[config_name]['function']=lines[i+6][12:-1]
                    for j in range(15):
                        if i+7+j >= len(lines):
                            break
                        if "\t" not in lines[i+7+j]:
                            break
                        d_sprdconfig[config_name]['function']+='\n'+lines[i+7+j][:-1]
    add_diffconfig_to_dictconfig()
    f_sprdconfig.close()

def print_incomplete_info():
    print('All configs           : ',len(d_sprdconfig))
    print('Not completed configs : ',not_defined)
    if not_defined > 0:
        print('File {} has been updated, Please Check.'.format(file_name))

def incomplete_item():
    global not_defined
    if os.path.exists(file_name):
        os.remove(file_name)
    f_need_completed=open(file_name, 'a+')
    for key in d_sprdconfig:
        if d_sprdconfig[key]['subsys'] == "":
            not_defined += 1
            f_need_completed.write(key+'\n')
    f_need_completed.close()
    print_incomplete_info()

def configs_resort():
    list_configs=list(d_sprdconfig)
    list_configs.sort()
    os.remove(config_path)
    f=open(config_path,'a')
    for line in list_configs:
        f.write(line+'\n')
        f.write("\t[arch] {}\n".format(d_sprdconfig[line]['arch']))
        f.write("\t[plat] {}\n".format(d_sprdconfig[line]['plat']))
        f.write("\t[field] {}\n".format(d_sprdconfig[line]['field']))
        f.write("\t[subsys] {}\n".format(d_sprdconfig[line]['subsys']))
        f.write("\t[must] {}\n".format(d_sprdconfig[line]['must']))
        f.write("\t[function] {}\n".format(d_sprdconfig[line]['function']))
    f.close()


def auto_addinfo():
    print(file_name)
    f=open(file_name, 'a+')
    f.close()
    pass

#d_defconfig={'project_name':{config_name:y/n},}
def create_defconfig_dict():
    """
    create each defconfig dict for each project.
    sys.argv[0] is .py
    sys.argv[1] is check
    """
    for i in range(len(sys.argv)-2):
        d_defconfig[sys.argv[i+2]]={}
        if len(sys.argv) > 1 and sys.argv[i+2] in d_defconfig_path:
            path=d_defconfig_path[sys.argv[i+2]]['defconfig']
            f_defconfig = open(path)

            lines = f_defconfig.readlines()
            for j in range(len(lines)):
                if '=' in lines[j]:
                    config_name=lines[j].split('=')[0][7:]
                    d_defconfig[sys.argv[i+2]][config_name]='y'
                elif 'is not set' in lines[j]:
                    config_name=lines[j].split(' ')[1][7:]
                    d_defconfig[sys.argv[i+2]][config_name]='n'
            f_defconfig.close()

def configs_check():
    # Check config need modify and output to file named sprd_configs_modified.txt.
    file_name='sprd_configs_modified.txt'
    if os.path.exists(file_name):
        os.remove(file_name)
    f=open(file_name,'a')

    f.write("Need add to sprd-configs.txt\n")
    l_defproject=list(d_defconfig)
    l_defproject.sort()

    for key_defproject in l_defproject:
        f.write(key_defproject + ":\n")
        l_defconfig=list(d_defconfig[key_defproject])
        l_defconfig.sort()


        for key_defconfig in l_defconfig:
            if key_defconfig not in d_sprdconfig:
                if d_defconfig[key_defproject][key_defconfig] == 'y':
                    if flag_modify == 1:
                        d_sprdconfig[key_defconfig]={'arch':'','plat':'','field':'','subsys':'','must':'','function':''}
                    else:
                        f.write("{}: {}\n".format(key_defconfig, d_defconfig[key_defproject][key_defconfig]))

        l_diffconfig=list(d_diffconfig)
        for key_diffconfig in l_diffconfig:
            if key_diffconfig in d_sprdconfig:
                continue
            d_sprdconfig[key_diffconfig]={'arch':'','plat':'','field':'','subsys':'','must':'','function':''}

        f.write("\n")

    f.write("=====================================================\n")
    f.write("Need delete from sprd-configs.txt\n")
    file_name='allconfig_status.txt'
    if os.path.exists(file_name):
        os.remove(file_name)
    f_all=open(file_name,'a')
    l_sprdconfig=list(d_sprdconfig)
    l_sprdconfig.sort()
    for key_sprdconfig in l_sprdconfig:
        notset_num=0
        set_num=0
        str_tmp=[]

        f_all.write(key_sprdconfig+" : \t")
        for key_defproject in l_defproject:
            if key_sprdconfig not in d_defconfig[key_defproject]:
                notset_num += 1
            elif  d_defconfig[key_defproject][key_sprdconfig] == 'n':
                notset_num += 1
            else:
                set_num += 1
                f_all.write(key_defproject+",")
        f_all.write("\n")

        if key_sprdconfig in d_diffconfig:
            continue
        if notset_num == len(sys.argv)-2:
            if flag_modify == 1:
                del d_sprdconfig[key_sprdconfig]
            else:
                f.write(key_sprdconfig+'\n')

    f_all.close()
    f.close()

def output_allconfigs():
    file_name='all_sprdconfigs.txt'
    if os.path.exists(file_name):
        os.remove(file_name)
    f=open(file_name,'a')

    l_sprdconfig=list(d_sprdconfig)
    l_sprdconfig.sort()
    for key_sprdconfig in l_sprdconfig:
        f.write(key_sprdconfig+'\n')

    f.close()



def help_info():
    print(
    """usage: sprdconfig_check.py [option] [project]
    sprdconfig_check.py : Check the uncompleted configs and output to need_completed.txt
    Options:
        sort        : Resort the sprd-configs.txt
        incomplete  : Check the sprd-configs.txt incompleted config and output to need_completed.txt
        check       : Need [project]. Check the defconfig of project.
                      Found need add/del configs and output to sprd_configs_modified.txt
        modify      : Need [project]. First do as check, then merge the diffconfig to sprd-configs.
        allconfigs  : Output allconfigs of sprd-configs.txt to all_sprdconfigs.txt
        help        : Print the help information.
    """
    )
    print("Project must be one or more of {}".format(list(d_defconfig_path)))

def aiaiai_check_parameters2():
    print("========BEGIN========")
    old_path=os.getcwd()
    d_del_config={}
    d_add_config={}

    #get the kernel path
    kernel_path=old_path[:-13]

    os.system("git show HEAD -1 > lastest.diff")

    f_diff = open("lastest.diff", 'r')
    f_diff_lines=f_diff.readlines()
    for i in range(len(f_diff_lines)):
        if "diff --git" in f_diff_lines[i]:
            if "defconfig" not in f_diff_lines[i]:
                continue
            arch = f_diff_lines[i].split(" ").pop(2).split("/").pop(2)
            change_file = f_diff_lines[i].split(" ").pop(2).split("/").pop(4)
            plat = change_file.split("_").pop(1)
            i = i + 5
            while True:
                if i < len(f_diff_lines):
                    if "@@" in f_diff_lines[i]:
                        i = i+1
                        continue
                    if '+' in f_diff_lines[i]:
                        for j in range(len(f_diff_lines[i].split(" "))):
                            if "CONFIG" in f_diff_lines[i].split(" ").pop(j):
                                if "is not set" in f_diff_lines[i]:
                                    d_del_config[f_diff_lines[i].split(" ").pop(j)[7:]]={'arch':arch,'plat':plat}
                                if "=y" in f_diff_lines[i] or "=m" in f_diff_lines[i]:
                                    d_add_config[f_diff_lines[i].split(" ").pop(j)[8:-3]]={'arch':arch,'plat':plat}
                    i = i+1
                else:
                    break

    for lines in d_add_config:
        if lines not in d_sprdconfig:
            print("EMERG: NEW: Need add " + lines)
            continue

        if d_add_config[lines]['arch'] not in d_sprdconfig[lines]['arch']:
            if d_sprdconfig[lines]['arch'] == 'all':
                continue
            print("EMERG: ADD: Need add [arch] " + d_add_config[lines]['arch'])

        if d_add_config[lines]['plat'] not in d_sprdconfig[lines]['plat']:
            if d_sprdconfig[lines]['plat'] == 'all':
                continue
            print("EMERG: ADD: Need add [plat] " + d_add_config[lines]['plat'])

    for lines in list(d_del_config):
        if lines not in d_sprdconfig:
            continue

        if d_del_config[lines]['arch'] not in d_sprdconfig[lines]['arch']:
            if d_sprdconfig[lines]['arch'] == 'all':
                print("EMERG: DEL: Need del " + lines + " [arch] " + d_del_config[lines]['arch'])
        else:
            for i in range(len(d_sprdconfig[lines]['arch'].split(","))):
                if d_del_config[lines]['arch'] == d_sprdconfig[lines]['arch'].split(",").pop(i):
                    print("EMERG: DEL: Need del " + lines + " [arch] " + d_del_config[lines]['arch'])

        if d_del_config[lines]['plat'] not in d_sprdconfig[lines]['plat']:
            if d_sprdconfig[lines]['plat'] == 'all':
                print("EMERG: DEL: Need del " + lines + " [plat] " + d_del_config[lines]['plat'])
        else:
            for i in range(len(d_sprdconfig[lines]['plat'].split(","))):
                if d_del_config[lines]['plat'] == d_sprdconfig[lines]['plat'].split(",").pop(i):
                    print("EMERG: DEL: Need del " + lines + " [plat] " + d_del_config[lines]['plat'])

    print("=========END=========")

def clean():
    os.system("rm -rf *.diff")
    os.system("rm -rf *.txt")

def main():
    create_defconfig_dict()
    create_sprdconfigs_dict()

    if len(sys.argv) > 1:
        if sys.argv[1] == 'allconfigs':
            output_allconfigs()
        elif sys.argv[1] == 'sort':
            configs_resort()
            print("The sprd-configs.txt has been resorted.")
        elif sys.argv[1] == 'check':
            if len(sys.argv) == 2:
                print("PARAMETERS ERROR:")
                help_info()
                return
            else:
                for i in range(len(sys.argv)-2):
                    if sys.argv[i+2] not in d_defconfig_path:
                        print("PARAMETERS ERROR: {} error".format(sys.argv[i+2]))
                        help_info()
                        return
            configs_check()
        elif sys.argv[1] == 'modify':
            if len(sys.argv) == 2:
                print("PARAMETERS ERROR:")
                help_info()
                return
            else:
                for i in range(len(sys.argv)-2):
                    if sys.argv[i+2] not in d_defconfig_path:
                        print("PARAMETERS ERROR: {} error".format(sys.argv[i+2]))
                        help_info()
                        return

            global flag_modify
            flag_modify=1
            configs_check()
        elif sys.argv[1] == 'help':
            help_info()
        elif sys.argv[1] == 'aiaiai':
            if len(sys.argv) == 2:
                aiaiai_check_parameters2()
                clean()
            else:
                print("PARAMETERS ERROR:")
                print("./sprd_check-config_check.py aiaiai")
        elif sys.argv[1] == 'incomplete':
            incomplete_item()
        elif sys.argv[1] == 'resort':
            configs_resort()
        else:
            print("PARAMETERS ERROR:")
            help_info()
    elif len(sys.argv) == 1:
        configs_check()
    output_allconfigs()



if __name__ == '__main__':
    main()

