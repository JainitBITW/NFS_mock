#include <stdio.h>
#include <dirent.h>
#include <string.h>
typedef struct StorageServer {
    char ipAddress[16];  // IPv4 Address
    int nmPort;          // Port for NM Connection
    int clientPort;      // Port for Client Connection
    char accessiblePaths[1000][1000]; // List of accessible paths
    int numPaths ; 

    // Other metadata as needed
} StorageServer;

StorageServer storageServer;
void update_accessible_paths_recursive(char *path) {
    DIR *dir;
    struct dirent *entry;

    // Open the directory
    dir = opendir(path);

    if (dir == NULL) {
        perror("opendir");
        return;
    }

    // Iterate over entries in the directory
    while ((entry = readdir(dir)) != NULL) {
        // Skip "." and ".."
        if (strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0) {
            continue;
        }

        // Create the full path of the entry
        char full_path[PATH_MAX];
        snprintf(full_path, sizeof(full_path), "%s/%s", path, entry->d_name);

        // Print or process the full path as needed
        strcpy(storageServer.accessiblePaths[storageServer.numPaths], full_path);
        storageServer.numPaths++;


        // If it's a directory, recursively call the function
        if (entry->d_type == DT_DIR) {
            update_accessible_paths_recursive(full_path);
        }
    }

    // Close the directory
    closedir(dir);
}

int main() {
    char path[] = "./src";
    storageServer.numPaths = 1;
    strcpy(storageServer.accessiblePaths[0], path);
    update_accessible_paths_recursive(path);

    for (int i = 0; i < storageServer.numPaths; i++) {
        printf("%s\n", storageServer.accessiblePaths[i]);
    }

    return 0;
}
