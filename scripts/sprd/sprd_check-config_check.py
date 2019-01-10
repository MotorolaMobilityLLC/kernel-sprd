#!/usr/bin/python3

import os
import sys
import csv

tmp_path="./tmp_config_check/"
d_sprdconfig={}
d_defconfig={}
d_diffconfig={}
d_corrected_config={}
not_defined=0
config_path="Documentation/sprd-configs.txt"
list_configs=[]
tool_name=sys.argv[0][2:-3]
kernel_version=''

all_arch=[]
all_plat=[]
l_sprdconfig=[]
l_defproject=[]
l_diffconfig=[]

d_defconfig_path={
        'kernel4.4':{
            'pike2':{'defconfig':'arch/arm/configs/sprd_pike2_defconfig', 'diffconfig':'sprd-diffconfig/pike2', 'arch':'arm'},
            'sharkle32':{'defconfig':'arch/arm/configs/sprd_sharkle_defconfig', 'diffconfig':'sprd-diffconfig/sharkle', 'arch':'arm'},
            'sharkl3':{'defconfig':'arch/arm64/configs/sprd_sharkl3_defconfig', 'diffconfig':'sprd-diffconfig/sharkl3', 'arch':'arm64'},
            'sharkle':{'defconfig':'arch/arm64/configs/sprd_sharkle_defconfig', 'diffconfig':'sprd-diffconfig/sharkle', 'arch':'arm64'},
            'sharkle32_fp':{'defconfig':'arch/arm/configs/sprd_sharkle_fp_defconfig', 'diffconfig':'sprd-diffconfig/sharkle', 'arch':'arm'},
            'sharkl3_32':{'defconfig':'arch/arm/configs/sprd_sharkl3_defconfig', 'diffconfig':'sprd-diffconfig/sharkl3', 'arch':'arm'},
        },
        'kernel4.14':{
            'roc1':{'defconfig':'arch/arm64/configs/sprd_roc1_defconfig', 'diffconfig':'sprd-diffconfig/roc1','arch':'arm64'},
            'sharkl5':{'defconfig':'arch/arm64/configs/sprd_sharkl5_defconfig', 'diffconfig':'sprd-diffconfig/sharkl5','arch':'arm64'},
            'sharkl5_32':{'defconfig':'arch/arm/configs/sprd_sharkl5_defconfig', 'diffconfig':'sprd-diffconfig/sharkl5','arch':'arm'},
        },
}

