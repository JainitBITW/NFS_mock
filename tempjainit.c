#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <dirent.h>
#include <unistd.h>

void deleteDirectory(const char *path);

int main() {
    const char *directoryPath = "./src/1d";  // Replace with the path of the directory you want to delete
    deleteDirectory(directoryPath);

    return 0;
}

void deleteDirectory(const char *path) {
    DIR *dir;
    struct dirent *entry;
    char fullPath[PATH_MAX];

    // Open the directory
    if ((dir = opendir(path)) == NULL) {
        perror("opendir");
        exit(EXIT_FAILURE);
    }

    // Iterate through each entry in the directory
    while ((entry = readdir(dir)) != NULL) {
        if (strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0) {
            continue;  // Skip "." and ".." entries
        }

        // Create full path to the entry
        snprintf(fullPath, sizeof(fullPath), "%s/%s", path, entry->d_name);

        // Recursively delete subdirectories
        if (entry->d_type == DT_DIR) {
            deleteDirectory(fullPath);
        } else {
            // Delete regular files
            if (unlink(fullPath) != 0) {
                perror("unlink");
                exit(EXIT_FAILURE);
            }
        }
    }

    // Close the directory
    closedir(dir);

    // Remove the directory itself
    if (rmdir(path) != 0) {
        perror("rmdir");
        exit(EXIT_FAILURE);
    }
}