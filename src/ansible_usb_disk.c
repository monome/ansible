#include "monome.h"
#include "init_common.h"
#include "main.h"

#include "ansible_usb_disk.h"

#include "events.h"
#include "fat.h"
#include "file.h"
#include "fs_com.h"
#include "navigation.h"

#include "uhi_msc.h"
#include "uhi_msc_mem.h"

static void handler_UsbDiskKey(int32_t data);

static bool usb_disk_mount_drive(void);
static bool usb_disk_backup_binary(void);
static bool usb_disk_restore_backup(void);
static bool usb_disk_load_flash(void);
static bool usb_disk_save_flash(void);

static void handler_UsbDiskKey(int32_t data) {
	switch (data) {
	case 0:
		break;
	case 1:
		// key 1 - load
		if (usb_disk_backup_binary()) {
			if (!usb_disk_load_flash()) {
				usb_disk_restore_backup();
			}
		}
		break;
	case 3:
		// key 2 - save
		usb_disk_save_flash();
		break;
	default:
		break;
	}
	usb_disk_exit();
}

void set_mode_usb_disk(void) {
	nav_reset();
	nav_select(0);
	if (!usb_disk_mount_drive()) {
		usb_disk_exit();
	}

	app_event_handlers[kEventKey] = &handler_UsbDiskKey;
}

void usb_disk_exit() {
	nav_exit();
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

static bool usb_disk_backup_binary(void) {
	if (!nav_file_create((FS_STRING)"ansible-backup.bin")) {
		if (fs_g_status != FS_ERR_FILE_EXIST) {
			return false;
		}
	}
	if (!file_open(FOPEN_MODE_W)) {
		return false;
	}

	file_write_buf((uint8_t*)"////////", 6);

	// file_write_buf accepts a uint16_t size
	/* size_t flash_remaining = sizeof(nvram_data_t); */
	/* while (flash_remaining > 0) { */
	/* 	file_write_buf( */
	/* 		(uint8_t*)&f + sizeof(nvram_data_t) - flash_remaining, */
	/* 		min(flash_remaining, UINT16_MAX)); */
	/* 	flash_remaining -= UINT16_MAX; */
	/* } */
	file_close();
	return true;
}

static bool usb_disk_restore_backup(void) {
	if (!nav_setcwd((FS_STRING)"ansible-backup.bin", true, true)) {
		return false;
	}
	if (!file_open(FOPEN_MODE_R)) {
		return false;
	}
	file_close();
	return true;
}

static bool usb_disk_load_flash(void) {
	return false;
}

static bool usb_disk_save_flash(void) {
	return false;
}
