#
# Makefile for nfc devices
obj-m += pn557.o

pn557-y += nfc.o
pn557-y += sn1xx.o
pn557-y += pn8xt.o
ccflags-y := -DNFC_PLATFORM=sn1xx
ccflags-y := -DNFC_PLATFORM=pn8xt
