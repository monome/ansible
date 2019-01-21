#include "monome.h"
#include "init_common.h"
#include "main.h"
#include "print_funcs.h"

#include "ansible_usb_disk.h"
#include "ansible_preset_docdef.h"

#include "events.h"
#include "flashc.h"
#include "fat.h"
#include "file.h"
#include "fs_com.h"
#include "json/encoding.h"
#include "json/serdes.h"
#include "navigation.h"

#include "uhi_msc.h"
#include "uhi_msc_mem.h"

static void handler_UsbDiskKey(int32_t data);

static bool usb_disk_mount_drive(void);
static bool usb_disk_backup_binary(FS_STRING fname);
static bool usb_disk_restore_backup(FS_STRING fname);
static bool usb_disk_load_flash(FS_STRING fname);
static bool usb_disk_save_flash(FS_STRING fname);

static char ansible_usb_disk_textbuf[ANSIBLE_USBDISK_TXTBUF_LEN] = {  0 };
static jsmntok_t ansible_usb_disk_tokbuf[ANSIBLE_USBDISK_TOKBUF_LEN];

static volatile bool usb_disk_locked = false;

static bool usb_disk_lock(uint8_t leds) {
	if (!usb_disk_locked) {
		usb_disk_locked = true;
		update_leds(leds);
		return true;
	}
	return false;
}

static void usb_disk_unlock(void) {
	usb_disk_locked = false;
	usb_disk_exit();
	usb_disk_enter();
	update_leds(0);
}

static void handler_UsbDiskKey(int32_t data) {
	switch (data) {
	case 0:
		break;
	case 1:
		// key 1 - load
		if (usb_disk_lock(2)) {
			if (usb_disk_backup_binary(ANSIBLE_BACKUP_FILE)) {
				if (usb_disk_load_flash(ANSIBLE_PRESET_FILE)) {
					load_flash_state();
				} else {
					usb_disk_restore_backup(ANSIBLE_BACKUP_FILE);
				}
			}
			usb_disk_unlock();
		}
		break;
	case 3:
		// key 2 - save
		if (usb_disk_lock(1)) {
			usb_disk_save_flash(ANSIBLE_PRESET_FILE);
			usb_disk_unlock();
		}
		break;
	default:
		break;
	}
}

void set_mode_usb_disk(void) {
	update_leds(0);
	usb_disk_enter();
	app_event_handlers[kEventKey] = &handler_UsbDiskKey;
}

void usb_disk_enter() {
	nav_reset();
	nav_select(0);
	if (!usb_disk_mount_drive()) {
		usb_disk_exit();
	}
}

void usb_disk_exit() {
	nav_filelist_reset();
	nav_exit();
}

size_t gets_chunks(char* dst, size_t len) {
	size_t read = 0;
	uint16_t count, chunk;
	do {
		chunk = min(len - read, ANSIBLE_USBDISK_BLOCKSIZE);
		count = file_read_buf((uint8_t*)dst + read, chunk);
		read += count;
	} while (read < len && count > 0);
	return read;
}

static void copy_chunks(char* dst, const char* src, size_t len) {
	size_t read = 0;
	uint16_t chunk;

	/* print_dbg("\r\n copy_chunks: "); */
	/* print_dbg_hex(len); */
	/* print_dbg("@"); */
	/* print_dbg_hex(src); */
	/* print_dbg(" -> "); */
	/* print_dbg_hex(dst); */

	do {
		chunk = min(len - read, ANSIBLE_FLASH_BLOCKSIZE);
		/* print_dbg("\r\n   "); */
		/* print_dbg_hex(chunk); */
		flashc_memcpy(dst + read, src + read, chunk, true);
		/* print_dbg(" ok"); */
		read += chunk;
	} while (read < len);
	/* print_dbg("\r\n copy_chunks ok"); */
}

void puts_chunks(const char* src, size_t len) {
	size_t written = 0;
	uint16_t chunk;
	do {
		chunk = min(len - written, ANSIBLE_USBDISK_BLOCKSIZE);
		file_write_buf((uint8_t*)src + written, chunk);
		written += chunk;
	} while (written < len);
}

static bool usb_disk_mount_drive(void) {
	if (uhi_msc_is_available()) {
		for (int i = 0; i < uhi_msc_get_lun(); i++) {
			if (nav_drive_set(i)) {
				if (nav_partition_mount()) {
					return true;
				}
			}
		}
	}
	return false;
}

static bool usb_disk_backup_binary(FS_STRING fname) {
	if (!nav_file_create(fname)) {
		if (fs_g_status != FS_ERR_FILE_EXIST) {
			return false;
		}
	}
	if (!file_open(FOPEN_MODE_W)) {
		return false;
	}
	puts_chunks((char*)&f, sizeof(nvram_data_t));
	file_flush();
	file_close();
	return true;
}

static bool usb_disk_restore_backup(FS_STRING fname) {
	if (!nav_setcwd(fname, true, true)) {
		return false;
	}
	if (!file_open(FOPEN_MODE_R)) {
		return false;
	}
	size_t read = 0;
	uint16_t chunk;
	do {
		chunk = min(sizeof(nvram_data_t) - read, ANSIBLE_USBDISK_TXTBUF_LEN);
		file_read_buf((uint8_t*)ansible_usb_disk_textbuf, chunk);
		flashc_memcpy((uint8_t*)&f + read, ansible_usb_disk_textbuf, chunk, false);
		read += chunk;
	} while (read < sizeof(nvram_data_t));
	file_close();
	return true;
}

static bool usb_disk_load_flash(FS_STRING fname) {
	if (!nav_setcwd(fname, true, true)) {
		return false;
	}
	if (!file_open(FOPEN_MODE_R)) {
		return false;
	}
	json_read_result_t result = json_read(
		gets_chunks,
		copy_chunks,
		(void*)&f, &ansible_preset_docdef,
		ansible_usb_disk_textbuf, ANSIBLE_USBDISK_TXTBUF_LEN,
		ansible_usb_disk_tokbuf, ANSIBLE_USBDISK_TOKBUF_LEN);
	file_close();
	return result == JSON_READ_OK;
}

static bool usb_disk_save_flash(FS_STRING fname) {
	if (!nav_file_create(fname)) {
		if (fs_g_status != FS_ERR_FILE_EXIST) {
			return false;
		}
	}
	if (!file_open(FOPEN_MODE_W)) {
		return false;
	}
	json_write_result_t result = json_write(
		puts_chunks,
		(void*)&f, &ansible_preset_docdef);
	file_flush();
	file_close();
	return result == JSON_WRITE_OK;
}
