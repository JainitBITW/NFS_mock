#include <stdio.h>
#include <dirent.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <limits.h>
#include <string.h>

#define MAX_FILES 1000
#define MAX_PATH_LENGTH 1000

char files[MAX_FILES][MAX_PATH_LENGTH];
int fileCount = 0;

void listFilesRecursively(const char *path) {
    DIR *dir;
    struct dirent *entry;

    // Open the directory
    if ((dir = opendir(path)) != NULL) {
        // Read each entry in the directory
        while ((entry = readdir(dir)) != NULL) {
            // Skip "." and ".."
            if (strcmp(entry->d_name, ".") != 0 && strcmp(entry->d_name, "..") != 0) {
                // Construct the full path of the entry
                char fullpath[PATH_MAX];
                snprintf(fullpath, sizeof(fullpath), "%s/%s", path, entry->d_name);

                // Check if the entry is a regular file
                struct stat statbuf;
                if (stat(fullpath, &statbuf) == 0 && S_ISREG(statbuf.st_mode)) {
                    // Store the file path in the array
                    strncpy(files[fileCount], fullpath, sizeof(files[fileCount]) - 1);
                    files[fileCount][sizeof(files[fileCount]) - 1] = '\0';
                    fileCount++;

                    // Ensure we don't exceed the array size
                    if (fileCount >= MAX_FILES) {
                        printf("Too many files. Increase the array size.\n");
                        break;
                    }
                }

                // If it's a directory, recursively list files within the subdirectory
                else if (S_ISDIR(statbuf.st_mode)) {
                    listFilesRecursively(fullpath);
                }
            }
        }

        // Close the directory
        closedir(dir);
    } else {
        perror("Error opening directory");
    }
}

int main() {
    const char *path = "./src";

    listFilesRecursively(path);

    // Print the stored files
    for (int i = 0; i < fileCount; i++) {
        printf("Stored File %d: %s\n", i, files[i]);
    }

    return 0;
}