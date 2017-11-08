/*
 * MELFAS MMS400 Touchscreen
 *
 * Copyright (C) 2014 MELFAS Inc.
 *
 *
 * Command Functions (Optional)
 *
 */

#include "melfas_mms400.h"

#if MMS_USE_CMD_MODE

#define NAME_OF_UNKNOWN_CMD "not_support_cmd"

enum CMD_STATUS {
	CMD_STATUS_WAITING = 0,
	CMD_STATUS_RUNNING,
	CMD_STATUS_OK,
	CMD_STATUS_FAIL,
	CMD_STATUS_NONE,
};

/**
* Clear command result
*/
static void cmd_clear_result(struct mms_ts_info *info)
{
	char delim = ':';
	memset(info->cmd_result, 0x00, ARRAY_SIZE(info->cmd_result));
	memcpy(info->cmd_result, info->cmd, strnlen(info->cmd, CMD_LEN));
	strncat(info->cmd_result, &delim, 1);
}

/**
* Set command result
*/
static void cmd_set_result(struct mms_ts_info *info, char *buf, int len)
{
	strncat(info->cmd_result, buf, len);
}

/**
* Command : Update firmware
*/
static void cmd_fw_update(void *device_data)
{
	struct mms_ts_info *info = (struct mms_ts_info *)device_data;
	char buf[64] = { 0 };

	int fw_location = info->cmd_param[0];

	cmd_clear_result(info);

	switch(fw_location){
		case 0:
			if(mms_fw_update_from_kernel(info)){
				goto ERROR;
			}
			break;
		case 1:
			if(mms_fw_update_from_storage(info, true)){
				goto ERROR;
			}
			break;
		default:
			goto ERROR;
			break;
	}

	sprintf(buf, "%s", "OK");
	info->cmd_state = CMD_STATUS_OK;
	goto EXIT;

ERROR:
	sprintf(buf, "%s", "NG");
	info->cmd_state = CMD_STATUS_FAIL;
	goto EXIT;

EXIT:
	cmd_set_result(info, buf, strnlen(buf, sizeof(buf)));
	dev_dbg(&info->client->dev, "%s - cmd[%s] len[%d] state[%d]\n", __func__, buf, strnlen(buf, sizeof(buf)), info->cmd_state);
}

/**
* Command : Get firmware version from MFSB file
*/
static void cmd_get_fw_ver_bin(void *device_data)
{
	struct mms_ts_info *info = (struct mms_ts_info *)device_data;
	char buf[64] = { 0 };

	const char *fw_name = INTERNAL_FW_PATH;
	const struct firmware *fw;
	struct mms_bin_hdr *fw_hdr;
	struct mms_fw_img **img;
	u8 ver_file[MMS_FW_MAX_SECT_NUM * 2];
	int i = 0;
	int offset = sizeof(struct mms_bin_hdr);

	cmd_clear_result(info);

	request_firmware(&fw, fw_name, &info->client->dev);

	if (!fw) {
		sprintf(buf, "%s", "NG");
		info->cmd_state = CMD_STATUS_FAIL;
		goto EXIT;
	}

	fw_hdr = (struct mms_bin_hdr *)fw->data;
	img = kzalloc(sizeof(*img) * fw_hdr->section_num, GFP_KERNEL);

	for (i = 0; i < fw_hdr->section_num; i++, offset += sizeof(struct mms_fw_img)) {
		img[i] = (struct mms_fw_img *)(fw->data + offset);
		ver_file[i * 2] = ((img[i]->version) >> 8) & 0xFF;
		ver_file[i * 2 + 1] = (img[i]->version) & 0xFF;
	}

	release_firmware(fw);

	sprintf(buf, "%02X.%02X %02X.%02X %02X.%02X %02X.%02X\n", ver_file[0], ver_file[1], ver_file[2], ver_file[3], ver_file[4], ver_file[5], ver_file[6], ver_file[7]);
	info->cmd_state = CMD_STATUS_OK;

EXIT:
	cmd_set_result(info, buf, strnlen(buf, sizeof(buf)));
	dev_dbg(&info->client->dev, "%s - cmd[%s] len[%d] state[%d]\n", __func__, buf, strnlen(buf, sizeof(buf)), info->cmd_state);
}

