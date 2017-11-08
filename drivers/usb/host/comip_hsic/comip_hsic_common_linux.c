#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/module.h>
#include <linux/kthread.h>

/* OS-Level Implementations */

/* This is the Linux kernel implementation of the COMIP platform library. */
#include <linux/moduleparam.h>
#include <linux/ctype.h>
#include <linux/crypto.h>
#include <linux/delay.h>
#include <linux/device.h>
#include <linux/dma-mapping.h>
#include <linux/cdev.h>
#include <linux/errno.h>
#include <linux/interrupt.h>
#include <linux/jiffies.h>
#include <linux/list.h>
#include <linux/pci.h>
#include <linux/random.h>
#include <linux/scatterlist.h>
#include <linux/slab.h>
#include <linux/stat.h>
#include <linux/string.h>
#include <linux/timer.h>
#include <linux/usb.h>

#include <linux/version.h>

#if LINUX_VERSION_CODE >= KERNEL_VERSION(2,6,24)
# include <linux/usb/gadget.h>
#else
# include <linux/usb_gadget.h>
#endif

#include <asm/io.h>
#include <asm/page.h>
#include <asm/uaccess.h>
#include <asm/unaligned.h>

#include "comip_hsic_os.h"
#include "comip_hsic_list.h"


/* MISC */
int COMIP_HSIC_STRLEN(char const *str)
{
    return strlen(str);
}

char *COMIP_HSIC_STRCPY(char *to, char const *from)
{
    return strcpy(to, from);
}

char *COMIP_HSIC_STRDUP(char const *str)
{
    int len = COMIP_HSIC_STRLEN(str) + 1;
    char *new = COMIP_HSIC_ALLOC_ATOMIC(len);

    if (!new) {
        return NULL;
    }

    memcpy(new, str, len);
    return new;
}





/* comip_hsic_debug.h */

comip_hsic_bool_t COMIP_HSIC_IN_IRQ(void)
{
    return in_irq();
}

comip_hsic_bool_t COMIP_HSIC_IN_BH(void)
{
    return in_softirq();
}

void COMIP_HSIC_VPRINTF(char *format, va_list args)
{
    vprintk(format, args);
}

int COMIP_HSIC_VSNPRINTF(char *str, int size, char *format, va_list args)
{
    return vsnprintf(str, size, format, args);
}

void COMIP_HSIC_PRINTF(char *format, ...)
{
    va_list args;

    va_start(args, format);
    COMIP_HSIC_VPRINTF(format, args);
    va_end(args);
}

int COMIP_HSIC_SPRINTF(char *buffer, char *format, ...)
{
    int retval;
    va_list args;

    va_start(args, format);
    retval = vsprintf(buffer, format, args);
    va_end(args);
    return retval;
}

int COMIP_HSIC_SNPRINTF(char *buffer, int size, char *format, ...)
{
    int retval;
    va_list args;

    va_start(args, format);
    retval = vsnprintf(buffer, size, format, args);
    va_end(args);
    return retval;
}

void __COMIP_HSIC_WARN(char *format, ...)
{
    va_list args;

    va_start(args, format);
    COMIP_HSIC_PRINTF(KERN_WARNING);
    COMIP_HSIC_VPRINTF(format, args);
    va_end(args);
}

void __COMIP_HSIC_ERROR(char *format, ...)
{
    va_list args;

    va_start(args, format);
    COMIP_HSIC_PRINTF(KERN_ERR);
    COMIP_HSIC_VPRINTF(format, args);
    va_end(args);
}

void COMIP_HSIC_EXCEPTION(char *format, ...)
{
    va_list args;

    va_start(args, format);
    COMIP_HSIC_PRINTF(KERN_ERR);
    COMIP_HSIC_VPRINTF(format, args);
    va_end(args);
    BUG_ON(1);
}

