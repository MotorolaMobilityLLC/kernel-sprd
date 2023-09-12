
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

#ifndef __XT_LIBRARY_LOADER_H__
#define __XT_LIBRARY_LOADER_H__

#ifdef __cplusplus
extern "C" {
#endif

	/* "packaged libraries" are the form of the library prior to loading.

	   This is an opaque structure.
	 */
	typedef struct xtlib_packaged_library xtlib_packaged_library;

	/* Some users will want to use a custom memory copy or setting function, (if,
	   for example, they want the library to be copied via DMA instead of copied
	   normally.
	 */

#ifdef __XTENSA__

	typedef void *xt_ptr;
	typedef int xt_int;
	typedef unsigned int xt_uint;

#else

	typedef unsigned int xt_ptr;
	typedef int xt_int;
	typedef unsigned int xt_uint;

#endif

	typedef xt_ptr(*memcpy_func) (xt_ptr dest, const void *src, unsigned int n);
	typedef xt_ptr(*memset_func) (xt_ptr s, int c, unsigned int n);

	/*  Memory copy and mem set callbacks with context pointer.
	*/

	typedef xt_ptr(*memcpy_func_ex) (xt_ptr dest, const void *src, unsigned int n, void *user);
	typedef xt_ptr(*memset_func_ex) (xt_ptr s, int c, unsigned int n, void *user);


	/* Error handling */
	enum {
		XTLIB_NO_ERR = 0,
		XTLIB_NOT_ELF = 1,
		XTLIB_NOT_DYNAMIC = 2,
		XTLIB_NOT_STATIC = 3,
		XTLIB_NO_DYNAMIC_SEGMENT = 4,
		XTLIB_UNKNOWN_SYMBOL = 5,
		XTLIB_NOT_ALIGNED = 6,
		XTLIB_NOT_SPLITLOAD = 7,
		XTLIB_RELOCATION_ERR = 8,
		XTLIB_NULL_PTR = 9,	/* Null pointer passed as parameter */
		XTLIB_NO_START = 10,	/* No start function found in lib */
		XTLIB_NOT_SAME_GB = 11,	/* Lib and app not in same GB of memory */
	};

	/* If a function in the API fails, call this function to get
	   one of the error codes above.
	 */
	unsigned int xtlib_error(void);

	/* An overlay on Xtensa processor can be loaded via xtlib_load_overlay.

	   Returns the address of the entry point defined by the -e option to
	   package_loadable_library script. Or NULL if for some reason the
	   load fails. Check the error code in that case.

	   No additional information is needed or required. To unload the library
	   just overwrite it with the new stuff. Make sure it has released all
	   the resources it has reserved first though.

	   To use custom memory copy and setting functions, use the second form.
	 */

#ifdef __XTENSA__

	void *xtlib_load_overlay (xtlib_packaged_library *library);

	void *xtlib_load_overlay_custom_fn (xtlib_packaged_library *library,
		memcpy_func mcpy_fn,
		memset_func mset_fn);

	/* For use with the newer API only */
	typedef struct xtlib_ovl_info {
		xt_ptr init_fn;
		xt_ptr fini_fn;
	} xtlib_ovl_info;

	/* The newer overlay functions listed below support loading and unloading
	   fixed-location overlays that require initialization and finalization.
	   This API is incompatible with the older libraries and requires that the
	   library's entry point function be defined in a specific way, as below.
	   See the Xtensa System Software Reference Manual for more information.

	   An instance of xtlib_ovl_info must be allocated by the caller and passed
	   to the load function. The load function will use this to save state, and
	   this struct must be preserved and passed into the unload function before
	   the library can be overwritten.

	   The library entry point (defined by the -e option to the packaging
	   script) must be implemented as -

	   void _start (void ** init_fn_p, void ** fini_fn_p, void ** entry_fn_p)
	   {
	   init_fn_p  = (void *) _init;
	   fini_fn_p  = (void *) _fini;
	   entry_fn_p = (void *) entry;
	   }

	   The address of the 'entry' function will be returned to the caller.
	   NULL will be returned if the load fails. In that case, the error code
	   should be checked by calling xtlib_error().
	 */

	void *xtlib_load_overlay_ext (xtlib_packaged_library *library,
		xtlib_ovl_info *info);

	void *xtlib_load_overlay_ext_custom_fn (xtlib_packaged_library *library,
		xtlib_ovl_info *info,
		memcpy_func mcpy_fn,
		memset_func mset_fn);

	void xtlib_unload_overlay_ext (xtlib_ovl_info *info);

#endif

	/* To load a position-independent library, you'll need to allocate
	   memory of xtlib_pi_library_size. Returns (unsigned int)(-1) if
	   failed.
	 */

	unsigned int xtlib_pi_library_size(xtlib_packaged_library *library);

	/* xtlib_split_pi_library_size gives code and data memory sizes.
	   It expecting library packaged for loading code and data separately.
	   Returns XTLIB_XXX status.
	 */

	unsigned int xtlib_split_pi_library_size(xtlib_packaged_library *library,
		unsigned int *code_size,
		unsigned int *data_size);


	/* To actually load the library on Xtensa processor:

	   First, allocate a xtlib_pil_info. On the stack or via
	   malloc is fine.

	   Next, call xtlib_load_pi_library. It will fill out the structure,
	   which you'll need later for symbol lookup and related. Accessing the
	   fields directly may result in unexpected behavior. If you stick to the
	   API, you'll be fine.

	   xtlib_load_pi_library returns the address of the entry point defined
	   by the -e option to package_loadable_library script. Or NULL if for
	   some reason the load fails. Check the error code in that case.

	   When you want to unload the library, call "xtlib_unload_pi_library" first
	   and it will call the termination function of the pil.

	   To lookup the address of a symbol, use xtlib_lookup_pi_library_symbol,
	   which will return the correct address, or NULL if the symbol isn't found.

	   To use a custom memory functions, use the second form of
	   xtlib_load_pi_library.

	   Loading code and data separatly follows the same steps but with '_split_'
	   functions and with library packaged for split loading. XTLIB_NOT_SPLITLOAD
	   error status is set if this is not the case.
	 */

	typedef struct xtlib_pil_info {
		xt_ptr  dst_addr;
		xt_uint src_offs;
		xt_ptr  dst_data_addr;
		xt_uint src_data_offs;
		xt_ptr  start_sym;
		xt_ptr  text_addr;
		xt_ptr  init;
		xt_ptr  fini;
		xt_ptr  rel;
		xt_int  rela_count;
		xt_ptr  hash;
		xt_ptr  symtab;
		xt_ptr  strtab;
		xt_int  align;
	} xtlib_pil_info;

#ifdef __XTENSA__

	void *xtlib_load_pi_library (xtlib_packaged_library *library,
		void *destination_address,
		xtlib_pil_info *lib_info);

	void *xtlib_load_pi_library_custom_fn (xtlib_packaged_library *library,
		void *destination_address,
		xtlib_pil_info *lib_info,
		memcpy_func mcpy_fn,
		memset_func mset_fn);

	void *xtlib_load_split_pi_library (xtlib_packaged_library *library,
		void *destination_code_address,
		void *destination_data_address,
		xtlib_pil_info *lib_info);

	void *xtlib_load_split_pi_library_custom_fn (xtlib_packaged_library *library,
		void *destination_code_address,
		void *destination_data_address,
		xtlib_pil_info *lib_info,
		memcpy_func mcpy_fn,
		memset_func mset_fn);

	void xtlib_unload_pi_library (xtlib_pil_info *lib_info);

	void *xtlib_lookup_pi_library_symbol (xtlib_pil_info *lib_info,
		const char *symbolname);

#endif

	/*
	   Loading libraries from host processor:

	   NOTE: 1) This functions do not perform cache synchronization between host and
	   target processors.

	   2) All xt_ptr return values and function arguments are in host processor byte
	   order but in target processor address space (Including arguments of memcopy
	   and memset callbacks).

	   3) <user> argument is a user defined context pointer for mcpy_fn and mset_fn
	   callbacks and it's passed to callbacks as is.

	   An overlay can be loaded via xtlib_host_load_overlay.

	   Returns the address of the entry point or NULL if for some reason the
	   load fails. Check the error code in that case.

	   To load position-independent library from host processor:

	   Find out required memory size using xtlib_pi_library_size function (or using
	   xtlib_split_pi_library_size if it's a split load library)

	   Pass sizes to target processor so it can reserve memory and report address(es)
	   to host.

	   Allocate a xtlib_pil_info.

	   Call xtlib_host_load_pi_library. Please note that destination address is in target
	   processor address space.

	   xtlib_host_load_pi_library fills out xtlib_pil_info structure and returns non NULL value on success.

	   Pass xtlib_pil_info structure to target processor.

	   At this point host initialization part is complete. Please make sure that data
	   actually written out and available for target processor.

	   On target processor:

	   Call xtlib_target_init_pi_library to complete initialization. It returns entry point address
	   or NULL if failed.

	 */

#ifdef __XTENSA__

	/* xtlib_target_init_pi_library initializing library loaded by host processor */
	void *xtlib_target_init_pi_library (xtlib_pil_info *lib_info);

#endif

	xt_ptr xtlib_host_load_overlay(xtlib_packaged_library *library,
		memcpy_func_ex mcpy_fn,
		memset_func_ex mset_fn,
		void *user);

	xt_ptr xtlib_host_load_pi_library(xtlib_packaged_library *library,
		xt_ptr destination_address,
		xtlib_pil_info *lib_info,
		memcpy_func_ex mcpy_fn,
		memset_func_ex mset_fn,
		void *user);

	xt_ptr xtlib_host_load_split_pi_library(xtlib_packaged_library *library,
		xt_ptr destination_code_address,
		xt_ptr destination_data_address,
		xtlib_pil_info *lib_info,
		memcpy_func_ex mcpy_fn,
		memset_func_ex mset_fn,
		void *user);

#ifdef __cplusplus
}
#endif
#endif				/* __XT_LIBRARY_LOADER_H__ */
