#include <stdio.h>
#include <stdlib.h>
#include <string.h>

int main()
{
    char path[100] = "./a";
    char *lastSlash = strrchr(path, '/');
    printf("%s\n", path);
    printf("%d\n", strlen(path));
    if (lastSlash != NULL && lastSlash != path)
    {
        // Null-terminate the string at the last '/'
        *lastSlash = '\0';

        // Print the new path
        printf("%s\n", path);
    }
    else
    {
        // Handle the case where no '/' is found or it's the first character
        printf("Invalid path format\n");
    }
}

else if (strcmp(tokenArray[0], "CREATE") == 0)
		{
			char path[1024];
			strcpy(path, tokenArray[1]);
			// strip the path of white spaces
			int len = strlen(path);
			if(isspace(path[len - 1]))
			{
				path[len - 1] = '\0';
			}

			// find out the path to be found
			char path_copy[1024];

		}