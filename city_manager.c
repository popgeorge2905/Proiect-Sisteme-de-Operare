#define _POSIX_C_SOURCE 200809L

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/stat.h>
#include <time.h>

// ── Structura înregistrării ──────────────────────────────────────────
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

// Variabile globale pentru rol și utilizator curent
static char current_role[32] = "";
static char current_user[64] = "";

// ── Funcții utilitare ────────────────────────────────────────────────
void mode_to_string(mode_t m, char *str) {
    strcpy(str, "---------");
    if (m & S_IRUSR) str[0] = 'r';
    if (m & S_IWUSR) str[1] = 'w';
    if (m & S_IXUSR) str[2] = 'x';
    if (m & S_IRGRP) str[3] = 'r';
    if (m & S_IWGRP) str[4] = 'w';
    if (m & S_IXGRP) str[5] = 'x';
    if (m & S_IROTH) str[6] = 'r';
    if (m & S_IWOTH) str[7] = 'w';
    if (m & S_IXOTH) str[8] = 'x';
}

void log_action(const char *district, const char *action) {
    // Doar managerul scrie in log
    if (strcmp(current_role, "manager") != 0) return;

    char path[256];
    snprintf(path, sizeof(path), "%s/logged_district", district);
    
    int fd = open(path, O_WRONLY | O_CREAT | O_APPEND, 0644);
    if (fd < 0) return;

    char line[512];
    int len = snprintf(line, sizeof(line), "Time: %ld | Role: %s | User: %s | Action: %s\n",
                       time(NULL), current_role, current_user, action);
    write(fd, line, len);
    close(fd);
}

void setup_district(const char *district) {
    struct stat st;
    char path[256];
    
    // 1. Creare director cu permisiuni 750
    if (stat(district, &st) < 0) {
        mkdir(district, 0750);
        chmod(district, 0750);
    }

    // 2. Creare district.cfg (cu threshold default 1) dacă nu există deja
    snprintf(path, sizeof(path), "%s/district.cfg", district);
    if (stat(path, &st) < 0) {
        int fd = open(path, O_WRONLY | O_CREAT, 0640);
        if (fd >= 0) {
            write(fd, "threshold=1\n", 12);
            close(fd);
            chmod(path, 0640); // Forțăm permisiunile 640 conform enunțului
        }
    }

    // 3. Creare logged_district gol, dacă nu există deja
    snprintf(path, sizeof(path), "%s/logged_district", district);
    if (stat(path, &st) < 0) {
        int fd = open(path, O_WRONLY | O_CREAT, 0644);
        if (fd >= 0) {
            close(fd);
            chmod(path, 0644); // Forțăm permisiunile 644
        }
    }

    // 4. Creare symlink
    char symlink_name[256], target[256];
    snprintf(symlink_name, sizeof(symlink_name), "active_reports-%s", district);
    snprintf(target, sizeof(target), "%s/reports.dat", district);
    
    if (lstat(symlink_name, &st) < 0) {
        symlink(target, symlink_name);
    } else if (S_ISLNK(st.st_mode)) {
        if (stat(symlink_name, &st) < 0) {
            printf("Warning: dangling symlink detected!\n");
        }
    }
}

// ── Funcții Asistate de AI (Cerință directă din enunț) ───────────────
int parse_condition(const char *input, char *field, char *op, char *value) {
    if (sscanf(input, "%31[^:]:%3[^:]:%63s", field, op, value) == 3) return 1;
    return 0;
}

int match_condition(Report *r, const char *field, const char *op, const char *value) {
    if (strcmp(field, "severity") == 0) {
        int v = atoi(value);
        if (strcmp(op, "==") == 0) return r->severity == v;
        if (strcmp(op, ">=") == 0) return r->severity >= v;
        // Se pot adăuga restul operatorilor aici...
    }
    if (strcmp(field, "category") == 0 && strcmp(op, "==") == 0) {
        return strcmp(r->category, value) == 0;
    }
    return 0;
}

// ── Comenzile Principale ─────────────────────────────────────────────
void cmd_add(const char *district) {
    setup_district(district);
    
    char path[256];
    snprintf(path, sizeof(path), "%s/reports.dat", district);

    // Verificăm permisiunile înainte de a scrie
    struct stat st;
    if (stat(path, &st) == 0) {
        if (strcmp(current_role, "manager") == 0 && !(st.st_mode & S_IWUSR)) {
            printf("Error: Manager lacks write permission.\n"); return;
        }
        if (strcmp(current_role, "inspector") == 0 && !(st.st_mode & S_IWGRP)) {
            printf("Error: Inspector lacks write permission.\n"); return;
        }
    }

    Report r;
    r.id = (int)time(NULL); // ID simplificat bazat pe timp
    r.timestamp = time(NULL);
    strncpy(r.inspector, current_user, 63);
    
    printf("Latitude: "); scanf("%lf", &r.latitude);
    printf("Longitude: "); scanf("%lf", &r.longitude);
    printf("Category: "); scanf("%31s", r.category);
    printf("Severity (1-3): "); scanf("%d", &r.severity);
    printf("Description: "); scanf(" %255[^\n]", r.description);

    int fd = open(path, O_WRONLY | O_CREAT | O_APPEND, 0664);
    if (fd >= 0) {
        write(fd, &r, sizeof(Report));
        close(fd);
        chmod(path, 0664); // Forțăm 664 după creare
        printf("Report added successfully.\n");
        log_action(district, "add");
    }
}

