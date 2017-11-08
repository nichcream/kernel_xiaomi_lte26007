/* =========================================================================
 * $File: //dwh/usb_iip/dev/software/comip_hsic_common_port_2/comip_hsic_os.h $
 * $Revision: #14 $
 * $Date: 2010/11/04 $
 * $Change: 1621695 $
 *
 * Synopsys Portability Library Software and documentation
 * (hereinafter, "Software") is an Unsupported proprietary work of
 * Synopsys, Inc. unless otherwise expressly agreed to in writing
 * between Synopsys and you.
 *
 * The Software IS NOT an item of Licensed Software or Licensed Product
 * under any End User Software License Agreement or Agreement for
 * Licensed Product with Synopsys or any supplement thereto. You are
 * permitted to use and redistribute this Software in source and binary
 * forms, with or without modification, provided that redistributions
 * of source code must retain this notice. You may not view, use,
 * disclose, copy or distribute this file or any information contained
 * herein except pursuant to this license grant from Synopsys. If you
 * do not agree with this notice, including the disclaimer below, then
 * you are not authorized to use the Software.
 *
 * THIS SOFTWARE IS BEING DISTRIBUTED BY SYNOPSYS SOLELY ON AN "AS IS"
 * BASIS AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS
 * FOR A PARTICULAR PURPOSE ARE HEREBY DISCLAIMED. IN NO EVENT SHALL
 * SYNOPSYS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE
 * USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH
 * DAMAGE.
 * ========================================================================= */
#ifndef _COMIP_HSIC_OS_H_
#define _COMIP_HSIC_OS_H_

