#!/bin/bash

:<<!
本地运行gki检查脚本方法：

1.同步脚本工具库
git clone ssh://gitadmin@gitmirror.unisoc.com/aiaiai/abigail

如本地新建test目录存放脚本工具库。
执行指令如下：
$ mkdir test
$ cd test
$ git clone ssh://gitadmin@gitmirror.unisoc.com/aiaiai/abigail


2.修改代码,打开相关的config项,提交commit
修改Documentation/sprd-gki-diff-config文件打开相应的config项。
config修改规则如下：
ADD:CONFIG_ARCH_SPRD            ADD---增加CONFIG_ARCH_SPRD为y
MOD:CONFIG_MFD_SC27XX_PMIC      MOD---增加CONFIG_MFD_SC27XX_PMIC为m
DEL:CONFIG_MFD_SC27XX_PMIC      DEL---删除CONFIG_MFD_SC27XX_PMIC项
VAL:CONFIG_LOG_BUF_SHIFT=18     VAL---设置CONFIG_LOG_BUF_SHIFT为18
STR:CONFIG_CMDLINE=root=/dev    STR---设置CONFIG_CMDLINE为字符串root=/dev

提交本地commit，不需要upload到远程仓库，本地提交用于做修改c文件编译检查。

3.运行sprd_check_gki.sh
sprd_check_gki.sh需要脚本工具绝对路径,[--cktoolpath|-p|-P] <abigail的绝对路径>

执行指令如下：
$ cd kernel5.4
$ ./scripts/sprd/sprd_check_gki.sh -p /home/liangcai.fan/code/test/abigail

4.查看运行结果。
如结果打印如下，说明gki检查通过。
否则gki检查失败查看:out_abi/sprdlinux5.4/dist/abi.report文件，
whitelist检查失败查看:out_abi/sprdlinux5.4/dist/diff_whitelist.report文件.

log信息如下：
++++++++++++++++++++++++check end+++++++++++++++++++++++

abi.report size: 250
check gki --------------------------------------------- PASS
check whitelist --------------------------------------- PASS

check compile ----------------------------------------- PASS

++++++++++++++++++++++check gki end+++++++++++++++++++++
!

echo "+++++++++++++++++++++check gki start++++++++++++++++++++"

export MAIN_SCRIPT_DIR=$(readlink -f $(dirname $0)/../..)

