#define _POSIX_C_SOURCE 200809L
#include <stdio.h>
#include <stdlib.h>
#include <signal.h>
#include <unistd.h>
#include <fcntl.h>
#include <string.h>

// Handler pentru SIGUSR1 - Notificare adaugare raport
// Folosim siginfo_t ca sa aflam PID-ul procesului care a trimis semnalul
void handler_notificare(int sig, siginfo_t *info, void *context) {
    char msg[128];
    // info->si_pid ne zice exact cine a rulat city_manager
    int len = snprintf(msg, sizeof(msg), 
        "\n[Monitor] Notificare primita! Managerul (PID: %d) a adaugat un raport.\n", 
        info->si_pid);
    
    // In handlere folosim write, e mai sigur decat printf
    write(STDOUT_FILENO, msg, len);
}

// Handler pentru SIGINT (Ctrl+C)
void handler_inchidere(int sig) {
    const char *msg = "\n[Monitor] Inchidere... Curat fisierul .monitor_pid\n";
    write(STDOUT_FILENO, msg, strlen(msg));
    unlink(".monitor_pid"); // Stergem urma sa nu ramana PID vechi acolo
    _exit(0);
}

int main() {
    // 1. Inregistram PID-ul curent intr-un fisier ascuns
    // Managerul il va citi de aici ca sa stie unde sa trimita semnalul.
    int fd = open(".monitor_pid", O_WRONLY | O_CREAT | O_TRUNC, 0644);
    if (fd >= 0) {
        char pid_str[16];
        int len = snprintf(pid_str, sizeof(pid_str), "%d", getpid());
        write(fd, pid_str, len);
        close(fd);
    }

    printf("[Monitor] Proces pornit cu PID: %d. In asteptare...\n", getpid());

    // 2. Setam SIGUSR1 cu flag-ul SA_SIGINFO pentru a primi detalii despre expeditor
    struct sigaction sa_usr;
    sa_usr.sa_sigaction = handler_notificare;
    sigemptyset(&sa_usr.sa_mask);
    sa_usr.sa_flags = SA_SIGINFO; 
    sigaction(SIGUSR1, &sa_usr, NULL);

    // 3. Setam SIGINT pentru o iesire curata
    struct sigaction sa_int;
    sa_int.sa_handler = handler_inchidere;
    sigemptyset(&sa_int.sa_mask);
    sa_int.sa_flags = 0;
    sigaction(SIGINT, &sa_int, NULL);

    // 4. Bucla infinita - sta in asteptare cu procesorul la 0
    while(1) {
        pause(); 
    }

    return 0;
}