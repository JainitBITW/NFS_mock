# Distributed File System

### Team Members:

- Amey Choudhary 
- Jainit Bafna 
- Yash Bhaskar

We have designed and developed a distributed file system, which allows users to perform file operations and file management on files stored over distributed storage servers. The system integrates multiple storage servers and simplifies the user experience, akin to being on a single server. Our file system consists of various modules to ensure concurrency, redundancy, logging, search optimisation and error handling. It has been implemented in C. The following is the [Project Description](https://karthikv1392.github.io/cs3301_osn/project/).

![image](https://github.com/AmeyChoudhary/final-project-44-main/assets/102692017/d88c5438-2f5c-4d6f-a003-db1a772375cd)


### Design of our NFS

#### High-Level Modules and Descriptions:
1. Client Module - handles client interactions, such as reading, writing, and obtaining information about files.
2. Naming Server Module - the central server that manages directories and file locations.
3. Storage Server Module - responsible for the actual data storage and file operations.
4. Communication Module - handles all network communication using TCP sockets.
5. Concurrency Module - handles concurrent client access and file operations.
6. Error Handling Module - defines and processes error codes.
7. Search Optimization Module - optimizes search operations within the Naming Server.
8. Redundancy Module - handles redundancy and replication of data.
9. Logging Module - for logging and message display.

### Instructions to run our NFS

To run our NFS, we have provided a script file, 'make.sh'. On running it, it shall compile and run the newly compiled files: client, naming server and storage server (by default 3 storage servers). 


### Functionality of our NFS for client

1. Reading, Writing and Getting Size of a file.
2. Creating and Deleting a file.
3. Copying files.
4. Listing files in a directory.


