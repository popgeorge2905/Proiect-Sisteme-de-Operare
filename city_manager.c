#define _POSIX_C_SOURCE 200809L

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/stat.h>
#include <time.h>
#include <sys/wait.h>
#include <signal.h>

// Structura principala - am pus time_t ca sa avem timestamp real
typedef struct {
    int    id;
    char   inspector[64];
    double latitude;
    double longitude;
    char   category[32];
    int    severity;
    time_t timestamp;
    char   description[256];
} Report;

char rol_curent[32] = "";
char user_curent[64] = "";

// Functie utilitara ca sa transformam bitii de permisiuni in format tip ls -l
void permisiuni_in_string(mode_t mode, char *str) {
    strcpy(str, "---------");
    if (mode & S_IRUSR) str[0] = 'r';
    if (mode & S_IWUSR) str[1] = 'w';
    if (mode & S_IXUSR) str[2] = 'x';
    if (mode & S_IRGRP) str[3] = 'r';
    if (mode & S_IWGRP) str[4] = 'w';
    if (mode & S_IXGRP) str[5] = 'x';
    if (mode & S_IROTH) str[6] = 'r';
    if (mode & S_IWOTH) str[7] = 'w';
    if (mode & S_IXOTH) str[8] = 'x';
}

// Aici scriem log-ul doar daca suntem manageri. 
// Important: folosim O_APPEND ca sa nu stergem ce a scris altcineva inainte.
void scrie_in_log(const char *district, const char *actiune) {
    if (strcmp(rol_curent, "manager") != 0) return; 

    char cale_log[256];
    snprintf(cale_log, sizeof(cale_log), "%s/logged_district", district);
    
    int fd = open(cale_log, O_WRONLY | O_CREAT | O_APPEND, 0644);
    if (fd < 0) return;

    char linie[512];
    int len = snprintf(linie, sizeof(linie), "Time: %ld | Role: %s | User: %s | Action: %s\n",
                       time(NULL), rol_curent, user_curent, actiune);
    write(fd, linie, len);
    close(fd);
}

// Functia asta se asigura ca folderele si link-urile exista inainte de orice operatie.
// Am pus chmod imediat dupa creare ca sa fim siguri de permisiuni.
void pregateste_district(const char *district) {
    struct stat st;
    char cale[256];
    
    // Daca folderul nu exista, il facem noi.
    if (stat(district, &st) < 0) {
        mkdir(district, 0750);
        chmod(district, 0750);
    }

    // Config-ul are threshold=1 default.
    snprintf(cale, sizeof(cale), "%s/district.cfg", district);
    if (stat(cale, &st) < 0) {
        int fd = open(cale, O_WRONLY | O_CREAT, 0640);
        if (fd >= 0) {
            write(fd, "threshold=1\n", 12);
            close(fd);
            chmod(cale, 0640);
        }
    }

    // Log-ul trebuie sa fie acolo mereu.
    snprintf(cale, sizeof(cale), "%s/logged_district", district);
    if (stat(cale, &st) < 0) {
        int fd = open(cale, O_WRONLY | O_CREAT, 0644);
        if (fd >= 0) { close(fd); chmod(cale, 0644); }
    }

    // Link simbolic tip 'active_reports-nume'. 
    // Am folosit lstat ca sa verificam daca link-ul exista deja.
    char nume_link[256], tinta[256];
    snprintf(nume_link, sizeof(nume_link), "active_reports-%s", district);
    snprintf(tinta, sizeof(tinta), "%s/reports.dat", district);
    
    if (lstat(nume_link, &st) < 0) {
        symlink(tinta, nume_link);
    }
}

// Parser simplu pentru filtre. Format cerut: camp:operator:valoare
int extrage_conditie(const char *input, char *camp, char *op, char *valoare) {
    if (sscanf(input, "%31[^:]:%3[^:]:%63s", camp, op, valoare) == 3) {
        return 1;
    }
    return 0;
}

// Aici facem comparatia propriu-zisa pentru filtru.
// Am bagat atoi() aici pentru ca severitatea e int in structura, nu string.
int verifica_raport(Report *rap, const char *camp, const char *op, const char *val) {
    if (strcmp(camp, "severity") == 0) {
        int valoare_int = atoi(val); 
        if (strcmp(op, "==") == 0) return rap->severity == valoare_int;
        if (strcmp(op, ">=") == 0) return rap->severity >= valoare_int;
        if (strcmp(op, "<=") == 0) return rap->severity <= valoare_int;
    }
    if (strcmp(camp, "category") == 0 && strcmp(op, "==") == 0) {
        return strcmp(rap->category, val) == 0;
    }
    return 0;
}

