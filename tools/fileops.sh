#!/bin/bash

### LOG ###
START_TIME=$(date +%s)
TIMESTAMP=$(date +%Y%m%d_%H%M%S)
LOG_FILE="logs/fileops_$TIMESTAMP.log"
mkdir -p logs

function finalizeaza_log() {
    local EXIT_CODE=$?
    local END_TIME=$(date +%s)
    local DURATA=$((END_TIME - START_TIME))
    {
        echo "Subcomanda rulata: $comanda"
        echo "Timestamp start: $(date -d @$START_TIME)"
        echo "Timestamp end:   $(date -d @$END_TIME)"
        echo "Durata:          ${DURATA} secunde"
        echo "Exit code:       $EXIT_CODE"
    } > "$LOG_FILE"
}

trap finalizeaza_log EXIT

### 1) Initializare ###
check_gcc(){
    if ! command -v gcc &> /dev/null; then
        echo "Nu este instalat gcc" >&2
        exit 1
    fi
}

init(){
    echo "Se creeaza structura proiectului."
    check_gcc
    mkdir -p bin src include data logs reports tmp/obj tests doc tools
    echo "Structura creata cu succes"
}

### 2) Build ###
find_c_files(){
    local n=$1
    # Verificam daca folderul exista
    [ ! -d "$n" ] && return
    for item in "$n"/*; do
        if [ -d "$item" ]; then
            find_c_files "$item"
        elif [[ "$item" == *.c ]]; then
            C_FILES+=("$item")
        fi
    done
}

build(){
    local src_dir="src"
    while [ "$#" -gt 0 ]; do
       case $1 in
            --src) src_dir="$2"; shift ;;
       esac
       shift
    done

    mkdir -p tmp/obj bin
    declare -a C_FILES=()
    find_c_files "$src_dir"
    
    # Mai intai compilam toate modulele in .o
    local all_objs=()
    for param in "${C_FILES[@]}"; do
        local filename=$(basename "$param")
        local obj="tmp/obj/${filename%.c}.o"
        
        if [ ! -f "$obj" ] || [ "$param" -nt "$obj" ]; then
            gcc $CFLAGS -c "$param" -o "$obj" -Iinclude -Wall -Wextra
        fi
        
        if [[ "$filename" != main_* ]]; then
            all_objs+=("$obj")
        fi
    done

    # Apoi generam executabilele pentru main-uri
    for param in "${C_FILES[@]}"; do
        local filename=$(basename "$param")
        if [[ "$filename" == main_* ]]; then
            local obj="tmp/obj/${filename%.c}.o"
            local exe_name="bin/${filename#main_}"
            exe_name="${exe_name%.c}"
            # Link-uim cu TOATE obiectele pentru a avea acces la utilitare
            gcc "$obj" "${all_objs[@]}" -o "$exe_name"
        fi
    done

}

### 3) Run ###
run(){
    # Trecem peste 'run'
    shift 
    # Cautam separatorul --
    while [[ "$1" != "--" && $# -gt 0 ]]; do shift; done  
    shift # Trecem peste --
    
    local exe="./bin/$1"
    shift # Raman doar argumentele programului
    if [ -x "$exe" ]; then
        "$exe" "$@"
    else
        echo "Eroare: $exe nu exista sau nu e executabil." >&2
        exit 1
    fi
}

### 4) Clean ###
clean(){
    rm -rf tmp/obj/* bin/*
}

### 5) Test ###
testare(){
    echo "Incepe procesul de testare"
    mkdir -p reports
    local report_file="reports/T2_tests.txt"
    > "$report_file"

    mapfile -t test_files < <(find tests -type f -name "*.sh")

    local global_status=0 
    
    for t_script in "${test_files[@]}"; do
        chmod +x "$t_script"
        # Rulam si capturam rezultatul
        if "./$t_script"; then
            echo "$(basename "$t_script") PASS" >> "$report_file"
        else
            echo "$(basename "$t_script") FAIL" >> "$report_file"
            global_status=1
        fi
    done
    cat "$report_file"
    return $global_status
}

# --- ENTRY POINT ---
comanda=$1
case $comanda in
    init)  init ;;
    build) build "$@" ;;
    run)   run "$@" ;;
    clean) clean ;;
    test)  testare ;;
    *)     echo "Utilizare: $0 {init|build|run|clean|test}" ;;
esac
