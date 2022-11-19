// SPDX-License-Identifier: GPL-2.0-only
/*
 * Copyright (C) 2018-2020 Oplus. All rights reserved.
 */

#include <linux/gpio.h>
#include <linux/crc32.h>
#include <linux/firmware.h>
#include "synaptics_tcm_core.h"

#define FW_IMAGE_NAME "synaptics/hdl_firmware.img"

#define BOOT_CONFIG_ID "BOOT_CONFIG"

#define F35_APP_CODE_ID "F35_APP_CODE"

#define ROMBOOT_APP_CODE_ID "ROMBOOT_APP_CODE"

#define OPEN_SHORT_ID "OPENSHORT"

#define RESERVED_BYTES 14

#define APP_CONFIG_ID "APP_CONFIG"

#define DISP_CONFIG_ID "DISPLAY"

#define IMAGE_FILE_MAGIC_VALUE 0x4818472b

#define FLASH_AREA_MAGIC_VALUE 0x7c05e516

#define PDT_START_ADDR 0x00e9

#define PDT_END_ADDR 0x00ee

#define UBL_FN_NUMBER 0x35

#define F35_CTRL3_OFFSET 18

#define F35_CTRL7_OFFSET 22

#define F35_WRITE_FW_TO_PMEM_COMMAND 4

#define RESET_TO_HDL_DELAY_MS 12

#ifdef CONFIG_TOUCHPANEL_SYNAPTICS_TD4330_NOFLASH
#define DOWNLOAD_APP_FAST_RETRY

#define DOWNLOAD_RETRY_COUNT 50
#else
#define DOWNLOAD_RETRY_COUNT 10
#endif

enum f35_error_code {
	SUCCESS = 0,
	UNKNOWN_FLASH_PRESENT,
	MAGIC_NUMBER_NOT_PRESENT,
	INVALID_BLOCK_NUMBER,
	BLOCK_NOT_ERASED,
	NO_FLASH_PRESENT,
	CHECKSUM_FAILURE,
	WRITE_FAILURE,
	INVALID_COMMAND,
	IN_DEBUG_MODE,
	INVALID_HEADER,
	REQUESTING_FIRMWARE,
	INVALID_CONFIGURATION,
	DISABLE_BLOCK_PROTECT_FAILURE,
};

enum config_download {
	HDL_INVALID = 0,
	HDL_TOUCH_CONFIG_TO_PMEM,
	HDL_DISPLAY_CONFIG_TO_PMEM,
	HDL_DISPLAY_CONFIG_TO_RAM,
};

struct zeroflash_hcd *g_zeroflash_hcd = NULL;

int zeroflash_init_done = 0;
int check_uboot_failed_count = 0;