#ifdef DEBUG
void __COMIP_HSIC_DEBUG(char *format, ...)
{
    va_list args;

    va_start(args, format);
    COMIP_HSIC_PRINTF(KERN_DEBUG);
    COMIP_HSIC_VPRINTF(format, args);
    va_end(args);
}
#endif
void *__COMIP_HSIC_DMA_ALLOC(void *dma_ctx, uint32_t size, comip_hsic_dma_t *dma_addr)
{
#ifdef xxCOSIM /* Only works for 32-bit cosim */
    void *buf = dma_alloc_coherent(dma_ctx, (size_t)size, dma_addr, GFP_KERNEL);
#else
    void *buf = dma_alloc_coherent(dma_ctx, (size_t)size, dma_addr, GFP_KERNEL/* | GFP_DMA32*/);
#endif
    if (!buf) {
        return NULL;
    }

    memset(buf, 0, (size_t)size);
    return buf;
}

void *__COMIP_HSIC_DMA_ALLOC_ATOMIC(void *dma_ctx, uint32_t size, comip_hsic_dma_t *dma_addr)
{
    void *buf = dma_alloc_coherent(NULL, (size_t)size, dma_addr, GFP_ATOMIC);
    if (!buf) {
        return NULL;
    }
    memset(buf, 0, (size_t)size);
    return buf;
}

void __COMIP_HSIC_DMA_FREE(void *dma_ctx, uint32_t size, void *virt_addr, comip_hsic_dma_t dma_addr)
{
    dma_free_coherent(dma_ctx, size, virt_addr, dma_addr);
}

void *__COMIP_HSIC_ALLOC(void *mem_ctx, uint32_t size)
{
    return kzalloc(size, GFP_KERNEL);
}

void *__COMIP_HSIC_ALLOC_ATOMIC(void *mem_ctx, uint32_t size)
{
    return kzalloc(size, GFP_ATOMIC);
}

void __COMIP_HSIC_FREE(void *mem_ctx, void *addr)
{
    kfree(addr);
}


#ifdef COMIP_HSIC_CRYPTOLIB
/* comip_hsic_crypto.h */

void COMIP_HSIC_RANDOM_BYTES(uint8_t *buffer, uint32_t length)
{
    get_random_bytes(buffer, length);
}

int COMIP_HSIC_AES_CBC(uint8_t *message, uint32_t messagelen, uint8_t *key, uint32_t keylen, uint8_t iv[16], uint8_t *out)
{
    struct crypto_blkcipher *tfm;
    struct blkcipher_desc desc;
    struct scatterlist sgd;
    struct scatterlist sgs;

    tfm = crypto_alloc_blkcipher("cbc(aes)", 0, CRYPTO_ALG_ASYNC);
    if (tfm == NULL) {
        printk("failed to load transform for aes CBC\n");
        return -1;
    }

    crypto_blkcipher_setkey(tfm, key, keylen);
    crypto_blkcipher_set_iv(tfm, iv, 16);

    sg_init_one(&sgd, out, messagelen);
    sg_init_one(&sgs, message, messagelen);

    desc.tfm = tfm;
    desc.flags = 0;

    if (crypto_blkcipher_encrypt(&desc, &sgd, &sgs, messagelen)) {
        crypto_free_blkcipher(tfm);
        COMIP_HSIC_ERROR("AES CBC encryption failed");
        return -1;
    }

    crypto_free_blkcipher(tfm);
    return 0;
}

int COMIP_HSIC_SHA256(uint8_t *message, uint32_t len, uint8_t *out)
{
    struct crypto_hash *tfm;
    struct hash_desc desc;
    struct scatterlist sg;

    tfm = crypto_alloc_hash("sha256", 0, CRYPTO_ALG_ASYNC);
    if (IS_ERR(tfm)) {
        COMIP_HSIC_ERROR("Failed to load transform for sha256: %ld\n", PTR_ERR(tfm));
        return 0;
    }
    desc.tfm = tfm;
    desc.flags = 0;

    sg_init_one(&sg, message, len);
    crypto_hash_digest(&desc, &sg, len, out);
    crypto_free_hash(tfm);

    return 1;
}

