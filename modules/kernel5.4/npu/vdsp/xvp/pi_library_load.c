
/*
 * Copyright (c) 2012-2013 by Tensilica Inc. ALL RIGHTS RESERVED.
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
 * THE SOFTWARE.
 */

#include "elf.h"
#include "xt_library_loader.h"
#include "loader_internal.h"
#include <linux/bsearch.h>
#include <linux/device.h>
#include <linux/of.h>
#include <linux/of_address.h>
#include <linux/slab.h>
#include <linux/sort.h>

#ifdef pr_fmt
#undef pr_fmt
#endif
#define pr_fmt(fmt) "sprd-vdsp: pi_library %d %d %s : "\
        fmt, current->pid, __LINE__, __func__

static xt_ptr align_ptr(xt_ptr ptr, xt_uint align)
{
	return (xt_ptr) (((xt_uint) ptr + align - 1) & ~(align - 1));
}

static int find_align(Elf32_Ehdr * header)
{
	int align = 0;
	int sec = 0;
	Elf32_Shdr *sheader = (Elf32_Shdr *) (((char *)header) + xtlib_host_word(header->e_shoff));
	int num = (int)xtlib_host_half(header->e_shnum);

	while (sec < num) {
		if (sheader[sec].sh_type != SHT_NULL && xtlib_host_word(sheader[sec].sh_size) > 0) {
			int sec_align = xtlib_host_word(sheader[sec].sh_addralign);

			if (sec_align > align)
				align = sec_align;
		}
		sec++;
	}

	return align;
}

static Elf32_Dyn *find_dynamic_info(Elf32_Ehdr * eheader)
{
	int seg = 0;
	char *base_addr = (char *)eheader;
	Elf32_Phdr *pheader = (Elf32_Phdr *) (base_addr + xtlib_host_word(eheader->e_phoff));
	int num = (int)xtlib_host_half(eheader->e_phnum);

	while (seg < num) {
		if (xtlib_host_word(pheader[seg].p_type) == PT_DYNAMIC)
			return (Elf32_Dyn *) (base_addr + xtlib_host_word(pheader[seg].p_offset));
		seg++;
	}

	return 0;
}

static int load_pi_lib(xtlib_pil_info * lib_info, Elf32_Ehdr * eheader, void *lib_addr,
	memcpy_func_ex mcpy, memset_func_ex mset, void *user)
{
	uint32_t offset;
	int seg = 0;
	char *pindex = (char *)user;

	Elf32_Phdr *pheader = (Elf32_Phdr *) ((char *)lib_addr + xtlib_host_word(eheader->e_phoff));
	xt_ptr dst_addr = (xt_ptr) xtlib_host_word((Elf32_Word) lib_info->dst_addr);
	int num = (int)xtlib_host_half(eheader->e_phnum);

	while (seg < num) {
		if (xtlib_host_word(pheader[seg].p_type) == PT_LOAD) {
			void *src = (char *)lib_addr + xtlib_host_word(pheader[seg].p_offset);
			xt_ptr dst = (xt_ptr) (dst_addr + xtlib_host_word(pheader[seg].p_paddr));
			offset = dst - dst_addr;
			xtlib_load_seg(&pheader[seg], src, dst, mcpy, mset, pindex + offset);
			if (lib_info->text_addr == 0)
				lib_info->text_addr = (xt_ptr) xtlib_xt_word((Elf32_Word) dst);
		}
		seg++;
	}

	return XTLIB_NO_ERR;
}

static xt_ptr xt_ptr_offs(xt_ptr base, Elf32_Word offs)
{
	return (xt_ptr) xtlib_xt_word((unsigned int)base + xtlib_host_word(offs));
}