int zeroflash_parse_fw_image(void)
{
	unsigned int idx;
	unsigned int addr;
	unsigned int offset;
	unsigned int length;
	unsigned int checksum;
	unsigned int flash_addr;
	unsigned int magic_value;
	unsigned int num_of_areas;
	struct zeroflash_image_header *header;
	struct image_info *image_info;
	struct area_descriptor *descriptor;
	//struct syna_tcm_hcd *tcm_hcd = g_zeroflash_hcd->tcm_hcd;
	const unsigned char *image;
	const unsigned char *content;

	image = g_zeroflash_hcd->image;
	image_info = &g_zeroflash_hcd->image_info;
	header = (struct zeroflash_image_header *)image;

	magic_value = le4_to_uint(header->magic_value);
	if (magic_value != IMAGE_FILE_MAGIC_VALUE) {
		TPD_INFO(
			"Invalid image file magic value\n");
		return -EINVAL;
	}

	memset(image_info, 0x00, sizeof(*image_info));

	offset = sizeof(*header);
	num_of_areas = le4_to_uint(header->num_of_areas);

	for (idx = 0; idx < num_of_areas; idx++) {
		addr = le4_to_uint(image + offset);
		descriptor = (struct area_descriptor *)(image + addr);
		offset += 4;

		magic_value = le4_to_uint(descriptor->magic_value);
		if (magic_value != FLASH_AREA_MAGIC_VALUE) {
			continue;
		}

		length = le4_to_uint(descriptor->length);
		content = (unsigned char *)descriptor + sizeof(*descriptor);
		flash_addr = le4_to_uint(descriptor->flash_addr_words) * 2;
		checksum = le4_to_uint(descriptor->checksum);

		if (0 == strncmp((char *)descriptor->id_string,
				 BOOT_CONFIG_ID,
				 strlen(BOOT_CONFIG_ID))) {
			if (checksum != (crc32(~0, content, length) ^ ~0)) {
				TPD_INFO(
					"Boot config checksum error\n");
				return -EINVAL;
			}
			image_info->boot_config.size = length;
			image_info->boot_config.data = content;
			image_info->boot_config.flash_addr = flash_addr;
			TPD_INFO(
				"Boot config size = %d\n",
				length);
			TPD_INFO(
				"Boot config flash address = 0x%08x\n",
				flash_addr);
		} else if (0 == strncmp((char *)descriptor->id_string,
					F35_APP_CODE_ID,
					strlen(F35_APP_CODE_ID))) {
			if (checksum != (crc32(~0, content, length) ^ ~0)) {
				TPD_INFO(
					"Application firmware checksum error\n");
				return -EINVAL;
			}
			image_info->app_firmware.size = length;
			image_info->app_firmware.data = content;
			image_info->app_firmware.flash_addr = flash_addr;
			TPD_INFO(
				"Application firmware size = %d\n",
				length);
			TPD_INFO(
				"Application firmware flash address = 0x%08x\n",
				flash_addr);
		}  else if ((0 == strncmp((char *)descriptor->id_string,
					  ROMBOOT_APP_CODE_ID,
					  strlen(ROMBOOT_APP_CODE_ID)))) {
			if (checksum != (crc32(~0, content, length) ^ ~0)) {
				TPD_INFO(
					"HDL_ROMBoot firmware checksum error\n");
				return -EINVAL;
			}
			image_info->app_firmware.size = length;
			image_info->app_firmware.data = content;
			image_info->app_firmware.flash_addr = flash_addr;
			TPD_INFO(
				"HDL_ROMBoot firmware size = %d\n",
				length);
			TPD_INFO(
				"HDL_ROMBoot firmware flash address = 0x%08x\n",
				flash_addr);

		} else if (0 == strncmp((char *)descriptor->id_string,
					APP_CONFIG_ID,
					strlen(APP_CONFIG_ID))) {
			if (checksum != (crc32(~0, content, length) ^ ~0)) {
				TPD_INFO(
					"Application config checksum error\n");
				return -EINVAL;
			}
			image_info->app_config.size = length;
			image_info->app_config.data = content;
			image_info->app_config.flash_addr = flash_addr;
			image_info->packrat_number = le4_to_uint(&content[14]);
			TPD_INFO(
				"Application config size = %d\n",
				length);
			TPD_INFO(
				"Application config flash address = 0x%08x\n",
				flash_addr);
		} else if (0 == strncmp((char *)descriptor->id_string,
					DISP_CONFIG_ID,
					strlen(DISP_CONFIG_ID))) {
			if (checksum != (crc32(~0, content, length) ^ ~0)) {
				TPD_INFO(
					"Display config checksum error\n");
				return -EINVAL;
			}
			image_info->disp_config.size = length;
			image_info->disp_config.data = content;
			image_info->disp_config.flash_addr = flash_addr;
			TPD_INFO(
				"Display config size = %d\n",
				length);
			TPD_INFO(
				"Display config flash address = 0x%08x\n",
				flash_addr);
		}
#if 0
		else if (0 == strncmp((char *)descriptor->id_string,
				      OPEN_SHORT_ID, strlen(OPEN_SHORT_ID))) {

			if (checksum != (crc32(~0, content, length) ^ ~0)) {
				TPD_INFO(
					"open_short config checksum error\n");
				return -EINVAL;
			}
			image_info->open_short_config.size = length;
			image_info->open_short_config.data = content;
			image_info->open_short_config.flash_addr = flash_addr;
			TPD_INFO(
				"open_short config size = %d\n",
				length);
			TPD_INFO(
				"open_short config flash address = 0x%08x\n",
				flash_addr);
		}
#endif
	}

	return 0;
}

static int zeroflash_get_fw_image(void)
{
	int retval = 0;
	struct syna_tcm_hcd *tcm_hcd = g_zeroflash_hcd->tcm_hcd;
	struct touchpanel_data *ts = spi_get_drvdata(tcm_hcd->s_client);
	struct firmware *request_fw_headfile = NULL;

	if(!g_zeroflash_hcd->fw_entry) {
		TPD_INFO("oplus tp update can't get fw, get fw from headfile\n");
		request_fw_headfile = vmalloc(sizeof(struct firmware));
		if (request_fw_headfile == NULL) {
			TPD_INFO("%s kzalloc failed!\n", __func__);
			return -1;
		} else if (tcm_hcd->p_firmware_headfile->firmware_data) {
			request_fw_headfile->size = tcm_hcd->p_firmware_headfile->firmware_size;
			request_fw_headfile->data = tcm_hcd->p_firmware_headfile->firmware_data;
			g_zeroflash_hcd->fw_entry = request_fw_headfile;
			tcm_hcd->tp_fw_update_headfile = true;
		}
	}

	if (g_zeroflash_hcd->fw_entry != NULL) {
		TPD_INFO(
			"Firmware image size = %d\n",
			(unsigned int)g_zeroflash_hcd->fw_entry->size);

		g_zeroflash_hcd->image = g_zeroflash_hcd->fw_entry->data;
	} else {
		TPD_INFO("null fw entry return\n");
		vfree(request_fw_headfile);
		return -1;
	}
	if(!tcm_hcd->tp_fw_update_parse) {
		return 0;
	}

	retval = zeroflash_parse_fw_image();
	if (retval < 0) {
		TPD_INFO(
			"Failed to parse firmware image\n");
		if (g_zeroflash_hcd->fw_entry != NULL) {
			release_firmware(g_zeroflash_hcd->fw_entry);
			g_zeroflash_hcd->fw_entry = NULL;
			g_zeroflash_hcd->image = NULL;
		}
		return retval;
	}
	tcm_hcd->tp_fw_update_parse = false;
	return 0;
}

