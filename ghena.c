#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <dirent.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <unistd.h>

typedef struct {
    char nume[256];
    long dimensiune;
} Metadate;

void creareSnapshot(const char *cale, const char *directorSnapshot) {
    DIR *dir = opendir(cale);
    if (dir == NULL) {
        perror("Eroare la deschiderea directorului");
        exit(EXIT_FAILURE);
    }

    struct dirent *intrare;
    struct stat statusFisier;
    Metadate metadate;

    while ((intrare = readdir(dir)) != NULL) {
        if (strcmp(intrare->d_name, ".") && strcmp(intrare->d_name, "..")) {
            char caleNoua[257];
            sprintf(caleNoua, "%s/%s", cale, intrare->d_name);

            if (stat(caleNoua, &statusFisier) == -1) {
                perror("Eroare la obținerea metadatelor fișierului");
                exit(EXIT_FAILURE);
            }

            strcpy(metadate.nume, intrare->d_name);
            metadate.dimensiune = statusFisier.st_size;

            char caleSnapshot[512];
            sprintf(caleSnapshot, "%s/%s.snapshot", directorSnapshot, metadate.nume);

            FILE *snapshot = fopen(caleSnapshot, "w");
            if (snapshot != NULL) {
                fwrite(&metadate, sizeof(Metadate), 1, snapshot);
                fclose(snapshot);
                printf("Snapshot pentru %s creat cu succes.\n", metadate.nume);
            } else {
                perror("Eroare la crearea snapshot-ului");
                exit(EXIT_FAILURE);
            }
        }
    }

    closedir(dir);
}

void parcurgere(const char *path) {
    struct stat fisIn;
    stat(path, &fisIn);

    if (S_ISDIR(fisIn.st_mode)) {
        struct dirent *db;
        DIR *director = opendir(path);

        while ((db = readdir(director)) != NULL) {
            char new[257];
            sprintf(new, "%s/%s", path, db->d_name);

            if (strcmp(db->d_name, ".") && strcmp(db->d_name, "..")) {
                printf("| - %s\n", db->d_name);
                parcurgere(new);
            }
        }
        closedir(director);
    }
}

int main(int argc, char *argv[]) {
    if (argc != 4) {
        fprintf(stderr, "Utilizare: %s director1 director2 ... director10 -o director_iesire\n", argv[0]);
        return EXIT_FAILURE;
    }

    char *directorIesire = NULL;
    int index = -1;

    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "-o") == 0 && i < argc - 1) {
            directorIesire = argv[i + 1];
            index = i + 1;
            break;
        }
    }

    if (index == -1) {
        fprintf(stderr, "Utilizare: %s director1 director2 ... director10 -o director_iesire\n", argv[0]);
        return EXIT_FAILURE;
    }

    DIR *verificareDir = opendir(directorIesire);
    if (verificareDir != NULL) {
        printf("Directorul de ieșire \"%s\" există deja.\n", directorIesire);
        closedir(verificareDir);
    } else {
        if (mkdir(directorIesire, 0755) == -1) {
            perror("Eroare la crearea directorului de ieșire");
            return EXIT_FAILURE;
        }
    }

    for (int i = 1; i < argc; i++) {
        if (i != index && strcmp(argv[i], "-o") != 0) {
            pid_t pid = fork();
            if (pid == -1) {
                perror("Eroare la crearea procesului copil");
                exit(EXIT_FAILURE);
            } else if (pid == 0) {
                creareSnapshot(argv[i], directorIesire);
                exit(EXIT_SUCCESS);
            }
        }
    }

    int status;
    pid_t childPid;
    while ((childPid = wait(&status)) != -1) {
        if (WIFEXITED(status)) {
            printf("Procesul copil %d a fost terminat cu codul de ieșire %d.\n", childPid, WEXITSTATUS(status));
        } else {
            printf("Procesul copil %d a fost terminat anormal.\n", childPid);
        }
    }

    return EXIT_SUCCESS;
}
