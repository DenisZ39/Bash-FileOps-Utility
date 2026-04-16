# T1-comenzi

## A) Filesystem
### A1) Listarea continutului directorului curent intr-un anumit format
1. Descriere: Listeaza continutul directorului curent in format "long" si include fisierele ascunse
2. Comanda: `ls -la` 
	* ls listeaza, -l arata sub forma de lista, -a arata toate fisierele, si cele ascunse
3. Output: "reports/fs/A1_ls_long.txt"
4. Comanda completa: "ls -la > reports/fs/A1_ls_long.txt"
### A2) Gasire fisiere cu o anumita extensie
1. Gasesc fisierele cu extensia ".sh" si afisez calea lor relativa
2. `find . -type f -name "*.sh"`
	* find . cauta in folderul curent , -tyoe f cauta doar fisiere(nu foldere),-namw"*.sh" doar cele care se termina in .sh
3. 'reports/fs/A2-find_sh.txt'
4. 'find . -type f -name "*.sh" > reports/fs/A2_find_sh.txt'
### A3) Afisez inaltimea pt fisierele de nivel 1 din proiect
1. Afisez inaltimea (human-readable) pt directoarele de nivel 1 din proiect
2. `du -h -d 1`
	* du arata cat spatiu consuma, -h le transforma in human readable(mb,gb), -d 1 sa faca doar la fisierele de ordin 1
3. 'reports/fs/A3_du_level1.txt'
4. 'du -h -d 1 > reports/fs/A3_du_level1.txt'

## B) Procese
### B1) Afisez descrescator primele 10 procese dupa consumul de memorie
1. Afişează primele 10 procese ordonate descrescător după consumul de memorie (RSS sau %MEM)
2. `ps aux --sort=-%mem | head -n 11`
	* ps aux arata toate procesele pornite, --sort=-%mem sorteaza descrescator dupa consum, head -n 11 arata doar primeele 10
3. 'reports/process/B1_top_mem.txt'
4. 'ps aux --sort=-%mem | head -n 11 > reports/process/B1_top_mem.txt'
### B2) Arborele de procese
1. Afişează arborele de procese (process tree) pentru sistem, cu PID-uri vizibile
2. `pstree -p`
	* pstree repr un arbore genealogic al progr deschise, -p ne arata si pid ul acestora
3. 'reports/process/B2_pstree.txt'
4. 'pstree -p > reports/process/B2_pstree.txt'
### B3) Pornesc un proces si in identific dupa nume si PID
1. Pornește un proces de test (ex: sleep 60 în background) și demonstrează că îl poți identifica după nume și PID
2. `sleep 60 & pgrep -l sleep` &&  `kill [nr]`
	* sleep 60 pauza 60sec, & ca sa ruleze in fundal, pgrep -l cauta pid ul
3. 'reports/process/B3_pgrep_sleep.txt'
4. 'sleep 60 & pgrep -l sleep > reports/process/B3_pgrep_sleep.txt'
* Nota: Am pornit procesul si am afisat pid si numele cu comanda sleep 60 & pgrep -l sleep apoi l am oprit eliminandul cu pkill sleep

## C) /proc
### C1) Identific modelul CPU
1. Extrage modelul CPU (sau “model name”) din /proc/cpuinfo
2. `grep "model name" /proc/cpuinfo`
	* cauta cuintele "model name" in fisierul sistemului...
3. 'reports/proc/C1_cpu_model.txt'
4. 'grep "model name" /proc/cpuinfo > reports/proc/C1_cpu_model.txt'
### C2)  Extrage “MemTotal” și “MemAvailable”
1. Extrage “MemTotal” și “MemAvailable” din /proc/meminfo
2. `grep -E "MemTotal|MemAvailable" /proc/meminfo`
	* grep -E cautare care foloseste | ca si un "sau"
3. 'reports/proc/C2_mem_total_avail.txt'
4. 'grep -E "MemTotal|MemAvailable" /proc/meminfo > reports/proc/C2_mem_total_avail.txt'
### C3) Afisez "uptime"
1. Afișează “uptime” din /proc/uptime (valoarea brută, în secunde)
2. `cut -f1 -d" " /proc/uptime`
	* cauta cat timp a trecut de cand a pornit laptop ul, -d" " arata doar ce e pama la " ",-f1 ne arata doar prima parte
3. 'reports/proc/C3_uptime.txt'
4. 'cut -f1 -d" " /proc/uptime > reports/proc/C3_uptime.txt'

## D) Pipeline
### D1) Top 5 cele mai mari fisiere
1. Construiește un top 5 al celor mai “mari” fișiere (după dimensiune) din proiect, afișând doar dimensiunea și calea
2. `find . -type f | xargs du -b | sort -nr | head -6`
	* - find . aduna fisierele din proiect
	  - xargs du -b ia fiecare fisier pe rand si masoara in bytes
	  - sort -nr le ordo cu nr si descrescator(de la mare la mic)
	  - head -6 afiseaza primele 5
3. 'reports/pipeline/D1_top5_large_files.txt'
4. 'find . -type f | xargs du -b | sort -nr | head -6 > reports/pipeline/D1_top5_large_files.txt'
### D2) Top 5 procese dupa memorie
1. Construiește un “top 5” procese după memorie, extrăgând doar câmpurile relevante (PID și nume), eliminând duplicatele (dacă apar) și limitând la 5 linii de output
2. `ps -eo pid,comm --sort=-%mem | grep -v "PID" | uniq | head -6`
	* - ps -eo afiseaza doar coloanele cu pid si nume dl toate procesele
	  - grep -v "PID" ascunde randul unde scrie PID
	  - uniq elimina duplicatele
	  - head -6 afis primele 5
3. 'reports/pipeline/D2_top5_proc_mem_pid_name.txt'
4. 'ps -eo pid,comm --sort=-%mem | grep -v "PID" | uniq | head -6 > reports/pipeline/D2_top5_proc_mem_pid_name.txt'
### D3) Extrag liniile care contin comenzi din doc/T1_comenzi.md
1. Din fișierul doc/T1_comenzi.md, extrage liniile care conțin comenzi (de ex. cele din blocuri marcate sau cele care încep cu un prompt), sortează-le și numără câte sunt
2. `grep "\`" doc/T1_comenzi.md | sort | uniq | wc -l`
	* - grep "\`" afiseaza doar randurile care contin `
	  - sort sorteaza alfabetic
	  - uniq elimina duplicatele
	  - wc -l nr liniile
3. 'reports/pipeline/D3_count_commands.txt'
4. 'grep "\`" doc/T1_comenzi.md | sort | uniq | wc -l > reports/pipeline/D3_count_commands.txt'