void zeroflash_check_download_config(void)
{
	struct firmware_status *fw_status;
	struct syna_tcm_hcd *tcm_hcd = g_zeroflash_hcd->tcm_hcd;

	fw_status = &g_zeroflash_hcd->fw_status;

	TPD_DETAIL("fw status:need app:%d need display:%d\n", g_zeroflash_hcd->fw_status.need_app_config,
		   g_zeroflash_hcd->fw_status.need_disp_config);
	if (!fw_status->need_app_config && !fw_status->need_disp_config) {
		/*
		if (atomic_read(&tcm_hcd->helper.task) == HELP_NONE) {
		    atomic_set(&tcm_hcd->helper.task,
		            HELP_SEND_RESET_NOTIFICATION);
		    queue_work(tcm_hcd->helper.workqueue,
		            &tcm_hcd->helper.work);
		}*/
		TPD_INFO(
			"zero reflash done..............\n");

		atomic_set(&tcm_hcd->host_downloading, 0);
		syna_tcm_hdl_done(tcm_hcd);
		if (tcm_hcd->cb.async_work) {
			tcm_hcd->cb.async_work();
		}
		complete(&tcm_hcd->config_complete);
		//wake_up_interruptible(&tcm_hcd->hdl_wq);
		return;
	}
	//msleep(50);
	queue_work(g_zeroflash_hcd->config_workqueue, &g_zeroflash_hcd->config_work);

	return;
}
void zeroflash_download_config(void)
{
	int retval;
	struct firmware_status *fw_status;
	struct syna_tcm_hcd *tcm_hcd = g_zeroflash_hcd->tcm_hcd;

	fw_status = &g_zeroflash_hcd->fw_status;
	retval = secure_memcpy((unsigned char *)fw_status, sizeof(g_zeroflash_hcd->fw_status),
			       tcm_hcd->report.buffer.buf,
			       tcm_hcd->report.buffer.buf_size,
			       sizeof(g_zeroflash_hcd->fw_status));
	if (retval < 0) {
		TPD_INFO("Failed to copy fw status\n");
	}

	TPD_INFO("zeroflash_download_config fw status:need app:%d need display:%d\n", g_zeroflash_hcd->fw_status.need_app_config,
		 g_zeroflash_hcd->fw_status.need_disp_config);
	if (!fw_status->need_app_config && !fw_status->need_disp_config) {
		/*
		if (atomic_read(&tcm_hcd->helper.task) == HELP_NONE) {
		    atomic_set(&tcm_hcd->helper.task,
		            HELP_SEND_RESET_NOTIFICATION);
		    queue_work(tcm_hcd->helper.workqueue,
		            &tcm_hcd->helper.work);
		}*/
		TPD_INFO(
			"zero reflash done..............\n");
		atomic_set(&tcm_hcd->host_downloading, 0);
		//wake_up_interruptible(&tcm_hcd->hdl_wq);
		return;
	}
	queue_work(g_zeroflash_hcd->config_workqueue, &g_zeroflash_hcd->config_work);

	return;
}

void zeroflash_download_firmware(void)
{
	struct syna_tcm_hcd *tcm_hcd = g_zeroflash_hcd->tcm_hcd;
	if (zeroflash_init_done) {
		if (tcm_hcd->id_info.mode != MODE_ROMBOOTLOADER) {
			if (g_zeroflash_hcd->tcm_hcd->health_monitor_support) {
				g_zeroflash_hcd->tcm_hcd->monitor_data->reserve1++;
			}
			TPD_INFO(
				"uboot check fail\n");
			msleep(50);
			enable_irq(g_zeroflash_hcd->tcm_hcd->s_client->irq);
			return;
		}

		queue_work(g_zeroflash_hcd->firmware_workqueue, &g_zeroflash_hcd->firmware_work);
	} else {
	}
	return;
}

