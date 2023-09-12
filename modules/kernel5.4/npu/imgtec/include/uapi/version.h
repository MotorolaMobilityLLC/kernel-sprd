/*!
******************************************************************************
 @file   : version.h

 @brief  Version information for VHA tools

 @Author Imagination Technologies

 @date   08/05/2013

 @License  MIT

  Copyright (c) Imagination Technologies Ltd.

  Permission is hereby granted, free of charge, to any person obtaining a copy
  of this software and associated documentation files (the "Software"), to deal
  in the Software without restriction, including without limitation the rights
  to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
  copies of the Software, and to permit persons to whom the Software is
  furnished to do so, subject to the following conditions:

  The above copyright notice and this permission notice shall be included in
  all copies or substantial portions of the Software.

  THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
  IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
  FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.  IN NO EVENT SHALL THE
  AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
  LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
  OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
  THE SOFTWARE.

 \n<b>Description:</b>\n
         This file is automatically updated by the build system to contain
         the correct version information.

 \n<b>Platform:</b>\n
         Platform Independent

******************************************************************************/

#ifndef VERSION_H
#define VERSION_H

/* ------------ DDK API ------------*/
#define DDK_API_MAJOR_NUMBER 2
#define DDK_API_MINOR_NUMBER 0
#define DDK_API_PATCH_NUMBER 0

/* ------------ DDK Implementation ------------*/
#define DDK_MAJOR_NUMBER 3
#define DDK_MINOR_NUMBER 14

/* ------------ Common build info ------------*/
#define VERSION_STRING "REL_3.14-cl6273746"

/* ------------ DDK version string helper macro ------------*/
#define _VSTR_(s) #s
#define VSTR(s) _VSTR_(s)
#define NNA_VER_STR \
    ("NNA_API_" VSTR(DDK_API_MAJOR_NUMBER) "." VSTR(DDK_API_MINOR_NUMBER) "." VSTR(DDK_API_PATCH_NUMBER) \
     "_DDK_" VSTR(DDK_MAJOR_NUMBER) "." VSTR(DDK_MINOR_NUMBER) "@" VERSION_STRING)

/* ------------ Digest of the vha.h kernel interface -------------*/
#define KERNEL_INTERFACE_DIGEST "c0d861123db93c890132d3b2a181f656"

#endif  // VERSION_H
