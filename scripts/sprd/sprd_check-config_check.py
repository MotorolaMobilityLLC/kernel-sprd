#!/usr/bin/python3

import os
import sys

tmp_path="./tmp_config_check/"
d_sprdconfig={}
d_defconfig={}
d_diffconfig={}
not_defined=0
config_path="Documentation/sprd-configs.txt"
list_configs=[]
flag_modify=0
tool_name=sys.argv[0][2:-3]
kernel_version=''

all_arch=[]
all_plat=[]

d_defconfig_path={
        'kernel4.4':{
            'pike2':{'defconfig':'arch/arm/configs/sprd_pike2_defconfig', 'diffconfig':'sprd-diffconfig/pike2', 'arch':'arm'},
            'sharkle32':{'defconfig':'arch/arm/configs/sprd_sharkle_defconfig', 'diffconfig':'sprd-diffconfig/sharkle', 'arch':'arm'},
            'sharkl3':{'defconfig':'arch/arm64/configs/sprd_sharkl3_defconfig', 'diffconfig':'sprd-diffconfig/sharkl3', 'arch':'arm64'},
            'sharkle':{'defconfig':'arch/arm64/configs/sprd_sharkle_defconfig', 'diffconfig':'sprd-diffconfig/sharkle', 'arch':'arm64'},
            'sharklefp':{'defconfig':'arch/arm/configs/sprd_sharkle_fp_defconfig', 'diffconfig':'sprd-diffconfig/sharkle', 'arch':'arm'},
        },
        'kernel4.14':{
            'sharkl3':{'defconfig':'arch/arm64/configs/sprd_all_defconfig', 'diffconfig':'sprd-diffconfig/sharkl3','arch':'arm64'},
            'roc1':{'defconfig':'arch/arm64/configs/sprd_roc1_defconfig', 'diffconfig':'sprd-diffconfig/roc1','arch':'arm64'},
            'sharkl5':{'defconfig':'arch/arm64/configs/sprd_sharkl5_defconfig', 'diffconfig':'sprd-diffconfig/sharkl5','arch':'arm64'},
        },
}

def add_diffconfig_to_dictconfig():
    result=[]

    if len(sys.argv) >= 3:
        for i in range(len(sys.argv)-2):
            if len(sys.argv) > 1 and sys.argv[i+2] in d_defconfig_path[kernel_version]:
                for maindir,subdir,file_name_list in os.walk(d_defconfig_path[kernel_version][sys.argv[i+2]]['diffconfig']):
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
                            if 'ADD:' in lines[j] or 'MOD:' in lines[j]:
                                d_diffconfig[lines[j][11:-1]]={'arch':'','plat':'','field':'','subsys':'','must':'','function':''}
                        f.close
    else:
        for key in d_defconfig_path[kernel_version]:
            for maindir,subdir,file_name_list in os.walk(d_defconfig_path[kernel_version][key]['diffconfig']):
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
                        if 'ADD:' in lines[j] or 'MOD:' in lines[j]:
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

def print_incomplete_info(file_name):
    print('All configs           : ',len(d_sprdconfig))
    print('Not completed configs : ',not_defined)
    if not_defined > 0:
        print('File {} has been updated, Please Check.'.format(file_name))

def incomplete_item():
    global not_defined
    file_name=tmp_path+'need_completed.txt'
    if os.path.exists(file_name):
        os.remove(file_name)
    f_need_completed=open(file_name, 'a+')
    tmp_list = list(d_sprdconfig)
    tmp_list.sort()
    for key in tmp_list:
        if d_sprdconfig[key]['subsys'] == "":
            not_defined += 1
            f_need_completed.write(key+'\n')
    f_need_completed.close()
    print_incomplete_info(file_name)

def configs_resort():
    list_configs=list(d_sprdconfig)
    list_configs.sort()
    os.remove(config_path)
    f=open(config_path,'a')
    for line in list_configs:
        #TODO if plat = '', It's useless.
        if d_sprdconfig[line]['plat'] == '':
            continue
        f.write(line+'\n')
        f.write("\t[arch] {}\n".format(d_sprdconfig[line]['arch']))
        f.write("\t[plat] {}\n".format(d_sprdconfig[line]['plat']))
        f.write("\t[field] {}\n".format(d_sprdconfig[line]['field']))
        f.write("\t[subsys] {}\n".format(d_sprdconfig[line]['subsys']))
        f.write("\t[must] {}\n".format(d_sprdconfig[line]['must']))
        f.write("\t[function] {}\n".format(d_sprdconfig[line]['function']))
    f.close()