#ifdef __cplusplus
extern "C" {
#endif

/** @file
 *
 * COMIP portability library, low level os-wrapper functions
 *
 */

/* These basic types need to be defined by some OS header file or custom header
 * file for your specific target architecture.
 *
 * uint8_t, int8_t, uint16_t, int16_t, uint32_t, int32_t, uint64_t, int64_t
 *
 * Any custom or alternate header file must be added and enabled here.
 */
# define COMIP_HSIC_LINUX
#ifdef COMIP_HSIC_LINUX
# include <linux/types.h>
# ifdef CONFIG_DEBUG_MUTEXES
#  include <linux/mutex.h>
# endif
# include <linux/errno.h>
# include <stdarg.h>
# include <linux/slab.h>
# include <linux/delay.h>

#endif

/** @name Primitive Types and Values */

/** We define a boolean type for consistency.  Can be either YES or NO */
typedef uint8_t comip_hsic_bool_t;
#define YES  1
#define NO   0

/** @name Tracing/Logging Functions
 *
 * These function provide the capability to add tracing, debugging, and error
 * messages, as well exceptions as assertions.  The WUDEV uses these
 * extensively.  These could be logged to the main console, the serial port, an
 * internal buffer, etc.  These functions could also be no-op if they are too
 * expensive on your system.  By default undefining the DEBUG macro already
 * no-ops some of these functions. */

/** Returns non-zero if in interrupt context. */
extern comip_hsic_bool_t COMIP_HSIC_IN_IRQ(void);
//#define comip_hsic_in_irq COMIP_HSIC_IN_IRQ

/** Returns "IRQ" if COMIP_HSIC_IN_IRQ is true. */
static inline char *comip_hsic_irq(void) {
    return COMIP_HSIC_IN_IRQ() ? "IRQ" : "";
}

/** Returns non-zero if in bottom-half context. */
extern comip_hsic_bool_t COMIP_HSIC_IN_BH(void);
#define comip_hsic_in_bh COMIP_HSIC_IN_BH

/** Returns "BH" if COMIP_HSIC_IN_BH is true. */
static inline char *comip_hsic_bh(void) {
    return COMIP_HSIC_IN_BH() ? "BH" : "";
}

/**
 * A vprintf() clone.  Just call vprintf if you've got it.
 */
extern void COMIP_HSIC_VPRINTF(char *format, va_list args);
#define comip_hsic_vprintf COMIP_HSIC_VPRINTF

/**
 * A vsnprintf() clone.  Just call vprintf if you've got it.
 */
extern int COMIP_HSIC_VSNPRINTF(char *str, int size, char *format, va_list args);
#define comip_hsic_vsnprintf COMIP_HSIC_VSNPRINTF

/**
 * printf() clone.  Just call printf if you've go it.
 */
extern void COMIP_HSIC_PRINTF(char *format, ...)
/* This provides compiler level static checking of the parameters if you're
 * using GCC. */
#ifdef __GNUC__
    __attribute__ ((format(printf, 1, 2)));
#else
    ;
#endif
#define comip_hsic_printf COMIP_HSIC_PRINTF

/**
 * sprintf() clone.  Just call sprintf if you've got it.
 */
extern int COMIP_HSIC_SPRINTF(char *string, char *format, ...)
#ifdef __GNUC__
    __attribute__ ((format(printf, 2, 3)));
#else
    ;
#endif
#define comip_hsic_sprintf COMIP_HSIC_SPRINTF

/**
 * snprintf() clone.  Just call snprintf if you've got it.
 */
extern int COMIP_HSIC_SNPRINTF(char *string, int size, char *format, ...)
#ifdef __GNUC__
    __attribute__ ((format(printf, 3, 4)));
#else
    ;
#endif
#define comip_hsic_snprintf COMIP_HSIC_SNPRINTF

/**
 * Prints a WARNING message.  On systems that don't differentiate between
 * warnings and regular log messages, just print it.  Indicates that something
 * may be wrong with the driver.  Works like printf().
 *
 * Use the COMIP_HSIC_WARN macro to call this function.
 */
extern void __COMIP_HSIC_WARN(char *format, ...)
#ifdef __GNUC__
    __attribute__ ((format(printf, 1, 2)));
#else
    ;
#endif

/**
 * Prints an error message.  On systems that don't differentiate between errors
 * and regular log messages, just print it.  Indicates that something went wrong
 * with the driver.  Works like printf().
 *
 * Use the COMIP_HSIC_ERROR macro to call this function.
 */
extern void __COMIP_HSIC_ERROR(char *format, ...)
#ifdef __GNUC__
    __attribute__ ((format(printf, 1, 2)));
#else
    ;
#endif

/**
 * Prints an exception error message and takes some user-defined action such as
 * print out a backtrace or trigger a breakpoint.  Indicates that something went
 * abnormally wrong with the driver such as programmer error, or other
 * exceptional condition.  It should not be ignored so even on systems without
 * printing capability, some action should be taken to notify the developer of
 * it.  Works like printf().
 */
extern void COMIP_HSIC_EXCEPTION(char *format, ...)
#ifdef __GNUC__
    __attribute__ ((format(printf, 1, 2)));
#else
    ;
#endif
#define comip_hsic_exception COMIP_HSIC_EXCEPTION

#ifdef DEBUG
/**
 * Prints out a debug message.  Used for logging/trace messages.
 *
 * Use the COMIP_HSIC_DEBUG macro to call this function
 */
extern void __COMIP_HSIC_DEBUG(char *format, ...)
#ifdef __GNUC__
    __attribute__ ((format(printf, 1, 2)));
#else
    ;
#endif
#else
#define __COMIP_HSIC_DEBUG(...)
#endif

/**
 * Prints out a Debug message.
 */
#define COMIP_HSIC_DEBUG(_format, _args...) __COMIP_HSIC_DEBUG("DEBUG:%s:%s: " _format "\n", \
                         __func__, comip_hsic_irq(), ## _args)
#define comip_hsic_debug COMIP_HSIC_DEBUG
/**
 * Prints out an informative message.
 */
#define COMIP_HSIC_INFO(_format, _args...) COMIP_HSIC_PRINTF("INFO:%s: " _format "\n", \
                           comip_hsic_irq(), ## _args)
#define comip_hsic_info COMIP_HSIC_INFO
/**
 * Prints out a warning message.
 */
#define COMIP_HSIC_WARN(_format, _args...) __COMIP_HSIC_WARN("WARN:%s:%s:%d: " _format "\n", \
                    comip_hsic_irq(), __func__, __LINE__, ## _args)
#define comip_hsic_warn COMIP_HSIC_WARN
/**
 * Prints out an error message.
 */
#define COMIP_HSIC_ERROR(_format, _args...) __COMIP_HSIC_ERROR("ERROR:%s:%s:%d: "  _format "\n", \
                    comip_hsic_irq(), __func__, __LINE__, ## _args)
#define comip_hsic_error COMIP_HSIC_ERROR

#define COMIP_HSIC_PROTO_ERROR(_format, _args...) __COMIP_HSIC_WARN("ERROR:%s:%s:%d: " _format "\n", \
                        comip_hsic_irq(), __func__, __LINE__, ## _args)
#define comip_hsic_proto_error COMIP_HSIC_PROTO_ERROR

#ifdef DEBUG
/** Prints out a exception error message if the _expr expression fails.  Disabled
 * if DEBUG is not enabled. */
#define COMIP_HSIC_ASSERT(_expr, _format, _args...) do { \
    if (!(_expr)) { COMIP_HSIC_EXCEPTION("%s:%s:%d: " _format "\n", comip_hsic_irq(), \
                      __FILE__, __LINE__, ## _args); } \
    } while (0)
#else
#define COMIP_HSIC_ASSERT(_x...)
#endif
#define comip_hsic_assert COMIP_HSIC_ASSERT


/** @name Byte Ordering
 * The following functions are for conversions between processor's byte ordering
 * and specific ordering you want.
 */

/** Converts 32 bit data in CPU byte ordering to little endian. */
extern uint32_t COMIP_HSIC_CPU_TO_LE32(uint32_t *p);
#define comip_hsic_cpu_to_le32 COMIP_HSIC_CPU_TO_LE32

/** Converts 32 bit data in CPU byte orderint to big endian. */
extern uint32_t COMIP_HSIC_CPU_TO_BE32(uint32_t *p);
#define comip_hsic_cpu_to_be32 COMIP_HSIC_CPU_TO_BE32

/** Converts 32 bit little endian data to CPU byte ordering. */
extern uint32_t COMIP_HSIC_LE32_TO_CPU(uint32_t *p);
#define comip_hsic_le32_to_cpu COMIP_HSIC_LE32_TO_CPU

/** Converts 32 bit big endian data to CPU byte ordering. */
extern uint32_t COMIP_HSIC_BE32_TO_CPU(uint32_t *p);
#define comip_hsic_be32_to_cpu COMIP_HSIC_BE32_TO_CPU

/** Converts 16 bit data in CPU byte ordering to little endian. */
extern uint16_t COMIP_HSIC_CPU_TO_LE16(uint16_t *p);
#define comip_hsic_cpu_to_le16 COMIP_HSIC_CPU_TO_LE16

/** Converts 16 bit data in CPU byte orderint to big endian. */
extern uint16_t COMIP_HSIC_CPU_TO_BE16(uint16_t *p);
#define comip_hsic_cpu_to_be16 COMIP_HSIC_CPU_TO_BE16

/** Converts 16 bit little endian data to CPU byte ordering. */
extern uint16_t COMIP_HSIC_LE16_TO_CPU(uint16_t *p);
#define comip_hsic_le16_to_cpu COMIP_HSIC_LE16_TO_CPU

/** Converts 16 bit bi endian data to CPU byte ordering. */
extern uint16_t COMIP_HSIC_BE16_TO_CPU(uint16_t *p);
#define comip_hsic_be16_to_cpu COMIP_HSIC_BE16_TO_CPU


/** @name Register Read/Write
 *
 * The following six functions should be implemented to read/write registers of
 * 32-bit and 64-bit sizes.  All modules use this to read/write register values.
 * The reg value is a pointer to the register calculated from the void *base
 * variable passed into the driver when it is started.  */

#ifdef COMIP_HSIC_LINUX
/* Linux doesn't need any extra parameters for register read/write, so we
 * just throw away the IO context parameter.
 */
 
/** Writes to a 32-bit register. */
extern void COMIP_HSIC_WRITE_REG32(uint32_t volatile *reg, uint32_t value);
#define comip_hsic_write_reg32(_ctx_,_reg_,_val_) COMIP_HSIC_WRITE_REG32(_reg_, _val_)
/**
 * Modify bit values in a register.  Using the
 * algorithm: (reg_contents & ~clear_mask) | set_mask.
 */
extern void COMIP_HSIC_MODIFY_REG32(uint32_t volatile *reg, uint32_t clear_mask, uint32_t set_mask);
#define comip_hsic_modify_reg32(_ctx_,_reg_,_cmsk_,_smsk_) COMIP_HSIC_MODIFY_REG32(_reg_,_cmsk_,_smsk_)
extern void COMIP_HSIC_MODIFY_REG64(uint64_t volatile *reg, uint64_t clear_mask, uint64_t set_mask);
#define comip_hsic_modify_reg64(_ctx_,_reg_,_cmsk_,_smsk_) COMIP_HSIC_MODIFY_REG64(_reg_,_cmsk_,_smsk_)

#endif  /* COMIP_HSIC_LINUX */

#if defined(COMIP_HSIC_FREEBSD) || defined(COMIP_HSIC_NETBSD)
typedef struct comip_hsic_ioctx {
    struct device *dev;
    bus_space_tag_t iot;
    bus_space_handle_t ioh;
} comip_hsic_ioctx_t;

/** BSD needs two extra parameters for register read/write, so we pass
 * them in using the IO context parameter.
 */
/** Reads the content of a 32-bit register. */
extern uint32_t readl(void *io_ctx, uint32_t volatile *reg);
#define comip_hsic_read_reg32 readl

/** Reads the content of a 64-bit register. */
extern uint64_t COMIP_HSIC_READ_REG64(void *io_ctx, uint64_t volatile *reg);
#define comip_hsic_read_reg64 COMIP_HSIC_READ_REG64

/** Writes to a 32-bit register. */
extern void COMIP_HSIC_WRITE_REG32(void *io_ctx, uint32_t volatile *reg, uint32_t value);
#define comip_hsic_write_reg32 writel

/** Writes to a 64-bit register. */
extern void COMIP_HSIC_WRITE_REG64(void *io_ctx, uint64_t volatile *reg, uint64_t value);
#define comip_hsic_write_reg64 COMIP_HSIC_WRITE_REG64

/**
 * Modify bit values in a register.  Using the
 * algorithm: (reg_contents & ~clear_mask) | set_mask.
 */
extern void COMIP_HSIC_MODIFY_REG32(void *io_ctx, uint32_t volatile *reg, uint32_t clear_mask, uint32_t set_mask);
#define comip_hsic_modify_reg32 COMIP_HSIC_MODIFY_REG32
extern void COMIP_HSIC_MODIFY_REG64(void *io_ctx, uint64_t volatile *reg, uint64_t clear_mask, uint64_t set_mask);
#define comip_hsic_modify_reg64 COMIP_HSIC_MODIFY_REG64

#endif  /* COMIP_HSIC_FREEBSD || COMIP_HSIC_NETBSD */

/** @cond */

/** @name Some convenience MACROS used internally.  Define COMIP_HSIC_DEBUG_REGS to log the
 * register writes. */

#ifdef COMIP_HSIC_LINUX

# ifdef COMIP_HSIC_DEBUG_REGS

#define comip_hsic_define_read_write_reg_n(_reg,_container_type) \
static inline uint32_t comip_hsic_read_##_reg##_n(_container_type *container, int num) { \
    return readl(&container->regs->_reg[num]); \
} \
static inline void comip_hsic_write_##_reg##_n(_container_type *container, int num, uint32_t data) { \
    COMIP_HSIC_DEBUG("WRITING %8s[%d]: %p: %08x", #_reg, num, \
          &(((uint32_t*)container->regs->_reg)[num]), data); \
    COMIP_HSIC_WRITE_REG32(&(((uint32_t*)container->regs->_reg)[num]), data); \
}