static int get_dyn_info(Elf32_Ehdr * eheader, xt_ptr dst_addr, xt_uint src_offs,
	xt_ptr dst_data_addr, xt_uint src_data_offs, xtlib_pil_info * info)
{
	unsigned int jmprel = 0;
	unsigned int pltrelsz = 0;

	Elf32_Dyn *dyn_entry = find_dynamic_info(eheader);

	if (dyn_entry == 0)
		return XTLIB_NO_DYNAMIC_SEGMENT;

	info->dst_addr = (xt_ptr) xtlib_xt_word((Elf32_Word) dst_addr);
	info->src_offs = xtlib_xt_word(src_offs);
	info->dst_data_addr = (xt_ptr) xtlib_xt_word((Elf32_Word) dst_data_addr);
	info->src_data_offs = xtlib_xt_word(src_data_offs);

	dst_addr -= src_offs;
	dst_data_addr -= src_data_offs;

	info->start_sym = xt_ptr_offs(dst_addr, eheader->e_entry);
	info->align = xtlib_xt_word(find_align(eheader));
	info->text_addr = 0;

	while (dyn_entry->d_tag != DT_NULL) {
		switch ((Elf32_Sword)
			xtlib_host_word((Elf32_Word) dyn_entry->d_tag)) {
		case DT_RELA:
			info->rel = xt_ptr_offs(dst_data_addr, dyn_entry->d_un.d_ptr);
			break;
		case DT_RELASZ:
			info->rela_count = xtlib_xt_word(xtlib_host_word(dyn_entry->d_un.d_val) / sizeof(Elf32_Rela));
			break;
		case DT_INIT:
			info->init = xt_ptr_offs(dst_addr, dyn_entry->d_un.d_ptr);
			break;
		case DT_FINI:
			info->fini = xt_ptr_offs(dst_addr, dyn_entry->d_un.d_ptr);
			break;
		case DT_HASH:
			info->hash = xt_ptr_offs(dst_data_addr, dyn_entry->d_un.d_ptr);
			break;
		case DT_SYMTAB:
			info->symtab = xt_ptr_offs(dst_data_addr, dyn_entry->d_un.d_ptr);
			break;
		case DT_STRTAB:
			info->strtab = xt_ptr_offs(dst_data_addr, dyn_entry->d_un.d_ptr);
			break;
		case DT_JMPREL:
			jmprel = dyn_entry->d_un.d_val;
			break;
		case DT_PLTRELSZ:
			pltrelsz = dyn_entry->d_un.d_val;
			break;
		case DT_LOPROC + 2:
			info->text_addr = xt_ptr_offs(dst_addr, dyn_entry->d_un.d_ptr);
			break;

		default:
			/* do nothing */
			break;
		}
		dyn_entry++;
	}

	return XTLIB_NO_ERR;
}

static int validate_dynamic(Elf32_Ehdr * header)
{
	if (xtlib_verify_magic(header) != 0) {
		pr_err("[ERROR]xtlib_verify_magic failed\n");
		return XTLIB_NOT_ELF;
	}

	if (xtlib_host_half(header->e_type) != ET_DYN) {
		pr_err("[ERROR]xtlib_host_half failed\n");
		return XTLIB_NOT_DYNAMIC;
	}

	return XTLIB_NO_ERR;
}

