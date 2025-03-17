#include <stdio.h>
#include <dirent.h>
#include <string.h>
#include <stdio.h>
#include <unistd.h>
#include <sys/stat.h>

//long targetFileCount = 1347375; //the aim is to process 1347375 files in a reasonable speed
//char devFilePath[] = "/dev/nvme0n1p2";
char osSep = '/';

void addChar(char *s, char c) {
    //https://www.geeksforgeeks.org/how-to-append-a-character-to-a-string-in-c/
    while (*s++); // Move pointer to the end
    *(s - 1) = c; // Append the new character
    *s = '\0'; // Add null terminator to mark new end
}

void ensureOsSep(char *s){
    if (s[strlen(s)-1] != osSep) {
        addChar(s, osSep);
    }
}

void getFileInfo(char sourcePath[]){
    printf("\033[34m%s\033[0m\n", sourcePath);
    
    //get stats
    struct stat filePathStatBuf;
    stat(sourcePath, &filePathStatBuf);
    
    //print stats
    printf("\nFile size: %d", filePathStatBuf.st_size);
}

void travelDirectory(char sourcePath[]){        
    //ensure source path ends with the os seperator
    ensureOsSep(sourcePath);
    getFileInfo(sourcePath);
    
    //https://iq.opengenus.org/traversing-folders-in-c/    
    DIR *sourceDir = opendir(sourcePath); //get sourceDir as a pointer to a DIR struct from sourceFolder string. returns DIR upon success, NULL upon failure
    struct dirent *dp; 
    char* file_name;  //define the filename variable
    
    struct stat filePathStatBuf;
    char filePath[1000];    

    // opendir returns NULL if couldn't open directory 
    if (!sourceDir)
        return;

    //loop through every file under sourcePath
    while ((dp=readdir(sourceDir)) != NULL) {        
        //get the file name from dp
        file_name = dp->d_name;         
        
        //if we are on a refrence to the current(.) or parent(..) directory, skip this loop
        if (strcmp(file_name, ".") == 0 || strcmp(file_name, "..") == 0)
            continue;
        
        //concatonate the source path and file name together into a new variable        
        strcpy(filePath, sourcePath);
        strcat(filePath, file_name);

        //find if the filepath is a directory or not
        stat(filePath, &filePathStatBuf);
        if (S_ISDIR(filePathStatBuf.st_mode)){
            //if it is a directory
            travelDirectory(filePath);
        } else {
            //if it is a filepath
            getFileInfo(filePath);
            //printf("\033[34m%s\033[0m\n", filePath);
        }        
    }
    closedir(sourceDir); //closes the sourceDir DIR struct
}

int main() {
    //ensure last character of source folder is the os seperator
    char sourceFolder[] = "/media/zoey/DATA/BACKUP/Pictures/this user/";
    ensureOsSep(sourceFolder);
    
    //ensure the last character of dest folder is the os seperator
    char destFolder[] = "/home/zoey/Desktop/test";
    ensureOsSep(destFolder);
    
    //recursively itterate through every file and folder in source directory, printing out the names of all directories and files, including source directory
    printf("Copying from \"%s\" to \"%s\"\n", sourceFolder, destFolder);
    travelDirectory(sourceFolder);    
    return 0;
} 

