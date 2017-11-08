#ifndef __ARCH_ARM_CLOCK_H
#define __ARCH_ARM_CLOCK_H

struct module;

struct clk {
	struct list_head	node;
	struct module		*owner;
	const char		*name;
	int			id;
	struct clk		*parent;
	void			*cust;
	unsigned long		rate;
	unsigned int		flag;
	unsigned int		user;
	unsigned int		fix_div;
	unsigned int		parent_id;
	void __iomem		*mclk_reg;
	void __iomem		*ifclk_reg;
	void __iomem		*divclk_reg;
	void __iomem		*grclk_reg;
	void __iomem		*busclk_reg;
	unsigned char		mclk_we_bit;
	unsigned char		mclk_bit;
	unsigned char		ifclk_we_bit;
	unsigned char		ifclk_bit;
	unsigned char		divclk_we_bit;
	unsigned char		divclk_bit;
	unsigned char		divclk_val;
	unsigned char		divclk_mask;
	unsigned char		grclk_we_bit;
	unsigned char		grclk_bit;
	unsigned char		grclk_val;
	unsigned char		grclk_mask;
	unsigned char		busclk_we_bit;
	unsigned char		busclk_bit;
	void			(*init)(struct clk *);
	int			(*enable)(struct clk *);
	void			(*disable)(struct clk *);
	int			(*set_rate)(struct clk *, unsigned long);
	int			(*set_parent)(struct clk *, struct clk *);
};

struct clk_functions {
	int		(*clk_enable)(struct clk *clk);
	void		(*clk_disable)(struct clk *clk);
	long		(*clk_round_rate)(struct clk *clk, unsigned long rate);
	int		(*clk_set_rate)(struct clk *clk, unsigned long rate);
	int		(*clk_set_parent)(struct clk *clk, struct clk *parent);
	struct clk *	(*clk_get_parent)(struct clk *clk);
};

extern int clk_init(struct clk_functions * custom_clocks);
extern int clk_register(struct clk *clk);
extern void clk_unregister(struct clk *clk);
extern struct clk *clk_get(struct device *dev, const char *id);
extern void clk_put(struct clk *clk);
extern int clk_get_usecount(struct clk *clk);

/* Clock flags */
#define CLK_RATE_FIXED		(1 << 0)
#define CLK_RATE_DIV_FIXED	(1 << 1)
#define CLK_ALWAYS_ENABLED	(1 << 2)
#define CLK_INIT_DISABLED	(1 << 3)
#define CLK_INIT_ENABLED	(1 << 4)

#endif /* __ARCH_ARM_CLOCK_H */