static int validate_dynamic_splitload(Elf32_Ehdr * header)
{
	Elf32_Phdr *pheader;
	int err = validate_dynamic(header);

	if (err != XTLIB_NO_ERR)
		return err;

	/* make sure it's split load pi library, expecting three headers,
	   code, data and dynamic, for example:

	   LOAD off    0x00000094 vaddr 0x00000000 paddr 0x00000000 align 2**0
	   filesz 0x00000081 memsz 0x00000081 flags r-x
	   LOAD off    0x00000124 vaddr 0x00000084 paddr 0x00000084 align 2**0
	   filesz 0x000001ab memsz 0x000011bc flags rwx
	   DYNAMIC off    0x00000124 vaddr 0x00000084 paddr 0x00000084 align 2**2
	   filesz 0x000000a0 memsz 0x000000a0 flags rw-
	 */

	if (xtlib_host_half(header->e_phnum) != 3)
		return XTLIB_NOT_SPLITLOAD;

	pheader = (Elf32_Phdr *) ((char *)header + xtlib_host_word(header->e_phoff));

	/* LOAD R-X */
	if (xtlib_host_word(pheader[0].p_type) != PT_LOAD
	    || (xtlib_host_word(pheader[0].p_flags) & (PF_R | PF_W | PF_X)) != (PF_R | PF_X))
		return XTLIB_NOT_SPLITLOAD;

	/* LOAD RWX */
	if (xtlib_host_word(pheader[1].p_type) != PT_LOAD
	    || (xtlib_host_word(pheader[1].p_flags) & (PF_R | PF_W | PF_X)) != (PF_R | PF_W | PF_X))
		return XTLIB_NOT_SPLITLOAD;

	/* DYNAMIC RW- */
	if (xtlib_host_word(pheader[2].p_type) != PT_DYNAMIC
	    || (xtlib_host_word(pheader[2].p_flags) & (PF_R | PF_W | PF_X)) != (PF_R | PF_W))
		return XTLIB_NOT_SPLITLOAD;

	return XTLIB_NO_ERR;
}

unsigned int xtlib_pi_library_size(xtlib_packaged_library * library)
{
	int seg, num;
	unsigned int bytes = 0;
	unsigned int size;
	Elf32_Phdr *pheader;
	Elf32_Ehdr *header = (Elf32_Ehdr *) library;

	int err = validate_dynamic(header);

	if (err != XTLIB_NO_ERR) {
		pr_err("[ERROR]return -1\n");
		xtlib_globals.err = err;
		return -1;
	}
	seg = 0;
	num = (int)xtlib_host_half(header->e_phnum);
	pheader = (Elf32_Phdr *) ((char *)library + xtlib_host_word(header->e_phoff));
	while (seg < num) {
		if (xtlib_host_word(pheader[seg].p_type) == PT_LOAD) {
			size = xtlib_host_word(pheader[seg].p_paddr) + xtlib_host_word(pheader[seg].p_memsz);
			if (size > bytes)
				bytes = size;
		}
		seg++;
	}
	bytes += find_align(header);

	return bytes;
}

unsigned int xtlib_split_pi_library_size(xtlib_packaged_library * library,
	unsigned int *code_size, unsigned int *data_size)
{
	Elf32_Phdr *pheader;
	Elf32_Ehdr *header = (Elf32_Ehdr *) library;
	int align;

	int err = validate_dynamic_splitload(header);

	if (err != XTLIB_NO_ERR) {
		xtlib_globals.err = err;
		return err;
	}

	align = find_align(header);
	pheader = (Elf32_Phdr *) ((char *)library + xtlib_host_word(header->e_phoff));

	/*
	   the size calculation assumes the starting address of destination buffer may
	   not be aligned to the required alignment, so extra space is needed in order to
	   maintain the same alignment as the source data.  Besides this, the source data
	   may not start at the alignment boundary, so another extra space is needed.  At the
	   end of the buffer, some padding may also necessary if the buffer is not ended at
	   the alignment boundary, in order to safe guard the write operation.
	   So, the total size is in multiple of alignment.
	 */
	*code_size =
	    (align + (xtlib_host_word(pheader[0].p_paddr) & (align - 1)) +
	     xtlib_host_word(pheader[0].p_memsz) + align - 1) & (~(align - 1));
	*data_size =
	    (align + (xtlib_host_word(pheader[1].p_paddr) & (align - 1)) +
	     xtlib_host_word(pheader[1].p_memsz) + align - 1) & (~(align - 1));

	return XTLIB_NO_ERR;
}

