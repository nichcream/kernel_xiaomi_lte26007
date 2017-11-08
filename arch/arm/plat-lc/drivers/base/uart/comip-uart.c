/*
 *
 * Use of source code is subject to the terms of the LeadCore license agreement
 * under which you licensed source code. If you did not accept the terms of the
 * license agreement, you are not authorized to use source code. For the terms
 * of the license, please see the license agreement between you and LeadCore.
 *
 * Copyright (c) 2010-2019  LeadCoreTech Corp.
 *
 * PURPOSE: This files contains comip uart driver.
 *
 * CHANGE HISTORY:
 *
 * Version	Date		Author		Description
 * 1.0.0	2012-3-12	lengyansong 	created
 *
 */

#if defined(CONFIG_SERIAL_COMIP_CONSOLE) && defined(CONFIG_MAGIC_SYSRQ)
#define SUPPORT_SYSRQ
#endif

#include <linux/module.h>
#include <linux/ioport.h>
#include <linux/init.h>
#include <linux/console.h>
#include <linux/sysrq.h>
#include <linux/serial_reg.h>
#include <linux/circ_buf.h>
#include <linux/delay.h>
#include <linux/interrupt.h>
#include <linux/platform_device.h>
#include <linux/tty.h>
#include <linux/tty_flip.h>
#include <linux/serial_core.h>
#include <linux/gpio.h>
#include <linux/clk.h>
#include <linux/irq.h>
#include <linux/dma-mapping.h>
#include <linux/hrtimer.h>

#include <asm/io.h>
#include <asm/irq.h>
#include <asm/dma.h>
#include <plat/hardware.h>
#include <plat/uart.h>
#include <plat/dmas.h>

#define COMIP_UART_DEBUG
#ifdef COMIP_UART_DEBUG
#define UART_PRINT(fmt, args...) printk(KERN_ERR "[UART]" fmt, ##args)
#else
#define UART_PRINT(fmt, args...) printk(KERN_DEBUG "[UART]" fmt, ##args)
#endif

/*
 * The Leadcore COMIP on-chip UARTs define these bits
 */
#define UART_USR			31		/* UART Status Register. */
#define UART_USR_BUSY			0x01	/* UART Busy. */
#define UART_LSR_RX_FIFOE		0x80	/* receive FIFO error */
#define UART_MCR_SIRE			0x40	/* Enable IRDA */
#define UART_IER_PTIME			0x80	/* Enable write THRE. */
#define UART_FCR_RX_T1			0x00	/* receive FIFO threshold = 1 */
#define UART_FCR_RX_T4			0x40	/* receive FIFO threshold = 4 */
#define UART_FCR_RX_T8			0x80	/* receive FIFO threshold = 8 */
#define UART_FCR_RX_T14			0xc0	/* receive FIFO threshold = 14 */
#define UART_FCR_TX_T0			0x00	/* send FIFO threshold = 0 */
#define UART_FCR_TX_T2			0x10	/* send FIFO threshold = 2 */
#define UART_FCR_TX_T4			0x20	/* send FIFO threshold = 4 */
#define UART_FCR_TX_T8			0x30	/* send FIFO threshold = 8 */

#define UART_COMIP_NR_PORTS		(4)
#define UART_COMIP_TX_DMA_BUF_SIZE	(UART_XMIT_SIZE)
#define UART_COMIP_RX_DMA_BUF_SIZE	(64 * 1024)
#define UART_COMIP_RX_CHECK_TIMEOUT	(20 * 1000000)	/* 20 ms. */
#define UART_COMIP_RX_DATA_TIMEOUT	(2 * 1000000)	/* 2 ms. */
#define UART_COMIP_WAIT_IDLE_TIMEOUT	(500)	/* ms. */
#define UART_COMIP_TX_DMAS_WAIT_IDLE_TIMEOUT	(500)	/* ms. */
#define UART_COMIP_CLK_NORMAL		(921600 * 16)
#define UART_COMIP_CLK_1M		(1000000 * 16UL)
#define UART_COMIP_CLK_2M		(2000000 * 16UL)
#define UART_COMIP_CLK_3M		(3000000 * 16UL)
#define UART_COMIP_CLK_4M		(4000000 * 16UL)

struct uart_comip_port {
	struct uart_port port;
	struct hrtimer rx_hrtimer;
	unsigned int id;
	unsigned int flags;
	unsigned int ier;
	unsigned char lcr;
	unsigned int mcr;
	unsigned int lsr_break_flag;
	struct clk *clk;
	char name[10];
	struct comip_uart_platform_data *mach;

	unsigned int txdma;
	unsigned int rxdma;
	void *txdma_addr;
	void *rxdma_addr;
	void *rxdma_start_addr;
	dma_addr_t txdma_addr_phys;
	dma_addr_t rxdma_addr_phys;
	u32 rxdma_addr_last;
	struct dmas_ch_cfg dma_rx_cfg;
	struct dmas_ch_cfg dma_tx_cfg;
	int tx_stop;
	int rx_stop;
	struct tasklet_struct tklet;
	struct workqueue_struct *wq;
	struct work_struct buffer_work;
	unsigned int fcr;
};

static inline unsigned int serial_in(struct uart_comip_port *up, int offset)
{
	offset <<= 2;
	return readl(up->port.membase + offset);
}

static inline void serial_out(struct uart_comip_port *up,
			int offset, int value)
{
	offset <<= 2;
	writel(value, up->port.membase + offset);
}

static void serial_comip_wait_for_idle(struct uart_comip_port *up)
{
	unsigned long timeout = jiffies + (msecs_to_jiffies(UART_COMIP_WAIT_IDLE_TIMEOUT));
	unsigned int fcr_clr = UART_FCR_ENABLE_FIFO | UART_FCR_CLEAR_RCVR | UART_FCR_CLEAR_XMIT;

	while (serial_in(up, UART_USR)) {
		if (time_after(jiffies, timeout)){
			serial_out(up, UART_FCR, fcr_clr);
			UART_PRINT("uart%d wait for idle timeout. MSR = 0x%08x\n", up->id, serial_in(up, UART_MSR));
		}
	}

	if (time_after(jiffies, timeout))
		serial_out(up, UART_FCR, up->fcr);
}

static void serial_comip_enable_ms(struct uart_port *port)
{
	struct uart_comip_port *up = (struct uart_comip_port *)port;

	if (!(up->flags &
		(COMIP_UART_SUPPORT_MCTRL | COMIP_UART_RX_USE_DMA | COMIP_UART_TX_USE_DMA)))
		return;

	if (up->flags & COMIP_UART_DISABLE_MSI)
		return;

	up->ier |= UART_IER_MSI;
	serial_out(up, UART_IER, up->ier);
}

