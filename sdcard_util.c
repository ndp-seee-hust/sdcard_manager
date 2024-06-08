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

#include "sdcard_util.h"

#define SYS_BLOCK_PATH "/sys/block"
#define MOUNTS_PATH "/proc/mounts"

#define SDCARD_FULL_FREE 50 // Less than 50M is defined as full

// typedef struct
// {
//     struct sockaddr_nl sa;
//     int sock_fd;
//     sdcard_cb_p normal2other;
//     sdcard_cb_p other2normal;
//     int partition;
//     sdcard_state_e state;
//     char dev_name[16];
//     char mount_path[64];
// } sdcard_manager_t;

static int sdcard_manager_run_flag = 0;

/*
 * /dev/mmcblk0p1 /media/mmcblk0p1 vfat rw,relatime,fmask=0000,dmask=0000,allow_utime=0022,codepage=437,iocharset=iso8859-1,shortname=mixed,errors=remount-ro 0 0
 */
static int find_mount_point(sdcard_manager_t *manager)
{
    FILE *fp = NULL;
    char dev_path[64] = {0};
    char line_str[256] = {0};
    char *mount_info = NULL;
    int blank_index[2] = {0};
    int blank_count = 0;
    int i = 0;
    long long total, free;

    if (strstr(manager->dev_name, "mmcblk") == NULL || manager->partition <= 0)
    {
        // fprintf(stderr, "[%s]no sdcard\n", __func__);
        return -1;
    }

    fp = fopen(MOUNTS_PATH, "r");
    if (fp == NULL)
    {
        fprintf(stderr, "cannot open %s", MOUNTS_PATH);
        return -1;
    }

    snprintf(dev_path, sizeof(dev_path), "/dev/%sp%d", manager->dev_name, manager->partition);

    while (fgets(line_str, sizeof(line_str), fp))
    {
        mount_info = strstr(line_str, dev_path);
        if (mount_info != NULL)
        {
            for (i = 0; i < strlen(mount_info); i++)
            {
                if (mount_info[i] == ' ')
                {
                    blank_index[blank_count] = i;
                    blank_count += 1;
                }
                if (blank_count >= sizeof(blank_index) / sizeof(int))
                {
                    memcpy(manager->mount_path, mount_info + blank_index[0] + 1, blank_index[1] - blank_index[0] - 1);
                    if (strstr(mount_info, "ro,") != NULL)
                    {
                        manager->state = SDST_BROKEN;
                    }
                    else
                    {
                        if (sdcard_get_size(manager, &total, &free) == 0)
                        {
                            if (free < 1024 * SDCARD_FULL_FREE)
                            {
                                manager->state = SDST_FULL;
                            }
                            else
                            {
                                manager->state = SDST_NORMAL;
                            }
                        }
                        else
                        {
                            manager->state = SDST_BROKEN;
                        }
                    }
                    fclose(fp);
                    return 0;
                }
            }
        }
    }

    fclose(fp);
    manager->state = SDST_EJECT;
    return 0;
}

static int check_sdcard(sdcard_manager_t *manager)
{
    struct dirent *entry;
    char *dev_name = NULL;
    DIR *dir = NULL;
    char block_path[64] = {0};
    int ret = 0;

    memset(manager->dev_name, 0, sizeof(manager->dev_name));
    memset(manager->mount_path, 0, sizeof(manager->mount_path));
    memset(manager->format_type, 0, sizeof(manager->format_type));
    manager->state = SDST_NOT_EXISTS;
    manager->partition = -1;

    dir = opendir(SYS_BLOCK_PATH);
    if (dir == NULL)
    {
        fprintf(stderr, "[%s]Error opening %s\n", __func__, SYS_BLOCK_PATH);
        return -1;
    }

    while ((entry = readdir(dir)) != NULL)
    {
        dev_name = strstr(entry->d_name, "mmcblk");
        if (dev_name != NULL)
        {
            if (strlen(dev_name) + 1 <= sizeof(manager->dev_name))
            {
                memcpy(manager->dev_name, dev_name, strlen(dev_name) + 1);
                break;
            }
            else
            {
                fprintf(stderr, "[%s]dev_name is too long", __func__);
                return -1;
            }
        }
        else
        {
            continue;
        }
    }
    closedir(dir);
    dir = NULL;

    if (strstr(manager->dev_name, "mmcblk") != NULL)
    {
        snprintf(block_path, sizeof(block_path), "%s/%s", SYS_BLOCK_PATH, manager->dev_name);
        dir = opendir(block_path);
        if (dir == NULL)
        {
            fprintf(stderr, "[%s]Error opening %s\n", __func__, block_path);
            return -1;
        }

        while ((entry = readdir(dir)) != NULL)
        {
            dev_name = strstr(entry->d_name, "mmcblk");
            if (dev_name != NULL)
            {
                manager->partition = atoi(dev_name + strlen(manager->dev_name) + 1);
            }
            else
            {
                continue;
            }
        }
    }

   

    if (manager->partition != -1)
    {
        ret = find_mount_point(manager);
        get_sdcard_fs_type(manager);
        
        if (ret < 0)
        {
            manager->state = SDST_EJECT;
        }
        sleep(2);
        printf("---------------------------------------\n");
        printf("dev_name = %s\n", manager->dev_name);
        printf("partition = %d\n", manager->partition);
        printf("state = %d\n", manager->state);
        printf("mount_path = %s\n", manager->mount_path);
        printf("format_type = %s\n", manager->format_type);
        printf("---------------------------------------\n");

        return 0;
    }

    return -1;
}

