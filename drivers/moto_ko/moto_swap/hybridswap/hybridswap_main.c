/*
 * file name: hello.c
 */

#include <linux/module.h>
#include <linux/init.h>
#include <linux/device.h>
#include <linux/kernel.h>
#include <linux/slab.h>
#include "hybridswap.h"
#include "../zram-5.15/zcomp.h"
#include "../zram-5.15/zram_drv.h"

MODULE_AUTHOR("Ruifeng.Zhang");
MODULE_LICENSE("GPL");

static int __init hello_init(void)
{
	printk(KERN_ALERT"Init hello mudule...\n");

	printk(KERN_ALERT"Hello, how are you? \n"); 

	//ramboost_test_01();
	ramboost_test_02();
	ramboost_test_03();
	ramboost_test_04();
	ramboost_test_05();

	return 0;
}

static void __exit hello_exit(void)
{
	printk(KERN_ALERT"Exit hello mudule...\n");

	printk(KERN_ALERT"I come from hello's module, I have been unload.\n");

}

module_init(hello_init);
module_exit(hello_exit);