#define comip_hsic_define_read_write_reg(_reg,_container_type) \
static inline uint32_t comip_hsic_read_##_reg(_container_type *container) { \
    return readl(&container->regs->_reg); \
} \
static inline void comip_hsic_write_##_reg(_container_type *container, uint32_t data) { \
    COMIP_HSIC_DEBUG("WRITING %11s: %p: %08x", #_reg, &container->regs->_reg, data); \
    COMIP_HSIC_WRITE_REG32(&container->regs->_reg, data); \
}

# else  /* COMIP_HSIC_DEBUG_REGS */

#define comip_hsic_define_read_write_reg_n(_reg,_container_type) \
static inline uint32_t comip_hsic_read_##_reg##_n(_container_type *container, int num) { \
    return readl(&container->regs->_reg[num]); \
} \
static inline void comip_hsic_write_##_reg##_n(_container_type *container, int num, uint32_t data) { \
    COMIP_HSIC_WRITE_REG32(&(((uint32_t*)container->regs->_reg)[num]), data); \
}

#define comip_hsic_define_read_write_reg(_reg,_container_type) \
static inline uint32_t comip_hsic_read_##_reg(_container_type *container) { \
    return readl(&container->regs->_reg); \
} \
static inline void comip_hsic_write_##_reg(_container_type *container, uint32_t data) { \
    COMIP_HSIC_WRITE_REG32(&container->regs->_reg, data); \
}

# endif /* COMIP_HSIC_DEBUG_REGS */

#endif  /* COMIP_HSIC_LINUX */

#if defined(COMIP_HSIC_FREEBSD) || defined(COMIP_HSIC_NETBSD)

# ifdef COMIP_HSIC_DEBUG_REGS

#define comip_hsic_define_read_write_reg_n(_reg,_container_type) \
static inline uint32_t comip_hsic_read_##_reg##_n(void *io_ctx, _container_type *container, int num) { \
    return readl(io_ctx, &container->regs->_reg[num]); \
} \
static inline void comip_hsic_write_##_reg##_n(void *io_ctx, _container_type *container, int num, uint32_t data) { \
    COMIP_HSIC_DEBUG("WRITING %8s[%d]: %p: %08x", #_reg, num, \
          &(((uint32_t*)container->regs->_reg)[num]), data); \
    COMIP_HSIC_WRITE_REG32(io_ctx, &(((uint32_t*)container->regs->_reg)[num]), data); \
}

#define comip_hsic_define_read_write_reg(_reg,_container_type) \
static inline uint32_t comip_hsic_read_##_reg(void *io_ctx, _container_type *container) { \
    return readl(io_ctx, &container->regs->_reg); \
} \
static inline void comip_hsic_write_##_reg(void *io_ctx, _container_type *container, uint32_t data) { \
    COMIP_HSIC_DEBUG("WRITING %11s: %p: %08x", #_reg, &container->regs->_reg, data); \
    COMIP_HSIC_WRITE_REG32(io_ctx, &container->regs->_reg, data); \
}

# else  /* COMIP_HSIC_DEBUG_REGS */

#define comip_hsic_define_read_write_reg_n(_reg,_container_type) \
static inline uint32_t comip_hsic_read_##_reg##_n(void *io_ctx, _container_type *container, int num) { \
    return readl(io_ctx, &container->regs->_reg[num]); \
} \
static inline void comip_hsic_write_##_reg##_n(void *io_ctx, _container_type *container, int num, uint32_t data) { \
    COMIP_HSIC_WRITE_REG32(io_ctx, &(((uint32_t*)container->regs->_reg)[num]), data); \
}