static void *sdcard_in_out_monitor(void *arg)
{
    sdcard_manager_t *manager = NULL;
    ssize_t rec_len = 0;
    struct iovec iov;
    struct msghdr msg;
    char buf[4096];
    int i = 0;

    pthread_detach(pthread_self());

    manager = (sdcard_manager_t *)arg;

    memset(&msg, 0, sizeof(msg));
    iov.iov_base = (void *)buf;
    iov.iov_len = sizeof(buf);
    msg.msg_name = (void *)&manager->sa;
    msg.msg_namelen = sizeof(manager->sa);
    msg.msg_iov = &iov;
    msg.msg_iovlen = 1;

    while (sdcard_manager_run_flag)
    {
        rec_len = recvmsg(manager->sock_fd, &msg, 0);
        if (rec_len < 0)
        {
            fprintf(stderr, "[%s]recvmsg error\n", __func__);
        }
        else if (rec_len < 32 || rec_len > sizeof(buf))
        {
            fprintf(stderr, "[%s]invalid message\n", __func__);
        }

        for (i = 0; i < rec_len; i++)
        {
            if (*(buf + i) == '\0')
                buf[i] = '\n';
        }

        // printf("received %d bytes\n%s\n", rec_len, buf);
        // printf("\n\n\n\n");

        if (strstr(buf, "ACTION=add") != 0 && strstr(buf, "SUBSYSTEM=block") != 0)
        {
            if (manager->state == SDST_NOT_EXISTS)
            {
                printf("sdcard in\n");
                check_sdcard(manager);
            }
        }
        else if (strstr(buf, "ACTION=remove") != 0 && strstr(buf, "SUBSYSTEM=block") != 0)
        {
            if (manager->state != SDST_NOT_EXISTS)
            {
                printf("sdcard out\n");
                memset(manager->dev_name, 0, sizeof(manager->dev_name));
                memset(manager->mount_path, 0, sizeof(manager->mount_path));
                manager->state = SDST_NOT_EXISTS;
                manager->partition = -1;
            }
        }
    }
}

static void *sdcard_mount_monitor(void *arg)
{
    sdcard_manager_t *manager = NULL;
    sdcard_state_e pre_state = SDST_NORMAL;
    int ret = 0;

    pthread_detach(pthread_self());

    manager = (sdcard_manager_t *)arg;
    pre_state = manager->state;

    while(sdcard_manager_run_flag)
    {
        ret = find_mount_point(manager);
        if (manager->state != pre_state)
        {
            printf("sdcard state change from %d to %d\n", pre_state, manager->state);
            if (manager->state == SDST_NORMAL)
            {
                if (manager->other2normal != NULL)
                {
                    manager->other2normal();
                }
            }
            else
            {
                if (manager->normal2other != NULL)
                {
                    manager->normal2other();
                }
            }
            pre_state = manager->state;
        }

        sleep(2);
    }
}

sdcard_handle sdcard_util_init(sdcard_cb_p normal2other, sdcard_cb_p other2normal)
{
    sdcard_manager_t *manager = NULL;
    pthread_t sys_tid;
    pthread_t mount_tid;
    int ret;

    manager = (sdcard_manager_t *)malloc(sizeof(sdcard_manager_t));
    if (manager == NULL)
    {
        return NULL;
    }

    check_sdcard(manager);
    manager->normal2other = normal2other;
    manager->other2normal = other2normal;

    manager->sock_fd = socket(AF_NETLINK, SOCK_RAW, NETLINK_KOBJECT_UEVENT);
    if (manager->sock_fd == -1)
    {
        fprintf(stderr, "socket creating failed:%s\n", strerror(errno));
        return NULL;
    }

    memset(&manager->sa, 0, sizeof(manager->sa));
    manager->sa.nl_family = AF_NETLINK;
    manager->sa.nl_groups = NETLINK_KOBJECT_UEVENT;
    manager->sa.nl_pid = 0;
    if (bind(manager->sock_fd, (struct sockaddr *)&manager->sa, sizeof(manager->sa)) == -1)
    {
        fprintf(stderr, "bind error:%s\n", strerror(errno));
        close(manager->sock_fd);
        return NULL;
    }

    sdcard_manager_run_flag = 1;
    if ((ret = pthread_create(&sys_tid, NULL, &sdcard_in_out_monitor, (void *)manager)))
    {
        fprintf(stderr, "pthread_create sdcard_in_out_monitor ret=%d\n", ret);
        close(manager->sock_fd);
        return NULL;
    }

    if ((ret = pthread_create(&mount_tid, NULL, &sdcard_mount_monitor, (void *)manager)))
    {
        fprintf(stderr, "pthread_create sdcard_mount_monitor ret=%d\n", ret);
        close(manager->sock_fd);
        return NULL;
    }

    return (sdcard_handle)manager;
}

