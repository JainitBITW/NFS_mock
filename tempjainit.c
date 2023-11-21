#include <stdio.h>
#include <string.h>

void getSubstringBeforeLastSlash(const char *input, char *output, size_t outputSize) {
    const char *slashPosition = strrchr(input, '/');

    if (slashPosition != NULL) {
        size_t length = slashPosition - input;
        if (length < outputSize) {
            strncpy(output, input, length);
            output[length] = '\0';  // Null-terminate the output string
        } else {
            fprintf(stderr, "Output buffer too small for substring.\n");
        }
    } else {
        // No '/' found, copy the whole string
        strncpy(output, input, outputSize - 1);
        output[outputSize - 1] = '\0';  // Null-terminate the output string
    }
}

int main() {
    const char *inputString = "path/to/your/directory/yourfile.txt";
    char outputBuffer[256];  // Adjust the size based on your needs

    getSubstringBeforeLastSlash(inputString, outputBuffer, sizeof(outputBuffer));

    printf("Substring before the last '/': %s\n", outputBuffer);

    return 0;
}