int COMIP_HSIC_HMAC_SHA256(uint8_t *message, uint32_t messagelen,
            uint8_t *key, uint32_t keylen, uint8_t *out)
{
    struct crypto_hash *tfm;
    struct hash_desc desc;
    struct scatterlist sg;

    tfm = crypto_alloc_hash("hmac(sha256)", 0, CRYPTO_ALG_ASYNC);
    if (IS_ERR(tfm)) {
        COMIP_HSIC_ERROR("Failed to load transform for hmac(sha256): %ld\n", PTR_ERR(tfm));
        return 0;
    }
    desc.tfm = tfm;
    desc.flags = 0;

    sg_init_one(&sg, message, messagelen);
    crypto_hash_setkey(tfm, key, keylen);
    crypto_hash_digest(&desc, &sg, messagelen, out);
    crypto_free_hash(tfm);

    return 1;
}
#endif  /* COMIP_HSIC_CRYPTOLIB */


/* Byte Ordering Conversions */

uint32_t COMIP_HSIC_CPU_TO_LE32(uint32_t *p)
{
#ifdef __LITTLE_ENDIAN
    return *p;
#else
    uint8_t *u_p = (uint8_t *)p;

    return (u_p[3] | (u_p[2] << 8) | (u_p[1] << 16) | (u_p[0] << 24));
#endif
}

uint32_t COMIP_HSIC_CPU_TO_BE32(uint32_t *p)
{
#ifdef __BIG_ENDIAN
    return *p;
#else
    uint8_t *u_p = (uint8_t *)p;

    return (u_p[3] | (u_p[2] << 8) | (u_p[1] << 16) | (u_p[0] << 24));
#endif
}

uint32_t COMIP_HSIC_LE32_TO_CPU(uint32_t *p)
{
#ifdef __LITTLE_ENDIAN
    return *p;
#else
    uint8_t *u_p = (uint8_t *)p;

    return (u_p[3] | (u_p[2] << 8) | (u_p[1] << 16) | (u_p[0] << 24));
#endif
}

uint32_t COMIP_HSIC_BE32_TO_CPU(uint32_t *p)
{
#ifdef __BIG_ENDIAN
    return *p;
#else
    uint8_t *u_p = (uint8_t *)p;

    return (u_p[3] | (u_p[2] << 8) | (u_p[1] << 16) | (u_p[0] << 24));
#endif
}

uint16_t COMIP_HSIC_CPU_TO_LE16(uint16_t *p)
{
#ifdef __LITTLE_ENDIAN
    return *p;
#else
    uint8_t *u_p = (uint8_t *)p;
    return (u_p[1] | (u_p[0] << 8));
#endif
}

uint16_t COMIP_HSIC_CPU_TO_BE16(uint16_t *p)
{
#ifdef __BIG_ENDIAN
    return *p;
#else
    uint8_t *u_p = (uint8_t *)p;
    return (u_p[1] | (u_p[0] << 8));
#endif
}

uint16_t COMIP_HSIC_LE16_TO_CPU(uint16_t *p)
{
#ifdef __LITTLE_ENDIAN
    return *p;
#else
    uint8_t *u_p = (uint8_t *)p;
    return (u_p[1] | (u_p[0] << 8));
#endif
}

uint16_t COMIP_HSIC_BE16_TO_CPU(uint16_t *p)
{
#ifdef __BIG_ENDIAN
    return *p;
#else
    uint8_t *u_p = (uint8_t *)p;
    return (u_p[1] | (u_p[0] << 8));
#endif
}

void COMIP_HSIC_WRITE_REG32(uint32_t volatile *reg, uint32_t value)
{
    writel(value, reg);
}


