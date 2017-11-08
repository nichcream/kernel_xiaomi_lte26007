#ifndef __MIPI_CMD_H__
#define __MIPI_CMD_H__

/*
 * MIPI CMD
 */
enum {
	MIPI_GEN_CMD = 0,
	MIPI_DCS_CMD,
};

enum {
	GEN_SW_0P_TX = 1,
	GEN_SW_1P_TX,
	GEN_SW_2P_TX,
	GEN_SR_0P_TX,
	GEN_SR_1P_TX,
	GEN_SR_2P_TX,
	DCS_SW_0P_TX,
	DCS_SW_1P_TX,
	DCS_SR_0P_TX,
	MAX_RD_PKT_SIZE,
	GEN_LW_TX,
	DCS_LW_TX,
};

extern int mipi_dsih_gen_wr_cmd(struct comipfb_info *fbi, uint8_t vc, uint8_t* params, uint16_t param_length);
extern int mipi_dsih_dcs_wr_cmd(struct comipfb_info *fbi, uint8_t vc, uint8_t* params, uint16_t param_length);
extern uint16_t mipi_dsih_dcs_rd_cmd(struct comipfb_info *fbi, uint8_t vc, uint8_t command, uint8_t bytes_to_read, uint8_t* read_buffer);
extern uint16_t mipi_dsih_gen_rd_cmd(struct comipfb_info *fbi, uint8_t vc, uint8_t* params, uint16_t param_length, uint8_t bytes_to_read, uint8_t* read_buffer);
extern int mipi_dsih_gen_wr_packet(struct comipfb_info *fbi, uint8_t vc, uint8_t data_type, uint8_t* params, uint16_t param_length);
#endif