#d_defconfig={'project_name':{config_name:y/n},}
def create_defconfig_dict():
    """
    create each defconfig dict for each project.
    sys.argv[0] is .py
    sys.argv[1] is check
    """
    if len(sys.argv) >= 3:
        for i in range(len(sys.argv)-2):
            d_defconfig[sys.argv[i+2]]={}
            if len(sys.argv) > 1 and sys.argv[i+2] in d_defconfig_path[kernel_version]:
                path=d_defconfig_path[kernel_version][sys.argv[i+2]]['defconfig']
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
    else:
        for key in d_defconfig_path[kernel_version]:
            d_defconfig[key]={}
            path=d_defconfig_path[kernel_version][key]['defconfig']
            f_defconfig = open(path)
            lines = f_defconfig.readlines()

            for j in range(len(lines)):
                if '=' in lines[j]:
                    config_name=lines[j].split('=')[0][7:]
                    d_defconfig[key][config_name]='y'
                elif 'is not set' in lines[j]:
                    config_name=lines[j].split(' ')[1][7:]
                    d_defconfig[key][config_name]='n'
            f_defconfig.close()

def configs_check():
    # Check config need modify and output to file named sprd_configs_modified.txt.
    file_name=tmp_path+'sprd_configs_modified.txt'
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
    file_name=tmp_path+'allconfig_status.txt'
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
    file_name=tmp_path + 'all_sprdconfigs.txt'
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
    print("Project must be one or more of {}".format(list(d_defconfig_path[kernel_version])))

def aiaiai_check():
    print("========BEGIN========")
    d_del_config={}
    d_add_config={}

    file_name=tmp_path+"lastest.diff"
    os.system("git show HEAD -1 > " + file_name)

    f_diff = open(file_name, 'r')
    f_diff_lines=f_diff.readlines()
    for i in range(len(f_diff_lines)):
        if "diff --git" in f_diff_lines[i]:
            if "defconfig" not in f_diff_lines[i]:
                continue
            arch = f_diff_lines[i].split(" ").pop(2).split("/").pop(2)
            change_file = f_diff_lines[i].split(" ").pop(2).split("/").pop(4)
            plat = change_file.split("_").pop(1)

            if plat not in d_defconfig_path[kernel_version]:
                continue

            if plat == "sharkle" and arch == "arm":
                if change_file == "sprd_sharkle_fp_defconfig":
                    plat="sharklefp"
                else:
                    plat="sharkle32"
            if plat == "sharkl3" and arch =="arm":
                continue

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
                                    if f_diff_lines[i].split(" ").pop(j)[7:] not in d_diffconfig:
                                        d_del_config[f_diff_lines[i].split(" ").pop(j)[7:]]={'arch':arch,'plat':plat}
                                if "=y" in f_diff_lines[i] or "=m" in f_diff_lines[i]:
                                    d_add_config[f_diff_lines[i].split(" ").pop(j)[8:-3]]={'arch':arch,'plat':plat}
                    i = i+1
                else:
                    break

    for lines in d_add_config:
        if lines not in d_sprdconfig:
            print("ERROR: NEW: Need create new item to Documentation/sprd-configs.txt. " + lines)
            continue

        if d_add_config[lines]['arch'] not in d_sprdconfig[lines]['arch']:
            if d_sprdconfig[lines]['arch'] == 'all':
                continue
            print("ERROR: ADD: Need add " + lines + " to Documentation/sprd-configs.txt. Should add: [arch] "\
                    + d_add_config[lines]['arch'] + "\t Current status: [arch] " + d_sprdconfig[lines]['arch'])

        if d_add_config[lines]['plat'] not in d_sprdconfig[lines]['plat']:
            if d_sprdconfig[lines]['plat'] == 'all':
                continue
            print("ERROR: ADD: Need add " + lines + " to Documentation/sprd-configs.txt. Should add: [plat] "\
                    + d_add_config[lines]['plat'] + "\t Current status: [plat] " + d_sprdconfig[lines]['plat'])

    for lines in list(d_del_config):
        if lines not in d_sprdconfig:
            continue

        if d_sprdconfig[lines]['plat'] == d_del_config[lines]['plat']:
            print("ERROR: DEL: Need del " + lines + " from Documentation/sprd-configs.txt.")
            break

        if d_sprdconfig[lines]['plat'] != 'all':
            all_plat_num = 0
            unexisted_plat_num = 0
            for key in d_defconfig_path[kernel_version]:
                if d_defconfig_path[kernel_version][key]['arch'] == d_del_config[lines]['arch']:
                    all_plat_num = all_plat_num + 1

                    if key not in d_sprdconfig[lines]['plat'].split(","):
                        unexisted_plat_num = unexisted_plat_num + 1
                        continue

            if d_del_config[lines]['plat'] in d_sprdconfig[lines]['plat'].split(","):
                unexisted_plat_num = unexisted_plat_num + 1

            if unexisted_plat_num == all_plat_num:
                print("ERROR: DEL: Need del " + lines + " from Documentation/sprd-configs.txt. Should delete: [arch] "\
                        + d_del_config[lines]['arch'] + "\t Current status: [arch] " + d_sprdconfig[lines]['arch'])

        if d_sprdconfig[lines]['plat'] == 'all' or d_del_config[lines]['plat'] in d_sprdconfig[lines]['plat'].split(","):
            print("ERROR: DEL: Need del " + lines + " from Documentation/sprd-configs.txt. Should delete: [plat] "\
                    + d_del_config[lines]['plat'] + "\t Current status: [plat] " + d_sprdconfig[lines]['plat'])

    print("=========END=========")

