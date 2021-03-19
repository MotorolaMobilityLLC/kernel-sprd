# SPDX-License-Identifier: GPL-2.0
# scripts/dtc makefile

hostprogs-y	:= dtc
always		:= $(hostprogs-y)

dtc-objs	:= dtc.o flattree.o fstree.o data.o livetree.o treesource.o \
		   srcpos.o checks.o util.o
dtc-objs	+= dtc-lexer.lex.o dtc-parser.tab.o

dtc-objs	+= libfdt/fdt.o libfdt/fdt_ro.o libfdt/fdt_wip.o libfdt/fdt_sw.o \
			libfdt/fdt_rw.o libfdt/fdt_strerror.o libfdt/fdt_empty_tree.o \
			libfdt/fdt_addresses.o libfdt/fdt_overlay.o libfdt/dtbo_merge_check.o
# Source files need to get at the userspace version of libfdt_env.h to compile
HOST_EXTRACFLAGS := -I$(src)/libfdt

# CJ: it not ready for yaml build. I have check the http://192.168.33.19:8080/jenkins/, it also not have yaml.h
# android@HTSVR19:/usr/include$ ls | grep yaml
# android@HTSVR19:/usr/include$
# so I disable this for other platform build. (MTK R need yaml.h)
# force HOST_EXTRACFLAGS += -DNO_YAML
# gcc  -o scripts/dtc/dtc scripts/dtc/dtc.o scripts/dtc/flattree.o scripts/dtc/fstree.o scripts/dtc/data.o scripts/dtc/livetree.o scripts/dtc/treesource.o scripts/dtc/srcpos.o scripts/dtc/checks.o scripts/dtc/util.o scripts/dtc/dtc-lexer.lex.o scripts/dtc/dtc-parser.tab.o scripts/dtc/libfdt/fdt.o scripts/dtc/libfdt/fdt_ro.o scripts/dtc/libfdt/fdt_wip.o scripts/dtc/libfdt/fdt_sw.o scripts/dtc/libfdt/fdt_rw.o scripts/dtc/libfdt/fdt_strerror.o scripts/dtc/libfdt/fdt_empty_tree.o scripts/dtc/libfdt/fdt_addresses.o scripts/dtc/libfdt/fdt_overlay.o scripts/dtc/libfdt/dtbo_merge_check.o scripts/dtc/yamltree.o
# scripts/dtc/yamltree.o: In function `yaml_propval_int':
# yamltree.c:(.text+0x15c): undefined reference to `yaml_sequence_start_event_initialize'
# yamltree.c:(.text+0x169): undefined reference to `yaml_emitter_emit'
# yamltree.c:(.text+0x28b): undefined reference to `yaml_scalar_event_initialize'
# yamltree.c:(.text+0x29a): undefined reference to `yaml_emitter_emit'
# yamltree.c:(.text+0x3d7): undefined reference to `yaml_scalar_event_initialize'
# yamltree.c:(.text+0x3ee): undefined reference to `yaml_sequence_end_event_initialize'
# yamltree.c:(.text+0x3fb): undefined reference to `yaml_emitter_emit'

# ifeq ($(wildcard /usr/include/yaml.h),)
HOST_EXTRACFLAGS += -DNO_YAML
# else
# dtc-objs	+= yamltree.o
# HOSTLDLIBS_dtc	:= -lyaml
# endif

# Generated files need one more search path to include headers in source tree
HOSTCFLAGS_dtc-lexer.lex.o := -I$(src)
HOSTCFLAGS_dtc-parser.tab.o := -I$(src)

# dependencies on generated files need to be listed explicitly
$(obj)/dtc-lexer.lex.o: $(obj)/dtc-parser.tab.h

# generated files need to be cleaned explicitly
clean-files	:= dtc-lexer.lex.c dtc-parser.tab.c dtc-parser.tab.h
