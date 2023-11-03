/*
 * file name: hello.c
 */

#include <linux/module.h>
#include <linux/kernel.h>

MODULE_AUTHOR("Ruifeng.Zhang");
MODULE_LICENSE("GPL");

static int __init hello_init(void)
{
	printk(KERN_ALERT"Init hello mudule...\n");

	printk(KERN_ALERT"Hello, how are you? \n"); 

	return 0;
}

static void __exit hello_exit(void)
{
	printk(KERN_ALERT"Exit hello mudule...\n");

	printk(KERN_ALERT"I come from hello's module, I have been unload.\n");

}

module_init(hello_init);
module_exit(hello_exit);

