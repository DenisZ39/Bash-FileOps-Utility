# Documentație Format Binar `index.db`

---

## 1. Introducere
Acest document descrie formatul binar utilizat pentru stocarea indexului de fișiere. Formatul este optimizat pentru **acces concurent** și **dimensiune fixă**.

## 2. Structura Header-ului
Fiecare fișier bază de date începe cu un header de **32 de octeți** (incluzând padding-ul de aliniere).

| Offset | Câmp | Tip | Descriere |
| :--- | :--- | :--- | :--- |
| 0 | `magic` | `char[4]` | Trebuie să fie `IDX1` |
| 4 | `version` | `unsigned int` | Momentan versiunea `1` |
| 8 | `snapshot_id` | `unsigned long` | ID-ul sesiunii (Unix Timestamp) |

### 2.1 Definirea în C
```c
typedef struct {
    char magic[4];
    unsigned int version;
    unsigned long id;
    // ... restul campurilor
} db_header_t;