def add_diffconfig_to_dictconfig():
    result=[]

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
                        tmp_arch = apath.split("/").pop(2)
                        tmp_plat = apath.split("/").pop(1)

                        if tmp_arch == 'arm' and tmp_plat == 'sharkle':
                            file_name = apath.split("/").pop()
                            if 'mocor5' in file_name or 'kaios' in file_name:
                                tmp_plat = 'sharkle32_fp'
                            else:
                                tmp_plat = 'sharkle32'
                        elif tmp_arch == 'arm' and tmp_plat == 'sharkl3':
                            tmp_plat = 'sharkl3_32'
                        elif tmp_plat == 'pike2':
                            tmp_arch = 'arm'
                        elif tmp_arch == 'arm' and tmp_plat == 'sharkl5':
                            tmp_plat = 'sharkl5_32'
                        elif tmp_arch == 'common' and tmp_plat == 'sharkle':
                            if lines[j][4:-1] not in d_diffconfig:
                                d_diffconfig[lines[j][4:-1]]={
                                    'arch': 'arm,arm64,',
                                    'plat': 'sharkle,sharkle32,',
                                    'field':'',
                                    'subsys':'',
                                    'must':'',
                                    'function':''
                                    }
                                continue
                            if 'sharkle' not in d_diffconfig[lines[j][4:-1]]['plat'].split(','):
                                d_diffconfig[lines[j][4:-1]]['plat'] = d_diffconfig[lines[j][4:-1]]['plat'] + 'sharkle,'
                            if 'sharkle32' not in d_diffconfig[lines[j][4:-1]]['plat'].split(','):
                                d_diffconfig[lines[j][4:-1]]['plat'] = d_diffconfig[lines[j][4:-1]]['plat'] + 'sharkle32,'

                            if 'arm' not in d_diffconfig[lines[j][4:-1]]['arch'].split(','):
                                d_diffconfig[lines[j][4:-1]]['arch'] = d_diffconfig[lines[j][4:-1]]['arch'] + 'arm,'
                            if 'arm64' not in d_diffconfig[lines[j][4:-1]]['arch'].split(','):
                                d_diffconfig[lines[j][4:-1]]['arch'] = d_diffconfig[lines[j][4:-1]]['arch'] + 'arm64,'
                            continue

                        elif tmp_arch == 'common' and tmp_plat == 'sharkl3':
                            if lines[j][4:-1] not in d_diffconfig:
                                d_diffconfig[lines[j][4:-1]]={
                                    'arch': 'arm,arm64,',
                                    'plat': 'sharkl3,sharkl3_32,',
                                    'field':'',
                                    'subsys':'',
                                    'must':'',
                                    'function':''
                                    }
                                continue
                            if 'sharkl3' not in d_diffconfig[lines[j][4:-1]]['plat'].split(','):
                                d_diffconfig[lines[j][4:-1]]['plat'] = d_diffconfig[lines[j][4:-1]]['plat'] + 'sharkl3,'
                            if 'sharkl3_32' not in d_diffconfig[lines[j][4:-1]]['plat'].split(','):
                                d_diffconfig[lines[j][4:-1]]['plat'] = d_diffconfig[lines[j][4:-1]]['plat'] + 'sharkl3_32,'

                            if 'arm' not in d_diffconfig[lines[j][4:-1]]['arch'].split(','):
                                d_diffconfig[lines[j][4:-1]]['arch'] = d_diffconfig[lines[j][4:-1]]['arch'] + 'arm,'
                            if 'arm64' not in d_diffconfig[lines[j][4:-1]]['arch'].split(','):
                                d_diffconfig[lines[j][4:-1]]['arch'] = d_diffconfig[lines[j][4:-1]]['arch'] + 'arm64,'
                            continue
                        elif tmp_arch == 'common' and tmp_plat == 'sharkl5':
                            if lines[j][4:-1] not in d_diffconfig:
                                d_diffconfig[lines[j][4:-1]]={
                                    'arch': 'arm,arm64,',
                                    'plat': 'sharkl5,sharkl5_32,',
                                    'field':'',
                                    'subsys':'',
                                    'must':'',
                                    'function':''
                                    }
                                continue
                            if 'sharkl5' not in d_diffconfig[lines[j][4:-1]]['plat'].split(','):
                                d_diffconfig[lines[j][4:-1]]['plat'] = d_diffconfig[lines[j][4:-1]]['plat'] + 'sharkl5,'
                            if 'sharkl5_32' not in d_diffconfig[lines[j][4:-1]]['plat'].split(','):
                                d_diffconfig[lines[j][4:-1]]['plat'] = d_diffconfig[lines[j][4:-1]]['plat'] + 'sharkl5_32,'

                            if 'arm' not in d_diffconfig[lines[j][4:-1]]['arch'].split(','):
                                d_diffconfig[lines[j][4:-1]]['arch'] = d_diffconfig[lines[j][4:-1]]['arch'] + 'arm,'
                            if 'arm64' not in d_diffconfig[lines[j][4:-1]]['arch'].split(','):
                                d_diffconfig[lines[j][4:-1]]['arch'] = d_diffconfig[lines[j][4:-1]]['arch'] + 'arm64,'
                            continue
                        elif tmp_arch == 'common' and tmp_plat == 'roc1':
                            tmp_arch = 'arm64'

                        if lines[j][4:-1] in d_diffconfig:
                            if tmp_arch not in d_diffconfig[lines[j][4:-1]]['arch'].split(','):
                                d_diffconfig[lines[j][4:-1]]['arch'] = d_diffconfig[lines[j][4:-1]]['arch'] + tmp_arch + ','
                            if tmp_plat not in d_diffconfig[lines[j][4:-1]]['plat'].split(','):
                                d_diffconfig[lines[j][4:-1]]['plat'] = d_diffconfig[lines[j][4:-1]]['plat'] + tmp_plat + ','
                        else:
                            d_diffconfig[lines[j][4:-1]]={
                                'arch': tmp_arch + ',',
                                'plat': tmp_plat + ',',
                                'field':'',
                                'subsys':'',
                                'must':'',
                                'function':''
                                }
                f.close