void sdcard_util_deinit(sdcard_handle handle)
{
    sdcard_manager_t *manager = (sdcard_manager_t *)handle;

    close(manager->sock_fd);
    free(manager);
    sdcard_manager_run_flag = 0;
}

int sdcard_get_size(sdcard_handle handle, long long *total_kb, long long *free_kb)
{
    int ret = 0;
    struct statfs stfs;
    sdcard_manager_t *manager = (sdcard_manager_t *)handle;

    if (manager->state == SDST_NOT_EXISTS)
    {
        return -1;
    }

    ret = statfs(manager->mount_path, &stfs);
    if (ret < 0)
    {
        fprintf(stderr, "sdcard statfs failed\n");
        return -1;
    }

    *total_kb = ((long long)stfs.f_blocks * stfs.f_bsize) / 1024;
    *free_kb = ((long long)stfs.f_bavail * stfs.f_bsize) / 1024;

    return 0;
}

sdcard_state_e sdcard_get_state(sdcard_handle handle)
{
    sdcard_manager_t *manager = (sdcard_manager_t *)handle;

    return manager->state;
}

void delete_file(char* mount_path) 
{

    DIR *dir;
    struct dirent *entry;
    struct stat file_stat;
    char filepath[PATH_MAX];
    long long file_size = -1; // Biến lưu kích thước của tệp đã xóa

    dir = opendir(mount_path);
    if (dir == NULL) 
    {
        perror("opendir");
    }

    while ((entry = readdir(dir)) != NULL) 
    {
        if (strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0) 
        {
            continue;
        }

        snprintf(filepath, sizeof(filepath), "%s/%s", mount_path, entry->d_name);

        if (stat(filepath, &file_stat) == -1) 
        {
            perror("stat");
            continue;
        }

        if (S_ISREG(file_stat.st_mode)) 
        {
            file_size = file_stat.st_size;

            if (remove(filepath) == 0) 
            {
                printf("Successfully deleted file: %s, file size: %lld MB\n", filepath, file_size/(1024*1024));
            } 
            else 
            {
                perror("Error deleting the file");
                file_size = -1; 
            }
            break; 
        }
        else if(S_ISDIR(file_stat.st_mode))
        {
            delete_file(filepath);
        }
            
    }
    closedir(dir);
}

static int get_sdcard_fs_type(sdcard_manager_t *manager) 
{
    char* mount_path = manager->mount_path;
    char command[256];
    FILE *fp;

    snprintf(command, sizeof(command), "findmnt -n -o FSTYPE %s", mount_path);
    
    fp = popen(command, "r");
    if (fp == NULL) 
    {
        perror("popen");
        exit(1);
    }

    if (fgets(manager->format_type, sizeof(manager->format_type), fp) != NULL) 
    {
        manager->format_type[strcspn(manager->format_type, "\n")] = 0; // Remove newline character
        pclose(fp);
        return 1;
    }
    
    pclose(fp);
    return -1;
}

int umount_sdcard(const char* dev_path) 
{
    char command[256];
    snprintf(command, sizeof(command), "sudo umount %s", dev_path);
    return system(command);
}

int mount_sdcard(const char* dev_path, const char* mount_path)
{
    char command[256];
    snprintf(command, sizeof(command), "sudo mount %s %s", dev_path, mount_path);
    return system(command);
}

int format_sdcard(sdcard_handle handle, const char *fs_type) 
{
    char dev_path[64] = {0};
    char command[256];
    sdcard_manager_t *manager = (sdcard_manager_t *)handle;
   
    snprintf(dev_path, sizeof(dev_path), "/dev/%sp%d", manager->dev_name, manager->partition);
    printf("dev path: %s\n", dev_path);
    if(umount_sdcard(dev_path))
    {
        manager->state = SDST_EJECT;
        printf("SD card eject\n");
    }

    printf("SD card fomatting ...\n");
    manager->state = SDST_FORMATTING;
    snprintf(command, sizeof(command), "sudo mkfs.%s %s", fs_type, dev_path);
    system(command);
    sleep(2);

    if(mount_sdcard(dev_path, manager->mount_path))
    {
        manager->state = SDST_NORMAL;
        printf("SD card normal\n");
        return 1;
    }

    return 0;
}