/* Registers */
void COMIP_HSIC_MODIFY_REG32(uint32_t volatile *reg, uint32_t clear_mask, uint32_t set_mask)
{
    writel((readl(reg) & ~clear_mask) | set_mask, reg);
}
/* Locking */

comip_hsic_spinlock_t *COMIP_HSIC_SPINLOCK_ALLOC(void)
{
    spinlock_t *sl = (spinlock_t *)1;

#if defined(CONFIG_PREEMPT) || defined(CONFIG_SMP)
    sl = COMIP_HSIC_ALLOC(sizeof(*sl));
    if (!sl) {
        COMIP_HSIC_ERROR("Cannot allocate memory for spinlock\n");
        return NULL;
    }

    spin_lock_init(sl);
#endif
    return (comip_hsic_spinlock_t *)sl;
}

void COMIP_HSIC_SPINLOCK_FREE(comip_hsic_spinlock_t *lock)
{
#if defined(CONFIG_PREEMPT) || defined(CONFIG_SMP)
    COMIP_HSIC_FREE(lock);
#endif
}

void COMIP_HSIC_SPINLOCK(comip_hsic_spinlock_t *lock)
{
#if defined(CONFIG_PREEMPT) || defined(CONFIG_SMP)
    spin_lock((spinlock_t *)lock);
#endif
}

void COMIP_HSIC_SPINUNLOCK(comip_hsic_spinlock_t *lock)
{
#if defined(CONFIG_PREEMPT) || defined(CONFIG_SMP)
    spin_unlock((spinlock_t *)lock);
#endif
}

void COMIP_HSIC_SPINLOCK_IRQSAVE(comip_hsic_spinlock_t *lock, comip_hsic_irqflags_t *flags)
{
    comip_hsic_irqflags_t f;

#if defined(CONFIG_PREEMPT) || defined(CONFIG_SMP)
    spin_lock_irqsave((spinlock_t *)lock, f);
#else
    local_irq_save(f);
#endif
    *flags = f;
}

void COMIP_HSIC_SPINUNLOCK_IRQRESTORE(comip_hsic_spinlock_t *lock, comip_hsic_irqflags_t flags)
{
#if defined(CONFIG_PREEMPT) || defined(CONFIG_SMP)
    spin_unlock_irqrestore((spinlock_t *)lock, flags);
#else
    local_irq_restore(flags);
#endif
}

comip_hsic_mutex_t *COMIP_HSIC_MUTEX_ALLOC(void)
{
    struct mutex *m;
    comip_hsic_mutex_t *mutex = (comip_hsic_mutex_t *)COMIP_HSIC_ALLOC(sizeof(struct mutex));

    if (!mutex) {
        COMIP_HSIC_ERROR("Cannot allocate memory for mutex\n");
        return NULL;
    }

    m = (struct mutex *)mutex;
    mutex_init(m);
    return mutex;
}

#if (defined(COMIP_HSIC_LINUX) && defined(CONFIG_DEBUG_MUTEXES))
#else
void COMIP_HSIC_MUTEX_FREE(comip_hsic_mutex_t *mutex)
{
    mutex_destroy((struct mutex *)mutex);
    COMIP_HSIC_FREE(mutex);
}
#endif

void COMIP_HSIC_MUTEX_LOCK(comip_hsic_mutex_t *mutex)
{
    struct mutex *m = (struct mutex *)mutex;
    mutex_lock(m);
}

int COMIP_HSIC_MUTEX_TRYLOCK(comip_hsic_mutex_t *mutex)
{
    struct mutex *m = (struct mutex *)mutex;
    return mutex_trylock(m);
}

void COMIP_HSIC_MUTEX_UNLOCK(comip_hsic_mutex_t *mutex)
{
    struct mutex *m = (struct mutex *)mutex;
    mutex_unlock(m);
}


/* Timing */
uint32_t COMIP_HSIC_TIME(void)
{
    return jiffies_to_msecs(jiffies);
}


/* Timers */