def create_sprdconfigs_dict():

    global d_sprdconfig

    f_sprdconfig = open(config_path,'r')
    lines = f_sprdconfig.readlines()
    for i in range(len(lines)):
        if lines[i][:7] == "CONFIG_":
            config_name=lines[i][:-1]
            d_sprdconfig[config_name]={'arch':'','plat':'','field':'','subsys':'','must':'','function':''}
            if i+6 < len(lines):
                if "[arch]" in lines[i+1]:
                    if lines[i+1][-7:-1] == '[arch]':
                        d_sprdconfig[config_name]['arch']=''
                    else:
                        d_sprdconfig[config_name]['arch']=lines[i+1].split('[arch] ').pop()[:-1]
                if "[plat]" in lines[i+2]:
                    if lines[i+2][-7:-1] == '[plat]':
                        d_sprdconfig[config_name]['plat']=''
                    else:
                        d_sprdconfig[config_name]['plat']=lines[i+2].split('[plat] ').pop()[:-1]
                if "[field]" in lines[i+3]:
                    if lines[i+3][-8:-1] == '[field]':
                        d_sprdconfig[config_name]['field']=''
                    else:
                        d_sprdconfig[config_name]['field']=lines[i+3].split('[field] ').pop()[:-1]
                if "[subsys]" in lines[i+4]:
                    if lines[i+4][-9:-1] == '[subsys]':
                        d_sprdconfig[config_name]['subsys']=''
                    else:
                        d_sprdconfig[config_name]['subsys']=lines[i+4].split('[subsys] ').pop()[:-1]
                if "[must]" in lines[i+5]:
                    if lines[i+5][-7:-1] == '[must]':
                        d_sprdconfig[config_name]['must']=''
                    else:
                        d_sprdconfig[config_name]['must']=lines[i+5].split('[must] ').pop()[:-1]
                if "[function]" in lines[i+6]:
                    if lines[i+6][-11:-1] == '[function]':
                        d_sprdconfig[config_name]['function']=''
                    else:
                        d_sprdconfig[config_name]['function']=lines[i+6].split('[function] ').pop()[:-1]

                    for j in range(15):
                        if i+7+j >= len(lines):
                            break
                        if lines[i+7+j][:7] == "CONFIG_":
                            break
                        d_sprdconfig[config_name]['function']+='\n'+lines[i+7+j][:-1]
    add_diffconfig_to_dictconfig()
    f_sprdconfig.close()

    # if defconfig has new config, add it into d_sprdconfig
    for key_defplat in d_defconfig:
        for key_defconfig in d_defconfig[key_defplat]:
            if d_defconfig[key_defplat][key_defconfig] == 'y' and key_defconfig not in d_sprdconfig:
                d_sprdconfig[key_defconfig]={'arch':'','plat':'','field':'','subsys':'','must':'','function':''}

    # create the d_sprdconfig by Documentation/sprdconfigs.txt
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
        # if plat = '', It should be deleted.
        if d_sprdconfig[line]['plat'] == '':
            continue
        f.write(line+'\n')
        if d_sprdconfig[line]['arch'] == '':
            f.write("\t[arch]\n")
        else:
            f.write("\t[arch] {}\n".format(d_sprdconfig[line]['arch']))

        if d_sprdconfig[line]['plat'] == '':
            f.write("\t[plat]\n")
        else:
            f.write("\t[plat] {}\n".format(d_sprdconfig[line]['plat']))

        if d_sprdconfig[line]['field'] == '':
            f.write("\t[field]\n")
        else:
            f.write("\t[field] {}\n".format(d_sprdconfig[line]['field']))

        if d_sprdconfig[line]['subsys'] == '':
            f.write("\t[subsys]\n")
        else:
            f.write("\t[subsys] {}\n".format(d_sprdconfig[line]['subsys']))

        if d_sprdconfig[line]['must'] == '':
            f.write("\t[must]\n")
        else:
            f.write("\t[must] {}\n".format(d_sprdconfig[line]['must']))

        if d_sprdconfig[line]['function'] == '':
            f.write("\t[function]\n")
        else:
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
                        config_name=lines[j].split('=')[0]
                        d_defconfig[sys.argv[i+2]][config_name]='y'
                    elif 'is not set' in lines[j]:
                        config_name=lines[j].split(' ')[1]
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
                    config_name=lines[j].split('=')[0]
                    d_defconfig[key][config_name]='y'
                elif 'is not set' in lines[j]:
                    config_name=lines[j].split(' ')[1]
                    d_defconfig[key][config_name]='n'
            f_defconfig.close()

