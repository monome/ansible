#pragma once

#include "ansible_usb_disk.h"

#include "fs_com.h"

#define ANSIBLE_BACKUP_FILE ((FS_STRING)"ansible-backup.bin")
#define ANSIBLE_PRESET_FILE ((FS_STRING)"ansible-preset.json")
#define ANSIBLE_USBDISK_TXTBUF_LEN 64
#define ANSIBLE_USBDISK_TOKBUF_LEN 12
#define ANSIBLE_USBDISK_BLOCKSIZE 1024
#define ANSIBLE_FLASH_BLOCKSIZE (1 << 14)

void set_mode_usb_disk(void);
bool usb_disk_enter(void);
void usb_disk_exit(void);
void usb_disk_skip_apps(bool skip);
void usb_disk_select_app(ansible_mode_t mode);

size_t gets_chunks(char* dst, size_t len);
void puts_buffered(const char* src, size_t len);
