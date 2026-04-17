#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <dirent.h>
#include <sys/stat.h>
#include <unistd.h>
#include "structures.h"

// Funcție pentru logarea acțiunilor în 'logged_district'
void log_district_action(const char *dist_path, const char *user, const char *role, const char *msg) {
    char log_path[512];
    snprintf(log_path, sizeof(log_path), "%s/logged_district", dist_path);
    
    FILE *f = fopen(log_path, "a");
    if (!f) return;

    time_t now = time(NULL);
    char *t_str = ctime(&now);
    t_str[strlen(t_str) - 1] = '\0'; // Scoatem newline-ul

    fprintf(f, "[%s] User: %s (%s) -> %s\n", t_str, user, role, msg);
    fclose(f);
}

// Funcție care accesează și procesează un District
void process_district(const char *dist_path, const char *user, const char *role) {
    char cfg_path[512], dat_path[512];
    snprintf(cfg_path, sizeof(cfg_path), "%s/district.cfg", dist_path);
    snprintf(dat_path, sizeof(dat_path), "%s/reports.dat", dist_path);

    // 1. Citire prag severitate din district.cfg
    int threshold = 1; 
    FILE *f_cfg = fopen(cfg_path, "r");
    if (f_cfg) {
        fscanf(f_cfg, "severity_threshold=%d", &threshold);
        fclose(f_cfg);
    }

    // 2. Deschidere reports.dat (Binary)
    FILE *f_dat = fopen(dat_path, "rb");
    if (!f_dat) {
        printf("Districtul %s nu are rapoarte valide.\n", dist_path);
        return;
    }

    log_district_action(dist_path, user, role, "Accesat pentru scanare rapoarte");

    ReportRecord rec;
    printf("\n--- District: %s (Prag: %d) ---\n", dist_path, threshold);
    
    // Citire record cu record din fișierul binar
    while (fread(&rec, sizeof(ReportRecord), 1, f_dat) == 1) {
        if (rec.severity >= threshold) {
            printf("[ALERTA] ID %d | Cat: %s | Sev: %d | Desc: %s\n", 
                    rec.report_id, rec.category, rec.severity, rec.description);
        }
    }

    fclose(f_dat);
}

int main(int argc, char *argv[]) {
    if (argc < 4) {
        printf("Utilizare: %s --user <nume> <city_directory>\n", argv[0]);
        return 1;
    }

    char *user_name = argv[2];
    char *city_dir = argv[3];
    char *role = "Manager"; // Rolul implicit pentru această componentă

    DIR *dir = opendir(city_dir);
    if (!dir) {
        perror("Eroare la deschiderea directorului orasului");
        return 1;
    }

    struct dirent *entry;
    struct stat st;

    // Infrastructura de scanare a directoarelor de tip District
    while ((entry = readdir(dir)) != NULL) {
        if (strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0)
            continue;

        char full_path[512];
        snprintf(full_path, sizeof(full_path), "%s/%s", city_dir, entry->d_name);

        if (stat(full_path, &st) == 0 && S_ISDIR(st.st_mode)) {
            process_district(full_path, user_name, role);
        }
    }

    closedir(dir);
    return 0;
}