def configs_check():
    for key_defproject in l_defproject:
        l_defconfig=list(d_defconfig[key_defproject])
        l_defconfig.sort()

        for key_defconfig in l_defconfig:
            if key_defconfig not in d_sprdconfig:
                if d_defconfig[key_defproject][key_defconfig] == 'y':
                    d_sprdconfig[key_defconfig]={'arch':'','plat':'','field':'','subsys':'','must':'','function':''}


    for key_diffconfig in l_diffconfig:
        if key_diffconfig in d_sprdconfig:
            continue
        d_sprdconfig[key_diffconfig]={'arch':'','plat':'','field':'','subsys':'','must':'','function':''}


    #print allconfigs and status defined in Documentation/sprd-configs.txt
    file_name=tmp_path+'allconfig_status.txt'
    if os.path.exists(file_name):
        os.remove(file_name)
    f_all=open(file_name,'a')
    for key_sprdconfig in l_sprdconfig:
        tmp_str=''

        f_all.write(key_sprdconfig+" : \t")
        for key_defproject in l_defproject:
            if key_sprdconfig in d_defconfig[key_defproject] and d_defconfig[key_defproject][key_sprdconfig] != 'n':
                tmp_str = tmp_str + key_defproject+","

        f_all.write(tmp_str + "\n")

        if key_sprdconfig in d_diffconfig:
            continue

        if tmp_str == '':
            del d_sprdconfig[key_sprdconfig]

    f_all.close()


#print all config defined in Documentation/sprd-configs.txt
def output_allconfigs():
    file_name=tmp_path + 'all_sprdconfigs.txt'
    if os.path.exists(file_name):
        os.remove(file_name)
    f=open(file_name,'a')

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

def sprdconfigs_check():
    ret = 0
    for key in l_sprdconfig:
        for i in range(len(d_sprdconfig[key]['arch'].split(','))):
            if d_sprdconfig[key]['arch'].split(',').pop(i) != 'all' and\
                d_sprdconfig[key]['arch'].split(',').pop(i) not in all_arch:
                print("ERROR: arch: The arch must be one of " + str(all_arch)[:-1] + ", 'all']. Need modify " +\
                    key + " [arch] : " + d_sprdconfig[key]['arch'].split(',').pop(i))
                ret = -1

        for i in range(len(d_sprdconfig[key]['plat'].split(','))):
            if d_sprdconfig[key]['plat'].split(',').pop(i) != 'all' and\
                d_sprdconfig[key]['plat'].split(',').pop(i) not in all_plat:
                print("ERROR: plat: The plat must be one of " + str(all_plat)[:-1] + ", 'all']. Need modify " +\
                    key + " [plat] : " + d_sprdconfig[key]['plat'].split(',').pop(i))
                ret = -1
    return ret

