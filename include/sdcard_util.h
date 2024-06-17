#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <dirent.h>
#include <errno.h>
#include <pthread.h>
#include <sys/socket.h>
#include <linux/netlink.h>
#include <sys/statfs.h>
#include <time.h>
#include <sys/stat.h>
#include <limits.h>
#ifndef __SDCARD_UTIL_H__
#define __SDCARD_UTIL_H__

#ifdef __cplusplus
extern "C"
{
#endif

#define LINUX_GET_FORMAT_COMMAND "findmnt -n -o FSTYPE %s"
#define LINUX_UMOUNT_COMMAND "sudo umount %s"
#define LINUX_MOUNT_COMMAND "sudo mount -o uid=%d %s %s"
#define LINUX_FORMAT_SD_COMMAND "sudo mkfs.%s %s"
#define LINUX_MKDIR_COMMAND "sudo mkdir -p %s"


typedef void *sdcard_handle;
typedef int (*sdcard_cb_p) ();

typedef enum
{
    SDST_NORMAL = 0,
    SDST_NOT_EXISTS, //1
    SDST_FULL,       //2
    SDST_BROKEN,     //3
    SDST_FORMATTING, //4
    SDST_EJECT,      //5
    SDST_FIXING,     //6
    SDST_EJECTING    //7
} sdcard_state_e;


void normal2other_callback(void);

void other2normal_callback(void);

sdcard_handle sdcard_util_init(sdcard_cb_p normal2other, sdcard_cb_p other2normal);
int sdcard_get_size(sdcard_handle handle, long long *total_kb, long long *free_kb);
sdcard_state_e sdcard_get_state(sdcard_handle handle);
void sdcard_util_deinit(sdcard_handle handle);

int sdcard_delete_file(sdcard_handle handle, char* file_name);
int sdcard_get_format(sdcard_handle handle);

int format_sdcard(sdcard_handle handle, const char *fs_type);
int sdcard_write_file(sdcard_handle handle, char* file_name, char* mode, char **buffer);
int sdcard_read_file(sdcard_handle handle, char* file_name, char* mode, char **buffer);


#ifdef __cplusplus
}
#endif

#endif