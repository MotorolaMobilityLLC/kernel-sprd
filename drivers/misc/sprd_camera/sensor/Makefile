sprd_sensor-objs += sprd_sensor_drv.o sprd_sensor_core.o

ifeq ($(CONFIG_64BIT),y)
sprd_sensor-objs += compat_sensor_drv.o
endif

obj-y += sprd_sensor.o
obj-y += power/ csi2/