struct comip_hsic_timer {
    struct timer_list *t;
    char *name;
    comip_hsic_timer_callback_t cb;
    void *data;
    uint8_t scheduled;
    comip_hsic_spinlock_t *lock;
};

static void timer_callback(unsigned long data)
{
    comip_hsic_timer_t *timer = (comip_hsic_timer_t *)data;
    comip_hsic_irqflags_t flags;

    COMIP_HSIC_SPINLOCK_IRQSAVE(timer->lock, &flags);
    timer->scheduled = 0;
    COMIP_HSIC_SPINUNLOCK_IRQRESTORE(timer->lock, flags);
    COMIP_HSIC_DEBUG("Timer %s callback", timer->name);
    timer->cb(timer->data);
}

comip_hsic_timer_t *COMIP_HSIC_TIMER_ALLOC(char *name, comip_hsic_timer_callback_t cb, void *data)
{
    comip_hsic_timer_t *t = COMIP_HSIC_ALLOC(sizeof(*t));

    if (!t) {
        COMIP_HSIC_ERROR("Cannot allocate memory for timer");
        return NULL;
    }

    t->t = COMIP_HSIC_ALLOC(sizeof(*t->t));
    if (!t->t) {
        COMIP_HSIC_ERROR("Cannot allocate memory for timer->t");
        goto no_timer;
    }

    t->name = COMIP_HSIC_STRDUP(name);
    if (!t->name) {
        COMIP_HSIC_ERROR("Cannot allocate memory for timer->name");
        goto no_name;
    }

    t->lock = COMIP_HSIC_SPINLOCK_ALLOC();
    if (!t->lock) {
        COMIP_HSIC_ERROR("Cannot allocate memory for lock");
        goto no_lock;
    }

    t->scheduled = 0;
    t->t->base = &boot_tvec_bases;
    t->t->expires = jiffies;
    setup_timer(t->t, timer_callback, (uint32_t)t);

    t->cb = cb;
    t->data = data;

    return t;

 no_lock:
    COMIP_HSIC_FREE(t->name);
 no_name:
    COMIP_HSIC_FREE(t->t);
 no_timer:
    COMIP_HSIC_FREE(t);
    return NULL;
}

void COMIP_HSIC_TIMER_FREE(comip_hsic_timer_t *timer)
{
    comip_hsic_irqflags_t flags;

    COMIP_HSIC_SPINLOCK_IRQSAVE(timer->lock, &flags);

    if (timer->scheduled) {
        del_timer(timer->t);
        timer->scheduled = 0;
    }

    COMIP_HSIC_SPINUNLOCK_IRQRESTORE(timer->lock, flags);
    COMIP_HSIC_SPINLOCK_FREE(timer->lock);
    COMIP_HSIC_FREE(timer->t);
    COMIP_HSIC_FREE(timer->name);
    COMIP_HSIC_FREE(timer);
}

void COMIP_HSIC_TIMER_SCHEDULE(comip_hsic_timer_t *timer, uint32_t time)
{
    comip_hsic_irqflags_t flags;

    COMIP_HSIC_SPINLOCK_IRQSAVE(timer->lock, &flags);

    if (!timer->scheduled) {
        timer->scheduled = 1;
        COMIP_HSIC_DEBUG("Scheduling timer %s to expire in +%d msec", timer->name, time);
        timer->t->expires = jiffies + msecs_to_jiffies(time);
        add_timer(timer->t);
    } else {
        COMIP_HSIC_DEBUG("Modifying timer %s to expire in +%d msec", timer->name, time);
        mod_timer(timer->t, jiffies + msecs_to_jiffies(time));
    }

    COMIP_HSIC_SPINUNLOCK_IRQRESTORE(timer->lock, flags);
}

void COMIP_HSIC_TIMER_CANCEL(comip_hsic_timer_t *timer)
{
    del_timer(timer->t);
}


/* Wait Queues */