#define comip_hsic_define_read_write_reg(_reg,_container_type) \
static inline uint32_t comip_hsic_read_##_reg(void *io_ctx, _container_type *container) { \
    return readl(io_ctx, &container->regs->_reg); \
} \
static inline void comip_hsic_write_##_reg(void *io_ctx, _container_type *container, uint32_t data) { \
    COMIP_HSIC_WRITE_REG32(io_ctx, &container->regs->_reg, data); \
}

# endif /* COMIP_HSIC_DEBUG_REGS */

#endif  /* COMIP_HSIC_FREEBSD || COMIP_HSIC_NETBSD */

/** @endcond */


#ifdef COMIP_HSIC_CRYPTOLIB
/** @name Crypto Functions
 *
 * These are the low-level cryptographic functions used by the driver. */

/** Perform AES CBC */
extern int COMIP_HSIC_AES_CBC(uint8_t *message, uint32_t messagelen, uint8_t *key, uint32_t keylen, uint8_t iv[16], uint8_t *out);
#define comip_hsic_aes_cbc COMIP_HSIC_AES_CBC

/** Fill the provided buffer with random bytes.  These should be cryptographic grade random numbers. */
extern void COMIP_HSIC_RANDOM_BYTES(uint8_t *buffer, uint32_t length);
#define comip_hsic_random_bytes COMIP_HSIC_RANDOM_BYTES

/** Perform the SHA-256 hash function */
extern int COMIP_HSIC_SHA256(uint8_t *message, uint32_t len, uint8_t *out);
#define comip_hsic_sha256 COMIP_HSIC_SHA256

/** Calculated the HMAC-SHA256 */
extern int COMIP_HSIC_HMAC_SHA256(uint8_t *message, uint32_t messagelen, uint8_t *key, uint32_t keylen, uint8_t *out);
#define comip_hsic_hmac_sha256 COMIP_HSIC_HMAC_SHA256

#endif  /* COMIP_HSIC_CRYPTOLIB */


/** @name Memory Allocation
 *
 * These function provide access to memory allocation.  There are only 2 DMA
 * functions and 3 Regular memory functions that need to be implemented.  None
 * of the memory debugging routines need to be implemented.  The allocation
 * routines all ZERO the contents of the memory.
 *
 * Defining COMIP_HSIC_DEBUG_MEMORY turns on memory debugging and statistic gathering.
 * This checks for memory leaks, keeping track of alloc/free pairs.  It also
 * keeps track of how much memory the driver is using at any given time. */

#define COMIP_HSIC_PAGE_SIZE 4096
#define COMIP_HSIC_PAGE_OFFSET(addr) (((uint32_t)addr) & 0xfff)
#define COMIP_HSIC_PAGE_ALIGNED(addr) ((((uint32_t)addr) & 0xfff) == 0)

#define COMIP_HSIC_INVALID_DMA_ADDR 0x0

#ifdef COMIP_HSIC_LINUX
/** Type for a DMA address */
typedef dma_addr_t comip_hsic_dma_t;
#endif

#if defined(COMIP_HSIC_FREEBSD) || defined(COMIP_HSIC_NETBSD)
typedef bus_addr_t comip_hsic_dma_t;
#endif

#ifdef COMIP_HSIC_FREEBSD
typedef struct comip_hsic_dmactx {
    struct device *dev;
    bus_dma_tag_t dma_tag;
    bus_dmamap_t dma_map;
    bus_addr_t dma_paddr;
    void *dma_vaddr;
} comip_hsic_dmactx_t;
#endif

#ifdef COMIP_HSIC_NETBSD
typedef struct comip_hsic_dmactx {
    struct device *dev;
    bus_dma_tag_t dma_tag;
    bus_dmamap_t dma_map;
    bus_dma_segment_t segs[1];
    int nsegs;
    bus_addr_t dma_paddr;
    void *dma_vaddr;
} comip_hsic_dmactx_t;
#endif

/** Allocates a DMA capable buffer and zeroes its contents. */
extern void *__COMIP_HSIC_DMA_ALLOC(void *dma_ctx, uint32_t size, comip_hsic_dma_t *dma_addr);

/** Allocates a DMA capable buffer and zeroes its contents in atomic contest */
extern void *__COMIP_HSIC_DMA_ALLOC_ATOMIC(void *dma_ctx, uint32_t size, comip_hsic_dma_t *dma_addr);

/** Frees a previously allocated buffer. */
extern void __COMIP_HSIC_DMA_FREE(void *dma_ctx, uint32_t size, void *virt_addr, comip_hsic_dma_t dma_addr);

/** Allocates a block of memory and zeroes its contents. */
extern void *__COMIP_HSIC_ALLOC(void *mem_ctx, uint32_t size);

/** Allocates a block of memory and zeroes its contents, in an atomic manner
 * which can be used inside interrupt context.  The size should be sufficiently
 * small, a few KB at most, such that failures are not likely to occur.  Can just call
 * __COMIP_HSIC_ALLOC if it is atomic. */
extern void *__COMIP_HSIC_ALLOC_ATOMIC(void *mem_ctx, uint32_t size);

/** Frees a previously allocated buffer. */
extern void __COMIP_HSIC_FREE(void *mem_ctx, void *addr);

#ifndef COMIP_HSIC_DEBUG_MEMORY

#define COMIP_HSIC_ALLOC(_size_) __COMIP_HSIC_ALLOC(NULL, _size_)
#define COMIP_HSIC_ALLOC_ATOMIC(_size_) __COMIP_HSIC_ALLOC_ATOMIC(NULL, _size_)
#define COMIP_HSIC_FREE(_addr_) __COMIP_HSIC_FREE(NULL, _addr_)

# ifdef COMIP_HSIC_LINUX
#define COMIP_HSIC_DMA_ALLOC(_size_,_dma_) __COMIP_HSIC_DMA_ALLOC(NULL, _size_, _dma_)
#define COMIP_HSIC_DMA_ALLOC_ATOMIC(_size_,_dma_) __COMIP_HSIC_DMA_ALLOC_ATOMIC(NULL, _size_,_dma_)
#define COMIP_HSIC_DMA_FREE(_size_,_virt_,_dma_) __COMIP_HSIC_DMA_FREE(NULL, _size_, _virt_, _dma_)
# endif

# if defined(COMIP_HSIC_FREEBSD) || defined(COMIP_HSIC_NETBSD)
#define COMIP_HSIC_DMA_ALLOC __COMIP_HSIC_DMA_ALLOC
#define COMIP_HSIC_DMA_FREE __COMIP_HSIC_DMA_FREE
# endif

#else   /* COMIP_HSIC_DEBUG_MEMORY */