/**
* Command : Get firmware version from IC
*/
static void cmd_get_fw_ver_ic(void *device_data)
{
	struct mms_ts_info *info = (struct mms_ts_info *)device_data;
	char buf[64] = { 0 };
	u8 rbuf[64];

	cmd_clear_result(info);

	if(mms_get_fw_version(info, rbuf)){
		sprintf(buf, "%s", "NG");
		info->cmd_state = CMD_STATUS_FAIL;
		goto EXIT;
	}

	sprintf(buf, "%02X.%02X_%02X.%02X_%02X.%02X_%02X.%02X\n", rbuf[0], rbuf[1], rbuf[2], rbuf[3], rbuf[4], rbuf[5], rbuf[6], rbuf[7]);
	info->cmd_state = CMD_STATUS_OK;

EXIT:
	cmd_set_result(info, buf, strnlen(buf, sizeof(buf)));
	dev_dbg(&info->client->dev, "%s - cmd[%s] len[%d] state[%d]\n", __func__, buf, strnlen(buf, sizeof(buf)), info->cmd_state);
}

/**
* Command : Get chip vendor
*/
static void cmd_get_chip_vendor(void *device_data)
{
	struct mms_ts_info *info = (struct mms_ts_info *)device_data;
	char buf[64] = { 0 };
	//u8 rbuf[64];

	cmd_clear_result(info);

	sprintf(buf, "MELFAS");
	cmd_set_result(info, buf, strnlen(buf, sizeof(buf)));

	info->cmd_state = CMD_STATUS_OK;

	dev_dbg(&info->client->dev, "%s - cmd[%s] len[%d] state[%d]\n", __func__, buf, strnlen(buf, sizeof(buf)), info->cmd_state);
}

/**
* Command : Get chip name
*/
static void cmd_get_chip_name(void *device_data)
{
	struct mms_ts_info *info = (struct mms_ts_info *)device_data;
	char buf[64] = { 0 };
	//u8 rbuf[64];

	cmd_clear_result(info);

	sprintf(buf, CHIP_NAME);
	cmd_set_result(info, buf, strnlen(buf, sizeof(buf)));

	info->cmd_state = CMD_STATUS_OK;

	dev_dbg(&info->client->dev, "%s - cmd[%s] len[%d] state[%d]\n", __func__, buf, strnlen(buf, sizeof(buf)), info->cmd_state);
}

/**
* Command : Get X ch num
*/
static void cmd_get_x_num(void *device_data)
{
	struct mms_ts_info *info = (struct mms_ts_info *)device_data;
	char buf[64] = { 0 };
	u8 rbuf[64];
	u8 wbuf[64];
	int val;

	cmd_clear_result(info);

	wbuf[0] = MIP_R0_INFO;
	wbuf[1] = MIP_R1_INFO_NODE_NUM_X;
	if(mms_i2c_read(info, wbuf, 2, rbuf, 1)){
		info->cmd_state = CMD_STATUS_FAIL;
		goto EXIT;
	}

	val = rbuf[0];

	sprintf(buf, "%d", val);
	cmd_set_result(info, buf, strnlen(buf, sizeof(buf)));

	info->cmd_state = CMD_STATUS_OK;

EXIT:
	dev_dbg(&info->client->dev, "%s - cmd[%s] len[%d] state[%d]\n", __func__, buf, strnlen(buf, sizeof(buf)), info->cmd_state);
}

/**
* Command : Get Y ch num
*/
static void cmd_get_y_num(void *device_data)
{
	struct mms_ts_info *info = (struct mms_ts_info *)device_data;
	char buf[64] = { 0 };
	u8 rbuf[64];
	u8 wbuf[64];
	int val;

	cmd_clear_result(info);

	wbuf[0] = MIP_R0_INFO;
	wbuf[1] = MIP_R1_INFO_NODE_NUM_Y;
	if(mms_i2c_read(info, wbuf, 2, rbuf, 1)){
		info->cmd_state = CMD_STATUS_FAIL;
		goto EXIT;
	}

	val = rbuf[0];

	sprintf(buf, "%d", val);
	cmd_set_result(info, buf, strnlen(buf, sizeof(buf)));

	info->cmd_state = CMD_STATUS_OK;

EXIT:
	dev_dbg(&info->client->dev, "%s - cmd[%s] len[%d] state[%d]\n", __func__, buf, strnlen(buf, sizeof(buf)), info->cmd_state);
}