struct comip_hsic_waitq {
    wait_queue_head_t queue;
    int abort;
};

comip_hsic_waitq_t *COMIP_HSIC_WAITQ_ALLOC(void)
{
    comip_hsic_waitq_t *wq = COMIP_HSIC_ALLOC(sizeof(*wq));

    if (!wq) {
        COMIP_HSIC_ERROR("Cannot allocate memory for waitqueue\n");
        return NULL;
    }

    init_waitqueue_head(&wq->queue);
    wq->abort = 0;
    return wq;
}

void COMIP_HSIC_WAITQ_FREE(comip_hsic_waitq_t *wq)
{
    COMIP_HSIC_FREE(wq);
}

int32_t COMIP_HSIC_WAITQ_WAIT(comip_hsic_waitq_t *wq, comip_hsic_waitq_condition_t cond, void *data)
{
    int result = wait_event_interruptible(wq->queue,
                          cond(data) || wq->abort);
    if (result == -ERESTARTSYS) {
        wq->abort = 0;
        return -ERESTART;
    }

    if (wq->abort == 1) {
        wq->abort = 0;
        return -ECONNABORTED;
    }

    wq->abort = 0;

    if (result == 0) {
        return 0;
    }

    return -EINVAL;
}

int32_t COMIP_HSIC_WAITQ_WAIT_TIMEOUT(comip_hsic_waitq_t *wq, comip_hsic_waitq_condition_t cond,
                   void *data, int32_t msecs)
{
    int32_t tmsecs;
    int result = wait_event_interruptible_timeout(wq->queue,
                              cond(data) || wq->abort,
                              msecs_to_jiffies(msecs));
    if (result == -ERESTARTSYS) {
        wq->abort = 0;
        return -ERESTART;
    }

    if (wq->abort == 1) {
        wq->abort = 0;
        return -ECONNABORTED;
    }

    wq->abort = 0;

    if (result > 0) {
        tmsecs = jiffies_to_msecs(result);
        if (!tmsecs) {
            return 1;
        }

        return tmsecs;
    }

    if (result == 0) {
        return -ETIMEDOUT;
    }

    return -EINVAL;
}

void COMIP_HSIC_WAITQ_TRIGGER(comip_hsic_waitq_t *wq)
{
    wq->abort = 0;
    wake_up_interruptible(&wq->queue);
}

void COMIP_HSIC_WAITQ_ABORT(comip_hsic_waitq_t *wq)
{
    wq->abort = 1;
    wake_up_interruptible(&wq->queue);
}


/* Threading */

comip_hsic_thread_t *COMIP_HSIC_THREAD_RUN(comip_hsic_thread_function_t func, char *name, void *data)
{
    struct task_struct *thread = kthread_run(func, data, name);

    if (thread == ERR_PTR(-ENOMEM)) {
        return NULL;
    }

    return (comip_hsic_thread_t *)thread;
}

int COMIP_HSIC_THREAD_STOP(comip_hsic_thread_t *thread)
{
    return kthread_stop((struct task_struct *)thread);
}

comip_hsic_bool_t COMIP_HSIC_THREAD_SHOULD_STOP(void)
{
    return kthread_should_stop();
}


/* tasklets
 - run in interrupt context (cannot sleep)
 - each tasklet runs on a single CPU
 - different tasklets can be running simultaneously on different CPUs
 */
struct comip_hsic_tasklet {
    struct tasklet_struct t;
    comip_hsic_tasklet_callback_t cb;
    void *data;
};

static void tasklet_callback(unsigned long data)
{
    comip_hsic_tasklet_t *t = (comip_hsic_tasklet_t *)data;
    t->cb(t->data);
}

comip_hsic_tasklet_t *COMIP_HSIC_TASK_ALLOC(char *name, comip_hsic_tasklet_callback_t cb, void *data)
{
    comip_hsic_tasklet_t *t = COMIP_HSIC_ALLOC(sizeof(*t));

    if (t) {
        t->cb = cb;
        t->data = data;
        tasklet_init(&t->t, tasklet_callback, (uint32_t)t);
    } else {
        COMIP_HSIC_ERROR("Cannot allocate memory for tasklet\n");
    }

    return t;
}