static void serial_comip_stop_tx(struct uart_port *port)
{
	struct uart_comip_port *up = (struct uart_comip_port *)port;

	if (up->flags & COMIP_UART_TX_USE_DMA)
		up->tx_stop = 1;
	else {
		if (up->ier & UART_IER_THRI) {
			up->ier &= ~UART_IER_THRI;
			serial_out(up, UART_IER, up->ier);
		}
	}
}

static void serial_comip_stop_rx(struct uart_port *port)
{
	struct uart_comip_port *up = (struct uart_comip_port *)port;

	if (up->flags & COMIP_UART_RX_USE_DMA) {
		comip_dmas_stop(up->rxdma);
		up->rx_stop = 1;
	} else {
		up->ier &= ~UART_IER_RLSI;
		up->port.read_status_mask &= ~UART_LSR_DR;
		serial_out(up, UART_IER, up->ier);
	}
}

static void serial_comip_flip_buffer_push_wq(struct work_struct *work)
{
	struct uart_comip_port *up =
		container_of(work, struct uart_comip_port, buffer_work);
	struct tty_struct *tty = NULL;
	unsigned long flags;

	spin_lock_irqsave(&up->port.lock, flags);
	if (up && up->port.state)
		tty = up->port.state->port.tty;
	spin_unlock_irqrestore(&up->port.lock, flags);

	if (tty) {
		tty->port->low_latency = 1;
		tty_flip_buffer_push(tty->port);
	}
}

static void serial_comip_flip_buffer_push(struct uart_comip_port *up)
{
	struct tty_struct *tty = up->port.state->port.tty;

	if (up->flags & COMIP_UART_USE_WORKQUEUE)
		queue_work(up->wq, &up->buffer_work);
	else
		tty_flip_buffer_push(tty->port);
}

static inline void serial_comip_receive_chars(struct uart_comip_port *up, int *status)
{
	unsigned int ch, flag;
	int max_count = 256;

	do {
		ch = serial_in(up, UART_RX);
		flag = TTY_NORMAL;
		up->port.icount.rx++;

		if (unlikely(*status & (UART_LSR_BI | UART_LSR_PE |
					UART_LSR_FE | UART_LSR_OE))) {
			/*
			 * For statistics only
			 */

			if (*status & UART_LSR_BI) {
				*status &= ~(UART_LSR_FE | UART_LSR_PE);
				up->port.icount.brk++;
				/*
				 * We do the SysRQ and SAK checking
				 * here because otherwise the break
				 * may get masked by ignore_status_mask
				 * or read_status_mask.
				 */
				//if (uart_handle_break(&up->port))
				//	goto ignore_char;
			} else if (*status & UART_LSR_PE)
				up->port.icount.parity++;
			else if (*status & UART_LSR_FE)
				up->port.icount.frame++;
			if (*status & UART_LSR_OE)
				up->port.icount.overrun++;

			/*
			 * Mask off conditions which should be ignored.
			 */
			*status &= up->port.read_status_mask;

#ifdef CONFIG_SERIAL_COMIP_CONSOLE
			if (up->port.line == up->port.cons->index) {
				/* Recover the break flag from console xmit */
				*status |= up->lsr_break_flag;
				up->lsr_break_flag = 0;
			}
#endif
			if (*status & UART_LSR_BI) {
				flag = TTY_BREAK;
			} else if (*status & UART_LSR_PE)
				flag = TTY_PARITY;
			else if (*status & UART_LSR_FE)
				flag = TTY_FRAME;
		}

		if (uart_handle_sysrq_char(&up->port, ch))
			goto ignore_char;

		uart_insert_char(&up->port, *status, UART_LSR_OE, ch, flag);

ignore_char:
		*status = serial_in(up, UART_LSR);
	} while ((*status & UART_LSR_DR) && (max_count-- > 0));

	serial_comip_flip_buffer_push(up);
}

static void serial_comip_transmit_chars(struct uart_comip_port *up)
{
	struct circ_buf *xmit = &up->port.state->xmit;
	int count;

	if (up->port.x_char) {
		serial_out(up, UART_TX, up->port.x_char);
		up->port.icount.tx++;
		up->port.x_char = 0;
		return;
	}

	if (uart_circ_empty(xmit) || uart_tx_stopped(&up->port)) {
		serial_comip_stop_tx(&up->port);
		return;
	}

	count = up->port.fifosize;
	do {
		serial_out(up, UART_TX, xmit->buf[xmit->tail]);
		xmit->tail = (xmit->tail + 1) & (UART_XMIT_SIZE - 1);
		up->port.icount.tx++;
		if (uart_circ_empty(xmit)) {
			serial_comip_stop_tx(&up->port);
			break;
		}
	} while (--count > 0);

	if (uart_circ_chars_pending(xmit) < WAKEUP_CHARS)
		uart_write_wakeup(&up->port);

	if (uart_circ_empty(xmit))
		serial_comip_stop_tx(&up->port);
}

static void serial_comip_start_tx(struct uart_port *port)
{
	struct uart_comip_port *up = (struct uart_comip_port *)port;

	if (up->flags & COMIP_UART_TX_USE_DMA) {
		up->tx_stop = 0;
		tasklet_schedule(&up->tklet);
	} else {
		if (!(up->ier & UART_IER_THRI)) {
			up->ier |= UART_IER_THRI;
			serial_out(up, UART_IER, up->ier);
		}
	}
}

static inline void serial_comip_check_modem_status(struct uart_comip_port *up)
{
	int status;
	status = serial_in(up, UART_MSR);

	if ((status & UART_MSR_ANY_DELTA) == 0)
		return;

	if ((up->ier & UART_IER_MSI) && !(up->mcr & UART_MCR_AFE) && (status & UART_MSR_DCTS))
		uart_handle_cts_change(&up->port, status & UART_MSR_CTS);

	wake_up_interruptible(&up->port.state->port.delta_msr_wait);
}