def create_corrected_dict():
    global d_corrected_config

    for key in l_sprdconfig:
        tmp_arch=''
        tmp_plat=''
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
            if d_diffconfig[key]['arch'] not in tmp_arch:
                tmp_arch = tmp_arch + d_diffconfig[key]['arch'] + ","

            if d_diffconfig[key]['plat'] not in tmp_plat:
                tmp_plat = tmp_plat + d_diffconfig[key]['plat'] + ","

            tmp_arch_sort=''
            for i in range(len(tmp_arch[:-1].split(","))):
                if tmp_arch[:-1].split(",").pop(i) not in tmp_arch_sort.split(","):
                    tmp_arch_sort = tmp_arch_sort + tmp_arch[:-1].split(",").pop(i) + ','

            tmp_plat_sort=''
            l_tmp_plat = tmp_plat[:-1].split(",")
            l_tmp_plat.sort()
            for i in range(len(tmp_plat[:-1].split(","))):
                if tmp_plat[:-1].split(",").pop(i) not in tmp_plat_sort.split(","):
                    tmp_plat_sort = tmp_plat_sort + tmp_plat[:-1].split(",").pop(i) + ','

            tmp_arch = tmp_arch_sort
            tmp_plat = tmp_plat_sort

            tmp_plat_sort=''
            l_tmp_plat = tmp_plat[:-1].split(",")
            l_tmp_plat.sort()
            for i in range(len(l_tmp_plat)):
                if l_tmp_plat[i] not in tmp_plat_sort.split(","):
                    tmp_plat_sort = tmp_plat_sort + l_tmp_plat[i] + ','

            tmp_arch = tmp_arch_sort
            tmp_plat = tmp_plat_sort

            # if tmp_plat == '' means this config don't need any more.
            if tmp_plat == '':
                continue

        d_corrected_config[key]={ 'arch':'','plat':''}
        #write current status to dict d_sprdconfig
        if len(tmp_arch[:-1].split(",")) == len(all_arch):
            d_corrected_config[key]['arch'] = 'all'
        else:
            d_corrected_config[key]['arch'] = tmp_arch[:-1]

        if len(tmp_plat[:-1].split(",")) == len(all_plat):
            d_corrected_config[key]['plat'] = 'all'
        else:
            d_corrected_config[key]['plat'] = tmp_plat[:-1]

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
            if "sprd-diffconfig" in f_diff_lines[i]:
                tmp_arch = f_diff_lines[i].split(" ").pop(2).split("/").pop(3)
                tmp_plat = f_diff_lines[i].split(" ").pop(2).split("/").pop(2)
                change_file = f_diff_lines[i].split(" ").pop(2).split("/").pop(4)

                while True:
                    i = i+1
                    if i < len(f_diff_lines):
                        if "diff --git" in f_diff_lines[i]:
                            break
                        if "@@" in f_diff_lines[i]:
                            continue
                        if '+' in f_diff_lines[i]:
                            if "CONFIG" in f_diff_lines[i].split(":").pop():
                                if "+ADD:" in f_diff_lines[i]:
                                    if tmp_arch == 'arm' and tmp_plat == 'sharkle':
                                        if 'mocor5' in change_file or 'kaios' in change_file:
                                            tmp_plat = 'sharkle32_fp'
                                        else:
                                            tmp_plat = 'sharkle32'
                                    elif tmp_arch == 'arm' and tmp_plat == 'sharkl3':
                                        tmp_plat = 'sharkl3_32'
                                    elif tmp_plat == 'pike2':
                                        tmp_arch = 'arm'
                                    elif tmp_arch == 'common' and tmp_plat == 'sharkle':
                                        tmp_arch='arm,arm64'
                                        tmp_plat='sharkle,sharkle32'
                                    elif tmp_arch == 'common' and tmp_plat == 'sharkl3':
                                        tmp_arch='arm,arm64'
                                        tmp_plat='sharkl3,sharkl3_32'
                                    elif tmp_arch == 'arm' and tmp_plat == 'sharkl5':
                                        tmp_plat = 'sharkl5_32'
                                    elif tmp_arch == 'common' and tmp_plat == 'sharkl5':
                                        tmp_arch='arm,arm64'
                                        tmp_plat='sharkl5,sharkl5_32'
                                    d_add_config[f_diff_lines[i].split(":").pop()[:-1]] = {'arch':tmp_arch,'plat':tmp_plat}
                    else:
                        break

            elif "sprd-configs.txt" in f_diff_lines[i]:
                l_corrected_config = list(d_corrected_config)
                l_corrected_config.sort()
                for key in l_corrected_config:
                    if key in d_sprdconfig:
                        if d_corrected_config[key]['arch'] != d_sprdconfig[key]['arch']:
                            print("ERROR: doc: Value is different between code and sprd-configs.txt. " + \
                                    " CONFIG:" + key + \
                                    " CODE:[arch]:" + d_corrected_config[key]['arch'] + \
                                    " DOC:[arch]:" + d_sprdconfig[key]['arch'])
                        elif d_corrected_config[key]['plat'] != d_sprdconfig[key]['plat']:
                            print("ERROR: doc: Value is different between code and sprd-configs.txt." + \
                                    " CONFIG:" + key + \
                                    " CODE:[plat]:" + d_corrected_config[key]['plat'] + \
                                    " DOC:[plat]:" + d_sprdconfig[key]['plat'])

            elif "defconfig" in f_diff_lines[i]:
                arch = f_diff_lines[i].split(" ").pop(2).split("/").pop(2)
                change_file = f_diff_lines[i].split(" ").pop(2).split("/").pop(4)
                plat = change_file.split("_").pop(1)

                if plat not in d_defconfig_path[kernel_version]:
                    continue

                if plat == "sharkle" and arch == "arm":
                    if change_file == "sprd_sharkle_fp_defconfig":
                        plat="sharkle32_fp"
                    else:
                        plat="sharkle32"
                if plat == "sharkl3" and arch =="arm":
                    plat="sharkl3_32"
                if plat == "sharkl5" and arch =="arm":
                    plat="sharkl5_32"

                while True:
                    i = i+1
                    if i < len(f_diff_lines):
                        if "diff --git" in f_diff_lines[i]:
                            break
                        if "@@" in f_diff_lines[i]:
                            continue
                        if '+' in f_diff_lines[i]:
                            for j in range(len(f_diff_lines[i].split(" "))):
                                if "CONFIG" in f_diff_lines[i].split(" ").pop(j):
                                    if "is not set" in f_diff_lines[i]:
                                        if f_diff_lines[i].split(" ").pop(j) not in d_diffconfig:
                                            d_del_config[f_diff_lines[i].split(" ").pop(j)]={'arch':arch,'plat':plat}
                                    elif "=y" in f_diff_lines[i] or "=m" in f_diff_lines[i]:
                                        d_add_config[f_diff_lines[i].split(" ").pop(j)[1:-3]]={'arch':arch,'plat':plat}
                    else:
                        break

    for lines in d_add_config:
        if lines not in d_sprdconfig:
            print("ERROR: new: Need create new item to Documentation/sprd-configs.txt. " + lines)
            continue

        for j in range(len(d_add_config[lines]['arch'].split(','))):
            if d_add_config[lines]['arch'].split(',').pop(j) not in d_sprdconfig[lines]['arch']:
                if d_sprdconfig[lines]['arch'] == 'all':
                    continue
                print("ERROR: add: Need add " + lines + " to Documentation/sprd-configs.txt. Should add: [arch] "\
                        + d_add_config[lines]['arch'].split(',').pop(j) + "\t Current status: [arch] " + d_sprdconfig[lines]['arch'])

        for j in range(len(d_add_config[lines]['plat'].split(','))):
            if d_add_config[lines]['plat'].split(',').pop(j) not in d_sprdconfig[lines]['plat']:
                if d_sprdconfig[lines]['plat'] == 'all':
                    continue
                print("ERROR: add: Need add " + lines + " to Documentation/sprd-configs.txt. Should add: [plat] "\
                        + d_add_config[lines]['plat'].split(',').pop(j) + "\t Current status: [plat] " + d_sprdconfig[lines]['plat'])

    for lines in list(d_del_config):
        if lines not in d_sprdconfig:
            continue

        if d_sprdconfig[lines]['plat'] == d_del_config[lines]['plat']:
            print("ERROR: del: Need del " + lines + " from Documentation/sprd-configs.txt.")
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
                print("ERROR: del: Need del " + lines + " from Documentation/sprd-configs.txt. Should delete: [arch] "\
                        + d_del_config[lines]['arch'] + "\t Current status: [arch] " + d_sprdconfig[lines]['arch'])

        if d_sprdconfig[lines]['plat'] == 'all' or d_del_config[lines]['plat'] in d_sprdconfig[lines]['plat'].split(","):
            print("ERROR: del: Need del " + lines + " from Documentation/sprd-configs.txt. Should delete: [plat] "\
                    + d_del_config[lines]['plat'] + "\t Current status: [plat] " + d_sprdconfig[lines]['plat'])

    print("=========END=========")

