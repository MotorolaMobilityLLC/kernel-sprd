/*
*****************************************************************************
* Copyright by ams AG                                                       *
* All rights are reserved.                                                  *
*                                                                           *
* IMPORTANT - PLEASE READ CAREFULLY BEFORE COPYING, INSTALLING OR USING     *
* THE SOFTWARE.                                                             *
*                                                                           *
* THIS SOFTWARE IS PROVIDED FOR USE ONLY IN CONJUNCTION WITH AMS PRODUCTS.  *
* USE OF THE SOFTWARE IN CONJUNCTION WITH NON-AMS-PRODUCTS IS EXPLICITLY    *
* EXCLUDED.                                                                 *
*                                                                           *
* THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS       *
* "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT         *
* LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS         *
* FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT  *
* OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,     *
* SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT          *
* LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,     *
* DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY     *
* THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT       *
* (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE     *
* OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.      *
*****************************************************************************
*/
#ifndef TCS3430_REGS_H
#define TCS3430_REGS_H

#include <linux/types.h>

#define TCS3430_INTEGRATION_TIME_PER_CYCLE 278
#define TCS3430_INTEGRATION_FACTOR         100
#define TCS3430_ATIME_TO_MS(x) ((((x + 1) * TCS3430_INTEGRATION_TIME_PER_CYCLE) / TCS3430_INTEGRATION_FACTOR))
#define TCS3430_ATIME_TO_ITIME_FIXED(x) (((x + 1) * TCS3430_INTEGRATION_TIME_PER_CYCLE)/100)
#define TCS3430_MS_TO_ATIME(x) ((uint8_t)((x * TCS3430_INTEGRATION_FACTOR) / TCS3430_INTEGRATION_TIME_PER_CYCLE))
#define TCS3430_ATIME_TO_CYCLES(x) (x + 1)

#define ALS_PERSIST(p) (((p) & 0xf) << 0)

#define LUX_MESSAGES

enum tcs3430_reg_masks {
	TCS3430_ALS_GAIN_MASK = (3 << 0),
	MAX_ALS_VALUE = 0xffff,
	MIN_ALS_VALUE = 10,
};

extern u8 const als_gains[];

#endif