static unsigned int serial_comip_dma_rx(struct uart_comip_port *up)
{
	struct tty_struct *tty = up->port.state->port.tty;
	unsigned char *transfer_end_addr;
	unsigned char *transfer_addr;
	unsigned char *buf;
	unsigned int data_len = 0;
	unsigned int count;
	u32 addr_t;
	u32 addr;
	int ret;

	ret = comip_dmas_get(up->rxdma, &addr);
	if (!ret && (addr != up->rxdma_addr_last)) {
		while (addr != up->rxdma_addr_last) {
			addr_t = (up->rxdma_addr_last - (u32)up->rxdma_addr_phys);
			buf = (unsigned char*)up->rxdma_addr + addr_t;

			if (addr < up->rxdma_addr_last) {
				count = UART_COMIP_RX_DMA_BUF_SIZE - addr_t;
				up->rxdma_addr_last = (u32)up->rxdma_addr_phys;
			} else {
				count = addr - up->rxdma_addr_last;
				up->rxdma_addr_last = addr;
			}

			transfer_addr = buf;
			transfer_end_addr = buf + count;
			while (transfer_addr != transfer_end_addr) {
				unsigned int packet_size;
				packet_size = transfer_end_addr - transfer_addr > PAGE_SIZE ?
						PAGE_SIZE : transfer_end_addr - transfer_addr;
				tty_insert_flip_string(tty->port, transfer_addr, packet_size);
				transfer_addr += packet_size;
			}
			up->port.icount.rx += count;
			data_len += count;
		}

		tty_flip_buffer_push(tty->port);
	}

	return data_len;
}

static inline void serial_comip_dma_tx_irq(int irq, int type, void *dev_id)
{
	struct uart_comip_port *up = dev_id;
	struct circ_buf *xmit = &up->port.state->xmit;

	/* if tx stop, stop transmit DMA and return */
	if (up->tx_stop)
		return;

	if (uart_circ_chars_pending(xmit) < WAKEUP_CHARS)
		uart_write_wakeup(&up->port);

	if (!uart_circ_empty(xmit))
		tasklet_schedule(&up->tklet);
}

static inline void serial_comip_dma_rx_irq(int irq, int type, void *dev_id)
{
	struct uart_comip_port *up = dev_id;
	unsigned int data_len = 0;

	if (!up->rx_stop && (type & DMAS_INT_HBLK_FLUSH)) {
		data_len = serial_comip_dma_rx(up);
		if (!hrtimer_callback_running(&up->rx_hrtimer)) {
			if (data_len > 0)
				hrtimer_start(&up->rx_hrtimer,
					ktime_set(0, UART_COMIP_RX_DATA_TIMEOUT), HRTIMER_MODE_REL_PINNED);
			else
				hrtimer_start(&up->rx_hrtimer,
					ktime_set(0, UART_COMIP_RX_CHECK_TIMEOUT), HRTIMER_MODE_REL_PINNED);
		}
	}
}

static void serial_comip_config_dma_rx(struct uart_comip_port *up)
{
	struct dmas_ch_cfg *cfg = &up->dma_rx_cfg;

	cfg->flags = DMAS_CFG_ALL;
	cfg->block_size = UART_COMIP_RX_DMA_BUF_SIZE;
	cfg->src_addr = (unsigned int)(io_v2p(up->port.membase) + UART_RX);
	cfg->dst_addr = (unsigned int)up->rxdma_addr_phys;
	cfg->priority = DMAS_CH_PRI_DEFAULT;
	cfg->bus_width = DMAS_DEV_WIDTH_8BIT;
	cfg->rx_trans_type = DMAS_TRANS_WRAP;
	cfg->rx_timeout = 50;
	cfg->irq_en = DMAS_INT_HBLK_FLUSH;
	cfg->irq_handler = serial_comip_dma_rx_irq;
	cfg->irq_data = up;
	comip_dmas_config(up->rxdma, cfg);
}

static void serial_comip_config_dma_tx(struct uart_comip_port *up)
{
	struct dmas_ch_cfg *cfg = &up->dma_tx_cfg;

	cfg->flags = DMAS_CFG_ALL;
	cfg->block_size = UART_COMIP_TX_DMA_BUF_SIZE;
	cfg->src_addr = (unsigned int)up->txdma_addr_phys;
	cfg->dst_addr = (unsigned int)(io_v2p(up->port.membase) + UART_TX);
	cfg->priority = DMAS_CH_PRI_DEFAULT;
	cfg->bus_width = DMAS_DEV_WIDTH_8BIT;
	cfg->tx_trans_mode = DMAS_TRANS_NORMAL;
	cfg->tx_fix_value = 0;
	cfg->tx_block_mode = DMAS_SINGLE_BLOCK;
	cfg->irq_en = DMAS_INT_DONE;
	cfg->irq_handler = serial_comip_dma_tx_irq;
	cfg->irq_data = up;
	comip_dmas_config(up->txdma, cfg);
}

static void serial_comip_start_dma_rx(struct uart_comip_port *up)
{
	struct dmas_ch_cfg *cfg = &up->dma_rx_cfg;

	cfg->flags = DMAS_CFG_BLOCK_SIZE;
	cfg->block_size = UART_COMIP_RX_DMA_BUF_SIZE;
	comip_dmas_config(up->rxdma, cfg);
	comip_dmas_start(up->rxdma);
}

static void serial_comip_start_dma_tx(struct uart_comip_port *up, int count)
{
	struct dmas_ch_cfg *cfg = &up->dma_tx_cfg;

	cfg->flags = DMAS_CFG_BLOCK_SIZE;
	cfg->block_size = count;
	comip_dmas_config(up->txdma, cfg);
	comip_dmas_start(up->txdma);
}

static enum hrtimer_restart serial_comip_rx_hrtimer_fn(struct hrtimer *hrtimer)
{
	struct uart_comip_port *up = container_of(hrtimer, struct uart_comip_port,
			rx_hrtimer);
	unsigned int data_len = 0;

	if (!up->rx_stop && !comip_dmas_state(up->rxdma)) {
		UART_PRINT("Invalid uart%d dma%d state.\n", up->id, up->rxdma);
		serial_comip_start_dma_rx(up);
		goto restart_timer;
	}

	data_len = serial_comip_dma_rx(up);

	if (up->rx_stop)
		return HRTIMER_NORESTART;

restart_timer:
	if (data_len > 0) {
		hrtimer_forward_now(hrtimer, ktime_set(0, UART_COMIP_RX_DATA_TIMEOUT));
		return HRTIMER_RESTART;
	} else {
		if (up->flags & COMIP_UART_RX_USE_GPIO_IRQ) {
			enable_irq(gpio_to_irq(up->mach->rx_gpio));
			return HRTIMER_NORESTART;
		} else {
			hrtimer_forward_now(hrtimer, ktime_set(0, UART_COMIP_RX_CHECK_TIMEOUT));
			return HRTIMER_RESTART;
		}
	}
}