def clean():
    os.system("rm -rf " + tmp_path)

def print_support_arch_plat():
    print("Current kernel information\n[arch]:{}\n[plat]:{}".format(all_arch,all_plat))

def update_sprd_configs():

    print_support_arch_plat()

    configs_resort()

    for key in l_sprdconfig:
        tmp_arch=''
        tmp_plat=''
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
            if d_diffconfig[key]['arch'] not in tmp_arch:
                tmp_arch = tmp_arch + d_diffconfig[key]['arch'] + ","

            if d_diffconfig[key]['plat'] not in tmp_plat:
                tmp_plat = tmp_plat + d_diffconfig[key]['plat'] + ","

            tmp_arch_sort=''
            for i in range(len(tmp_arch[:-1].split(","))):
                if tmp_arch[:-1].split(",").pop(i) not in tmp_arch_sort.split(","):
                    tmp_arch_sort = tmp_arch_sort + tmp_arch[:-1].split(",").pop(i) + ','

            tmp_plat_sort=''
            l_tmp_plat = tmp_plat[:-1].split(",")
            l_tmp_plat.sort()
            for i in range(len(l_tmp_plat)):
                if l_tmp_plat[i] not in tmp_plat_sort.split(","):
                    tmp_plat_sort = tmp_plat_sort + l_tmp_plat[i] + ','

            tmp_arch = tmp_arch_sort
            tmp_plat = tmp_plat_sort

        # if tmp_plat == '' means this config don't need any more.
        if tmp_plat == '':
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