export KERNEL_CODE_DIR=$(dirname $MAIN_SCRIPT_DIR)
export KERNEL_DIR=${MAIN_SCRIPT_DIR##*/}

echo "= KERNEL_CODE_DIR: $KERNEL_CODE_DIR"
echo "= KERNEL_DIR: $KERNEL_DIR"

GKI_DIFF_CONFIG=gki_diff_config.diff
ABIGIAL_PATH_FLAG=0
COMPILE_CHECK_PASS_FLAG=0
RET_VAL=0

if [ $# -ge 2  ]; then
  for arg in $*
  do
    if [ $ABIGIAL_PATH_FLAG == 1 ]; then
      SCRIPTS_DIR=$arg
      echo "= arg SCRIPTS_DIR:$SCRIPTS_DIR"
      break
    fi
    if [ $arg == "--cktoolpath" -o $arg == "-p" -o $arg == "-P" ]; then
      ABIGIAL_PATH_FLAG=1
    fi
  done
  if [ ! $SCRIPTS_DIR ]; then
    SCRIPTS_DIR=$2
    echo "= default SCRIPTS_DIR:$SCRIPTS_DIR"
  fi
else
  RET_VAL=1
  echo "ERROR: Please provide the absolute path of the abigail tool"
  echo "eg: ./scripts/sprd/sprd_check_gki.sh -p <absolute path of the abigail tool>"
  exit 1
fi

function do_gki_ckeck() {
  cd ${SCRIPTS_DIR}

  echo "========================================================"
  echo "Run check gki script at: ${SCRIPTS_DIR}/build/check_gki.sh"
  bash ${SCRIPTS_DIR}/build/check_gki.sh ${KERNEL_CODE_DIR} ${KERNEL_DIR}

  if [ $? -ne 0 ]; then
    let RET_VAL+=2
    echo "ERROR: build bootimage error!"
  fi

  OUT_ABI_DIR=$(find ${KERNEL_CODE_DIR} -type d -name "out_abi")
  if [ -d "${OUT_ABI_DIR}" ]; then
    ABI_DIR=$(find ${OUT_ABI_DIR} -name "abi.report")
    WHITELIST_DIFF_DIR=$(find ${OUT_ABI_DIR} -name "diff_whitelist.report")
  fi

  if [ ! -f "${ABI_DIR}" ]; then
    if [ $RET_VAL -eq 0 ]; then
	  RET_VAL=1
	fi
    echo "ERROR: abi.report is not exist! "
  else
    #check abi report
    file_size=`ls -l ${ABI_DIR} | awk '{print $5}'`
    file_rows_count=$(awk 'END{print NR}' ${ABI_DIR})
    diff ${ABI_DIR} ${KERNEL_CODE_DIR}/${KERNEL_DIR}/Documentation/abi_base.report
    if [ $? -ne 0 -a ${file_size} -gt 0 ]; then
      if [[ $file_rows_count -le 5 ]]; then
        echo -e "WARNING-GKI: filtered out in GKI check! abi.report size: ${file_size}, rows: ${file_rows_count}"
        echo "check gki --------------------------------------------- PASS"
      else
        let RET_VAL+=4
        echo -e "ERROR: GKI check failed! abi.report size:${file_size} \nPlease read ${ABI_DIR}"
      fi
      echo -e "\nabi report info:"
      cat ${ABI_DIR}
    else
      echo "abi.report size: ${file_size}"
      echo "check gki --------------------------------------------- PASS"
    fi
  fi

  if [ ! -f "${WHITELIST_DIFF_DIR}" ]; then
    if [ $RET_VAL -eq 0 ];then
      RET_VAL=1
    fi
    echo "ERROR: diff_whitelist.report is not exist"
  else
    grep "to google" ${WHITELIST_DIFF_DIR} > null
    if [ $? -eq 0 ] ;then
      let RET_VAL+=16
	  whitelist_diff_flag=1
      echo -e "ERROR: whitelist has changed!\nPlease read ${WHITELIST_DIFF_DIR}"
      echo -e "New symbols need to be update to google!"
    fi
    grep "abi_gki_aarch64_unisoc" ${WHITELIST_DIFF_DIR} > null
    if [ $? -eq 0 ] ;then
      let RET_VAL+=32
	  whitelist_diff_flag=1
      echo -e "ERROR: whitelist has changed!\nPlease read ${WHITELIST_DIFF_DIR}"
      echo -e "Modify local files by diff_whitelist.report"
    fi

    if [[ $whitelist_diff_flag -ne 1 ]]; then
      echo "check whitelist --------------------------------------- PASS"
    fi
  fi

  if [ -d "${OUT_ABI_DIR}" ]; then
    if [ $(which python3) ]; then
      python3 $KERNEL_CODE_DIR/$KERNEL_DIR/scripts/sprd/sprd_check_compile.py ${OUT_ABI_DIR} 1
    else
      echo -e "\nWARNING: python3 is not exist! Please install python3!\n"
    fi
  else
    if [ $? -eq 0 ]; then
      RET_VAL=1
    fi
    echo "ERROR: ignore compilation check, ${OUT_ABI_DIR} is not exist!"
  fi

  if [ $? -eq 0 ]; then
    COMPILE_CHECK_PASS_FLAG=1
  fi
}

# save local gki diff config
cd ${KERNEL_CODE_DIR}/${KERNEL_DIR}
if [ -f ${KERNEL_CODE_DIR}/${KERNEL_DIR}/${GKI_DIFF_CONFIG} ]; then
  rm ${KERNEL_CODE_DIR}/${KERNEL_DIR}/${GKI_DIFF_CONFIG}
fi
if [ -f ${KERNEL_CODE_DIR}/${KERNEL_DIR}/Documentation/sprd-gki-diff-config ]; then
  git diff ${KERNEL_CODE_DIR}/${KERNEL_DIR}/Documentation/sprd-gki-diff-config > \
    ${KERNEL_CODE_DIR}/${KERNEL_DIR}/${GKI_DIFF_CONFIG}
  gki_diff_config_rows=$(awk 'END{print NR}' ${KERNEL_CODE_DIR}/${KERNEL_DIR}/${GKI_DIFF_CONFIG})
  if [ ${gki_diff_config_rows} -eq 0 ]; then
    echo "  remove gki_diff_config.diff!"
    rm ${KERNEL_CODE_DIR}/${KERNEL_DIR}/${GKI_DIFF_CONFIG}
  fi
fi
# RUN_GKI_CHECK_FLAG
# 0: no need gki check
# 1: need gki check
# 2: run gki check to end
RUN_GKI_CHECK_FLAG=0

if [ -f ${KERNEL_CODE_DIR}/${KERNEL_DIR}/Documentation/sprd-gki-choice-config ]; then
# get patch title
  cd ${KERNEL_CODE_DIR}/${KERNEL_DIR}
  PATCH_TITLE=$(git log -1 --pretty=format:"%s")
  echo "= patch title: ${PATCH_TITLE}"

  while read choice_config
  do
    if [[ $choice_config =~ ^[a-z:] ]]; then
      if [ "$RUN_GKI_CHECK_FLAG" -eq 1 ]; then
        RUN_GKI_CHECK_FLAG=2
      fi
      if [[ ${PATCH_TITLE} =~ $choice_config ]]; then
        echo "========================================================"
        echo "Run check gki for choice config of $choice_config"
        RUN_GKI_CHECK_FLAG=1
      fi
    elif [[ -z "$choice_config" && "$RUN_GKI_CHECK_FLAG" -eq 1 ]]; then
      do_gki_ckeck
# rebase Documentation/sprd-gki-diff-config
      cd ${KERNEL_CODE_DIR}/${KERNEL_DIR}
      git checkout ${KERNEL_CODE_DIR}/${KERNEL_DIR}/Documentation/sprd-gki-diff-config
      if [ -f ${KERNEL_CODE_DIR}/${KERNEL_DIR}/${GKI_DIFF_CONFIG} ]; then
        git apply ${KERNEL_CODE_DIR}/${KERNEL_DIR}/${GKI_DIFF_CONFIG}
      fi
    elif [ "$RUN_GKI_CHECK_FLAG" -eq 1 ]; then
      echo $choice_config >> ${KERNEL_CODE_DIR}/${KERNEL_DIR}/Documentation/sprd-gki-diff-config
    fi
  done < ${KERNEL_CODE_DIR}/${KERNEL_DIR}/Documentation/sprd-gki-choice-config
fi

if [ "$RUN_GKI_CHECK_FLAG" -ne 2 ]; then
  do_gki_ckeck
  RUN_GKI_CHECK_FLAG=2
# rebase Documentation/sprd-gki-diff-config
  cd ${KERNEL_CODE_DIR}/${KERNEL_DIR}
  git checkout ${KERNEL_CODE_DIR}/${KERNEL_DIR}/Documentation/sprd-gki-diff-config
  if [ -f ${KERNEL_CODE_DIR}/${KERNEL_DIR}/${GKI_DIFF_CONFIG} ]; then
    git apply ${KERNEL_CODE_DIR}/${KERNEL_DIR}/${GKI_DIFF_CONFIG}
  fi
fi

#remove gki_diff_config.diff
if [ -f ${KERNEL_CODE_DIR}/${KERNEL_DIR}/${GKI_DIFF_CONFIG} ]; then
  rm ${KERNEL_CODE_DIR}/${KERNEL_DIR}/${GKI_DIFF_CONFIG}
fi

if [ "$RET_VAL" -ne 0 ]; then
  echo "ERROR: gki check fail, Please read log of gki!"
fi

if [ "$COMPILE_CHECK_PASS_FLAG" -ne 1 ]; then
#  let RET_VAL+=128
  echo "WARNING: check compile warning!"
fi

echo -e  "++++++++++++++++++++++check gki end+++++++++++++++++++++"
exit $RET_VAL