/**
* Command : Get X resolution
*/
static void cmd_get_max_x(void *device_data)
{
	struct mms_ts_info *info = (struct mms_ts_info *)device_data;
	char buf[64] = { 0 };
	u8 rbuf[64];
	u8 wbuf[64];
	int val;

	cmd_clear_result(info);

	wbuf[0] = MIP_R0_INFO;
	wbuf[1] = MIP_R1_INFO_RESOLUTION_X;
	if(mms_i2c_read(info, wbuf, 2, rbuf, 2)){
		info->cmd_state = CMD_STATUS_FAIL;
		goto EXIT;
	}

	val = (rbuf[0] << 8) | rbuf[1];

	sprintf(buf, "%d", val);
	cmd_set_result(info, buf, strnlen(buf, sizeof(buf)));

	info->cmd_state = CMD_STATUS_OK;

EXIT:
	dev_dbg(&info->client->dev, "%s - cmd[%s] len[%d] state[%d]\n", __func__, buf, strnlen(buf, sizeof(buf)), info->cmd_state);
}

/**
* Command : Get Y resolution
*/
static void cmd_get_max_y(void *device_data)
{
	struct mms_ts_info *info = (struct mms_ts_info *)device_data;
	char buf[64] = { 0 };
	u8 rbuf[64];
	u8 wbuf[64];
	int val;

	cmd_clear_result(info);

	wbuf[0] = MIP_R0_INFO;
	wbuf[1] = MIP_R1_INFO_RESOLUTION_Y;
	if(mms_i2c_read(info, wbuf, 2, rbuf, 2)){
		info->cmd_state = CMD_STATUS_FAIL;
		goto EXIT;
	}

	val = (rbuf[0] << 8) | rbuf[1];

	sprintf(buf, "%d", val);
	cmd_set_result(info, buf, strnlen(buf, sizeof(buf)));

	info->cmd_state = CMD_STATUS_OK;

EXIT:
	dev_dbg(&info->client->dev, "%s - cmd[%s] len[%d] state[%d]\n", __func__, buf, strnlen(buf, sizeof(buf)), info->cmd_state);
}

/**
* Command : Power off
*/
static void cmd_module_off_master(void *device_data)
{
	struct mms_ts_info *info = (struct mms_ts_info *)device_data;
	char buf[64] = { 0 };

	cmd_clear_result(info);

	if(mms_power_off(info)){
		sprintf(buf, "%s", "NG");
		info->cmd_state = CMD_STATUS_FAIL;
		goto EXIT;
	}

	sprintf(buf, "%s", "OK");
	info->cmd_state = CMD_STATUS_OK;

EXIT:
	cmd_set_result(info, buf, strnlen(buf, sizeof(buf)));
	dev_dbg(&info->client->dev, "%s - cmd[%s] len[%d] state[%d]\n", __func__, buf, strnlen(buf, sizeof(buf)), info->cmd_state);
}

/**
* Command : Power on
*/
static void cmd_module_on_master(void *device_data)
{
	struct mms_ts_info *info = (struct mms_ts_info *)device_data;
	char buf[64] = { 0 };

	cmd_clear_result(info);

	if(mms_power_on(info)){
		sprintf(buf, "%s", "NG");
		info->cmd_state = CMD_STATUS_FAIL;
		goto EXIT;
	}

	sprintf(buf, "%s", "OK");
	info->cmd_state = CMD_STATUS_OK;

EXIT:
	cmd_set_result(info, buf, strnlen(buf, sizeof(buf)));
	dev_dbg(&info->client->dev, "%s - cmd[%s] len[%d] state[%d]\n", __func__, buf, strnlen(buf, sizeof(buf)), info->cmd_state);
}

