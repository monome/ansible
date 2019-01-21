#pragma once

#include "ansible_usb_disk.h"

#include "fs_com.h"

#define ANSIBLE_BACKUP_FILE (FS_STRING)"ansible-backup.bin"
#define ANSIBLE_PRESET_FILE (FS_STRING)"ansible-preset.json"
#define ANSIBLE_USBDISK_TXTBUF_LEN JSON_MAX_BUFFER_SIZE
#define ANSIBLE_USBDISK_TOKBUF_LEN 8
#define ANSIBLE_USBDISK_BLOCKSIZE 4096
#define ANSIBLE_FLASH_BLOCKSIZE 4096

void set_mode_usb_disk(void);
void usb_disk_enter(void);
void usb_disk_exit(void);

size_t gets_chunks(char* dst, size_t len);
void puts_chunks(const char* src, size_t len);
