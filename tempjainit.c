#include <stdio.h>
#include <stdlib.h>
#include <string.h>

char* getDirectoryPath(const char* path) {
    int length = strlen(path);

    // Find the last occurrence of '/'
    int lastSlashIndex = -1;
    for (int i = length - 1; i >= 0; i--) {
        if (path[i] == '/') {
            lastSlashIndex = i;
            break;
        }
    }

    if (lastSlashIndex != -1) {
        // Allocate memory for the new string
        char* directoryPath = (char*)malloc((lastSlashIndex + 2) * sizeof(char));

        // Copy the directory path into the new string
        strncpy(directoryPath, path, lastSlashIndex + 1);

        // Add null terminator
        directoryPath[lastSlashIndex + 1] = '\0';

        return directoryPath;
    } else {
        // No '/' found, the path is already a directory
        return strdup(path);  // Duplicate the string to ensure the original is not modified
    }
}

int main() {
    // Example usage
    const char* path = "/path/to/some/file.txt";
    printf("Original path: %s\n", path);

    // Get the directory path
    char* directoryPath = getDirectoryPath(path);
    printf("Original path: %s\n", path);

    printf("Directory path: %s\n", directoryPath);

    // Don't forget to free the allocated memory when done
    free(directoryPath);

    return 0;
}