def scan():
    l_corrected_config = list(d_corrected_config)
    l_corrected_config.sort()

    str_all_arm_plat=''
    str_all_arm64_plat=''

    for key_project in l_defproject:
        if d_defconfig_path[kernel_version][key_project]['arch'] == 'arm':
            str_all_arm_plat = str_all_arm_plat + key_project + ','
        elif d_defconfig_path[kernel_version][key_project]['arch'] == 'arm64':
            str_all_arm64_plat = str_all_arm64_plat + key_project + ','

    str_all_arm_plat = str_all_arm_plat[:-1]
    str_all_arm64_plat = str_all_arm64_plat[:-1]

    csv_filename = "config_plat_scan.csv"
    if os.path.exists(csv_filename):
        os.remove(csv_filename)
    csv_fd = open(csv_filename, 'a+')
    csv_writer=csv.writer(csv_fd)
    csv_writer.writerow(["Config name", "Enalbe arch", "Current enable plats", "ARM lack plat", "ARM64 lack plat"])
    for key_config in l_corrected_config:
        for key_arch in range(len(d_corrected_config[key_config]['arch'].split(","))):
            str_lack_arm=""
            str_lack_arm64=""
            l_write=[]
            if d_corrected_config[key_config]['arch'] == 'all':
                if d_corrected_config[key_config]['plat'] == 'all':
                    continue
                for i in range(len(str_all_arm_plat.split(","))):
                    if str_all_arm_plat.split(",").pop(i) not in d_corrected_config[key_config]['plat'].split(","):
                        str_lack_arm = str_lack_arm + str_all_arm_plat.split(",").pop(i) + ","
                        continue
                for i in range(len(str_all_arm64_plat.split(","))):
                    if str_all_arm64_plat.split(",").pop(i) not in d_corrected_config[key_config]['plat'].split(","):
                        str_lack_arm64 = str_lack_arm64 + str_all_arm64_plat.split(",").pop(i) + ","
                        continue
                if str_lack_arm != "" and str_lack_arm64 != "":
                    l_write = [key_config, d_corrected_config[key_config]['arch'], d_corrected_config[key_config]['plat'], str_lack_arm[:-1], str_lack_arm64[:-1]]
                    csv_writer.writerow(l_write)
            elif d_corrected_config[key_config]['arch'] == 'arm':
                for i in range(len(str_all_arm_plat.split(","))):
                    if str_all_arm_plat.split(",").pop(i) not in d_corrected_config[key_config]['plat'].split(","):
                        str_lack_arm = str_lack_arm + str_all_arm_plat.split(",").pop(i) + ","
                        continue
                if str_lack_arm != "":
                    l_write = [key_config, d_corrected_config[key_config]['arch'], d_corrected_config[key_config]['plat'], str_lack_arm[:-1], ""]
                    csv_writer.writerow(l_write)
            elif d_corrected_config[key_config]['arch'] == 'arm64':
                for i in range(len(str_all_arm64_plat.split(","))):
                    if str_all_arm64_plat.split(",").pop(i) not in d_corrected_config[key_config]['plat'].split(","):
                        str_lack_arm64 = str_lack_arm64 + str_all_arm64_plat.split(",").pop(i) + ","
                        continue
                if str_lack_arm64 != "":
                    l_write = [key_config, d_corrected_config[key_config]['arch'], d_corrected_config[key_config]['plat'], "", str_lack_arm64[:-1]]
                    csv_writer.writerow(l_write)
    csv_fd.close()

