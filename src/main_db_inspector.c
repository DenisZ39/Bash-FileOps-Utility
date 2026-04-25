#include "db_format.h"
#include <stdio.h>
#include <fcntl.h>
#include <unistd.h>

int main(int argc, char *argv[]) {
    if (argc < 2) return 1;
    int fd = open(argv[1], O_RDONLY);
    if (fd == -1) return 1;

    db_header h;
    read(fd, &h, sizeof(db_header));
    close(fd);

    int expected = sizeof(db_header) + (h.record_count * sizeof(proc_record));
    printf("%d", expected);
    
}
// program folosit special pentru a returna marimea teoretica a fisierului ca sa ma pot folosi de ea in t3_concurrency.sh