/**
* Command : Read intensity image
*/
static void cmd_read_intensity(void *device_data)
{
	struct mms_ts_info *info = (struct mms_ts_info *)device_data;
	char buf[64] = { 0 };

	int min = 999999;
	int max = -999999;
	int i = 0;

	cmd_clear_result(info);

	if(mms_get_image(info, MIP_IMG_TYPE_INTENSITY)){
		sprintf(buf, "%s", "NG");
		info->cmd_state = CMD_STATUS_FAIL;
		goto EXIT;
	}

	for(i = 0; i < (info->node_x * info->node_y); i++){
		if(info->image_buf[i] > max){
			max = info->image_buf[i];
		}
		if(info->image_buf[i] < min){
			min = info->image_buf[i];
		}
	}

	sprintf(buf, "%d,%d", min, max);
	info->cmd_state = CMD_STATUS_OK;

EXIT:
	cmd_set_result(info, buf, strnlen(buf, sizeof(buf)));
	dev_dbg(&info->client->dev, "%s - cmd[%s] len[%d] state[%d]\n", __func__, buf, strnlen(buf, sizeof(buf)), info->cmd_state);
}

/**
* Command : Get intensity data
*/
static void cmd_get_intensity(void *device_data)
{
	struct mms_ts_info *info = (struct mms_ts_info *)device_data;
	char buf[64] = { 0 };

	int x = info->cmd_param[0];
	int y = info->cmd_param[1];
	int idx = 0;

	cmd_clear_result(info);

	if((x < 0) || (x >= info->node_x) || (y < 0) || (y >= info->node_y)){
		sprintf(buf, "%s", "NG");
		info->cmd_state = CMD_STATUS_FAIL;
		goto EXIT;
	}

	idx = x + info->node_y * y;

	sprintf(buf, "%d", info->image_buf[idx]);
	info->cmd_state = CMD_STATUS_OK;

EXIT:
	cmd_set_result(info, buf, strnlen(buf, sizeof(buf)));
	dev_dbg(&info->client->dev, "%s - cmd[%s] len[%d] state[%d]\n", __func__, buf, strnlen(buf, sizeof(buf)), info->cmd_state);
}

/**
* Command : Read rawdata image
*/
static void cmd_read_rawdata(void *device_data)
{
	struct mms_ts_info *info = (struct mms_ts_info *)device_data;
	char buf[64] = { 0 };

	int min = 999999;
	int max = -999999;
	int i = 0;

	cmd_clear_result(info);

	if(mms_get_image(info, MIP_IMG_TYPE_RAWDATA)){
		sprintf(buf, "%s", "NG");
		info->cmd_state = CMD_STATUS_FAIL;
		goto EXIT;
	}

	for(i = 0; i < (info->node_x * info->node_y); i++){
		if(info->image_buf[i] > max){
			max = info->image_buf[i];
		}
		if(info->image_buf[i] < min){
			min = info->image_buf[i];
		}
	}

	sprintf(buf, "%d,%d", min, max);
	info->cmd_state = CMD_STATUS_OK;

EXIT:
	cmd_set_result(info, buf, strnlen(buf, sizeof(buf)));
	dev_dbg(&info->client->dev, "%s - cmd[%s] len[%d] state[%d]\n", __func__, buf, strnlen(buf, sizeof(buf)), info->cmd_state);
}

/**
* Command : Get rawdata
*/
static void cmd_get_rawdata(void *device_data)
{
	struct mms_ts_info *info = (struct mms_ts_info *)device_data;
	char buf[64] = { 0 };

	int x = info->cmd_param[0];
	int y = info->cmd_param[1];
	int idx = 0;

	cmd_clear_result(info);

	if((x < 0) || (x >= info->node_x) || (y < 0) || (y >= info->node_y)){
		sprintf(buf, "%s", "NG");
		info->cmd_state = CMD_STATUS_FAIL;
		goto EXIT;
	}

	idx = x + y * info->node_y;

	sprintf(buf, "%d", info->image_buf[idx]);
	info->cmd_state = CMD_STATUS_OK;

EXIT:
	cmd_set_result(info, buf, strnlen(buf, sizeof(buf)));
	dev_dbg(&info->client->dev, "%s - cmd[%s] len[%d] state[%d]\n", __func__, buf, strnlen(buf, sizeof(buf)), info->cmd_state);
}

