#include <stdio.h>
#include <sys/stat.h>

int isDirectory(const char *path) {
    struct stat fileStat;

    // Use stat to get information about the file or directory
    if (stat(path, &fileStat) == 0) {
        // Check if it's a directory
        if (S_ISDIR(fileStat.st_mode)) {
            return 1; // True, it's a directory
        } else {
            return 0; // False, it's not a directory
        }
    } else {
        perror("Error getting file/directory information");
        return -1; // Error
    }
}

int main() {
    const char *path = "./src/1d";

    int result = isDirectory(path);

    if (result == 1) {
        printf("%s is a directory.\n", path);
    } else if (result == 0) {
        printf("%s is not a directory.\n", path);
    } else {
        printf("Error checking %s\n", path);
    }

    return 0;
}