void COMIP_HSIC_TASK_FREE(comip_hsic_tasklet_t *task)
{
    COMIP_HSIC_FREE(task);
}

void COMIP_HSIC_TASK_SCHEDULE(comip_hsic_tasklet_t *task)
{
    tasklet_schedule(&task->t);
}


/* workqueues
 - run in process context (can sleep)
 */
typedef struct work_container {
    comip_hsic_work_callback_t cb;
    void *data;
    comip_hsic_workq_t *wq;
    char *name;

#ifdef DEBUG
    COMIP_HSIC_CIRCLEQ_ENTRY(work_container) entry;
#endif
    struct delayed_work work;
} work_container_t;

#ifdef DEBUG
COMIP_HSIC_CIRCLEQ_HEAD(work_container_queue, work_container);
#endif

struct comip_hsic_workq {
    struct workqueue_struct *wq;
    comip_hsic_spinlock_t *lock;
    comip_hsic_waitq_t *waitq;
    int pending;

#ifdef DEBUG
    struct work_container_queue entries;
#endif
};

static void do_work(struct work_struct *work)
{
    comip_hsic_irqflags_t flags;
    struct delayed_work *dw = container_of(work, struct delayed_work, work);
    work_container_t *container = container_of(dw, struct work_container, work);
    comip_hsic_workq_t *wq = container->wq;

    container->cb(container->data);

#ifdef DEBUG
    COMIP_HSIC_CIRCLEQ_REMOVE(&wq->entries, container, entry);
#endif
    COMIP_HSIC_DEBUG("Work done: %s, container=%p", container->name, container);
    if (container->name) {
        COMIP_HSIC_FREE(container->name);
    }
    COMIP_HSIC_FREE(container);

    COMIP_HSIC_SPINLOCK_IRQSAVE(wq->lock, &flags);
    wq->pending--;
    COMIP_HSIC_SPINUNLOCK_IRQRESTORE(wq->lock, flags);
    COMIP_HSIC_WAITQ_TRIGGER(wq->waitq);
}

static int work_done(void *data)
{
    comip_hsic_workq_t *workq = (comip_hsic_workq_t *)data;
    return workq->pending == 0;
}

int COMIP_HSIC_WORKQ_WAIT_WORK_DONE(comip_hsic_workq_t *workq, int timeout)
{
    return COMIP_HSIC_WAITQ_WAIT_TIMEOUT(workq->waitq, work_done, workq, timeout);
}

comip_hsic_workq_t *COMIP_HSIC_WORKQ_ALLOC(char *name)
{
    comip_hsic_workq_t *wq = COMIP_HSIC_ALLOC(sizeof(*wq));

    if (!wq) {
        return NULL;
    }

    wq->wq = create_singlethread_workqueue(name);
    if (!wq->wq) {
        goto no_wq;
    }

    wq->pending = 0;

    wq->lock = COMIP_HSIC_SPINLOCK_ALLOC();
    if (!wq->lock) {
        goto no_lock;
    }

    wq->waitq = COMIP_HSIC_WAITQ_ALLOC();
    if (!wq->waitq) {
        goto no_waitq;
    }

#ifdef DEBUG
    COMIP_HSIC_CIRCLEQ_INIT(&wq->entries);
#endif
    return wq;

 no_waitq:
    COMIP_HSIC_SPINLOCK_FREE(wq->lock);
 no_lock:
    destroy_workqueue(wq->wq);
 no_wq:
    COMIP_HSIC_FREE(wq);

    return NULL;
}