extern void *comip_hsic_alloc_debug(void *mem_ctx, uint32_t size, char const *func, int line);
extern void *comip_hsic_alloc_atomic_debug(void *mem_ctx, uint32_t size, char const *func, int line);
extern void comip_hsic_free_debug(void *mem_ctx, void *addr, char const *func, int line);
extern void *comip_hsic_dma_alloc_debug(void *dma_ctx, uint32_t size, comip_hsic_dma_t *dma_addr,
                 char const *func, int line);
extern void *comip_hsic_dma_alloc_atomic_debug(void *dma_ctx, uint32_t size, comip_hsic_dma_t *dma_addr, 
                char const *func, int line);
extern void comip_hsic_dma_free_debug(void *dma_ctx, uint32_t size, void *virt_addr,
                   comip_hsic_dma_t dma_addr, char const *func, int line);

extern int comip_hsic_memory_debug_start(void *mem_ctx);
extern void comip_hsic_memory_debug_stop(void);
extern void comip_hsic_memory_debug_report(void);

#define COMIP_HSIC_ALLOC(_size_) comip_hsic_alloc_debug(NULL, _size_, __func__, __LINE__)
#define COMIP_HSIC_ALLOC_ATOMIC(_size_) comip_hsic_alloc_atomic_debug(NULL, _size_, \
                            __func__, __LINE__)
#define COMIP_HSIC_FREE(_addr_) comip_hsic_free_debug(NULL, _addr_, __func__, __LINE__)

# ifdef COMIP_HSIC_LINUX
#define COMIP_HSIC_DMA_ALLOC(_size_,_dma_) comip_hsic_dma_alloc_debug(NULL, _size_, \
                        _dma_, __func__, __LINE__)
#define COMIP_HSIC_DMA_ALLOC_ATOMIC(_size_,_dma_) comip_hsic_dma_alloc_atomic_debug(NULL, _size_, \
                        _dma_, __func__, __LINE__)
#define COMIP_HSIC_DMA_FREE(_size_,_virt_,_dma_) comip_hsic_dma_free_debug(NULL, _size_, \
                        _virt_, _dma_, __func__, __LINE__)
# endif

# if defined(COMIP_HSIC_FREEBSD) || defined(COMIP_HSIC_NETBSD)
#define COMIP_HSIC_DMA_ALLOC(_ctx_,_size_,_dma_) comip_hsic_dma_alloc_debug(_ctx_, _size_, \
                        _dma_, __func__, __LINE__)
#define COMIP_HSIC_DMA_FREE(_ctx_,_size_,_virt_,_dma_) comip_hsic_dma_free_debug(_ctx_, _size_, \
                         _virt_, _dma_, __func__, __LINE__)
# endif

#endif /* COMIP_HSIC_DEBUG_MEMORY */

#define comip_hsic_alloc(_ctx_,_size_) COMIP_HSIC_ALLOC(_size_)
#define comip_hsic_alloc_atomic(_ctx_,_size_) COMIP_HSIC_ALLOC_ATOMIC(_size_)
#define comip_hsic_free(_ctx_,_addr_) COMIP_HSIC_FREE(_addr_)

#ifdef COMIP_HSIC_LINUX
/* Linux doesn't need any extra parameters for DMA buffer allocation, so we
 * just throw away the DMA context parameter.
 */
#define comip_hsic_dma_alloc(_ctx_,_size_,_dma_) COMIP_HSIC_DMA_ALLOC(_size_, _dma_)
#define comip_hsic_dma_alloc_atomic(_ctx_,_size_,_dma_) COMIP_HSIC_DMA_ALLOC_ATOMIC(_size_, _dma_)
#define comip_hsic_dma_free(_ctx_,_size_,_virt_,_dma_) COMIP_HSIC_DMA_FREE(_size_, _virt_, _dma_)
#endif

#if defined(COMIP_HSIC_FREEBSD) || defined(COMIP_HSIC_NETBSD)
/** BSD needs several extra parameters for DMA buffer allocation, so we pass
 * them in using the DMA context parameter.
 */
#define comip_hsic_dma_alloc COMIP_HSIC_DMA_ALLOC
#define comip_hsic_dma_free COMIP_HSIC_DMA_FREE
#endif


/** @name Memory and String Processing */

/** strlen() clone, for NULL terminated ASCII strings */
extern int COMIP_HSIC_STRLEN(char const *str);
#define comip_hsic_strlen COMIP_HSIC_STRLEN

/** strcpy() clone, for NULL terminated ASCII strings */
extern char *COMIP_HSIC_STRCPY(char *to, const char *from);
#define comip_hsic_strcpy COMIP_HSIC_STRCPY

/** strdup() clone.  If you wish to use memory allocation debugging, this
 * implementation of strdup should use the COMIP_HSIC_* memory routines instead of
 * calling a predefined strdup.  Otherwise the memory allocated by this routine
 * will not be seen by the debugging routines. */
extern char *COMIP_HSIC_STRDUP(char const *str);
#define comip_hsic_strdup(_ctx_,_str_) COMIP_HSIC_STRDUP(_str_)

/** NOT an atoi() clone.  Read the description carefully.  Returns an integer
 * converted from the string str in base 10 unless the string begins with a "0x"
 * in which case it is base 16.  String must be a NULL terminated sequence of
 * ASCII characters and may optionally begin with whitespace, a + or -, and a
 * "0x" prefix if base 16.  The remaining characters must be valid digits for
 * the number and end with a NULL character.  If any invalid characters are
 * encountered or it returns with a negative error code and the results of the
 * conversion are undefined.  On sucess it returns 0.  Overflow conditions are
 * undefined.  An example implementation using atoi() can be referenced from the
 * Linux implementation. */


/** @name Wait queues
 *
 * Wait queues provide a means of synchronizing between threads or processes.  A
 * process can block on a waitq if some condition is not true, waiting for it to
 * become true.  When the waitq is triggered all waiting process will get
 * unblocked and the condition will be check again.  Waitqs should be triggered
 * every time a condition can potentially change.*/
struct comip_hsic_waitq;

/** Type for a waitq */
typedef struct comip_hsic_waitq comip_hsic_waitq_t;

/** The type of waitq condition callback function.  This is called every time
 * condition is evaluated. */
typedef int (*comip_hsic_waitq_condition_t)(void *data);

/** Allocate a waitq */
extern comip_hsic_waitq_t *COMIP_HSIC_WAITQ_ALLOC(void);
#define comip_hsic_waitq_alloc(_ctx_) COMIP_HSIC_WAITQ_ALLOC()

/** Free a waitq */
extern void COMIP_HSIC_WAITQ_FREE(comip_hsic_waitq_t *wq);
#define comip_hsic_waitq_free COMIP_HSIC_WAITQ_FREE

