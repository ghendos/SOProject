#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <time.h>
#include <sys/sysmacros.h>
#include <fcntl.h>
#include <string.h>
#include <unistd.h>
#include <stdint.h>
#include <dirent.h>

void caleaFisierului(const char *path)
{
  struct stat fisIn;
}

void parcurgere(const char *path)
{
  struct stat fisIn;
  stat(path, &fisIn);

  if (S_ISDIR(fisIn.st_mode))
  {
    struct dirent *db;
    DIR *director = opendir(path);

    while ((db = readdir(director)) != NULL)
    {
      char new[257];
      sprintf(new, "%s/%s", path, db->d_name);

      if (strcmp(db->d_name, ".") && strcmp(db->d_name, ".."))
      {
        printf("| - %s\n", db->d_name);
        parcurgere(new);
      }
    }
  }
}

int main(int argc, char *argv[])
{

  parcurgere(argv[1]);

  return 0;
}