static int zeroflash_download_disp_config(void)
{
	int retval;
	unsigned char response_code;
	struct image_info *image_info;
	struct syna_tcm_hcd *tcm_hcd = g_zeroflash_hcd->tcm_hcd;
	static unsigned int retry_count;

	TPD_DETAIL(
		"Downloading display config\n");

	image_info = &g_zeroflash_hcd->image_info;

	if (image_info->disp_config.size == 0) {
		TPD_INFO(
			"No display config in image file\n");
		return -EINVAL;
	}

	LOCK_BUFFER(g_zeroflash_hcd->out);

	retval = syna_tcm_alloc_mem(tcm_hcd,
				    &g_zeroflash_hcd->out,
				    image_info->disp_config.size + 2);
	if (retval < 0) {
		TPD_INFO(
			"Failed to allocate memory for g_zeroflash_hcd->out.buf\n");
		goto unlock_out;
	}

	switch (g_zeroflash_hcd->fw_status.hdl_version) {
	case 0:
	case 1:
		g_zeroflash_hcd->out.buf[0] = 1;
		break;
	case 2:
		g_zeroflash_hcd->out.buf[0] = 2;
		break;
	default:
		retval = -EINVAL;
		TPD_INFO("Invalid HDL version (%d)\n",
			 g_zeroflash_hcd->fw_status.hdl_version);
		goto unlock_out;
	}
	g_zeroflash_hcd->out.buf[1] = HDL_DISPLAY_CONFIG_TO_PMEM;

	retval = secure_memcpy(&g_zeroflash_hcd->out.buf[2],
			       g_zeroflash_hcd->out.buf_size - 2,
			       image_info->disp_config.data,
			       image_info->disp_config.size,
			       image_info->disp_config.size);
	if (retval < 0) {
		TPD_INFO(
			"Failed to copy display config data\n");
		goto unlock_out;
	}

	g_zeroflash_hcd->out.data_length = image_info->disp_config.size + 2;

	LOCK_BUFFER(g_zeroflash_hcd->resp);

	retval = tcm_hcd->write_message(tcm_hcd,
					CMD_DOWNLOAD_CONFIG,
					g_zeroflash_hcd->out.buf,
					g_zeroflash_hcd->out.data_length,
					&g_zeroflash_hcd->resp.buf,
					&g_zeroflash_hcd->resp.buf_size,
					&g_zeroflash_hcd->resp.data_length,
					&response_code,
					0);
	if (retval < 0) {
		TPD_INFO(
			"Failed to write command %s\n",
			STR(CMD_DOWNLOAD_CONFIG));
		if (response_code != STATUS_ERROR) {
			goto unlock_resp;
		}
		retry_count++;
		if (DOWNLOAD_RETRY_COUNT && retry_count > DOWNLOAD_RETRY_COUNT) {
			goto unlock_resp;
		}
	} else {
		retry_count = 0;
	}

	retval = secure_memcpy((unsigned char *)&g_zeroflash_hcd->fw_status,
			       sizeof(g_zeroflash_hcd->fw_status),
			       g_zeroflash_hcd->resp.buf,
			       g_zeroflash_hcd->resp.buf_size,
			       sizeof(g_zeroflash_hcd->fw_status));
	TPD_INFO("zeroflash_download_disp_config fw status:need app:%d need display:%d\n", g_zeroflash_hcd->fw_status.need_app_config,
		 g_zeroflash_hcd->fw_status.need_disp_config);

	if (retval < 0) {
		TPD_INFO(
			"Failed to copy firmware status\n");
		goto unlock_resp;
	}

	TPD_INFO("Display config downloaded\n");

	retval = 0;

unlock_resp:
	UNLOCK_BUFFER(g_zeroflash_hcd->resp);

unlock_out:
	UNLOCK_BUFFER(g_zeroflash_hcd->out);

	return retval;
}

