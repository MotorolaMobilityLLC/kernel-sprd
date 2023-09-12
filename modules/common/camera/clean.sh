shpath=$(cd "$(dirname "${BASH_SOURCE[0]}")" >/dev/null && pwd)
echo $shpath
if [ "kernel_module" != ${shpath:0-13:13} ];then
shpath="."
fi

find $shpath -name *.o | xargs rm -f
find $shpath -name *.o.d | xargs rm -f
find $shpath -name *.cmd | xargs rm -f
find $shpath -name Module.symvers | xargs rm -f
find $shpath -name modules.order | xargs rm -f
find $shpath -name *.ko | xargs rm -f
find $shpath -name *.mod.c | xargs rm -f
find $shpath -name .tmp_versions | xargs rm -rf