/**
* Command : Run cm delta test
*/
static void cmd_run_test_cm_delta(void *device_data)
{
	struct mms_ts_info *info = (struct mms_ts_info *)device_data;
	char buf[64] = { 0 };

	int min = 999999;
	int max = -999999;
	int i = 0;

	cmd_clear_result(info);

	if(mms_run_test(info, MIP_TEST_TYPE_CM_DELTA)){
		sprintf(buf, "%s", "NG");
		info->cmd_state = CMD_STATUS_FAIL;
		goto EXIT;
	}

	for(i = 0; i < (info->node_x * info->node_y); i++){
		if(info->image_buf[i] > max){
			max = info->image_buf[i];
		}
		if(info->image_buf[i] < min){
			min = info->image_buf[i];
		}
	}

	sprintf(buf, "%d,%d", min, max);
	info->cmd_state = CMD_STATUS_OK;

EXIT:
	cmd_set_result(info, buf, strnlen(buf, sizeof(buf)));
	dev_dbg(&info->client->dev, "%s - cmd[%s] len[%d] state[%d]\n", __func__, buf, strnlen(buf, sizeof(buf)), info->cmd_state);
}

/**
* Command : Get result of cm delta test
*/
static void cmd_get_cm_delta(void *device_data)
{
	struct mms_ts_info *info = (struct mms_ts_info *)device_data;
	char buf[64] = { 0 };

	int x = info->cmd_param[0];
	int y = info->cmd_param[1];
	int idx = 0;

	cmd_clear_result(info);

	if((x < 0) || (x >= info->node_x) || (y < 0) || (y >= info->node_y)){
		sprintf(buf, "%s", "NG");
		info->cmd_state = CMD_STATUS_FAIL;
		goto EXIT;
	}

	idx = x + y * info->node_y;

	sprintf(buf, "%d", info->image_buf[idx]);
	info->cmd_state = CMD_STATUS_OK;

EXIT:
	cmd_set_result(info, buf, strnlen(buf, sizeof(buf)));
	dev_dbg(&info->client->dev, "%s - cmd[%s] len[%d] state[%d]\n", __func__, buf, strnlen(buf, sizeof(buf)), info->cmd_state);
}

/**
* Command : Run cm abs test
*/
static void cmd_run_test_cm_abs(void *device_data)
{
	struct mms_ts_info *info = (struct mms_ts_info *)device_data;
	char buf[64] = { 0 };

	int min = 999999;
	int max = -999999;
	int i = 0;

	cmd_clear_result(info);

	if(mms_run_test(info, MIP_TEST_TYPE_CM_ABS)){
		sprintf(buf, "%s", "NG");
		info->cmd_state = CMD_STATUS_FAIL;
		goto EXIT;
	}

	for(i = 0; i < (info->node_x * info->node_y); i++){
		if(info->image_buf[i] > max){
			max = info->image_buf[i];
		}
		if(info->image_buf[i] < min){
			min = info->image_buf[i];
		}
	}

	sprintf(buf, "%d,%d", min, max);
	info->cmd_state = CMD_STATUS_OK;

EXIT:
	cmd_set_result(info, buf, strnlen(buf, sizeof(buf)));
	dev_dbg(&info->client->dev, "%s - cmd[%s] len[%d] state[%d]\n", __func__, buf, strnlen(buf, sizeof(buf)), info->cmd_state);
}

/**
* Command : Get result of cm abs test
*/
static void cmd_get_cm_abs(void *device_data)
{
	struct mms_ts_info *info = (struct mms_ts_info *)device_data;
	char buf[64] = { 0 };

	int x = info->cmd_param[0];
	int y = info->cmd_param[1];
	int idx = 0;

	cmd_clear_result(info);

	if((x < 0) || (x >= info->node_x) || (y < 0) || (y >= info->node_y)){
		sprintf(buf, "%s", "NG");
		info->cmd_state = CMD_STATUS_FAIL;
		goto EXIT;
	}

	idx = x + y * info->node_y;

	sprintf(buf, "%d", info->image_buf[idx]);
	info->cmd_state = CMD_STATUS_OK;

EXIT:
	cmd_set_result(info, buf, strnlen(buf, sizeof(buf)));
	dev_dbg(&info->client->dev, "%s - cmd[%s] len[%d] state[%d]\n", __func__, buf, strnlen(buf, sizeof(buf)), info->cmd_state);
}