// Operatiile cerute in Phase 1 si 2

void adauga_raport(const char *district) {
    pregateste_district(district);
    
    char cale_fisier[256];
    snprintf(cale_fisier, sizeof(cale_fisier), "%s/reports.dat", district);

    Report rap;
    rap.id = (int)time(NULL); // Folosim timpul ca ID unic (aproximativ)
    rap.timestamp = time(NULL);
    strncpy(rap.inspector, user_curent, 63);
    
    // Citim datele de la tastatura.
    printf("Latitude: "); scanf("%lf", &rap.latitude);
    printf("Longitude: "); scanf("%lf", &rap.longitude);
    printf("Category: "); scanf("%31s", rap.category);
    printf("Severity (1-3): "); scanf("%d", &rap.severity);
    printf("Description: "); scanf(" %255[^\n]", rap.description);

    int fd = open(cale_fisier, O_WRONLY | O_CREAT | O_APPEND, 0664);
    if (fd >= 0) {
        write(fd, &rap, sizeof(Report));
        close(fd);
        chmod(cale_fisier, 0664); 

        // LOGICA DE SEMNALE (Phase 2):
        // Cautam PID-ul monitorului in fisier. Daca nu e acolo, ignoram.
        int monitor_notificat = 0;
        int fd_pid = open(".monitor_pid", O_RDONLY);
        
        if (fd_pid >= 0) {
            char buffer_pid[32] = {0};
            int bytes_read = read(fd_pid, buffer_pid, sizeof(buffer_pid) - 1);
            close(fd_pid);
            
            if (bytes_read > 0) {
                int pid_monitor = atoi(buffer_pid);
                // Trimitem SIGUSR1 daca procesul exista.
                if (pid_monitor > 0 && kill(pid_monitor, SIGUSR1) == 0) {
                    monitor_notificat = 1;
                }
            }
        }

        // Scriem in log rezultatul notificarii.
        char actiune_log[128];
        if (monitor_notificat) {
            snprintf(actiune_log, sizeof(actiune_log), "add (Monitor NOTIFIED)");
            printf("Raport adaugat. Monitorul a fost notificat!\n");
        } else {
            snprintf(actiune_log, sizeof(actiune_log), "add (Monitor COULD NOT BE INFORMED)");
            printf("Raport adaugat, dar monitorul n-a putut fi anuntat (probabil e oprit).\n");
        }
        scrie_in_log(district, actiune_log);
    }
}

// Afiseaza toate rapoartele dintr-un fisier binar.
void lista_rapoarte(const char *district) {
    char cale[256];
    snprintf(cale, sizeof(cale), "%s/reports.dat", district);

    struct stat st;
    if (stat(cale, &st) < 0) {
        printf("Niciun raport gasit in %s.\n", district);
        return;
    }

    char perms[10];
    permisiuni_in_string(st.st_mode, perms);
    printf("Fisier: %s | Perms: %s | Size: %ld bytes\n\n", cale, perms, st.st_size);

    int fd = open(cale, O_RDONLY);
    if (fd < 0) return;

    Report rap;
    printf("%-10s %-15s %-15s %-10s\n", "ID", "Inspector", "Category", "Severity");
    printf("----------------------------------------------------\n");
    while (read(fd, &rap, sizeof(Report)) == sizeof(Report)) {
        printf("%-10d %-15s %-15s %-10d\n", rap.id, rap.inspector, rap.category, rap.severity);
    }
    close(fd);
    scrie_in_log(district, "list");
}

// Cauta un raport dupa ID si il printeaza frumos.
void vizualizeaza_raport(const char *district, int id_raport) {
    char cale[256];
    snprintf(cale, sizeof(cale), "%s/reports.dat", district);

    int fd = open(cale, O_RDONLY);
    if (fd < 0) return;

    Report rap;
    int gasit = 0;
    while (read(fd, &rap, sizeof(Report)) == sizeof(Report)) {
        if (rap.id == id_raport) {
            printf("\n--- Detalii %d ---\n", rap.id);
            printf("User: %s | Cat: %s | Sev: %d\nDesc: %s\n", 
                    rap.inspector, rap.category, rap.severity, rap.description);
            gasit = 1; break;
        }
    }
    close(fd);
    if (!gasit) printf("Raportul %d nu exista.\n", id_raport);
    scrie_in_log(district, "view");
}

