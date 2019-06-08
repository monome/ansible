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
#define DISK_BLINK_INTERVAL 250

static void handler_UsbDiskKey(int32_t data);
static void handler_UsbDiskFront(int32_t data);

static bool usb_disk_mount_drive(void);
static bool usb_disk_backup_binary(FS_STRING fname);
static bool usb_disk_restore_backup(FS_STRING fname);
static bool usb_disk_load_flash(FS_STRING fname);
static bool usb_disk_save_flash(FS_STRING fname);
static void flush(void);
static void blink_read(void* o);
static void blink_write(void* o);

static char ansible_usb_disk_textbuf[ANSIBLE_USBDISK_TXTBUF_LEN] = {  0 };
static uint8_t usb_disk_buffer[ANSIBLE_USBDISK_BLOCKSIZE] = { 0 };
static jsmntok_t ansible_usb_disk_tokbuf[ANSIBLE_USBDISK_TOKBUF_LEN];

static volatile bool usb_disk_locked = false;

static bool usb_disk_lock(void) {
	if (!usb_disk_locked) {
		usb_disk_locked = true;
		print_dbg("\r\n\r\n> usb disk locked");
		return true;
	}
	return false;
}

static void usb_disk_unlock(void) {
	usb_disk_locked = false;
	print_dbg("\r\n> usb disk unlocked\r\n");
}

static volatile bool armed = false;
static bool blink = false;

static void handler_UsbDiskKey(int32_t data) {
	bool success;

	switch (data) {
	case 0:
		break;
	case 1:
		// key 1 - load
		if (usb_disk_lock()) {
			if (!armed) {
				update_leds(1);
				armed = true;
				usb_disk_unlock();
				return;
			}
			armed = false;
			blink = false;
			success = false;
			timer_add(&auxTimer[0], DISK_BLINK_INTERVAL, &blink_read, NULL);

			usb_disk_enter();
			if (usb_disk_backup_binary(ANSIBLE_BACKUP_FILE)) {
				if (usb_disk_load_flash(ANSIBLE_PRESET_FILE)) {
					success = true;
				} else {
					usb_disk_restore_backup(ANSIBLE_BACKUP_FILE);
				}
			}
			usb_disk_exit();

			usb_disk_unlock();
			timer_remove(&auxTimer[0]);
			update_leds(0);
			if (success) {
				flash_unfresh();
				load_flash_state();
			} else {
				update_leds(3);
			}
		}
		break;
	case 3:
		// key 2 - save
		if (usb_disk_lock()) {
			if (!armed) {
				update_leds(2);
				armed = true;
				usb_disk_unlock();
				return;
			}
			armed = false;
			blink = false;
			success = false;
			timer_add(&auxTimer[0], DISK_BLINK_INTERVAL, &blink_write, NULL);

			usb_disk_enter();
			success = usb_disk_save_flash(ANSIBLE_PRESET_FILE);
			usb_disk_exit();

			usb_disk_unlock();
			timer_remove(&auxTimer[0]);
			update_leds(0);
			if (!success) {
				update_leds(3);
			}
		}
		break;
	default:
		break;
	}
}

static void handler_UsbDiskFront(s32 data) {
	if (usb_disk_lock()) {
		armed = false;
		update_leds(0);
		usb_disk_unlock();
	}
}

static void blink_read(void* o) {
	update_leds(1 * blink);
	blink = !blink;
}

static void blink_write(void* o) {
	update_leds(2 * blink);
	blink = !blink;
}

void set_mode_usb_disk(void) {
	update_leds(0);
	app_event_handlers[kEventKey] = &handler_UsbDiskKey;
	app_event_handlers[kEventFront] = &handler_UsbDiskFront;
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

void usb_disk_skip_apps(bool skip) {
	json_docdef_t* apps = json_docdef_find_key(&ansible_preset_docdef, "apps");
	json_read_object_params_t* params = (json_read_object_params_t*)apps->params;
	for (int i = 0; i < params->docdef_ct; i++) {
		params->docdefs[i].skip = skip;
	}
}

void usb_disk_select_app(ansible_mode_t mode) {
	json_docdef_t* apps = json_docdef_find_key(&ansible_preset_docdef, "apps");
	json_docdef_t* app;
	switch (mode) {
	case mArcLevels:
		usb_disk_skip_apps(true);
		app = json_docdef_find_key(apps, "levels");
		app->skip = false;
		break;
	case mArcCycles:
		usb_disk_skip_apps(true);
		app = json_docdef_find_key(apps, "cycles");
		app->skip = false;
		break;
	case mGridKria:
		usb_disk_skip_apps(true);
		app = json_docdef_find_key(apps, "kria");
		app->skip = false;
		break;
	case mGridMP:
		usb_disk_skip_apps(true);
		app = json_docdef_find_key(apps, "mp");
		app->skip = false;
		break;
	// case mGridES:
	case mMidiStandard:
		usb_disk_skip_apps(true);
		app = json_docdef_find_key(apps, "midi_standard");
		app->skip = false;
		break;
	case mMidiArp:
		usb_disk_skip_apps(true);
		app = json_docdef_find_key(apps, "midi_arp");
		app->skip = false;
		break;
	case mTT:
		usb_disk_skip_apps(true);
		app = json_docdef_find_key(apps, "tt");
		app->skip = false;
		break;
	default: {
		usb_disk_skip_apps(false);
		break;
	}
	}
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
	puts_buffered((char*)&f, sizeof(nvram_data_t));
	flush();
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