static int zeroflash_download_app_config(void)
{
	int retval;
	unsigned char padding;
	unsigned char response_code;
	struct image_info *image_info;
	struct syna_tcm_hcd *tcm_hcd = g_zeroflash_hcd->tcm_hcd;
	static unsigned int retry_count;

	TPD_DETAIL("Downloading application config: version:%d\n", g_zeroflash_hcd->fw_status.hdl_version);

	image_info = &g_zeroflash_hcd->image_info;

	if (image_info->app_config.size == 0) {
		TPD_INFO(
			"No application config in image file\n");
		return -EINVAL;
	}

	padding = image_info->app_config.size % 8;
	if (padding) {
		padding = 8 - padding;
	}

	LOCK_BUFFER(g_zeroflash_hcd->out);

	retval = syna_tcm_alloc_mem(tcm_hcd,
				    &g_zeroflash_hcd->out,
				    image_info->app_config.size + 2 + padding);
	if (retval < 0) {
		TPD_INFO(
			"Failed to allocate memory for g_zeroflash_hcd->out.buf\n");
		goto unlock_out;
	}
	switch (g_zeroflash_hcd->fw_status.hdl_version) {
	case 0:
	case 1:
		g_zeroflash_hcd->out.buf[0] = 1;
		break;
	case 2:
		g_zeroflash_hcd->out.buf[0] = 2;
		break;
	default:
		retval = -EINVAL;
		TPD_INFO("Invalid HDL version (%d)\n",
			 g_zeroflash_hcd->fw_status.hdl_version);
		goto unlock_out;
	}

	//g_zeroflash_hcd->out.buf[0] = 1;
	g_zeroflash_hcd->out.buf[1] = HDL_TOUCH_CONFIG_TO_PMEM;

	retval = secure_memcpy(&g_zeroflash_hcd->out.buf[2],
			       g_zeroflash_hcd->out.buf_size - 2,
			       image_info->app_config.data,
			       image_info->app_config.size,
			       image_info->app_config.size);
	if (retval < 0) {
		TPD_INFO(
			"Failed to copy application config data\n");
		goto unlock_out;
	}

	g_zeroflash_hcd->out.data_length = image_info->app_config.size + 2;
	g_zeroflash_hcd->out.data_length += padding;

	LOCK_BUFFER(g_zeroflash_hcd->resp);

	retval = tcm_hcd->write_message(tcm_hcd,
					CMD_DOWNLOAD_CONFIG,
					g_zeroflash_hcd->out.buf,
					g_zeroflash_hcd->out.data_length,
					&g_zeroflash_hcd->resp.buf,
					&g_zeroflash_hcd->resp.buf_size,
					&g_zeroflash_hcd->resp.data_length,
					&response_code,
					0);
	if (retval < 0) {
		TPD_INFO(
			"Failed to write command %s\n",
			STR(CMD_DOWNLOAD_CONFIG));
		if (response_code != STATUS_ERROR) {
			goto unlock_resp;
		}
		retry_count++;
		if (DOWNLOAD_RETRY_COUNT && retry_count > DOWNLOAD_RETRY_COUNT) {
			goto unlock_resp;
		}
	} else {
		retry_count = 0;
	}

	retval = secure_memcpy((unsigned char *)&g_zeroflash_hcd->fw_status,
			       sizeof(g_zeroflash_hcd->fw_status),
			       g_zeroflash_hcd->resp.buf,
			       g_zeroflash_hcd->resp.buf_size,
			       sizeof(g_zeroflash_hcd->fw_status));


	TPD_INFO("zeroflash_download_app_config fw status:need app:%d need display:%d\n", g_zeroflash_hcd->fw_status.need_app_config,
		 g_zeroflash_hcd->fw_status.need_disp_config);

	if (retval < 0) {
		TPD_INFO(
			"Failed to copy firmware status\n");
		goto unlock_resp;
	}

	TPD_INFO("Application config downloaded\n");

	retval = 0;

unlock_resp:
	UNLOCK_BUFFER(g_zeroflash_hcd->resp);

unlock_out:
	UNLOCK_BUFFER(g_zeroflash_hcd->out);

	return retval;
}

static void zeroflash_download_config_work(struct work_struct *work)
{
	int retval = 0;
	struct syna_tcm_hcd *tcm_hcd = g_zeroflash_hcd->tcm_hcd;

	if(!g_zeroflash_hcd->fw_entry)
		retval = zeroflash_get_fw_image();
	if (retval < 0) {
		TPD_INFO(
			"Failed to get firmware image\n");
		return;
	}

	TPD_DETAIL("Start of config download\n");
	TPD_DETAIL("fw status:need app:%d need display:%d\n", g_zeroflash_hcd->fw_status.need_app_config,
		   g_zeroflash_hcd->fw_status.need_disp_config);
	if (g_zeroflash_hcd->fw_status.need_app_config) {
		retval = zeroflash_download_app_config();
		if (retval < 0) {
			if (tcm_hcd->health_monitor_support) {
				tcm_hcd->monitor_data->reserve2++;
			}
			TPD_INFO(
				"Failed to download application config\n");
			return;
		}
		goto exit;
	}

	if (g_zeroflash_hcd->fw_status.need_disp_config) {
		retval = zeroflash_download_disp_config();
		if (retval < 0) {
			if (tcm_hcd->health_monitor_support) {
				tcm_hcd->monitor_data->reserve2++;
			}
			TPD_INFO(
				"Failed to download display config\n");
			return;
		}
		goto exit;
	}

exit:
	TPD_DETAIL("End of config download\n");

	zeroflash_check_download_config();
	//zeroflash_download_config();

	return;
}