def prepare_info_first():
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

    l_defconfig_path = list(d_defconfig_path[kernel_version])
    l_defconfig_path.sort()
    for key in l_defconfig_path:
        all_plat.append(key)

        if d_defconfig_path[kernel_version][key]['arch'] not in all_arch:
            all_arch.append(d_defconfig_path[kernel_version][key]['arch'])

def print_script_info():
    print(
    """
    If sprd_check-config_check error, please using script to update Documentation/sprd-configs.txt.
    The step as following:
    1) Enter kernel root path. (kernel4.4 / kernel4.14)
    2) Using script to update [arch] & [plat] automatically which in Documentation/sprd-configs.txt.
        Command: ./scripts/sprd/sprd_check-config_check.py update
    3) Check the Documentation/sprd-configs.txt. Please don't modify the [arch] & [plat].
       The others such as [field] [subsys] [must] [sound] need to fill in by owner.
       The sprd_configs.txt introduction refer to Documentation/sprd_configs_introduction.txt
    """)

def prepare_info_second():
    global l_sprdconfig
    l_sprdconfig=list(d_sprdconfig)
    l_sprdconfig.sort()

    global l_defproject
    l_defproject=list(d_defconfig)
    l_defproject.sort()

    global l_diffconfig
    l_diffconfig=list(d_diffconfig)
    l_diffconfig.sort()

def main():
    folder = os.path.exists(tmp_path)
    if not folder:
        os.makedirs(tmp_path)
    prepare_info_first()
    create_defconfig_dict()
    create_sprdconfigs_dict()
    prepare_info_second()
    create_corrected_dict()


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

            configs_check()
            configs_resort()
        elif sys.argv[1] == 'help':
            help_info()
        elif sys.argv[1] == 'aiaiai':
            print_script_info()
            if len(sys.argv) == 2:
                sprdconfigs_check()

                aiaiai_check()
                clean()
            else:
                print("PARAMETERS ERROR:")
                print("./script/sprd/sprd_check-config_check.py aiaiai")
        elif sys.argv[1] == 'incomplete':
            incomplete_item()
        elif sys.argv[1] == 'update':
            configs_check()
            update_sprd_configs()
        elif sys.argv[1] == 'support':
            print_support_arch_plat()
        elif sys.argv[1] == 'scan':
            scan()
        else:
            print("PARAMETERS ERROR:")
            help_info()
    elif len(sys.argv) == 1:
        configs_check()

if __name__ == '__main__':
    main()