/**
* Command : Unknown cmd
*/
static void cmd_unknown_cmd(void *device_data)
{
	struct mms_ts_info *info = (struct mms_ts_info *)device_data;
	char buf[16] = { 0 };

	cmd_clear_result(info);

	snprintf(buf, sizeof(buf), "%s", NAME_OF_UNKNOWN_CMD);
	cmd_set_result(info, buf, strnlen(buf, sizeof(buf)));

	info->cmd_state = CMD_STATUS_NONE;

	dev_dbg(&info->client->dev, "%s - cmd[%s] len[%d] state[%d]\n", __func__, buf, strnlen(buf, sizeof(buf)), info->cmd_state);
}

#define MMS_CMD(name, func)	.cmd_name = name, .cmd_func = func

/**
* Info of command function
*/
struct mms_cmd {
	struct list_head list;
	const char *cmd_name;
	void (*cmd_func) (void *device_data);
};

/**
* List of command functions
*/
static struct mms_cmd mms_commands[] = {
	{MMS_CMD("fw_update", cmd_fw_update),},
	{MMS_CMD("get_fw_ver_bin", cmd_get_fw_ver_bin),},
	{MMS_CMD("get_fw_ver_ic", cmd_get_fw_ver_ic),},
	{MMS_CMD("get_chip_vendor", cmd_get_chip_vendor),},
	{MMS_CMD("get_chip_name", cmd_get_chip_name),},
	{MMS_CMD("get_x_num", cmd_get_x_num),},
	{MMS_CMD("get_y_num", cmd_get_y_num),},
	{MMS_CMD("get_max_x", cmd_get_max_x),},
	{MMS_CMD("get_max_y", cmd_get_max_y),},
	{MMS_CMD("module_off_master", cmd_module_off_master),},
	{MMS_CMD("module_on_master", cmd_module_on_master),},
	{MMS_CMD("run_intensity_read", cmd_read_intensity),},
	{MMS_CMD("get_intensity", cmd_get_intensity),},
	{MMS_CMD("run_rawdata_read", cmd_read_rawdata),},
	{MMS_CMD("get_rawdata", cmd_get_rawdata),},
	{MMS_CMD("run_inspection_read", cmd_run_test_cm_delta),},
	{MMS_CMD("get_inspection", cmd_get_cm_delta),},
	{MMS_CMD("run_cm_delta_read", cmd_run_test_cm_delta),},
	{MMS_CMD("get_cm_delta", cmd_get_cm_delta),},
	{MMS_CMD("run_cm_abs_read", cmd_run_test_cm_abs),},
	{MMS_CMD("get_cm_abs", cmd_get_cm_abs),},

	{MMS_CMD("get_config_ver", cmd_unknown_cmd),},
	{MMS_CMD("get_threshold", cmd_unknown_cmd),},
	{MMS_CMD("module_off_slave", cmd_unknown_cmd),},
	{MMS_CMD("module_on_slave", cmd_unknown_cmd),},
	{MMS_CMD(NAME_OF_UNKNOWN_CMD, cmd_unknown_cmd),},
};

