#include <stdio.h>
#include <stdarg.h>
#include <time.h>

// Define a function for timestamping log entries
void timestamp() {
    time_t now = time(NULL);
    struct tm *tm_struct = localtime(&now);
    printf("%02d-%02d-%04d %02d:%02d:%02d: ",
           tm_struct->tm_mday,
           tm_struct->tm_mon + 1,
           tm_struct->tm_year + 1900,
           tm_struct->tm_hour,
           tm_struct->tm_min,
           tm_struct->tm_sec);
}

// Log a request
void logRequest(const char *message, ...) {
    va_list args;
    timestamp();
    printf("REQUEST: ");
    va_start(args, message);
    vprintf(message, args);
    va_end(args);
    printf("\n");
}

// Log an acknowledgement
void logAck(const char *message, ...) {
    va_list args;
    timestamp();
    printf("ACK: ");
    va_start(args, message);
    vprintf(message, args);
    va_end(args);
    printf("\n");
}

// Log an error
void logError(const char *message, ...) {
    va_list args;
    timestamp();
    printf("ERROR: ");
    va_start(args, message);
    vprintf(message, args);
    va_end(args);
    printf("\n");
}

// General purpose display message
void displayMessage(const char *message, ...) {
    va_list args;
    timestamp();
    printf("INFO: ");
    va_start(args, message);
    vprintf(message, args);
    va_end(args);
    printf("\n");
}

// Main function to demonstrate logging
int main() {
    // Example usage of the logging functions
    logRequest("File read requested by user %s for file %s.", "Alice", "example.txt");
    logAck("Read operation acknowledged for file %s.", "example.txt");
    logError("File read error: %s.", "File not found");
    displayMessage("Server started and awaiting requests.");

    return 0;
}