void cmd_list(const char *district) {
    char path[256];
    snprintf(path, sizeof(path), "%s/reports.dat", district);

    struct stat st;
    if (stat(path, &st) < 0) {
        printf("No reports found for district %s.\n", district);
        return;
    }

    char perms[10];
    mode_to_string(st.st_mode, perms);
    printf("File: %s | Perms: %s | Size: %ld bytes\n\n", path, perms, st.st_size);

    int fd = open(path, O_RDONLY);
    if (fd < 0) return;

    Report r;
    printf("%-10s %-15s %-15s %-10s\n", "ID", "Inspector", "Category", "Severity");
    printf("----------------------------------------------------\n");
    while (read(fd, &r, sizeof(Report)) == sizeof(Report)) {
        printf("%-10d %-15s %-15s %-10d\n", r.id, r.inspector, r.category, r.severity);
    }
    close(fd);
    log_action(district, "list");
}

void cmd_remove_report(const char *district, int report_id) {
    if (strcmp(current_role, "manager") != 0) {
        printf("Error: Only managers can remove reports.\n");
        return;
    }

    char path[256];
    snprintf(path, sizeof(path), "%s/reports.dat", district);
    
    int fd = open(path, O_RDWR);
    if (fd < 0) return;

    Report r;
    off_t pos = 0;
    int found = 0;

    // Căutăm raportul
    while (read(fd, &r, sizeof(Report)) == sizeof(Report)) {
        if (r.id == report_id) {
            found = 1;
            break;
        }
        pos += sizeof(Report);
    }

    if (!found) {
        printf("Report %d not found.\n", report_id);
        close(fd);
        return;
    }

    // Shiftăm datele (suprascriem raportul sters cu urmatoarele)
    off_t read_pos = pos + sizeof(Report);
    off_t write_pos = pos;
    
    while (1) {
        lseek(fd, read_pos, SEEK_SET);
        if (read(fd, &r, sizeof(Report)) != sizeof(Report)) break;
        
        lseek(fd, write_pos, SEEK_SET);
        write(fd, &r, sizeof(Report));
        
        read_pos += sizeof(Report);
        write_pos += sizeof(Report);
    }

    // Tăiem ultimul record cu ftruncate
    struct stat st;
    fstat(fd, &st);
    ftruncate(fd, st.st_size - sizeof(Report));
    close(fd);
    
    printf("Report %d removed.\n", report_id);
    log_action(district, "remove_report");
}

void cmd_update_threshold(const char *district, int value) {
    if (strcmp(current_role, "manager") != 0) {
        printf("Error: Only managers can update threshold.\n");
        return;
    }

    setup_district(district);
    char path[256];
    snprintf(path, sizeof(path), "%s/district.cfg", district);

    struct stat st;
    if (stat(path, &st) == 0) {
        // Verificăm dacă permisiunile sunt FIX 640 conform enunțului
        if ((st.st_mode & 0777) != 0640) {
            printf("Error: district.cfg permissions are not exactly 640. Aborting.\n");
            return;
        }
    }

    int fd = open(path, O_WRONLY | O_CREAT | O_TRUNC, 0640);
    if (fd >= 0) {
        char buf[64];
        int len = snprintf(buf, sizeof(buf), "threshold=%d\n", value);
        write(fd, buf, len);
        close(fd);
        chmod(path, 0640);
        printf("Threshold updated to %d.\n", value);
        log_action(district, "update_threshold");
    }
}

void cmd_filter(const char *district, const char *condition) {
    char path[256];
    snprintf(path, sizeof(path), "%s/reports.dat", district);
    
    int fd = open(path, O_RDONLY);
    if (fd < 0) {
        printf("No reports to filter.\n");
        return;
    }

    char field[32], op[4], value[64];
    if (!parse_condition(condition, field, op, value)) {
        printf("Invalid condition format.\n");
        close(fd);
        return;
    }

    Report r;
    while (read(fd, &r, sizeof(Report)) == sizeof(Report)) {
        if (match_condition(&r, field, op, value)) {
            printf("Match found -> ID: %d | Cat: %s | Sev: %d\n", r.id, r.category, r.severity);
        }
    }
    close(fd);
    log_action(district, "filter");
}

// ── Main: Parsarea argumentelor ──────────────────────────────────────
int main(int argc, char *argv[]) {
    char district[128] = "";
    char command[32] = "";
    char arg1[128] = ""; 
    char arg2[128] = "";

    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "--role") == 0 && i + 1 < argc) {
            strncpy(current_role, argv[++i], 31);
        } else if (strcmp(argv[i], "--user") == 0 && i + 1 < argc) {
            strncpy(current_user, argv[++i], 63);
        } else if (strncmp(argv[i], "--", 2) == 0) {
            strncpy(command, argv[i] + 2, 31); // Sari peste "--"
            if (i + 1 < argc) strncpy(district, argv[++i], 127);
            if (i + 1 < argc && argv[i+1][0] != '-') strncpy(arg1, argv[++i], 127);
            if (i + 1 < argc && argv[i+1][0] != '-') strncpy(arg2, argv[++i], 127);
        }
    }

    if (strlen(current_role) == 0 || strlen(district) == 0) {
        printf("Usage: %s --role <inspector|manager> --user <name> --<command> <district> [args...]\n", argv[0]);
        return 1;
    }

    if (strcmp(command, "add") == 0) {
        cmd_add(district);
    } else if (strcmp(command, "list") == 0) {
        cmd_list(district);
    } else if (strcmp(command, "remove_report") == 0) {
        cmd_remove_report(district, atoi(arg1));
    } else if (strcmp(command, "update_threshold") == 0) {
        cmd_update_threshold(district, atoi(arg1));
    } else if (strcmp(command, "filter") == 0) {
        cmd_filter(district, arg1);
    } else {
        printf("Unknown command: %s\n", command);
    }

    return 0;
}