/**
* Sysfs - recv command
*/
static ssize_t mms_sys_cmd(struct device *dev, struct device_attribute *devattr, const char *buf, size_t count)
{
	struct mms_ts_info *info = dev_get_drvdata(dev);
	int ret;
	char *cur, *start, *end;
	char cbuf[CMD_LEN] = { 0 };
	int len, i;
	struct mms_cmd *mms_cmd_ptr = NULL;
	char delim = ',';
	bool cmd_found = false;
	int param_cnt = 0;

	dev_dbg(&info->client->dev, "%s [START]\n", __func__);
	dev_dbg(&info->client->dev, "%s - input [%s]\n", __func__, buf);

	if (!info) {
		dev_err(&info->client->dev, "%s [ERROR] mms_ts_info not found\n", __func__);
		ret = -EINVAL;
		goto ERROR;
	}

	if (!info->input_dev) {
		dev_err(&info->client->dev, "%s [ERROR] input_dev not found\n", __func__);
		ret = -EINVAL;
		goto ERROR;
	}

	if (info->cmd_busy == true) {
		dev_err(&info->client->dev, "%s [ERROR] previous command is not ended\n", __func__);
		ret = -1;
		goto ERROR;
	}

	mutex_lock(&info->lock);
	info->cmd_busy = true;
	mutex_unlock(&info->lock);

	info->cmd_state = 1;
	for (i = 0; i < ARRAY_SIZE(info->cmd_param); i++){
		info->cmd_param[i] = 0;
	}

	len = (int)count;
	if (*(buf + len - 1) == '\n'){
		len--;
	}

	memset(info->cmd, 0x00, ARRAY_SIZE(info->cmd));
	memcpy(info->cmd, buf, len);
	cur = strchr(buf, (int)delim);
	if (cur){
		memcpy(cbuf, buf, cur - buf);
	}
	else{
		memcpy(cbuf, buf, len);
	}
	dev_dbg(&info->client->dev, "%s - command [%s]\n", __func__, cbuf);

	//command
	list_for_each_entry(mms_cmd_ptr, &info->cmd_list_head, list) {
		if (!strncmp(cbuf, mms_cmd_ptr->cmd_name, CMD_LEN)) {
			cmd_found = true;
			break;
		}
	}
	if (!cmd_found) {
		list_for_each_entry(mms_cmd_ptr, &info->cmd_list_head, list) {
			if (!strncmp(NAME_OF_UNKNOWN_CMD, mms_cmd_ptr->cmd_name, CMD_LEN)){
				break;
			}
		}
	}

	//parameter
	if (cur && cmd_found) {
		cur++;
		start = cur;
		memset(cbuf, 0x00, ARRAY_SIZE(cbuf));

		do {
			if (*cur == delim || cur - buf == len) {
				end = cur;
				memcpy(cbuf, start, end - start);
				*(cbuf + strnlen(cbuf, ARRAY_SIZE(cbuf))) = '\0';
				if (kstrtoint(cbuf, 10, info->cmd_param + param_cnt) < 0){
					goto ERROR;
				}
				start = cur + 1;
				memset(cbuf, 0x00, ARRAY_SIZE(cbuf));
				param_cnt++;
			}
			cur++;
		} while (cur - buf <= len);
	}

	//print
	dev_dbg(&info->client->dev, "%s - cmd [%s]\n", __func__, mms_cmd_ptr->cmd_name);
	for (i = 0; i < param_cnt; i++){
		dev_dbg(&info->client->dev, "%s - param #%d [%d]\n", __func__, i, info->cmd_param[i]);
	}

	//execute
	mms_cmd_ptr->cmd_func(info);

	dev_dbg(&info->client->dev, "%s [DONE]\n", __func__);
	return count;

ERROR:
	dev_err(&info->client->dev, "%s [ERROR]\n", __func__);
	return count;
}
static DEVICE_ATTR(cmd, 0666, NULL, mms_sys_cmd);

/**
* Sysfs - print command status
*/
static ssize_t mms_sys_cmd_status(struct device *dev, struct device_attribute *devattr, char *buf)
{
	struct mms_ts_info *info = dev_get_drvdata(dev);
	int ret;
	char cbuf[32] = {0};

	dev_dbg(&info->client->dev, "%s [START]\n", __func__);

	dev_dbg(&info->client->dev, "%s - status [%d]\n", __func__, info->cmd_state);

	if (info->cmd_state == CMD_STATUS_WAITING){
		snprintf(cbuf, sizeof(cbuf), "WAITING");
	}
	else if (info->cmd_state == CMD_STATUS_RUNNING){
		snprintf(cbuf, sizeof(cbuf), "RUNNING");
	}
	else if (info->cmd_state == CMD_STATUS_OK){
		snprintf(cbuf, sizeof(cbuf), "OK");
	}
	else if (info->cmd_state == CMD_STATUS_FAIL){
		snprintf(cbuf, sizeof(cbuf), "FAIL");
	}
	else if (info->cmd_state == CMD_STATUS_NONE){
		snprintf(cbuf, sizeof(cbuf), "NOT_APPLICABLE");
	}

	ret = snprintf(buf, PAGE_SIZE, "%s\n", cbuf);
	//memset(info->print_buf, 0, 4096);

	dev_dbg(&info->client->dev, "%s [DONE]\n", __func__);

	return ret;
}
static DEVICE_ATTR(cmd_status, 0666, mms_sys_cmd_status, NULL);

