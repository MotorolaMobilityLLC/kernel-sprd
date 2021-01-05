#
# Makefile for the input misc drivers.
#

# Each configuration option enables a list of files.

ccflags-$(CONFIG_INPUT_SX933X) := -DCONFIG_CAPSENSE_USB_CAL

obj-$(CONFIG_INPUT_SX933X)		+= sx933x.o
obj-$(CONFIG_INPUT_SX933X)		+= sensors_class.o
