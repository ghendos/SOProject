#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <dirent.h>
#include <sys/stat.h>
#include <unistd.h>
#include <sys/wait.h>
#include <pwd.h>
#include <grp.h>
#include <time.h>

#define BUFFER_SIZE 1024

void setPermissions(mode_t mode, char *permStr)
{
    strcpy(permStr, "---------");
    if (mode & S_IRUSR) permStr[0] = 'r';
    if (mode & S_IWUSR) permStr[1] = 'w';
    if (mode & S_IXUSR) permStr[2] = 'x';
    if (mode & S_IRGRP) permStr[3] = 'r';
    if (mode & S_IWGRP) permStr[4] = 'w';
    if (mode & S_IXGRP) permStr[5] = 'x';
    if (mode & S_IROTH) permStr[6] = 'r';
    if (mode & S_IWOTH) permStr[7] = 'w';
    if (mode & S_IXOTH) permStr[8] = 'x';
}

int checkDangerous(const char *path)
{
    int status;
    pid_t pid = fork();

    if (pid == -1)
    {
        perror("Fork failed");
        return 0;
    }
    else if (pid == 0)
    {
        execlp("./verify_danger.sh", "verify_danger.sh", path, NULL);
        perror("Failed to execute script");
        exit(1);
    }

    waitpid(pid, &status, 0);
    return WIFEXITED(status) && WEXITSTATUS(status) == 1;
}

void isolateFile(const char *path, const char *isolationDir)
{
    char newLocation[PATH_MAX];
    sprintf(newLocation, "%s/%s", isolationDir, strrchr(path, '/') + 1);
    rename(path, newLocation);
    printf("File %s has been moved to %s for isolation.\n", path, newLocation);
}

void createDirectorySnapshot(char *directoryPath, FILE *outputFile, const char *isolationDir, int writeToPipe)
{
    char fullPath[1000];
    struct dirent *entry;
    DIR *dir = opendir(directoryPath);
    struct stat attributes;
    struct tm *modTime;
    char perms[10];
    int dangerousCount = 0;

    if (!dir)
    {
        perror("Directory could not be opened");
        return;
    }

    while ((entry = readdir(dir)) != NULL)
    {
        if (strcmp(entry->d_name, ".") != 0 && strcmp(entry->d_name, "..") != 0)
        {
            strcpy(fullPath, directoryPath);
            strcat(fullPath, "/");
            strcat(fullPath, entry->d_name);

            if (stat(fullPath, &attributes) == 0)
            {
                modTime = localtime(&attributes.st_mtime);
                setPermissions(attributes.st_mode, perms);
                fprintf(outputFile, "Name: %s\nSize: %ld\nPermissions: %s\nLast Modified: %d-%02d-%02d %02d:%02d:%02d\nOwner: %s, Group: %s\nInode Number: %ld\n\n",
                        fullPath, attributes.st_size, perms,
                        modTime->tm_year + 1900, modTime->tm_mon + 1, modTime->tm_mday,
                        modTime->tm_hour, modTime->tm_min, modTime->tm_sec,
                        getpwuid(attributes.st_uid)->pw_name,
                        getgrgid(attributes.st_gid)->gr_name, attributes.st_ino);

                if (strcmp(perms, "---------") == 0)
                {
                    if (checkDangerous(fullPath))
                    {
                        dangerousCount++;
                        isolateFile(fullPath, isolationDir);
                    }
                }

                if (S_ISDIR(attributes.st_mode))
                {
                    createDirectorySnapshot(fullPath, outputFile, isolationDir, writeToPipe);
                }
            }
        }
    }

    char message[BUFFER_SIZE];
    snprintf(message, BUFFER_SIZE, "Child process %d finished with PID %d and detected %d potentially dangerous files.\n", getpid(), getpid(), dangerousCount);
    write(writeToPipe, message, strlen(message));

    closedir(dir);
}

int main(int argc, char *argv[])
{
    if (argc < 6)
    {
        printf("Usage: %s -o <output_dir> -s <isolation_dir> <dir1> <dir2> ...\n", argv[0]);
        return 1;
    }

    char *outputDir = argv[2];
    char *isolationDir = argv[4];
    mkdir(outputDir, 0755);
    mkdir(isolationDir, 0755);

    int fds[2];
    if (pipe(fds) == -1)
    {
        perror("Could not create pipe");
        exit(1);
    }

    for (int i = 5; i < argc; i++)
    {
        pid_t pid = fork();
        if (pid == 0)
        {
            close(fds[0]);
            char snapshotFilePath[1024];
            snprintf(snapshotFilePath, sizeof(snapshotFilePath), "%s/%s_snapshot.txt", outputDir, argv[i]);
            FILE *filePtr = fopen(snapshotFilePath, "w");
            if (!filePtr)
            {
                perror("Failed to open snapshot file");
                exit(1);
            }
            createDirectorySnapshot(argv[i], filePtr, isolationDir, fds[1]);
            fclose(filePtr);
            close(fds[1]);
            exit(0);
        }
    }

    close(fds[1]);

    char readBuffer[BUFFER_SIZE];
    while (read(fds[0], readBuffer, BUFFER_SIZE) > 0)
    {
        printf("%s", readBuffer);
    }
    close(fds[0]);

    while (wait(NULL) > 0);

    return 0;
}
