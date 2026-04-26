\# AI Usage Report - Phase 1



\## Tool Used

Gemini (Google)



\---



\## Function 1: `parse\_condition`



\*\*Prompt folosit:\*\*

"Scrie o funcție în C cu semnătura `int parse\_condition(const char \*input, char \*field, char \*op, char \*value);` care primește un string sub forma `field:operator:value` și extrage cele 3 părți. Funcția trebuie să returneze 1 dacă a reușit și 0 dacă formatul este greșit."



\*\*Ce a generat inițial AI-ul:\*\*

AI-ul a generat o funcție destul de complicată, care folosea `strchr` pentru a căuta poziția caracterului `:` de două ori, calcula lungimea sub-șirurilor cu matematică pe pointeri și apoi folosea `strncpy` pentru a copia manual fiecare bucată în variabilele `field`, `op` și `value`.



\*\*Ce am schimbat și de ce:\*\*

Am renunțat la toată logica greoaie cu pointeri și `strncpy`. În schimb, am înlocuit totul cu un singur apel de funcție: `sscanf(input, "%31\[^:]:%3\[^:]:%63s", field, op, value)`. 

\*De ce?\* Pentru că `sscanf` este modul standard, mult mai curat și mai eficient în C pentru a extrage date dintr-un șir formatat folosind delimitatori (în cazul nostru, caracterul `:`). Este mai puțin predispus la bug-uri de memorie (buffer overflow) atâta timp cât am limitat dimensiunea citită (ex: `%31`).



\---



\## Function 2: `match\_condition`



\*\*Prompt folosit:\*\*

"Am următoarea structură în C: `typedef struct { int id; char inspector\[64]; double latitude; double longitude; char category\[32]; int severity; time\_t timestamp; char description\[256]; } Report;`. 

Generează o funcție `int match\_condition(Report \*r, const char \*field, const char \*op, const char \*value);` care să compare un câmp din structură cu valoarea dată, respectând operatorul dat (==, >=, etc.)."



\*\*Ce a generat inițial AI-ul:\*\*

AI-ul a generat un lanț imens de `if / else if` pentru fiecare câmp. Problema majoră a fost că pentru câmpul `severity` (care în structură este de tip `int`), AI-ul a încercat să folosească direct `strcmp` sau nu a făcut corect conversia operatorilor de tip `<` sau `>`.



\*\*Ce am schimbat și de ce:\*\*

Am corectat conversia tipurilor de date. Argumentul `value` este de tip `const char\*` (string). Dacă `field` este "severity", nu pot compara direct `r->severity` cu un string. Așa că am adăugat conversia `int v = atoi(value);` înainte de a face comparațiile matematice (`==`, `>=`, etc.). Pentru string-uri ("category"), am păstrat `strcmp`. De asemenea, am simplificat numărul de operatori pentru a păstra codul scurt și ușor de înțeles la prezentare.



\---



\## What I learned (Ce am învățat)

Am învățat că AI-ul este excelent pentru a genera rapid scheletul unei funcții plictisitoare (cum ar fi zeci de `if`-uri pentru fiecare caz), dar nu este 100% de încredere la manipularea corectă a tipurilor de date (string vs. int). Mai mult, AI-ul tinde să scrie cod "over-engineered" (cum a fost prima variantă cu pointeri), în timp ce în C există adesea soluții mult mai elegante și scurte, cum ar fi `sscanf`.