// Aici e partea complicata: stergerea dintr-un fisier binar.
// Metoda: rescriem peste el restul fisierului si apoi ii dam ftruncate.
void sterge_un_raport(const char *district, int id_raport) {
    if (strcmp(rol_curent, "manager") != 0) {
        printf("Eroare: N-ai voie, boss. Doar managerii.\n");
        return;
    }

    char cale[256];
    snprintf(cale, sizeof(cale), "%s/reports.dat", district);
    
    int fd = open(cale, O_RDWR);
    if (fd < 0) return;

    Report rap;
    off_t pos = 0;
    int gasit = 0;

    while (read(fd, &rap, sizeof(Report)) == sizeof(Report)) {
        if (rap.id == id_raport) { gasit = 1; break; }
        pos += sizeof(Report);
    }

    if (gasit) {
        off_t citire_pos = pos + sizeof(Report);
        off_t scriere_pos = pos;
        while (1) {
            lseek(fd, citire_pos, SEEK_SET);
            if (read(fd, &rap, sizeof(Report)) != sizeof(Report)) break;
            lseek(fd, scriere_pos, SEEK_SET);
            write(fd, &rap, sizeof(Report));
            citire_pos += sizeof(Report);
            scriere_pos += sizeof(Report);
        }
        struct stat st;
        fstat(fd, &st);
        ftruncate(fd, st.st_size - sizeof(Report));
        printf("Sters cu succes.\n");
    }
    close(fd);
    scrie_in_log(district, "remove_report");
}

// Schimba limita in fisierul cfg.
void actualizeaza_limita(const char *district, int valoare) {
    if (strcmp(rol_curent, "manager") != 0) return;
    char cale[256];
    snprintf(cale, sizeof(cale), "%s/district.cfg", district);
    int fd = open(cale, O_WRONLY | O_CREAT | O_TRUNC, 0640);
    if (fd >= 0) {
        char buf[64];
        int len = snprintf(buf, sizeof(buf), "threshold=%d\n", valoare);
        write(fd, buf, len);
        close(fd);
        scrie_in_log(district, "update_threshold");
    }
}

// LOGICA DE PROCESE (Phase 2):
// Folosim fork() si exec() ca sa chemam utilitarul rm din Linux.
void sterge_district_complet(const char *district) {
    if (strcmp(rol_curent, "manager") != 0) {
        printf("Error: Manager role required.\n");
        return;
    }

    // Verificare de securitate sa nu primim un nume de folder nasol.
    if (strchr(district, '/') != NULL) {
        printf("Error: Nu e voie cu slash-uri.\n");
        return;
    }

    pid_t pid = fork();
    if (pid == 0) {
        // Suntem in copil: executam rm -rf folder
        execlp("rm", "rm", "-rf", district, NULL);
        exit(1);
    } else if (pid > 0) {
        // Suntem in parinte: asteptam copilul sa termine
        wait(NULL); 
        char nume_link[256];
        snprintf(nume_link, sizeof(nume_link), "active_reports-%s", district);
        unlink(nume_link); // Stergem si link-ul simbolic.
        printf("District sters prin proces copil (fork+exec).\n");
    }
}

// Punctul de intrare - parsam argumentele din linia de comanda
int main(int argc, char *argv[]) {
    char district[128] = "";
    char comanda[32] = "";
    char argument_extra[128] = ""; 

    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "--role") == 0 && i + 1 < argc) {
            strncpy(rol_curent, argv[++i], 31);
        } else if (strcmp(argv[i], "--user") == 0 && i + 1 < argc) {
            strncpy(user_curent, argv[++i], 63);
        } else if (strncmp(argv[i], "--", 2) == 0) {
            strncpy(comanda, argv[i] + 2, 31); 
            if (i + 1 < argc) strncpy(district, argv[++i], 127);
            if (i + 1 < argc && argv[i+1][0] != '-') strncpy(argument_extra, argv[++i], 127);
        }
    }

    if (strlen(rol_curent) == 0 || strlen(district) == 0) {
        printf("Usage: %s --role <role> --user <name> --<cmd> <dist> [args]\n", argv[0]);
        return 1;
    }

    // Switch-ul de comenzi:
    if (strcmp(comanda, "add") == 0) adauga_raport(district);
    else if (strcmp(comanda, "list") == 0) lista_rapoarte(district);
    else if (strcmp(comanda, "view") == 0) vizualizeaza_raport(district, atoi(argument_extra));
    else if (strcmp(comanda, "remove_report") == 0) sterge_un_raport(district, atoi(argument_extra));
    else if (strcmp(comanda, "update_threshold") == 0) actualizeaza_limita(district, atoi(argument_extra));
    else if (strcmp(comanda, "remove_district") == 0) sterge_district_complet(district);
    else printf("Comanda inexistenta.\n");

    return 0;
}