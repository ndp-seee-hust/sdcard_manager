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

typedef struct
{
    struct sockaddr_nl sa;
    int sock_fd;
    sdcard_cb_p normal2other;
    sdcard_cb_p other2normal;
    int partition;
    sdcard_state_e state;
    char dev_name[16];
    char mount_path[64];
    char format_type[32];
} sdcard_manager_t;

sdcard_handle sdcard_util_init(sdcard_cb_p normal2other, sdcard_cb_p other2normal);
int sdcard_get_size(sdcard_handle handle, long long *total_kb, long long *free_kb);
sdcard_state_e sdcard_get_state(sdcard_handle handle);
void sdcard_util_deinit(sdcard_handle handle);

void delete_file(char* mount_path);
static int get_sdcard_fs_type(sdcard_manager_t *manager);
int umount_sdcard(const char* dev_path);
int mount_sdcard(const char * dev_path, const char* mount_path);
int format_sdcard(sdcard_handle handle, const char *fs_type);


#ifdef __cplusplus
}
#endif

#endif