static int zeroflash_download_app_fw(void)
{
	int retval;
	unsigned char command;
	struct image_info *image_info;
	struct syna_tcm_hcd *tcm_hcd = g_zeroflash_hcd->tcm_hcd;
#if RESET_TO_HDL_DELAY_MS
	//const struct syna_tcm_board_data *bdata = tcm_hcd->hw_if->bdata;
#endif

#ifdef DOWNLOAD_APP_FAST_RETRY
	unsigned char tmp_buf[256] = {0};
	int retry_cnt = 0;
#endif

	TPD_DETAIL("Downloading application firmware\n");

	image_info = &g_zeroflash_hcd->image_info;

	if (image_info->app_firmware.size == 0) {
		TPD_INFO(
			"No application firmware in image file\n");
		return -EINVAL;
	}

	LOCK_BUFFER(g_zeroflash_hcd->out);

	retval = syna_tcm_alloc_mem(tcm_hcd,
				    &g_zeroflash_hcd->out,
				    image_info->app_firmware.size);
	if (retval < 0) {
		TPD_INFO(
			"Failed to allocate memory for zeroflash_hcd->out.buf\n");
		UNLOCK_BUFFER(g_zeroflash_hcd->out);
		return retval;
	}

	retval = secure_memcpy(g_zeroflash_hcd->out.buf,
			       g_zeroflash_hcd->out.buf_size,
			       image_info->app_firmware.data,
			       image_info->app_firmware.size,
			       image_info->app_firmware.size);
	if (retval < 0) {
		TPD_INFO(
			"Failed to copy application firmware data\n");
		UNLOCK_BUFFER(g_zeroflash_hcd->out);
		return retval;
	}

	g_zeroflash_hcd->out.data_length = image_info->app_firmware.size;

	command = F35_WRITE_FW_TO_PMEM_COMMAND;

#ifdef DOWNLOAD_APP_FAST_RETRY
retry_app_download:
#endif

#if 0
#if RESET_TO_HDL_DELAY_MS
	//gpio_set_value(bdata->reset_gpio, bdata->reset_on_state);
	syna_reset_gpio(tcm_hcd, 0);
	usleep_range(1000, 1000);
	syna_reset_gpio(tcm_hcd, 1);
	mdelay(RESET_TO_HDL_DELAY_MS);
#endif

#ifdef DOWNLOAD_APP_FAST_RETRY
	//check f35 again
	retval = zeroflash_check_f35();
	if (retval < 0) {
		retry_cnt++;
		if (retry_cnt <= 3) {
			TPD_INFO("can not read F35, goto retry\n");
			goto retry_app_download;
		} else {
			TPD_INFO("retry three times, but still fail,return\n");
			return retval;
		}
	}
#endif
#endif
	retval = syna_tcm_rmi_write(tcm_hcd,
				    g_zeroflash_hcd->f35_addr.control_base + F35_CTRL3_OFFSET,
				    &command,
				    sizeof(command));
	if (retval < 0) {
		TPD_INFO(
			"Failed to write F$35 command\n");
		UNLOCK_BUFFER(g_zeroflash_hcd->out);
		return retval;
	}

	retval = syna_tcm_rmi_write(tcm_hcd,
				    g_zeroflash_hcd->f35_addr.control_base + F35_CTRL7_OFFSET,
				    g_zeroflash_hcd->out.buf,
				    g_zeroflash_hcd->out.data_length);
	if (retval < 0) {
		TPD_INFO(
			"Failed to write application firmware data\n");
		UNLOCK_BUFFER(g_zeroflash_hcd->out);
		return retval;
	}

	UNLOCK_BUFFER(g_zeroflash_hcd->out);
#ifdef DOWNLOAD_APP_FAST_RETRY
	//read and check the identify response
	msleep(20);
	syna_tcm_raw_read(tcm_hcd, tmp_buf, sizeof(tmp_buf));
	if ((tmp_buf[0] != MESSAGE_MARKER) && (tmp_buf[1] != REPORT_IDENTIFY)) {
		retry_cnt++;
		if (retry_cnt <= 3) {
			TPD_INFO("can not read a identify report, goto retry\n");

#if RESET_TO_HDL_DELAY_MS
			//gpio_set_value(bdata->reset_gpio, bdata->reset_on_state);
			syna_reset_gpio(tcm_hcd, 0);
			usleep_range(1000, 1000);
			syna_reset_gpio(tcm_hcd, 1);
			mdelay(RESET_TO_HDL_DELAY_MS);
#endif
			goto retry_app_download;
		}
	} else {
		//successful read message
		//tcm_hcd->host_download_mode = true;
		TPD_INFO("download firmware success\n");
	}
#endif
	TPD_INFO("Application firmware downloaded\n");

	return 0;
}