/**
* Sysfs - print command result
*/
static ssize_t mms_sys_cmd_result(struct device *dev, struct device_attribute *devattr, char *buf)
{
	struct mms_ts_info *info = dev_get_drvdata(dev);
	int ret;

	dev_dbg(&info->client->dev, "%s [START]\n", __func__);

	dev_dbg(&info->client->dev, "%s - result [%s]\n", __func__, info->cmd_result);

	mutex_lock(&info->lock);
	info->cmd_busy = false;
	mutex_unlock(&info->lock);

	info->cmd_state = CMD_STATUS_WAITING;

//EXIT:
	ret = snprintf(buf, PAGE_SIZE, "%s\n", info->cmd_result);
	//memset(info->print_buf, 0, 4096);

	dev_dbg(&info->client->dev, "%s [DONE]\n", __func__);

	return ret;
}
static DEVICE_ATTR(cmd_result, 0666, mms_sys_cmd_result, NULL);

/**
* Sysfs - print command list
*/
static ssize_t mms_sys_cmd_list(struct device *dev, struct device_attribute *attr, char *buf)
{
	struct mms_ts_info *info = dev_get_drvdata(dev);
	int ret;
	int i = 0;
	char buffer[info->cmd_buffer_size];
	char buffer_name[CMD_LEN];

	dev_dbg(&info->client->dev, "%s [START]\n", __func__);

	snprintf(buffer, 30, "== Command list ==\n");
	while (strncmp(mms_commands[i].cmd_name, NAME_OF_UNKNOWN_CMD, CMD_LEN) != 0) {
		snprintf(buffer_name, CMD_LEN, "%s\n", mms_commands[i].cmd_name);
		strcat(buffer, buffer_name);
		i++;
	}

	dev_dbg(&info->client->dev, "%s - length [%u / %d]\n", __func__, strlen(buffer), info->cmd_buffer_size);

	ret = snprintf(buf, PAGE_SIZE, "%s\n", buffer);
	//memset(info->print_buf, 0, 4096);

	dev_dbg(&info->client->dev, "%s [DONE]\n", __func__);

	return ret;
}
static DEVICE_ATTR(cmd_list, 0666, mms_sys_cmd_list, NULL);

/**
* Sysfs - cmd attr info
*/
static struct attribute *mms_cmd_attr[] = {
	&dev_attr_cmd.attr,
	&dev_attr_cmd_status.attr,
	&dev_attr_cmd_result.attr,
	&dev_attr_cmd_list.attr,
	NULL,
};

/**
* Sysfs - cmd attr group info
*/
static const struct attribute_group mms_cmd_attr_group = {
	.attrs = mms_cmd_attr,
};

extern struct class *melfas_class;

/**
* Create sysfs command functions
*/
int mms_sysfs_cmd_create(struct mms_ts_info *info)
{
	struct i2c_client *client = info->client;
	int i = 0;

	//init cmd list
	INIT_LIST_HEAD(&info->cmd_list_head);
	info->cmd_buffer_size = 0;

	for (i = 0; i < ARRAY_SIZE(mms_commands); i++) {
		list_add_tail(&mms_commands[i].list, &info->cmd_list_head);
		if(mms_commands[i].cmd_name){
			info->cmd_buffer_size += strlen(mms_commands[i].cmd_name) + 1;
		}
	}

	info->cmd_busy = false;
	info->print_buf = kzalloc(sizeof(u8) * 4096, GFP_KERNEL);

	//create sysfs
	if (sysfs_create_group(&client->dev.kobj, &mms_cmd_attr_group)) {
		dev_err(&client->dev, "%s [ERROR] sysfs_create_group\n", __func__);
		return -EAGAIN;
	}

	//create class
	/*
	info->cmd_class = class_create(THIS_MODULE, "melfas");
	info->cmd_dev = device_create(info->cmd_class, NULL, info->cmd_dev_t, NULL, "touchscreen");
	*/
	/*
	info->cmd_dev = device_create(melfas_class, NULL, info->cmd_dev_t, NULL, "touchscreen");
	*/
	/*
	if (sysfs_create_group(&info->cmd_dev->kobj, &mms_cmd_attr_group)) {
		dev_err(&client->dev, "%s [ERROR] sysfs_create_group\n", __func__);
		return -EAGAIN;
	}
	*/

	return 0;
}

/**
* Remove sysfs command functions
*/
void mms_sysfs_cmd_remove(struct mms_ts_info *info)
{
	sysfs_remove_group(&info->client->dev.kobj, &mms_cmd_attr_group);

	kfree(info->print_buf);

	return;
}

#endif

