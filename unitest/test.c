#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <pthread.h>
#include <sys/types.h>
#include "sdcard_util.h"
#include "unity.h"

char *BUFFER = NULL;
const char* file_output = "/home/ndp/output_test.txt";


void setUp(void) 
{
    //
}

void tearDown(void) 
{
    //
}
void push_data_to_buffer(char **buffer) 
{
    for (int i = 0; i <= 10; i++) 
    {
        char line[20]; 
        snprintf(line, sizeof(line), "test %d\n", i);
        
        size_t currentLen = (*buffer) ? strlen(*buffer) : 0;
        size_t dataLen = strlen(line);
        
        *buffer = (char *)realloc(*buffer, currentLen + dataLen + 1);
        if (*buffer == NULL) 
        {
            fprintf(stderr, "Unable to allocate memory\n");
            exit(EXIT_FAILURE);
        }

        strcpy(*buffer + currentLen, line);
    }
}

void write_buffer_to_file(const char *filename, const char *mode, char *buffer) 
{
    FILE *file = fopen(filename, mode);
    if (file == NULL) {
        fprintf(stderr, "Can't open %s\n", filename);
        return;
    }

    if (buffer) 
    {
        fprintf(file, "%s", buffer);
        printf("Write to output test file succesfully!\n");
    }
    else
    {
        printf("Can't write to output test file!\n");
    }

    fclose(file);
}

void *readInput(void *arg) 
{
    sdcard_handle handle = *(sdcard_handle *)arg;
    char input;
    
    while (1) 
    {
        scanf("%c", &input); 
        if (input == 'F' || input == 'f') 
        {
            format_sdcard(handle, "vfat"); 
        }
        else if (input == 'W' || input == 'w')
        {
            push_data_to_buffer(&BUFFER);
            sdcard_write_file(handle, "test.txt", "w", &BUFFER);
        }
        else if (input == 'R' || input == 'r')
        {
            sdcard_read_file(handle, "test.txt", "r", &BUFFER);
            write_buffer_to_file(file_output, "w", BUFFER);
        }
        else if (input == 'D' || input == 'd')
        {
            sdcard_delete_file(handle, "test.txt");
        }
        
    }
    return NULL;
}

void *check_sd_card(void *arg)
{
    sdcard_handle handle = *(sdcard_handle *)arg;
    long long total_kb, free_kb;
    while(1) 
    {
        
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
                sdcard_delete_file(handle, "test.txt");
                break;
            case SDST_BROKEN:
                printf("SD card broken\n");
                break;
        }
        
        sleep(2);
    } 

}



int main()
{

    sdcard_handle handle = sdcard_util_init((void *)normal2other_callback, (void *)other2normal_callback);
    pthread_t thread_input, thread_check_sd_card;

    pthread_create(&thread_input, NULL, readInput, (void *)&handle);
    pthread_create(&thread_check_sd_card, NULL, check_sd_card, (void *)&handle);

    pthread_join(thread_input, NULL);
    pthread_join(thread_check_sd_card, NULL);

    sdcard_util_deinit(handle);  
}