### db_format.h
## L-am creat pentru al scrie doar odata si apoi sa il refolosec cand am nevoie de el.
# structura db_header 
contine un camp magic care ne valideaza daca fisierul deschis este o baza de date create de noi, format_version lasa programul sa stie daca formatul a fost schimbat in timp, record_count retine nr. de fisiere indexate permitandu ne sa stim cate structuri file_record  urmeaza
# structura file_record
abspath esentiala pt a gasi fisierul indiferent unde se afla, checksum este spatiul pt amprenta digitala a continutului, st_dev=id ul, st_ino=inode ul(ramane acelasi chiar daca schimbam numele fisierului)

### indexer.c
## L am creat pentru a scrie functiile care scaneaza fisierele
# xor_checksum
citim fisierul pe bucati de 4kb pt a nu suprasolicita daca e prea mare, aplicam xor(^=) intre fiecare bit citit si variabila de stare,daca aceasta suma difera de ultima suma calculata rezulta ca fisierul a fost modificat, complexitate O(n),
# walk_dir
recursivitate de fiecare data cand gasesc un director, eviatarea buclelor infinite trecand peste cand gasim '.'(director curent) si '..'(dir parinte), folosesc lstat pt a citi info fara sa urmaresc link urile simbolice,iar pt fiecare fisier in calculezul hash ul si ii afisez adresa de la radacina
# main
am adaugat main ul de care nu ne putem lipsi pt a rula codul, din main, orice director rulez fiecarui fisier din el ii sunt salvate informatiile obtinute din fct walk_dir in index.db care este un fisier binar