extern int syna_tcm_run_bootloader_firmware(struct syna_tcm_hcd *tcm_hcd);
static void zeroflash_do_romboot_firmware_download(void)
{
	int retval;
	unsigned char *resp_buf = NULL;
	unsigned int resp_buf_size;
	unsigned int resp_length;
	unsigned int data_size_blocks;
	unsigned int image_size;
	struct syna_tcm_hcd *tcm_hcd = g_zeroflash_hcd->tcm_hcd;
	struct touchpanel_data *ts = spi_get_drvdata(tcm_hcd->s_client);

#ifdef CONFIG_TOUCHPANEL_MTK_PLATFORM
	if (ts->boot_mode == RECOVERY_BOOT)
#else
	if (ts->boot_mode == MSM_BOOT_MODE__RECOVERY)
#endif
	{
		tcm_hcd->tp_fw_update_first = false;
	}

	if(tcm_hcd->tp_fw_update_first) {
		tcm_hcd->tp_fw_update_first = false;
		return;
	}

	TPD_INFO(
		"Prepare ROMBOOT firmware download\n");

	atomic_set(&tcm_hcd->host_downloading, 1);
	resp_buf = NULL;
	resp_buf_size = 0;

	if (tcm_hcd->id_info.mode != MODE_ROMBOOTLOADER) {
		TPD_INFO("Not in romboot mode\n");
		atomic_set(&tcm_hcd->host_downloading, 0);
		goto exit;
	}

	retval = zeroflash_get_fw_image();
	if (retval < 0) {
		TPD_INFO("Failed to request romboot.img\n");
		goto exit;
	}

	image_size = (unsigned int)g_zeroflash_hcd->image_info.app_firmware.size;

	TPD_INFO("image_size = %d\n",
		 image_size);

	data_size_blocks = image_size / 16;

	LOCK_BUFFER(g_zeroflash_hcd->out);

	retval = syna_tcm_alloc_mem(tcm_hcd,
				    &g_zeroflash_hcd->out,
				    image_size + RESERVED_BYTES);
	if (retval < 0) {
		TPD_INFO(
			"Failed to allocate memory for application firmware\n");
		UNLOCK_BUFFER(g_zeroflash_hcd->out);
		goto exit;
	}

	g_zeroflash_hcd->out.buf[0] = g_zeroflash_hcd->image_info.app_firmware.size >> 16;

	retval = secure_memcpy(&g_zeroflash_hcd->out.buf[RESERVED_BYTES],
			       g_zeroflash_hcd->image_info.app_firmware.size,
			       g_zeroflash_hcd->image_info.app_firmware.data,
			       g_zeroflash_hcd->image_info.app_firmware.size,
			       g_zeroflash_hcd->image_info.app_firmware.size);
	if (retval < 0) {
		TPD_INFO(
			"Failed to copy payload\n");
		UNLOCK_BUFFER(g_zeroflash_hcd->out);
		goto exit;
	}

	TPD_INFO(
		"data_size_blocks: %d\n",
		data_size_blocks);

	retval = tcm_hcd->write_message(tcm_hcd,
					CMD_ROMBOOT_DOWNLOAD,
					g_zeroflash_hcd->out.buf,
					image_size + RESERVED_BYTES,
					&resp_buf,
					&resp_buf_size,
					&resp_length,
					NULL,
					0);

	UNLOCK_BUFFER(g_zeroflash_hcd->out);
	if (retval < 0) {
		TPD_INFO("Failed to write command ROMBOOT DOWNLOAD");
		syna_reset_gpio(tcm_hcd, false);
		msleep(30);
		syna_reset_gpio(tcm_hcd, true);
		msleep(1);
		goto exit;
	}

	retval = syna_tcm_run_bootloader_firmware(tcm_hcd);
	if (retval < 0) {
		syna_reset_gpio(tcm_hcd, false);
		msleep(30);
		syna_reset_gpio(tcm_hcd, true);
		msleep(1);
		TPD_INFO("Failed to switch to bootloader");
		goto exit;
	}

exit:
	kfree(resp_buf);

	return;
}
static void zeroflash_do_f35_firmware_downloading(void)
{
	int retval;
	struct rmi_f35_data data;
	struct syna_tcm_hcd *tcm_hcd = g_zeroflash_hcd->tcm_hcd;
	static unsigned int retry_count;

	atomic_set(&tcm_hcd->host_downloading, 1);

	retval = syna_tcm_rmi_read(tcm_hcd,
				   g_zeroflash_hcd->f35_addr.data_base,
				   (unsigned char *)&data,
				   sizeof(data));
	if (retval < 0) {
		TPD_INFO(
			"Failed to read F$35 data\n");
		goto exit;
	}

	if (data.error_code != REQUESTING_FIRMWARE) {
		TPD_INFO(
			"Microbootloader error code = 0x%02x\n",
			data.error_code);
		if (data.error_code != CHECKSUM_FAILURE) {
			retval = -EIO;
			goto exit;
		} else {
			retry_count++;
		}
	} else {
		retry_count = 0;
	}

	retval = zeroflash_get_fw_image();
	if (retval < 0) {
		TPD_INFO(
			"Failed to get firmware image\n");
		goto exit;
	}

	TPD_DETAIL("Start of firmware download\n");

	retval = zeroflash_download_app_fw();
	if (retval < 0) {
		TPD_INFO(
			"Failed to download application firmware\n");
		goto exit;
	}

	syna_tcm_start_reset_timer(tcm_hcd);
	TPD_DETAIL("End of firmware download\n");

exit:
	if (retval < 0) {
		retry_count++;
	}

	if (DOWNLOAD_RETRY_COUNT && retry_count > DOWNLOAD_RETRY_COUNT) {
		if (tcm_hcd->health_monitor_support) {
			tcm_hcd->monitor_data->fw_download_fail++;
		}
		disable_irq(tcm_hcd->s_client->irq);

		TPD_DETAIL("zeroflash_download_firmware_work disable_irq \n");


	} else {
		if (retval < 0) {
			if (tcm_hcd->health_monitor_support) {
				tcm_hcd->monitor_data->fw_download_retry++;
			}
			syna_reset_gpio(tcm_hcd, 0);
			msleep(20);
			syna_reset_gpio(tcm_hcd, 1);
			msleep(20);
			TPD_INFO("something wrong happen, add hw reset try to download\n");
		}
		//msleep(20);
		enable_irq(tcm_hcd->s_client->irq);

		TPD_DETAIL("zeroflash_download_firmware_work enable_irq \n");
	}

	return;
}

