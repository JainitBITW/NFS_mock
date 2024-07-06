# Distributed File System

### Team Members:

- Amey Choudhary 
- Jainit Bafna 
- Yash Bhaskar

We have designed and developed a distributed file system, which allows users to perform file operations and file management on files stored over distributed storage servers. The system integrates multiple storage servers and simplifies the user experience, akin to being on a single server. Our file system consists of various modules to ensure concurrency, redundancy, logging, search optimisation and error handling. It has been implemented in C. The following is the [Project Description](https://karthikv1392.github.io/cs3301_osn/project/).

![image](https://github.com/AmeyChoudhary/final-project-44-main/assets/102692017/d88c5438-2f5c-4d6f-a003-db1a772375cd)


## An overview of our NFS

### Key Features

**Initialization:**

1. **Naming Server (NM) Initialization:** The central coordinator, responsible for managing directory structure and file location information.
2. **Storage Server (SS) Initialization:** Each SS stores files, interacts with the NM and clients. Upon initialization, an SS registers with the NM, providing its IP address, ports for NM and client connections, and a list of accessible file paths.

**Storage Servers (SS):**

* **Adding New Storage Servers:** Enables dynamic addition of new SS to the system.
* **Commands from NM:**
    * Create files/directories
    * Delete files/directories
    * Copy files/directories between servers
* **Client Interactions:**
    * Read files
    * Write to files
    * Get file size and permissions

**Naming Server (NM):**

* Stores critical information from registered SS.
* Provides feedback to clients upon task completion. 

**Clients:**

* Interact with the NM for file system operations.
* Mounting directories: Locates the correct SS for a requested file/directory.
* **Client functionalities:**
    * Reading, writing, and retrieving file information
    * Creating and deleting files/folders
    * Copying files/directories between SS

**Other Features:**

* **Multiple Clients:**
    * The NM acknowledges client requests to avoid timeouts.
    * Non-blocking NM operations for smooth handling of concurrent requests.
    * Only one client can write to a file at a time, while multiple clients can read concurrently.
* **Error Codes:**
    * Defined error codes for various situations (e.g., file unavailable, write operation in progress).
* **Search in Naming Servers:**
    * Efficient search using Tries or Hashmaps for faster SS identification.
    * LRU caching to expedite subsequent requests for recently accessed data.
* **Redundancy/Replication:**
    * NM detects SS failures.
    * Data redundancy: Each file/folder is replicated in two other SS (for systems with more than 2 SS).
    * Read-only operations during SS recovery.
    * Asynchronous duplication of write commands for fault tolerance.
* **Bookkeeping:**
    * Logging for all requests/ acknowledgments from clients/SS.
    * Displaying messages indicating operation status and outcome.
    * Logging IP addresses and ports for better troubleshooting.


### High-Level Modules and Descriptions:
1. Client Module - handles client interactions, such as reading, writing, and obtaining information about files.
2. Naming Server Module - the central server that manages directories and file locations.
3. Storage Server Module - responsible for the actual data storage and file operations.
4. Communication Module - handles all network communication using TCP sockets.
5. Concurrency Module - handles concurrent client access and file operations.
6. Error Handling Module - defines and processes error codes.
7. Search Optimization Module - optimizes search operations within the Naming Server.
8. Redundancy Module - handles redundancy and replication of data.
9. Logging Module - for logging and message display.


## Instructions to run our NFS

To run our NFS, we have provided a script file, 'make.sh'. On running it, it shall compile and run the newly compiled files: client, naming server and storage server (by default 3 storage servers). 