static irqreturn_t serial_comip_rx_gpio_irq(int irq, void *dev_id)
{
	struct uart_comip_port *up = dev_id;

	disable_irq_nosync(gpio_to_irq(up->mach->rx_gpio));

	if (!hrtimer_callback_running(&up->rx_hrtimer))
		hrtimer_start(&up->rx_hrtimer,
			ktime_set(0, UART_COMIP_RX_DATA_TIMEOUT), HRTIMER_MODE_REL_PINNED);

	return IRQ_HANDLED;
}

static inline irqreturn_t serial_comip_irq(int irq, void *dev_id)
{
	struct uart_comip_port *up = dev_id;
	unsigned int iir, lsr;

	iir = serial_in(up, UART_IIR) & 0xf;
	if (iir == UART_IIR_NO_INT)
		return IRQ_NONE;

	if (iir == UART_IIR_BUSY)
		serial_in(up, UART_USR);

	lsr = serial_in(up, UART_LSR);
	if (!(up->flags & COMIP_UART_RX_USE_DMA)) {
		if ((lsr & UART_LSR_DR))
			serial_comip_receive_chars(up, &lsr);

		#ifdef CONFIG_CPU_LC1860
		/* Avoid lc1860 wrong rx timeout interrupt bug. */
		if ((iir & UART_IIR_RX_TIMEOUT) && !(lsr & UART_LSR_DR))
			serial_in(up, UART_RX);
		#endif
	}

	if (up->flags & COMIP_UART_SUPPORT_MCTRL)
		serial_comip_check_modem_status(up);

	if (!(up->flags & COMIP_UART_TX_USE_DMA)) {
		if (lsr & UART_LSR_THRE) {
			spin_lock(&up->port.lock);
			serial_comip_transmit_chars(up);
			spin_unlock(&up->port.lock);
		}
	}

	return IRQ_HANDLED;
}

static unsigned int serial_comip_tx_empty(struct uart_port *port)
{
	struct uart_comip_port *up = (struct uart_comip_port *)port;
	unsigned long flags;
	unsigned int ret;

	spin_lock_irqsave(&up->port.lock, flags);
	if (up->flags & COMIP_UART_TX_USE_DMA)
		ret = !comip_dmas_state(up->txdma);
	else
		ret = serial_in(up, UART_LSR) & UART_LSR_TEMT;
	spin_unlock_irqrestore(&up->port.lock, flags);

	return (ret ? TIOCSER_TEMT : 0);
}

static unsigned int serial_comip_get_mctrl(struct uart_port *port)
{
	struct uart_comip_port *up = (struct uart_comip_port *)port;
	unsigned char status;
	unsigned int ret = 0;

	if (!(up->flags & COMIP_UART_SUPPORT_MCTRL))
		return 0;

	status = serial_in(up, UART_MSR);

	if (status & UART_MSR_CTS)
		ret |= TIOCM_CTS;

	return ret;
}

static void serial_comip_set_mctrl(struct uart_port *port, unsigned int mctrl)
{
	struct uart_comip_port *up = (struct uart_comip_port *)port;
	unsigned int val = 0;

	if (!(up->flags & COMIP_UART_SUPPORT_MCTRL))
		return;

	val = serial_in(up, UART_MCR);

	if ((mctrl & TIOCM_RTS))
		val |= (UART_MCR_AFE | UART_MCR_RTS);
	else
		val &= ~(UART_MCR_AFE | UART_MCR_RTS);

	serial_out(up, UART_MCR, val);
}

static void serial_comip_break_ctl(struct uart_port *port, int break_state)
{
	struct uart_comip_port *up = (struct uart_comip_port *)port;
	unsigned long flags;
	unsigned long timeout;

	if (!(up->flags & COMIP_UART_SUPPORT_MCTRL))
		return;

	timeout = jiffies + (msecs_to_jiffies(UART_COMIP_TX_DMAS_WAIT_IDLE_TIMEOUT));
	if (up->flags & COMIP_UART_TX_USE_DMA) {
		while (comip_dmas_state(up->txdma) > 0) {
			if (time_after(jiffies, timeout)) {
				UART_PRINT("uart%d break ctl wait for tx dma idle timeout. MSR = 0x%08x\n", 
						up->id, serial_in(up, UART_MSR));
				break;
			}
		}
	}

	serial_comip_wait_for_idle(up);

	spin_lock_irqsave(&up->port.lock, flags);
	if (break_state == -1)
		up->lcr |= UART_LCR_SBC;
	else
		up->lcr &= ~UART_LCR_SBC;
	serial_out(up, UART_LCR, up->lcr);
	spin_unlock_irqrestore(&up->port.lock, flags);
}

static void serial_comip_task_action(unsigned long data)
{
	struct uart_comip_port *up = (struct uart_comip_port *)data;
	struct circ_buf *xmit = &up->port.state->xmit;
	unsigned char *tmp = up->txdma_addr;
	unsigned long flags;
	int count = 0, c;

	/* if the tx is stop, just return. */
	if (up->tx_stop || !tmp)
		return;

	if (comip_dmas_state(up->txdma) > 0)
		return;

	spin_lock_irqsave(&up->port.lock, flags);
	while (1) {
		c = CIRC_CNT_TO_END(xmit->head, xmit->tail, UART_XMIT_SIZE);
		if (c <= 0)
			break;

		memcpy(tmp, xmit->buf + xmit->tail, c);
		xmit->tail = (xmit->tail + c) & (UART_XMIT_SIZE - 1);
		tmp += c;
		count += c;
		up->port.icount.tx += c;
	}
	spin_unlock_irqrestore(&up->port.lock, flags);

	if (count) {
		up->tx_stop = 0;
		serial_comip_start_dma_tx(up, count);
	}
}

static int serial_comip_rx_gpio_irq_init(struct uart_comip_port *up)
{
	int ret;
	char irq_name[20];
	unsigned int rx_gpio = up->mach->rx_gpio;

	ret = gpio_request(rx_gpio, up->name);
	if (ret < 0) {
		UART_PRINT("Failed to request UART%d GPIO.\n", up->id);
		return ret;
	}

	ret = gpio_direction_input(rx_gpio);
	if (ret < 0) {
		UART_PRINT("Failed to set UART%d GPIO.\n", up->id);
		goto exit;
	}

	/* Request irq. */
	sprintf(irq_name, "uart%d rx irq", up->id);
	ret = request_irq(gpio_to_irq(rx_gpio), serial_comip_rx_gpio_irq,
			  IRQF_TRIGGER_LOW, irq_name, up);
	if (ret) {
		UART_PRINT("UART%d IRQ already in use.\n", up->id);
		goto exit;
	}

	/*
	 * Note : If RX GPIO use the irq function,
	 * it must disable the debounce opetation.
	 */
	gpio_set_debounce(rx_gpio, 0);

	return 0;

exit:
	gpio_free(rx_gpio);
	return ret;
}