void zeroflash_download_firmware_work(struct work_struct *work)
{
	struct syna_tcm_hcd *tcm_hcd = g_zeroflash_hcd->tcm_hcd;
	if (tcm_hcd->id_info.mode == MODE_ROMBOOTLOADER) {
		zeroflash_do_romboot_firmware_download();
	} else {
		zeroflash_do_f35_firmware_downloading();
	}
	return;
}

static int zeroflash_init(struct syna_tcm_hcd *tcm_hcd)
{
	g_zeroflash_hcd = kzalloc(sizeof(*g_zeroflash_hcd), GFP_KERNEL);
	if (!g_zeroflash_hcd) {
		TPD_INFO(
			"Failed to allocate memory for zeroflash_hcd\n");
		return -ENOMEM;
	}

	g_zeroflash_hcd->tcm_hcd = tcm_hcd;
	tcm_hcd->zeroflash_hcd = g_zeroflash_hcd;
	g_zeroflash_hcd->fw_name = tcm_hcd->fw_name;

	INIT_BUFFER(g_zeroflash_hcd->out, false);
	INIT_BUFFER(g_zeroflash_hcd->resp, false);

	g_zeroflash_hcd->config_workqueue =
		create_singlethread_workqueue("syna_tcm_zeroflash_config");
	g_zeroflash_hcd->firmware_workqueue =
		create_singlethread_workqueue("syna_tcm_zeroflash_firmware");
	INIT_WORK(&g_zeroflash_hcd->config_work,
		  zeroflash_download_config_work);
	INIT_WORK(&g_zeroflash_hcd->firmware_work,
		  zeroflash_download_firmware_work);

	g_zeroflash_hcd->init_done = true;
	zeroflash_init_done = 1;
	//if (tcm_hcd->init_okay == false/* &&
	//        tcm_hcd->hw_if->bus_io->type == BUS_SPI*/)
	//    zeroflash_download_firmware();

	return 0;
}


struct zeroflash_hcd *syna_remote_zeroflash_init(struct syna_tcm_hcd *tcm_hcd)
{
	zeroflash_init(tcm_hcd);

	return g_zeroflash_hcd;
}

void wait_zeroflash_firmware_work(void)
{
	cancel_work_sync(&g_zeroflash_hcd->config_work);
	flush_workqueue(g_zeroflash_hcd->config_workqueue);
	cancel_work_sync(&g_zeroflash_hcd->firmware_work);
	flush_workqueue(g_zeroflash_hcd->firmware_workqueue);
}