/** Check the condition and if it is false, block on the waitq.  When unblocked, check the
 * condition again.  The function returns when the condition becomes true.  The return value
 * is 0 on condition true, COMIP_HSIC_WAITQ_ABORTED on abort or killed, or COMIP_HSIC_WAITQ_UNKNOWN on error. */
extern int32_t COMIP_HSIC_WAITQ_WAIT(comip_hsic_waitq_t *wq, comip_hsic_waitq_condition_t cond, void *data);
#define comip_hsic_waitq_wait COMIP_HSIC_WAITQ_WAIT

/** Check the condition and if it is false, block on the waitq.  When unblocked,
 * check the condition again.  The function returns when the condition become
 * true or the timeout has passed.  The return value is 0 on condition true or
 * COMIP_HSIC_TIMED_OUT on timeout, or COMIP_HSIC_WAITQ_ABORTED, or COMIP_HSIC_WAITQ_UNKNOWN on
 * error. */
extern int32_t COMIP_HSIC_WAITQ_WAIT_TIMEOUT(comip_hsic_waitq_t *wq, comip_hsic_waitq_condition_t cond,
                      void *data, int32_t msecs);
#define comip_hsic_waitq_wait_timeout COMIP_HSIC_WAITQ_WAIT_TIMEOUT

/** Trigger a waitq, unblocking all processes.  This should be called whenever a condition
 * has potentially changed. */
extern void COMIP_HSIC_WAITQ_TRIGGER(comip_hsic_waitq_t *wq);
#define comip_hsic_waitq_trigger COMIP_HSIC_WAITQ_TRIGGER

/** Unblock all processes waiting on the waitq with an ABORTED result. */
extern void COMIP_HSIC_WAITQ_ABORT(comip_hsic_waitq_t *wq);
#define comip_hsic_waitq_abort COMIP_HSIC_WAITQ_ABORT


/** @name Threads
 *
 * A thread must be explicitly stopped.  It must check COMIP_HSIC_THREAD_SHOULD_STOP
 * whenever it is woken up, and then return.  The COMIP_HSIC_THREAD_STOP function
 * returns the value from the thread.
 */

struct comip_hsic_thread;

/** Type for a thread */
typedef struct comip_hsic_thread comip_hsic_thread_t;

/** The thread function */
typedef int (*comip_hsic_thread_function_t)(void *data);

/** Create a thread and start it running the thread_function.  Returns a handle
 * to the thread */
extern comip_hsic_thread_t *COMIP_HSIC_THREAD_RUN(comip_hsic_thread_function_t func, char *name, void *data);
#define comip_hsic_thread_run(_ctx_,_func_,_name_,_data_) COMIP_HSIC_THREAD_RUN(_func_, _name_, _data_)

/** Stops a thread.  Return the value returned by the thread.  Or will return
 * COMIP_HSIC_ABORT if the thread never started. */
extern int COMIP_HSIC_THREAD_STOP(comip_hsic_thread_t *thread);
#define comip_hsic_thread_stop COMIP_HSIC_THREAD_STOP

/** Signifies to the thread that it must stop. */
#ifdef COMIP_HSIC_LINUX
/* Linux doesn't need any parameters for kthread_should_stop() */
extern comip_hsic_bool_t COMIP_HSIC_THREAD_SHOULD_STOP(void);
#define comip_hsic_thread_should_stop(_thrd_) COMIP_HSIC_THREAD_SHOULD_STOP()

/* No thread_exit function in Linux */
#define comip_hsic_thread_exit(_thrd_)
#endif

#if defined(COMIP_HSIC_FREEBSD) || defined(COMIP_HSIC_NETBSD)
/** BSD needs the thread pointer for kthread_suspend_check() */
extern comip_hsic_bool_t COMIP_HSIC_THREAD_SHOULD_STOP(comip_hsic_thread_t *thread);
#define comip_hsic_thread_should_stop COMIP_HSIC_THREAD_SHOULD_STOP

/** The thread must call this to exit. */
extern void COMIP_HSIC_THREAD_EXIT(comip_hsic_thread_t *thread);
#define comip_hsic_thread_exit COMIP_HSIC_THREAD_EXIT
#endif


/** @name Work queues
 *
 * Workqs are used to queue a callback function to be called at some later time,
 * in another thread. */
struct comip_hsic_workq;

/** Type for a workq */
typedef struct comip_hsic_workq comip_hsic_workq_t;

/** The type of the callback function to be called. */
typedef void (*comip_hsic_work_callback_t)(void *data);

/** Allocate a workq */
extern comip_hsic_workq_t *COMIP_HSIC_WORKQ_ALLOC(char *name);
#define comip_hsic_workq_alloc(_ctx_,_name_) COMIP_HSIC_WORKQ_ALLOC(_name_)

/** Free a workq.  All work must be completed before being freed. */
extern void COMIP_HSIC_WORKQ_FREE(comip_hsic_workq_t *workq);
#define comip_hsic_workq_free COMIP_HSIC_WORKQ_FREE

/** Schedule a callback on the workq, passing in data.  The function will be
 * scheduled at some later time. */
extern void COMIP_HSIC_WORKQ_SCHEDULE(comip_hsic_workq_t *workq, comip_hsic_work_callback_t cb,
                   void *data, char *format, ...)
#ifdef __GNUC__
    __attribute__ ((format(printf, 4, 5)));
#else
    ;
#endif
#define comip_hsic_workq_schedule COMIP_HSIC_WORKQ_SCHEDULE

/** Schedule a callback on the workq, that will be called until at least
 * given number miliseconds have passed. */
extern void COMIP_HSIC_WORKQ_SCHEDULE_DELAYED(comip_hsic_workq_t *workq, comip_hsic_work_callback_t cb,
                       void *data, uint32_t time, char *format, ...)
#ifdef __GNUC__
    __attribute__ ((format(printf, 5, 6)));
#else
    ;
#endif
#define comip_hsic_workq_schedule_delayed COMIP_HSIC_WORKQ_SCHEDULE_DELAYED

/** The number of processes in the workq */
extern int COMIP_HSIC_WORKQ_PENDING(comip_hsic_workq_t *workq);
#define comip_hsic_workq_pending COMIP_HSIC_WORKQ_PENDING

/** Blocks until all the work in the workq is complete or timed out.  Returns <
 * 0 on timeout. */
extern int COMIP_HSIC_WORKQ_WAIT_WORK_DONE(comip_hsic_workq_t *workq, int timeout);
#define comip_hsic_workq_wait_work_done COMIP_HSIC_WORKQ_WAIT_WORK_DONE


