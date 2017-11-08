#ifndef	__COMIP_BBM_H__
#define	__COMIP_BBM_H__

enum {
	BBM_READ,
	BBM_WRITE,
	BBM_ERASE,
};

struct reloc_item {
	unsigned short from;
	unsigned short to;
};

struct reloc_table {
	unsigned short header;
	unsigned short total;
};

struct comip_bbm {
	u32			current_slot;

	/* NOTES: this field impact the partition table. Please make sure
	 * that this value align with partitions definition.
	 */
	u32			max_reloc_entry;

	void			*data_buf;

	/* These two fields should be in (one)nand_chip.
	 * Add here to handle onenand_chip and nand_chip
	 * at the same time.
	 */
	int			page_shift;
	int			erase_shift;

	unsigned int		table_init;
	struct reloc_table	*table;
	struct reloc_item	*reloc;

	int	(*init)(struct mtd_info *mtd, struct comip_bbm *bbm);
	int	(*uninit)(struct mtd_info *mtd, struct comip_bbm *bbm);
	int	(*search)(struct mtd_info *mtd, struct comip_bbm *bbm,
			unsigned int block);
	int	(*markbad)(struct mtd_info *mtd, struct comip_bbm *bbm,
			unsigned int block);

	int	(*nand_cmd)(struct mtd_info *mtd, uint32_t cmd, uint32_t addr, uint8_t* buf);
};

#ifdef CONFIG_NAND_BBM_COMIP
extern struct comip_bbm* alloc_comip_bbm(void);
extern void free_comip_bbm(struct comip_bbm *);
#else
static inline struct comip_bbm* alloc_comip_bbm(void){return NULL;}
static inline void free_comip_bbm(struct comip_bbm *bbm){}
#endif

#endif /*__COMIP_BBM_H__*/