def clean():
    os.system("rm -rf " + tmp_path)

def update_sprd_configs():

    print("Current kernel information\n[arch]:{}\n[plat]:{}".format(all_arch,all_plat))

    configs_resort()
    l_defconfig = list(d_defconfig).sort()
    for key in d_sprdconfig:
        tmp_arch=''
        tmp_plat=''
        l_defproject=list(d_defconfig)
        l_defproject.sort()
        for project in l_defproject:
            if key in d_defconfig[project]:

                if d_defconfig[project][key] == 'y' or d_defconfig[project][key] == 'm':
                    tmp_plat = tmp_plat + project + ','
                else:
                    continue

                if d_defconfig_path[kernel_version][project]['arch'] not in tmp_arch.split(','):
                    tmp_arch = tmp_arch + d_defconfig_path[kernel_version][project]['arch'] + ','

        #TODO Doesn't check diffconfig
        if key in d_diffconfig:
            continue

        #write current status to dict d_sprdconfig
        if len(tmp_arch[:-1].split(",")) == len(all_arch):
            d_sprdconfig[key]['arch'] = 'all'
        else:
            d_sprdconfig[key]['arch'] = tmp_arch[:-1]

        if len(tmp_plat[:-1].split(",")) == len(all_plat):
            d_sprdconfig[key]['plat'] = 'all'
        else:
            d_sprdconfig[key]['plat'] = tmp_plat[:-1]

    # regenerate sprd-configs.txt with dict d_sprdconfig
    configs_resort()

def prepare_information():
    f = open("Makefile", 'r')
    lines = f.readlines()
    for j in range(3):
        if 'VERSION' in lines[j]:
            version=lines[j].split(" ").pop(2)
        if 'PATCHLEVEL' in lines[j]:
            patchlevel=lines[j].split(" ").pop(2)
    f.close
    global kernel_version
    kernel_version = 'kernel' + version[:-1] + '.' + patchlevel[:-1]

    for key in d_defconfig_path[kernel_version]:
        all_plat.append(key)

        if d_defconfig_path[kernel_version][key]['arch'] not in all_arch:
            all_arch.append(d_defconfig_path[kernel_version][key]['arch'])

def main():
    folder = os.path.exists(tmp_path)
    if not folder:
        os.makedirs(tmp_path)
    prepare_information()
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
                    if sys.argv[i+2] not in d_defconfig_path[kernel_version]:
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
                    if sys.argv[i+2] not in d_defconfig_path[kernel_version]:
                        print("PARAMETERS ERROR: {} error".format(sys.argv[i+2]))
                        help_info()
                        return

            global flag_modify
            flag_modify=1
            configs_check()
            configs_resort()
        elif sys.argv[1] == 'help':
            help_info()
        elif sys.argv[1] == 'aiaiai':
            if len(sys.argv) == 2:
                aiaiai_check()
                clean()
            else:
                print("PARAMETERS ERROR:")
                print("./script/sprd/sprd_check-config_check.py aiaiai")
        elif sys.argv[1] == 'incomplete':
            incomplete_item()
        elif sys.argv[1] == 'update':
            update_sprd_configs()
        else:
            print("PARAMETERS ERROR:")
            help_info()
    elif len(sys.argv) == 1:
        configs_check()

if __name__ == '__main__':
    main()