static int serial_comip_rx_gpio_irq_uninit(struct uart_comip_port *up)
{
	unsigned int rx_gpio = up->mach->rx_gpio;

	/* Free irq. */
	disable_irq_nosync(gpio_to_irq(rx_gpio));
	free_irq(gpio_to_irq(rx_gpio), up);
	gpio_free(rx_gpio);

	return 0;
}

static int serial_comip_dma_init(struct uart_comip_port *up)
{
	int ret = -ENOMEM;

	if (up->flags & COMIP_UART_TX_USE_DMA) {
		comip_dmas_request(up->name, up->txdma);
		if (!up->txdma_addr) {
			up->txdma_addr = dma_alloc_coherent(up->port.dev,
						UART_COMIP_TX_DMA_BUF_SIZE,
						&(up->txdma_addr_phys),
						GFP_KERNEL);
			if (!up->txdma_addr)
				goto txdma_err_alloc;
		}

		up->tx_stop = 0;
		serial_comip_config_dma_tx(up);
		tasklet_init(&up->tklet, serial_comip_task_action, (unsigned long)up);
	}

	if (up->flags & COMIP_UART_RX_USE_DMA) {
		comip_dmas_request(up->name, up->rxdma);
		up->rxdma_addr = up->rxdma_start_addr;
		if (!up->rxdma_addr)
			goto rxdma_err_alloc;
		up->rxdma_addr_last = up->rxdma_addr_phys;

		serial_comip_config_dma_rx(up);
		serial_comip_start_dma_rx(up);

		hrtimer_init(&up->rx_hrtimer, CLOCK_MONOTONIC, HRTIMER_MODE_REL_PINNED);
		up->rx_hrtimer.function = serial_comip_rx_hrtimer_fn;

		if (up->flags & COMIP_UART_RX_USE_GPIO_IRQ) {
			ret = serial_comip_rx_gpio_irq_init(up);
			if (ret)
				goto rxirq_err_init;
		}

		up->rx_stop = 0;
	}

	return 0;

rxirq_err_init:
	up->rxdma_addr = NULL;
rxdma_err_alloc:
	dma_free_coherent(NULL, UART_COMIP_TX_DMA_BUF_SIZE,
		up->txdma_addr, up->txdma_addr_phys);
	up->txdma_addr = NULL;
	tasklet_kill(&up->tklet);
txdma_err_alloc:
	return ret;
}

static void serial_comip_dma_uninit(struct uart_comip_port *up)
{
	if (up->flags & COMIP_UART_TX_USE_DMA) {
		tasklet_kill(&up->tklet);

		up->tx_stop = 1;
		comip_dmas_stop(up->txdma);
		comip_dmas_free(up->txdma);
		if (up->txdma_addr != NULL) {
			dma_free_coherent(NULL, UART_COMIP_TX_DMA_BUF_SIZE,
				up->txdma_addr, up->txdma_addr_phys);
			up->txdma_addr = NULL;
		}
	}

	if (up->flags & COMIP_UART_RX_USE_DMA) {
		up->rx_stop = 1;
		comip_dmas_stop(up->rxdma);
		comip_dmas_free(up->rxdma);

		if (up->flags & COMIP_UART_RX_USE_GPIO_IRQ)
			serial_comip_rx_gpio_irq_uninit(up);

		hrtimer_cancel(&up->rx_hrtimer);
		up->rxdma_addr = NULL;
	}
}

static int serial_comip_startup(struct uart_port *port)
{
	struct uart_comip_port *up = (struct uart_comip_port *)port;
	int ret;

	/*
	 * Enable UART power.
	 */
	if (up->mach && up->mach->power)
		up->mach->power(port, 1);

	/*
	 * Enable UART clock.
	 */
	clk_enable(up->clk);

	/*
	 * Clear the FIFO buffers and disable them.
	 * (they will be reenabled in set_termios())
	 */
	serial_out(up, UART_FCR, UART_FCR_ENABLE_FIFO);
	serial_out(up, UART_FCR, UART_FCR_ENABLE_FIFO |
		   UART_FCR_CLEAR_RCVR | UART_FCR_CLEAR_XMIT);
	serial_out(up, UART_FCR, 0);
	up->fcr = 0;
	/*
	 * Allocate the IRQ
	 */
	serial_out(up, UART_IER, 0);
	ret = request_irq(up->port.irq, serial_comip_irq,
			IRQF_DISABLED, up->name, up);
	if (ret) {
		UART_PRINT("Failed to uart%d irq %d\n", up->id, up->port.irq);
		return ret;
	}

	/*
	 * Set the modem ctrl.
	 */
	up->mcr = 0;
	if (up->flags & COMIP_UART_SUPPORT_MCTRL)
		up->mcr |= UART_MCR_AFE | UART_MCR_RTS;
	if (up->flags & COMIP_UART_SUPPORT_IRDA)
		up->mcr |= UART_MCR_SIRE;

	/*
	 * Clear the interrupt registers.
	 */
	(void)serial_in(up, UART_LSR);
	(void)serial_in(up, UART_RX);
	(void)serial_in(up, UART_IIR);
	(void)serial_in(up, UART_MSR);

	/*
	 * Now, initialize the UART
	 */
	serial_out(up, UART_LCR, UART_LCR_WLEN8);

	/*
	 * Finally, enable interrupts.	Note: Modem status interrupts
	 * are set via set_termios(), which will be occurring imminently
	 * anyway, so we don't enable them here.
	 */
	up->ier = UART_IER_RLSI;
	if (!(up->flags & COMIP_UART_RX_USE_DMA))
		up->ier |= UART_IER_RDI;
	serial_out(up, UART_IER, up->ier);

	/*
	 * And clear the interrupt registers again for luck.
	 */
	(void)serial_in(up, UART_LSR);
	(void)serial_in(up, UART_RX);
	(void)serial_in(up, UART_IIR);
	(void)serial_in(up, UART_MSR);

	/*
	 * Initialize the DMA.
	 */
	ret = serial_comip_dma_init(up);
	if (ret)
		return ret;

	return 0;
}

