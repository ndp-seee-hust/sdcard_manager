#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include "sdcard_util.h"

void normal2other_callback(void)
{
    printf("SD card state changed from normal to other\n");
}

void other2normal_callback(void)
{
    printf("SD card state changed from other to normal\n");
}

int main()
{
    sdcard_handle handle = sdcard_util_init((void *)normal2other_callback, (void *)other2normal_callback);
    long long total_kb, free_kb;

    
    //char* mount_path = manager->mount_path;


    while(1) 
    {
        sdcard_manager_t *manager = (sdcard_manager_t *)handle;
        if (sdcard_get_size(handle, &total_kb, &free_kb) == 0) 
        {
            printf("Total: %lld MB, Free: %lld MB\n", total_kb/1024, free_kb/1024);
        }
        sdcard_state_e state = sdcard_get_state(handle);
        switch (state) 
        {
            case SDST_NOT_EXISTS:
                printf("SD card not found\n");
                break;
            case SDST_EJECT:
                printf("SD card ejected\n");
                break;
            case SDST_NORMAL:
                printf("SD card normal\n");
                break;
            case SDST_FULL:
                printf("SD card full\n");
                delete_file(manager->mount_path);
                break;
            case SDST_BROKEN:
                printf("SD card broken\n");
                break;
        }
        if(manager->state == SDST_NORMAL || manager->state == SDST_FULL)
        {
            printf("======> mount path: %s\n", manager->mount_path);
            printf("======> format type: %s\n", manager->format_type);
        }
        
        // if(format_sdcard(handle, "vfat"))
        // {
        //     printf("formatting ok\n");
        // }
        sleep(2);
        
    } 
    
    sdcard_util_deinit(handle);  
}