void COMIP_HSIC_WORKQ_FREE(comip_hsic_workq_t *wq)
{
#ifdef DEBUG
    if (wq->pending != 0) {
        struct work_container *wc;
        COMIP_HSIC_ERROR("Destroying work queue with pending work");
        COMIP_HSIC_CIRCLEQ_FOREACH(wc, &wq->entries, entry) {
            COMIP_HSIC_ERROR("Work %s still pending", wc->name);
        }
    }
#endif
    destroy_workqueue(wq->wq);
    COMIP_HSIC_SPINLOCK_FREE(wq->lock);
    COMIP_HSIC_WAITQ_FREE(wq->waitq);
    COMIP_HSIC_FREE(wq);
}

void COMIP_HSIC_WORKQ_SCHEDULE(comip_hsic_workq_t *wq, comip_hsic_work_callback_t cb, void *data,
            char *format, ...)
{
    comip_hsic_irqflags_t flags;
    work_container_t *container;
    static char name[128];
    va_list args;

    va_start(args, format);
    COMIP_HSIC_VSNPRINTF(name, 128, format, args);
    va_end(args);

    COMIP_HSIC_SPINLOCK_IRQSAVE(wq->lock, &flags);
    wq->pending++;
    COMIP_HSIC_SPINUNLOCK_IRQRESTORE(wq->lock, flags);
    COMIP_HSIC_WAITQ_TRIGGER(wq->waitq);

    container = COMIP_HSIC_ALLOC_ATOMIC(sizeof(*container));
    if (!container) {
        COMIP_HSIC_ERROR("Cannot allocate memory for container\n");
        return;
    }

    container->name = COMIP_HSIC_STRDUP(name);
    if (!container->name) {
        COMIP_HSIC_ERROR("Cannot allocate memory for container->name\n");
        COMIP_HSIC_FREE(container);
        return;
    }

    container->cb = cb;
    container->data = data;
    container->wq = wq;
    COMIP_HSIC_DEBUG("Queueing work: %s, container=%p", container->name, container);
    INIT_WORK(&container->work.work, do_work);

#ifdef DEBUG
    COMIP_HSIC_CIRCLEQ_INSERT_TAIL(&wq->entries, container, entry);
#endif
    queue_work(wq->wq, &container->work.work);
}

void COMIP_HSIC_WORKQ_SCHEDULE_DELAYED(comip_hsic_workq_t *wq, comip_hsic_work_callback_t cb,
                void *data, uint32_t time, char *format, ...)
{
    comip_hsic_irqflags_t flags;
    work_container_t *container;
    static char name[128];
    va_list args;

    va_start(args, format);
    COMIP_HSIC_VSNPRINTF(name, 128, format, args);
    va_end(args);

    COMIP_HSIC_SPINLOCK_IRQSAVE(wq->lock, &flags);
    wq->pending++;
    COMIP_HSIC_SPINUNLOCK_IRQRESTORE(wq->lock, flags);
    COMIP_HSIC_WAITQ_TRIGGER(wq->waitq);

    container = COMIP_HSIC_ALLOC_ATOMIC(sizeof(*container));
    if (!container) {
        COMIP_HSIC_ERROR("Cannot allocate memory for container\n");
        return;
    }

    container->name = COMIP_HSIC_STRDUP(name);
    if (!container->name) {
        COMIP_HSIC_ERROR("Cannot allocate memory for container->name\n");
        COMIP_HSIC_FREE(container);
        return;
    }

    container->cb = cb;
    container->data = data;
    container->wq = wq;
    COMIP_HSIC_DEBUG("Queueing work: %s, container=%p", container->name, container);
    INIT_DELAYED_WORK(&container->work, do_work);

#ifdef DEBUG
    COMIP_HSIC_CIRCLEQ_INSERT_TAIL(&wq->entries, container, entry);
#endif
    queue_delayed_work(wq->wq, &container->work, msecs_to_jiffies(time));
}

int COMIP_HSIC_WORKQ_PENDING(comip_hsic_workq_t *wq)
{
    return wq->pending;
}