static void serial_comip_shutdown(struct uart_port *port)
{
	struct uart_comip_port *up = (struct uart_comip_port *)port;

	/*
	 * Disable interrupts from this port
	 */
	up->ier = 0;
	serial_out(up, UART_IER, 0);

	/*
	 * Free the IRQ
	 */
	free_irq(up->port.irq, up);

	/*
	 * Disable break condition and FIFOs
	 */
	serial_out(up, UART_LCR, serial_in(up, UART_LCR) & ~UART_LCR_SBC);
	serial_out(up, UART_FCR, UART_FCR_ENABLE_FIFO |
		   UART_FCR_CLEAR_RCVR | UART_FCR_CLEAR_XMIT);
	serial_out(up, UART_FCR, 0);
	up->fcr = 0;

	(void)serial_in(up, UART_LSR);
	(void)serial_in(up, UART_RX);
	(void)serial_in(up, UART_IIR);
	(void)serial_in(up, UART_MSR);

	serial_comip_dma_uninit(up);

	/*
	 * Disable UART clock.
	 */
	clk_disable(up->clk);

	/*
	 * Disable UART power.
	 */
	if (up->mach && up->mach->power)
		up->mach->power(port, 0);
}

static void
serial_comip_set_termios(struct uart_port *port, struct ktermios *termios,
			 struct ktermios *old)
{
	struct uart_comip_port *up = (struct uart_comip_port *)port;
	unsigned char cval = 0;
	unsigned long flags;
	unsigned int baud, quot;

	if ((termios->c_cflag == 0) && up->port.cons &&
			(up->port.line == up->port.cons->index))
		termios->c_cflag = B115200 | CS8 | CSTOPB;

	switch (termios->c_cflag & CSIZE) {
	case CS5:
		cval = UART_LCR_WLEN5;
		break;
	case CS6:
		cval = UART_LCR_WLEN6;
		break;
	case CS7:
		cval = UART_LCR_WLEN7;
		break;
	default:
	case CS8:
		cval = UART_LCR_WLEN8;
		break;
	}

	if (termios->c_cflag & CSTOPB)
		cval |= UART_LCR_STOP;
	if (termios->c_cflag & PARENB)
		cval |= UART_LCR_PARITY;
	if (!(termios->c_cflag & PARODD))
		cval |= UART_LCR_EPAR;

	/*
	 * Ask the core to calculate the divisor for us.
	 */
	baud = uart_get_baud_rate(port, termios, old, 0, UART_COMIP_CLK_4M / 16);

	/*change uart baud */
	if(baud != (port->uartclk / 16)) {
		switch (baud){
			case 1000000:
				port->uartclk = UART_COMIP_CLK_1M;
				break;
			case 2000000:
				port->uartclk = UART_COMIP_CLK_2M;
				break;
			case 3000000:
				port->uartclk = UART_COMIP_CLK_3M;
				break;
			case 4000000:
				port->uartclk = UART_COMIP_CLK_4M;
				break;
			/*we need use default value*/
			default:
				port->uartclk = UART_COMIP_CLK_NORMAL;
				break;
		}
		clk_disable(up->clk);
		clk_set_rate(up->clk,port->uartclk);
		clk_enable(up->clk);
	}
	
	quot = uart_get_divisor(port, baud);

	if (up->flags & COMIP_UART_RX_USE_DMA)
		up->fcr = UART_FCR_ENABLE_FIFO | UART_FCR_RX_T8;
	else
		up->fcr = UART_FCR_ENABLE_FIFO | UART_FCR_RX_T1;

	/* Clear the FIFO buffers of console. */
	if (up->port.cons && (up->port.cons->index == up->port.line))
		serial_out(up, UART_FCR, up->fcr | UART_FCR_CLEAR_RCVR | UART_FCR_CLEAR_XMIT);

	/*
	 * Before configuring uart, make sure its status is not busy.
	 */
	serial_comip_wait_for_idle(up);

	/*
	 * MCR-based auto flow control.
	 */
	if (termios->c_cflag & CRTSCTS)
		up->mcr |= UART_MCR_AFE;

	/*
	 * Ok, we're now changing the port state.  Do it with
	 * interrupts disabled.
	 */
	spin_lock_irqsave(&up->port.lock, flags);
	/*
	 * Update the per-port timeout.
	 */
	uart_update_timeout(port, termios->c_cflag, baud);

	up->port.read_status_mask = UART_LSR_OE | UART_LSR_THRE | UART_LSR_DR;
	if (termios->c_iflag & INPCK)
		up->port.read_status_mask |= UART_LSR_FE | UART_LSR_PE;
	if (termios->c_iflag & (BRKINT | PARMRK))
		up->port.read_status_mask |= UART_LSR_BI;

	/*
	 * Characters to ignore
	 */
	up->port.ignore_status_mask = 0;
	if (termios->c_iflag & IGNPAR)
		up->port.ignore_status_mask |= UART_LSR_PE | UART_LSR_FE;
	if (termios->c_iflag & IGNBRK) {
		up->port.ignore_status_mask |= UART_LSR_BI;
		/*
		 * If we're ignoring parity and break indicators,
		 * ignore overruns too (for real raw support).
		 */
		if (termios->c_iflag & IGNPAR)
			up->port.ignore_status_mask |= UART_LSR_OE;
	}

	/*
	 * ignore all characters if CREAD is not set
	 */
	if ((termios->c_cflag & CREAD) == 0)
		up->port.ignore_status_mask |= UART_LSR_DR;

	/*
	 * CTS flow control flag and modem status interrupts
	 */
	up->ier &= ~UART_IER_MSI;
	if ((up->flags & COMIP_UART_SUPPORT_MCTRL)
		&& !(up->flags & COMIP_UART_DISABLE_MSI)
		&& UART_ENABLE_MS(&up->port, termios->c_cflag))
		up->ier |= UART_IER_MSI;

	serial_out(up, UART_IER, up->ier);

	serial_out(up, UART_LCR, cval | UART_LCR_DLAB); /* set DLAB */
#if !defined(CONFIG_MENTOR_DEBUG)
	serial_out(up, UART_DLL, quot & 0xff);	/* LS of divisor */
	serial_out(up, UART_DLM, quot >> 8);	/* MS of divisor */
#endif
	serial_out(up, UART_LCR, cval); /* reset DLAB */
	/*delay (2*DIV*16) uart_mclk after bandrate changed as spec acquired*/
	udelay((unsigned long)2 * 1000 * 1000 / baud + 1);

	up->lcr = cval; 	/* Save LCR */
	serial_out(up, UART_MCR, up->mcr);
	serial_out(up, UART_FCR, up->fcr);
	spin_unlock_irqrestore(&up->port.lock, flags);
}

