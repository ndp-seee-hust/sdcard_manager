// #include <stdio.h>
// #include <string.h>
// #include <stdlib.h>
// #include <sys/mount.h>

// int main()
// {
//     const char *target = "/dev/mmcblk0";
//     if (umount("/dev/mmcblk0p1") == -1) 
//     {
//         perror("umount");

//     }
   
// }

#include <stdio.h>
#include <pwd.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

uid_t get_uid_using_system() {
    char command[50]; // Đủ để chứa lệnh "id -u"
    char buffer[10];  // Đủ để chứa UID (tối đa là 10 ký tự)

    // Xây dựng lệnh "id -u" để lấy UID
    strcpy(command, "id -u");

    // Mở luồng để đọc kết quả từ lệnh
    FILE* fp = popen(command, "r");
    if (fp == NULL) {
        perror("Failed to run command");
        exit(EXIT_FAILURE);
    }

    // Đọc UID từ luồng
    if (fgets(buffer, sizeof(buffer), fp) == NULL) {
        perror("Failed to read output of command");
        exit(EXIT_FAILURE);
    }

    // Đóng luồng
    pclose(fp);

    // Chuyển đổi kết quả từ chuỗi sang uid_t (unsigned int)
    uid_t uid = atoi(buffer);

    return uid;
}

int main() {
    uid_t uid = get_uid_using_system();

    printf("UID: %d\n", uid);

    return 0;
}


