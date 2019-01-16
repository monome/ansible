#pragma once

#define ANSIBLE_BACKUP_FILE (FS_STRING)"ansible-backup.bin"
#define ANSIBLE_PRESET_FILE (FS_STRING)"ansible-preset.json"

void set_mode_usb_disk(void);
void usb_disk_enter(void);
void usb_disk_exit(void);

void puts_4k_chunks(const char* src, size_t len);
