#
# Makefile for nfc devices
obj-$(CONFIG_NFC_PN557) += pn557.o

pn557-$(CONFIG_NXP_NFC) += nfc.o
pn557-$(CONFIG_NFC_SN1XX) += sn1xx.o
pn557-$(CONFIG_NFC_PN8XT) += pn8xt.o
ccflags-$(CONFIG_NFC_SN1XX) := -DNFC_PLATFORM=sn1xx
ccflags-$(CONFIG_NFC_PN8XT) := -DNFC_PLATFORM=pn8xt
