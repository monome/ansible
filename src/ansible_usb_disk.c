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

#define DEBUG_ANSIBLE_USB_DISK 0

static void handler_UsbDiskKey(int32_t data);

static bool usb_disk_mount_drive(void);
static bool usb_disk_backup_binary(FS_STRING fname);
static bool usb_disk_restore_backup(FS_STRING fname);
static bool usb_disk_load_flash(FS_STRING fname);
static bool usb_disk_save_flash(FS_STRING fname);
static void flush(void);

static char ansible_usb_disk_textbuf[ANSIBLE_USBDISK_TXTBUF_LEN] = {  0 };
static jsmntok_t ansible_usb_disk_tokbuf[ANSIBLE_USBDISK_TOKBUF_LEN];

static volatile bool usb_disk_locked = false;

static bool usb_disk_lock(uint8_t leds) {
	if (!usb_disk_locked) {
		usb_disk_locked = true;
		update_leds(leds);
		print_dbg("\r\n\r\n\r\n> usb disk locked");
		return true;
	}
	return false;
}

static void usb_disk_unlock(void) {
	usb_disk_locked = false;
	usb_disk_exit();
	usb_disk_enter();
	update_leds(0);
	print_dbg("\r\n> usb disk unlocked\r\n\r\n");
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
				        flash_unfresh();
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

	do {
		chunk = min(len - read, ANSIBLE_FLASH_BLOCKSIZE);

#if DEBUG_ANSIBLE_USB_DISK
		print_dbg("\r\nsave ");
		print_dbg_hex(chunk);
		print_dbg(" at ");
		print_dbg_hex(src + read);
		print_dbg(" to flash @ ");
		print_dbg_hex(dst + read);
#endif

		flashc_memcpy(dst + read, src + read, chunk, true);
		read += chunk;
	} while (read < len);
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

static uint8_t usb_disk_buffer[ANSIBLE_USBDISK_BLOCKSIZE] = { 0 };
static uint16_t buf_pos = 0;
size_t total_written = 0;

static void flush(void) {
#if DEBUG_ANSIBLE_USB_DISK
  print_dbg("\r\n\r\nflush ");
  print_dbg_hex(buf_pos);
  print_dbg(" bytes to disk = \r\n");
    for (size_t i = 0; i < buf_pos; i++) {
      print_dbg_char(usb_disk_buffer[i]);
    }
    print_dbg("\r\n");
#endif

  file_write_buf(usb_disk_buffer, buf_pos);
  file_flush();
  total_written += buf_pos;
  buf_pos = 0;
}

void puts_buffered(const char* src, size_t len) {
  uint16_t chunk;

#if DEBUG_ANSIBLE_USB_DISK
    print_dbg("\r\nask to write ");
    print_dbg_hex(len);
    print_dbg(" = ");
    for (size_t i = 0; i < len; i++) {
      print_dbg_char(src[i]);
    }
#endif

  for (size_t written = 0; written < len; written += chunk) {
    if (buf_pos >= sizeof(usb_disk_buffer)) {
      flush();
    }
    chunk = min(len - written, sizeof(usb_disk_buffer) - buf_pos);
    memcpy(usb_disk_buffer + buf_pos, src + written, chunk);
    buf_pos += chunk;
  }
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
	print_dbg("\r\n> making binary backup");
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
	print_dbg("\r\n> binary backup done");
	return true;
}

static bool usb_disk_restore_backup(FS_STRING fname) {
	print_dbg("\r\n> restoring binary backup");
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
	print_dbg("\r\n> starting usb disk load");
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

	switch (result) {
	case JSON_READ_OK:
	  print_dbg("\r\n> disk load successful");
	  break;
	case JSON_READ_MALFORMED:
	  print_dbg("\r\n> disk backup malformed");
	  break;
	default:
	  print_dbg("\r\n> reached invalid state");
	  break;
	}
	print_dbg("\r\n> usb disk load done");

	return result == JSON_READ_OK;
}

static bool usb_disk_save_flash(FS_STRING fname) {
	print_dbg("\r\n> writing flash to disk");
	if (!nav_file_create(fname)) {
		if (fs_g_status != FS_ERR_FILE_EXIST) {
  print_dbg("\r\n!! could not create file");
			return false;
		}
	}
	if (!file_open(FOPEN_MODE_W)) {
  print_dbg("\r\n!! could not open file");
		return false;
	}
	total_written = 0;
	json_write_result_t result = json_write(
		/* puts_chunks, */
		puts_buffered,
		(void*)&f, &ansible_preset_docdef);
	flush();
	file_flush();
	file_close();

	print_dbg("\r\n> flash write complete: ");
	print_dbg_hex(total_written);
	print_dbg(" bytes total");

	if (result != JSON_WRITE_OK) {
		print_dbg("\r\n!! flash write error");
	}

	return result == JSON_WRITE_OK;
}
