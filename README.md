Nume: STIUCA Roxana-Elena
Grupa: 335CB

# Tema 4 SO - Planificator de threaduri

### Organizare
Planificatorul este definit prin structura scheduler_t, cu componentele:
- time_quantum: cuanta de timp alocata unui thread in executie;
- io_devices: numarul de device-uri IO permise;
- ready: coada de prioritati in care sunt tinute thread-urile ce asteapta
sa fie planificate spre executie;
- finished: coada cu thread-urile terminate;
- waiting_io: vector de cozi pentru thread-urile ce asteapta dupa fiecare
device IO;
- running: thread-ul ce ruleaza in acest moment;
- threads_no: numarul de thread-uri ce au fost adaugate planificatorului
prin so_fork;
- stop: element de sincronizare utilizat in marcarea terminarii tuturor
thread-urilor.

### Implementare
Este implementat intreg enuntul.

#### Implementare cozi/cozi prioritate
Acestea sunt implementate in queue.h/.c si in list.h/.c.
Coada are intern o lista simplu-inlanuita.
Coada are este generica (permite adaugarea unor elemente de orice tip).
Poate fi folosita ca o coada de prioritati prin specificarea functiei care
verifica prioritatea unui element. Sau poate fi folosita ca o coada normala
(inserare in back, pop din front) daca functia de prioritate intoarce aceeasi
valoare pentru orice element.

#### Implementare thread
Structura **thread_t** contine:
- tid: ID-ul thread-ului obtinut in urma pthread_create;
- handler: functia handler;
- priority: prioritatea;
- state: starea curenta a thread-ului (din valorile: NEW, READY, RUNNING, WAITING,
TERMINATED);
- time_remaining: cat mai are din cuanta de timp, in caz ca thread-ul este RUNNING;
- is_planned: element de sincronizare (semafor) prin care thread-ul a dat so_fork
asteapta pana ce noul thread a fost inclus in planificator;
- is_running: element de sincronizare (semafor) prin care un thread isi asteapta
executie.

#### Implementare planificator
**so_fork**:

Cand un nou thread este adaugat, el este creat efectiv prin pthread_create, iar apoi
este adaugat in coada ready (sau direct setat ca running). Se notifica thread-ul ce
a dat so_fork ca a fost planificat, ca operatia so_fork sa se incheie.
Noul thread asteapta pana cand este in starea RUNNING pentru a apela handler.
La intoarcerea din handler, seteaza campul state ca fiind TERMINATED si se actualizeaza
planificatorul (un nou thread trece din READY in RUNNING).

**so_exec**:

Decrementeaza timpul ramas al thread-ului curent. Daca ii expira cuanta, atunci este trecut
in starea READY si un alt thread din coada ready trece in RUNNING. Idem daca primul thread
din ready are prioritate mai mare decat cel RUNNING.

**IO**:

Fiecare dispozitiv IO disponibil are asociat un index in vectorul waiting_io. Cand
un thread apeleaza so_wait, el trece in starea WAITING si este transferat din running
in coada coresponzatoare dintre waiting_io.
Thread-ul asteapta pana este din nou RUNNING.

Cand un thread apeleaza so_signal, toate thread-urile din coada coresponzatoare
device-ului dintre waiting_io trec din WAITING in READY.

### Cum se compileaza si cum se ruleaza?
- **Compilare**: Linux - make.

### Git
https://github.com/roxanastiuca/thread_scheduler