static void serial_comip_pm(struct uart_port *port, unsigned int state,
		unsigned int oldstate)
{
	struct uart_comip_port *up = (struct uart_comip_port *)port;

	if (!state)
		clk_enable(up->clk);
	else
		clk_disable(up->clk);
}

static void serial_comip_release_port(struct uart_port *port)
{
}

static int serial_comip_request_port(struct uart_port *port)
{
	return 0;
}

static void serial_comip_config_port(struct uart_port *port, int flags)
{
	struct uart_comip_port *up = (struct uart_comip_port *)port;

	up->port.type = PORT_16550;
}

static int
serial_comip_verify_port(struct uart_port *port, struct serial_struct *ser)
{
	/* we don't want the core code to modify any port params */
	return -EINVAL;
}

static const char *serial_comip_type(struct uart_port *port)
{
	struct uart_comip_port *up = (struct uart_comip_port *)port;

	return up->name;
}

static struct uart_comip_port serial_comip_ports[UART_COMIP_NR_PORTS];
static struct uart_driver serial_comip_reg;
struct uart_ops serial_comip_pops = {
	.tx_empty = serial_comip_tx_empty,
	.set_mctrl = serial_comip_set_mctrl,
	.get_mctrl = serial_comip_get_mctrl,
	.stop_tx = serial_comip_stop_tx,
	.start_tx = serial_comip_start_tx,
	.stop_rx = serial_comip_stop_rx,
	.enable_ms = serial_comip_enable_ms,
	.break_ctl = serial_comip_break_ctl,
	.startup = serial_comip_startup,
	.shutdown = serial_comip_shutdown,
	.set_termios = serial_comip_set_termios,
	.pm = serial_comip_pm,
	.type = serial_comip_type,
	.release_port = serial_comip_release_port,
	.request_port = serial_comip_request_port,
	.config_port = serial_comip_config_port,
	.verify_port = serial_comip_verify_port,
};

static int __init serial_comip_init_port(struct uart_comip_port *sport,
					  struct platform_device *dev)
{
	struct uart_port *port = &sport->port;
	struct comip_uart_platform_data *data = dev->dev.platform_data;
	struct resource *mmres, *irqres;
	struct resource *dmarxres, *dmatxres;

	mmres = platform_get_resource(dev, IORESOURCE_MEM, 0);
	irqres = platform_get_resource(dev, IORESOURCE_IRQ, 0);
	dmarxres = platform_get_resource(dev, IORESOURCE_DMA, 0);
	dmatxres = platform_get_resource(dev, IORESOURCE_DMA, 1);
	if (!mmres || !irqres || !dmarxres || !dmatxres)
		return -ENODEV;

	port->type = PORT_16550;
	port->iotype = UPIO_MEM;
	port->flags = UPF_BOOT_AUTOCONF;
	port->uartclk = UART_COMIP_CLK_NORMAL;
	port->ops = &serial_comip_pops;
	#ifdef CONFIG_CPU_LC1860
	if (dev->id == 3)
		port->fifosize = 16;
	else
		port->fifosize = 32;
	#else
	port->fifosize = 32;
	#endif
	port->line = dev->id;
	port->dev = &dev->dev;
	port->irq = irqres->start;
	port->membase = (unsigned char*)io_p2v(mmres->start);
	sport->txdma = dmatxres->start;
	sport->rxdma = dmarxres->start;
	sport->txdma_addr = NULL;
	sport->rxdma_addr = NULL;
	sport->rxdma_start_addr = NULL;
	sport->txdma_addr_phys = 0;
	sport->rxdma_addr_phys = 0;
	sport->rxdma_addr_last = 0;
	sport->tx_stop = 1;
	sport->rx_stop = 1;
	sport->mach = data;
	sport->id = dev->id;

	if (data)
		sport->flags = data->flags;
	else
		sport->flags = 0;

	sprintf(sport->name, "uart%d", dev->id);
	sport->clk = clk_get(&dev->dev, "uart_clk");

#if defined(CONFIG_MENTOR_DEBUG)
	return 0;
#else
	if (IS_ERR(sport->clk))
		return PTR_ERR(sport->clk);
#endif

	clk_set_rate(sport->clk, port->uartclk);

	return 0;
}

#ifdef CONFIG_SERIAL_COMIP_CONSOLE

extern struct platform_device comip_device_uart0;
extern struct platform_device comip_device_uart1;
extern struct platform_device comip_device_uart2;
extern struct platform_device comip_device_uart3;

static struct platform_device *serial_comip_devices[UART_COMIP_NR_PORTS] = {
	&comip_device_uart0,
	&comip_device_uart1,
	&comip_device_uart2,
	&comip_device_uart3,
};

#define BOTH_EMPTY (UART_LSR_TEMT | UART_LSR_THRE)

/*
 *	Wait for transmitter & holding register to empty
 */
static inline void wait_for_xmitr(struct uart_comip_port *up)
{
	unsigned int status, tmout = 10000;

	/* Wait up to 10ms for the character(s) to be sent. */
	do {
		status = serial_in(up, UART_LSR);

		if (status & UART_LSR_BI)
			up->lsr_break_flag = UART_LSR_BI;

		if (--tmout == 0)
			break;
		udelay(1);
	} while ((status & BOTH_EMPTY) != BOTH_EMPTY);

	/* Wait up to 1s for flow control if necessary */
	if (up->port.flags & UPF_CONS_FLOW) {
		tmout = 1000000;
		while (--tmout &&
			   ((serial_in(up, UART_MSR) & UART_MSR_CTS) == 0))
			udelay(1);
	}
}

static void serial_comip_console_putchar(struct uart_port *port, int ch)
{
	struct uart_comip_port *up = (struct uart_comip_port *)port;

	wait_for_xmitr(up);
	serial_out(up, UART_TX, ch);
}

/*
 * Print a string to the serial port trying not to disturb
 * any possible real use of the port...
 *
 *	The console_lock must be held when we get here.
 */
static void
serial_comip_console_write(struct console *co, const char *s,
			   unsigned int count)
{
	struct uart_comip_port *up = &(serial_comip_ports[co->index]);
	//unsigned int ier;

	/*
	 * First save the IER then disable the interrupts
	 */
	//ier = serial_in(up, UART_IER);
	//serial_out(up, UART_IER, 0);

	uart_console_write(&up->port, s, count, serial_comip_console_putchar);

	/*
	 * Finally, wait for transmitter to become empty
	 * and restore the IER
	 */
	//wait_for_xmitr(up);
	//serial_out(up, UART_IER, ier);
}