/** @name Tasklets
 *
 */
struct comip_hsic_tasklet;

/** Type for a tasklet */
typedef struct comip_hsic_tasklet comip_hsic_tasklet_t;

/** The type of the callback function to be called */
typedef void (*comip_hsic_tasklet_callback_t)(void *data);

/** Allocates a tasklet */
extern comip_hsic_tasklet_t *COMIP_HSIC_TASK_ALLOC(char *name, comip_hsic_tasklet_callback_t cb, void *data);
#define comip_hsic_task_alloc(_ctx_,_name_,_cb_,_data_) COMIP_HSIC_TASK_ALLOC(_name_, _cb_, _data_)

/** Frees a tasklet */
extern void COMIP_HSIC_TASK_FREE(comip_hsic_tasklet_t *task);
#define comip_hsic_task_free COMIP_HSIC_TASK_FREE

/** Schedules a tasklet to run */
extern void COMIP_HSIC_TASK_SCHEDULE(comip_hsic_tasklet_t *task);
#define comip_hsic_task_schedule COMIP_HSIC_TASK_SCHEDULE


/** @name Timer
 *
 * Callbacks must be small and atomic.
 */
struct comip_hsic_timer;

/** Type for a timer */
typedef struct comip_hsic_timer comip_hsic_timer_t;

/** The type of the callback function to be called */
typedef void (*comip_hsic_timer_callback_t)(void *data);

/** Allocates a timer */
extern comip_hsic_timer_t *COMIP_HSIC_TIMER_ALLOC(char *name, comip_hsic_timer_callback_t cb, void *data);
#define comip_hsic_timer_alloc(_ctx_,_name_,_cb_,_data_) COMIP_HSIC_TIMER_ALLOC(_name_,_cb_,_data_)

/** Frees a timer */
extern void COMIP_HSIC_TIMER_FREE(comip_hsic_timer_t *timer);
#define comip_hsic_timer_free COMIP_HSIC_TIMER_FREE

/** Schedules the timer to run at time ms from now.  And will repeat at every
 * repeat_interval msec therafter
 *
 * Modifies a timer that is still awaiting execution to a new expiration time.
 * The mod_time is added to the old time.  */
extern void COMIP_HSIC_TIMER_SCHEDULE(comip_hsic_timer_t *timer, uint32_t time);
#define comip_hsic_timer_schedule COMIP_HSIC_TIMER_SCHEDULE

/** Disables the timer from execution. */
extern void COMIP_HSIC_TIMER_CANCEL(comip_hsic_timer_t *timer);
#define comip_hsic_timer_cancel COMIP_HSIC_TIMER_CANCEL


/** @name Spinlocks
 *
 * These locks are used when the work between the lock/unlock is atomic and
 * short.  Interrupts are also disabled during the lock/unlock and thus they are
 * suitable to lock between interrupt/non-interrupt context.  They also lock
 * between processes if you have multiple CPUs or Preemption.  If you don't have
 * multiple CPUS or Preemption, then the you can simply implement the
 * COMIP_HSIC_SPINLOCK and COMIP_HSIC_SPINUNLOCK to disable and enable interrupts.  Because
 * the work between the lock/unlock is atomic, the process context will never
 * change, and so you never have to lock between processes.  */

struct comip_hsic_spinlock;

/** Type for a spinlock */
typedef struct comip_hsic_spinlock comip_hsic_spinlock_t;

/** Type for the 'flags' argument to spinlock funtions */
typedef unsigned long comip_hsic_irqflags_t;

/** Returns an initialized lock variable.  This function should allocate and
 * initialize the OS-specific data structure used for locking.  This data
 * structure is to be used for the COMIP_HSIC_LOCK and COMIP_HSIC_UNLOCK functions and should
 * be freed by the COMIP_HSIC_FREE_LOCK when it is no longer used. */
extern comip_hsic_spinlock_t *COMIP_HSIC_SPINLOCK_ALLOC(void);
#define comip_hsic_spinlock_alloc(_ctx_) COMIP_HSIC_SPINLOCK_ALLOC()

/** Frees an initialized lock variable. */
extern void COMIP_HSIC_SPINLOCK_FREE(comip_hsic_spinlock_t *lock);
#define comip_hsic_spinlock_free(_ctx_,_lock_) COMIP_HSIC_SPINLOCK_FREE(_lock_)

/** Disables interrupts and blocks until it acquires the lock.
 *
 * @param lock Pointer to the spinlock.
 * @param flags Unsigned long for irq flags storage.
 */
extern void COMIP_HSIC_SPINLOCK_IRQSAVE(comip_hsic_spinlock_t *lock, comip_hsic_irqflags_t *flags);
#define comip_hsic_spinlock_irqsave COMIP_HSIC_SPINLOCK_IRQSAVE

/** Re-enables the interrupt and releases the lock.
 *
 * @param lock Pointer to the spinlock.
 * @param flags Unsigned long for irq flags storage.  Must be the same as was
 * passed into COMIP_HSIC_LOCK.
 */
extern void COMIP_HSIC_SPINUNLOCK_IRQRESTORE(comip_hsic_spinlock_t *lock, comip_hsic_irqflags_t flags);
#define comip_hsic_spinunlock_irqrestore COMIP_HSIC_SPINUNLOCK_IRQRESTORE

/** Blocks until it acquires the lock.
 *
 * @param lock Pointer to the spinlock.
 */
extern void COMIP_HSIC_SPINLOCK(comip_hsic_spinlock_t *lock);
#define comip_hsic_spinlock COMIP_HSIC_SPINLOCK

/** Releases the lock.
 *
 * @param lock Pointer to the spinlock.
 */
extern void COMIP_HSIC_SPINUNLOCK(comip_hsic_spinlock_t *lock);
#define comip_hsic_spinunlock COMIP_HSIC_SPINUNLOCK


/** @name Mutexes
 *
 * Unlike spinlocks Mutexes lock only between processes and the work between the
 * lock/unlock CAN block, therefore it CANNOT be called from interrupt context.
 */

struct comip_hsic_mutex;

/** Type for a mutex */
typedef struct comip_hsic_mutex comip_hsic_mutex_t;

/* For Linux Mutex Debugging make it inline because the debugging routines use
 * the symbol to determine recursive locking.  This makes it falsely think
 * recursive locking occurs. */