static xt_ptr xtlib_load_split_pi_library_common(xtlib_packaged_library *library,
	xt_ptr destination_code_address,
	xt_ptr destination_data_address,
	xtlib_pil_info * lib_info,
	memcpy_func_ex mcpy_fn,
	memset_func_ex mset_fn,
	void *user)
{
	Elf32_Ehdr *header = (Elf32_Ehdr *) library;
	Elf32_Phdr *pheader;
	unsigned int align;
	xt_ptr dst_code, dst_data;

	int err = validate_dynamic_splitload(header);

	if (err != XTLIB_NO_ERR) {
		xtlib_globals.err = err;
		return 0;
	}

	align = find_align(header);

	destination_code_address = align_ptr(destination_code_address, align);
	destination_data_address = align_ptr(destination_data_address, align);

	pheader = (Elf32_Phdr *) ((char *)library + xtlib_host_word(header->e_phoff));

	dst_code = destination_code_address + (xtlib_host_word(pheader[0].p_paddr) & (align - 1));
	dst_data = destination_data_address + (xtlib_host_word(pheader[1].p_paddr) & (align - 1));

	err = get_dyn_info(header,
			   dst_code, xtlib_host_word(pheader[0].p_paddr),
			   dst_data, xtlib_host_word(pheader[1].p_paddr),
			   lib_info);

	if (err != XTLIB_NO_ERR) {
		xtlib_globals.err = err;
		return 0;
	}

	/* loading code */
	xtlib_load_seg(&pheader[0], (char *)library + xtlib_host_word(pheader[0].p_offset),
		dst_code, mcpy_fn, mset_fn, user);

	if (lib_info->text_addr == 0)
		lib_info->text_addr = (xt_ptr) xtlib_xt_word((Elf32_Word) dst_code);

	/* loading data */
	xtlib_load_seg(&pheader[1], (char *)library + xtlib_host_word(pheader[1].p_offset),
		dst_data, mcpy_fn, mset_fn, user);

	return (xt_ptr) xtlib_host_word((Elf32_Word) lib_info->start_sym);
}

static xt_ptr xtlib_load_pi_library_common(xtlib_packaged_library * library,
	xt_ptr destination_address,
	xtlib_pil_info * lib_info,
	memcpy_func_ex mcpy,
	memset_func_ex mset,
	void *user)
{
	Elf32_Ehdr *header = (Elf32_Ehdr *) library;
	unsigned int align;

	int err = validate_dynamic(header);

	if (err != XTLIB_NO_ERR) {
		xtlib_globals.err = err;
		return 0;
	}

	align = find_align(header);
	destination_address = align_ptr(destination_address, align);

	err = get_dyn_info(header, destination_address, 0, destination_address, 0, lib_info);
	if (err != XTLIB_NO_ERR) {
		xtlib_globals.err = err;
		return 0;
	}

	err = load_pi_lib(lib_info, header, library, mcpy, mset, user);
	if (err != XTLIB_NO_ERR) {
		xtlib_globals.err = err;
		return 0;
	}

	return (xt_ptr) xtlib_host_word((Elf32_Word) lib_info->start_sym);
}

xt_ptr xtlib_host_load_pi_library(xtlib_packaged_library * library,
	xt_ptr destination_address,
	xtlib_pil_info * lib_info,
	memcpy_func_ex mcpy_fn,
	memset_func_ex mset_fn,
	void *user)
{
	return xtlib_load_pi_library_common(library, destination_address,lib_info, mcpy_fn,
		mset_fn, user);
}

xt_ptr xtlib_host_load_split_pi_library(xtlib_packaged_library * library,
	xt_ptr destination_code_address,
	xt_ptr destination_data_address,
	xtlib_pil_info * lib_info,
	memcpy_func_ex mcpy_fn,
	memset_func_ex mset_fn, void *user)
{
	return xtlib_load_split_pi_library_common(library, destination_code_address,
		destination_data_address, lib_info, mcpy_fn, mset_fn, user);
}
