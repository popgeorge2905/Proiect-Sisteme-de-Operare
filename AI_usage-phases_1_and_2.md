Raport utilizare AI - Faza 1 si 2
Student: Pop George
Tool: Google Gemini

Faza 1

Pentru prima parte a proiectului, m-am ajutat de AI la functia de filtrare (parse_condition). I-am cerut o solutie ca sa sparg un string in 3 variabile, iar el mi-a dat un cod super intortocheat cu strchr si pointeri. Era si greu de citit si periculos la memorie daca bagam un input aiurea. Pana la urma am sters ce a facut si am folosit direct sscanf, care a rezolvat totul elegant dintr-o linie. 

La functia match_condition, care verifica daca rapoartele respecta filtrele, AI-ul a facut o greseala de incepator: a uitat ca argumentele de pe comanda vin sub forma de text (string). El a incercat sa compare direct variabila "severity" (care in struct e int) cu un char*. Am gasit eroarea si a trebuit sa bag eu un atoi() pentru conversie inainte de comparatii ca sa compileze fara probleme.

Faza 2

La partea de procese si semnale, l-am pus sa imi faca un schelet pentru monitor_reports.c care sa prinda semnalele SIGINT si SIGUSR1. I-am specificat sa foloseasca sigaction, dar in interiorul handler-ului mi-a pus functia printf ca sa afiseze mesaje pe ecran. Am discutat si la curs ca printf nu e ok in semnale pentru ca are buffere interne si poate bloca programul (nu e async-signal-safe). Asa ca am inlocuit manual toate printf-urile din handler cu functia write direct catre STDOUT.

La partea de stergere a districtelor din manager, i-am cerut sa foloseasca fork si exec. AI-ul mi-a trantit direct un execlp cu rm -rf fix pe numele folderului primit din terminal. Mi-am dat seama ca e o problema imensa de securitate, pentru ca daca utilizatorul scria "/", comanda stergea tot sistemul de operare. Am adaugat eu in cod o protectie cu strchr ca sa interzic slash-urile din nume si m-am asigurat ca parintele da wait pe copil ca sa nu ramana agatat ca proces zombie.