#if defined(COMIP_HSIC_LINUX) && defined(CONFIG_DEBUG_MUTEXES)
#define COMIP_HSIC_MUTEX_ALLOC_LINUX_DEBUG(__mutexp) ({ \
    __mutexp = (comip_hsic_mutex_t *)COMIP_HSIC_ALLOC(sizeof(struct mutex)); \
    mutex_init((struct mutex *)__mutexp); \
})
#endif

/** Allocate a mutex */
extern comip_hsic_mutex_t *COMIP_HSIC_MUTEX_ALLOC(void);
#define comip_hsic_mutex_alloc(_ctx_) COMIP_HSIC_MUTEX_ALLOC()

/* For memory leak debugging when using Linux Mutex Debugging */
#if defined(COMIP_HSIC_LINUX) && defined(CONFIG_DEBUG_MUTEXES)
#define COMIP_HSIC_MUTEX_FREE(__mutexp) do { \
    mutex_destroy((struct mutex *)__mutexp); \
    COMIP_HSIC_FREE(__mutexp); \
} while(0)
#else
/** Free a mutex */
extern void COMIP_HSIC_MUTEX_FREE(comip_hsic_mutex_t *mutex);
#define comip_hsic_mutex_free(_ctx_,_mutex_) COMIP_HSIC_MUTEX_FREE(_mutex_)
#endif

/** Lock a mutex */
extern void COMIP_HSIC_MUTEX_LOCK(comip_hsic_mutex_t *mutex);
#define comip_hsic_mutex_lock COMIP_HSIC_MUTEX_LOCK

/** Non-blocking lock returns 1 on successful lock. */
extern int COMIP_HSIC_MUTEX_TRYLOCK(comip_hsic_mutex_t *mutex);
#define comip_hsic_mutex_trylock COMIP_HSIC_MUTEX_TRYLOCK

/** Unlock a mutex */
extern void COMIP_HSIC_MUTEX_UNLOCK(comip_hsic_mutex_t *mutex);
#define comip_hsic_mutex_unlock COMIP_HSIC_MUTEX_UNLOCK


/** @name Time */

/** Non-busy waiting.
 * Sleeps for specified number of milliseconds.
 *
 * @param msecs Milliseconds to sleep.
 */

/**
 * Returns number of milliseconds since boot.
 */
extern uint32_t COMIP_HSIC_TIME(void);
#define comip_hsic_time COMIP_HSIC_TIME




/* @mainpage COMIP Portability and Common Library
 *
 * This is the documentation for the COMIP Portability and Common Library.
 *
 * @section intro Introduction
 *
 * The COMIP Portability library consists of wrapper calls and data structures to
 * all low-level functions which are typically provided by the OS.  The WUDEV
 * driver uses only these functions.  In order to port the WUDEV driver, only
 * the functions in this library need to be re-implemented, with the same
 * behavior as documented here.
 *
 * The Common library consists of higher level functions, which rely only on
 * calling the functions from the COMIP Portability library.  These common
 * routines are shared across modules.  Some of the common libraries need to be
 * used directly by the driver programmer when porting WUDEV.  Such as the
 * parameter and notification libraries.
 *
 * @section low Portability Library OS Wrapper Functions
 *
 * Any function starting with COMIP and in all CAPS is a low-level OS-wrapper that
 * needs to be implemented when porting, for example COMIP_HSIC_MUTEX_ALLOC().  All of
 * these functions are included in the comip_hsic_os.h file.
 *
 * There are many functions here covering a wide array of OS services.  Please
 * see comip_hsic_os.h for details, and implementation notes for each function.
 *
 * @section common Common Library Functions
 *
 * Any function starting with comip and in all lowercase is a common library
 * routine.  These functions have a portable implementation and do not need to
 * be reimplemented when porting.  The common routines can be used by any
 * driver, and some must be used by the end user to control the drivers.  For
 * example, you must use the Parameter common library in order to set the
 * parameters in the WUDEV module.
 *
 * The common libraries consist of the following:
 *
 * - Connection Contexts - Used internally and can be used by end-user.  See comip_hsic_cc.h
 * - Parameters - Used internally and can be used by end-user.  See comip_hsic_params.h
 * - Notifications - Used internally and can be used by end-user.  See comip_hsic_notifier.h
 * - Lists - Used internally and can be used by end-user.  See comip_hsic_list.h
 * - Memory Debugging - Used internally and can be used by end-user.  See comip_hsic_os.h
 * - Modpow - Used internally only.  See comip_hsic_modpow.h
 * - DH - Used internally only.  See comip_hsic_dh.h
 * - Crypto - Used internally only.  See comip_hsic_crypto.h
 *
 *
 * @section prereq Prerequistes For comip_hsic_os.h
 * @subsection types Data Types
 *
 * The comip_hsic_os.h file assumes that several low-level data types are pre defined for the
 * compilation environment.  These data types are:
 *
 * - uint8_t - unsigned 8-bit data type
 * - int8_t - signed 8-bit data type
 * - uint16_t - unsigned 16-bit data type
 * - int16_t - signed 16-bit data type
 * - uint32_t - unsigned 32-bit data type
 * - int32_t - signed 32-bit data type
 * - uint64_t - unsigned 64-bit data type
 * - int64_t - signed 64-bit data type
 *
 * Ensure that these are defined before using comip_hsic_os.h.  The easiest way to do
 * that is to modify the top of the file to include the appropriate header.
 * This is already done for the Linux environment.  If the COMIP_HSIC_LINUX macro is
 * defined, the correct header will be added.  A standard header <stdint.h> is
 * also used for environments where standard C headers are available.
 *
 * @subsection stdarg Variable Arguments
 *
 * Variable arguments are provided by a standard C header <stdarg.h>.  it is
 * available in Both the Linux and ANSI C enviornment.  An equivalent must be
 * provided in your enviornment in order to use comip_hsic_os.h with the debug and
 * tracing message functionality.
 *
 * @subsection thread Threading
 *
 * WUDEV Core must be run on an operating system that provides for multiple
 * threads/processes.  Threading can be implemented in many ways, even in
 * embedded systems without an operating system.  At the bare minimum, the
 * system should be able to start any number of processes at any time to handle
 * special work.  It need not be a pre-emptive system.  Process context can
 * change upon a call to a blocking function.  The hardware interrupt context
 * that calls the module's ISR() function must be differentiable from process
 * context, even if your processes are impemented via a hardware interrupt.
 * Further locking mechanism between process must exist (or be implemented), and
 * process context must have a way to disable interrupts for a period of time to
 * lock them out.  If all of this exists, the functions in comip_hsic_os.h related to
 * threading should be able to be implemented with the defined behavior.
 *
 */

#ifdef __cplusplus
}
#endif

#endif /* _COMIP_HSIC_OS_H_ */