static int __init serial_comip_console_setup(struct console *co, char *options)
{
	struct uart_comip_port *up;
	int baud = 115200;
	int bits = 8;
	int parity = 'n';
	int flow = 'n';
	int ret = 0;

	if (co->index == -1 || co->index >= serial_comip_reg.nr)
		co->index = 0;

	up = &(serial_comip_ports[co->index]);
	if (!up)
		return -ENODEV;

	serial_comip_init_port(up, serial_comip_devices[co->index]);
	up->port.cons = co;

	/*
	 * Enable UART clock.
	 */
	clk_enable(up->clk);

	if (options)
		uart_parse_options(options, &baud, &parity, &bits, &flow);

	ret = uart_set_options(&up->port, co, baud, parity, bits, flow);

	return ret;
}

static struct console serial_comip_console = {
	.name = "ttyS",
	.write = serial_comip_console_write,
	.device = uart_console_device,
	.setup = serial_comip_console_setup,
	.flags = CON_PRINTBUFFER,
	.index = -1,
	.data = &serial_comip_reg,
};

static int __init serial_comip_console_init(void)
{
	register_console(&serial_comip_console);
	return 0;
}

console_initcall(serial_comip_console_init);

#define COMIP_CONSOLE	&serial_comip_console
#else
#define COMIP_CONSOLE	NULL
#endif

static struct uart_driver serial_comip_reg = {
	.owner = THIS_MODULE,
	.driver_name = "comip-serial",
	.dev_name = "ttyS",
	.major = TTY_MAJOR,
	.minor = 64,
	.nr = UART_COMIP_NR_PORTS,
	.cons = COMIP_CONSOLE,
};

#ifdef CONFIG_PM
static int serial_comip_suspend(struct platform_device *dev, pm_message_t state)
{
	struct uart_comip_port *sport = platform_get_drvdata(dev);
	unsigned long flags;

	if (sport) {
		if(sport->mach->pin_switch)
			sport->mach->pin_switch(1);

		if ((sport->flags & COMIP_UART_TX_USE_DMA) && sport->txdma_addr) {
			spin_lock_irqsave(&(sport->port.lock),flags);
			sport->tx_stop = 1;
			spin_unlock_irqrestore(&(sport->port.lock),flags);
		}

		if ((sport->flags & COMIP_UART_RX_USE_DMA) && sport->rxdma_addr) {
			spin_lock_irqsave(&(sport->port.lock),flags);
			sport->rx_stop = 1;
			spin_unlock_irqrestore(&(sport->port.lock),flags);
			comip_dmas_intr_disable(sport->rxdma, DMAS_INT_HBLK_FLUSH);
			hrtimer_cancel(&sport->rx_hrtimer);
			serial_comip_dma_rx(sport);
			comip_dmas_stop(sport->rxdma);
		}
		uart_suspend_port(&serial_comip_reg, &sport->port);
	}

	return 0;
}

static int serial_comip_resume(struct platform_device *dev)
{
	struct uart_comip_port *sport = platform_get_drvdata(dev);

	if (sport) {
		uart_resume_port(&serial_comip_reg, &sport->port);

		if ((sport->flags & COMIP_UART_TX_USE_DMA) && sport->txdma_addr) {
			sport->tx_stop = 0;
			tasklet_schedule(&sport->tklet);
		}
	}

	if(sport->mach->pin_switch)
		sport->mach->pin_switch(0);

	return 0;
}
#else
#define serial_comip_suspend	NULL
#define serial_comip_resume	NULL
#endif

static int __init serial_comip_probe(struct platform_device *dev)
{
	struct uart_comip_port *sport;
	int ret;

	sport = &(serial_comip_ports[dev->id]);
	ret = serial_comip_init_port(sport, dev);
	if (ret)
		return ret;

	if (sport->flags & COMIP_UART_USE_WORKQUEUE) {
		sport->wq = alloc_ordered_workqueue("comip serial%d", 0, dev->id);
		if (!sport->wq)
			return -ENOMEM;

		INIT_WORK(&sport->buffer_work, serial_comip_flip_buffer_push_wq);
	}

	if ((sport->flags & COMIP_UART_RX_USE_DMA) && !sport->rxdma_start_addr) {
		sport->rxdma_start_addr = dma_alloc_coherent(sport->port.dev,
							UART_COMIP_RX_DMA_BUF_SIZE,
							&(sport->rxdma_addr_phys),
							GFP_KERNEL);
		if (!sport->rxdma_start_addr) {
			ret = -ENOMEM;
			goto out_wq;
		}
	}

	uart_add_one_port(&serial_comip_reg, &sport->port);

	platform_set_drvdata(dev, sport);

	return 0;

out_wq:
	if (sport->flags & COMIP_UART_USE_WORKQUEUE)
		destroy_workqueue(sport->wq);

	return ret;
}

static int __exit serial_comip_remove(struct platform_device *dev)
{
	struct uart_comip_port *sport = platform_get_drvdata(dev);

	platform_set_drvdata(dev, NULL);

	if (sport->rxdma_start_addr) {
		dma_free_coherent(NULL, UART_COMIP_RX_DMA_BUF_SIZE, 
				sport->rxdma_start_addr, sport->rxdma_addr_phys);
		sport->rxdma_start_addr = NULL;
	}

	if (sport->flags & COMIP_UART_USE_WORKQUEUE)
		destroy_workqueue(sport->wq);

	uart_remove_one_port(&serial_comip_reg, &sport->port);
	clk_put(sport->clk);

	return 0;
}

static struct platform_driver serial_comip_driver = {
	.remove = __exit_p(serial_comip_remove),
	.suspend = serial_comip_suspend,
	.resume = serial_comip_resume,
	.driver = {
		.name = "comip-uart",
		.owner = THIS_MODULE,
	},
};

int __init serial_comip_init(void)
{
	int ret;

	ret = uart_register_driver(&serial_comip_reg);
	if (ret != 0)
		goto out;

	ret = platform_driver_probe(&serial_comip_driver, serial_comip_probe);
	if (ret != 0) {
		uart_unregister_driver(&serial_comip_reg);
		goto out;
	}
	return ret;

out:
	return ret;
}

void __exit serial_comip_exit(void)
{
	platform_driver_unregister(&serial_comip_driver);
	uart_unregister_driver(&serial_comip_reg);
}

module_init(serial_comip_init);
module_exit(serial_comip_exit);

MODULE_AUTHOR("lengyansong <lengyansong@leadcoretech.com>");
MODULE_DESCRIPTION("COMIP serial driver");
MODULE_LICENSE("GPL");
MODULE_ALIAS("platform:comip-uart");
