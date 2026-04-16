#!/bin/bash

mkdir -p logs
Timp_start_sec=$(date +%s)
Timp_start_formatat=$(date +"%Y-%m-%d %H:%M:%S")
Fisier_log="logs/fileops_$(date +%Y%m%d_%H%M%S).log"
Subcomanda_rulata="$1"
log_final(){
	cod_iesire=$?
	timp_end_sec=$(date +%s)
	timp_end_formatat=$(date +"%Y-%m-%d %H:%M:%S")
	diferenta=$((timp_end_sec - Timp_start_sec))
	echo "Subcomanda rulata: $Subcomanda_rulata" > "$Fisier_log"
	echo "Timestamp start: $Timp_start_formatat" >> "$Fisier_log"
	echo "Timestamp end: $timp_end_formatat" >> "$Fisier_log"
	echo "Diferenta: $diferenta" >> "$Fisier_log"
	echo "Exit code: $cod_iesire" >> "$Fisier_log"
}
trap log_final EXIT

function_init(){
echo "Start Init"
mkdir -p bin src include data logs reports tmp tests doc tools

if ! command -v gcc &> /dev/null; then 
	echo "Error: compilatorul gcc nu este intalat"
	exit 1
else echo "Succes"
fi
echo "Init complete"
}

recursiv(){
local folder_curent="$1"
for fisier in "$folder_curent"/*; do
	if [ -d "$fisier" ]; then
		recursiv "$fisier"
	elif [ -f "$fisier" ] && [[ "$fisier" == *.c ]]; then
		local nume_fisier=$(basename "$fisier")
		local nume_baza="${nume_fisier%.*}"
		local fisier_obj="tmp/obj/${nume_baza}.o"
		if [ ! -f "$fisier_obj" ] || [ "$fisier" -nt "$fisier_obj" ]; then
			echo "[GCC] Compilez: $fisier -> $fisier_obj"
			gcc $CFLAGS -c "$fisier" -o "$fisier_obj"
			if [ $? -ne 0 ]; then
				echo "Error la compilare $fisier!!!"
				exit 1
			fi
		else
			echo "[SKIP] $fisier este nu are modificare, nu-l compilam"
		fi

	fi
done
}

function_build(){
echo "Start Build"
Folder_sursa="src"
if [ "$2" == "--src" ] && [ -n "$3" ]; then
	Folder_sursa="$3"
fi
echo "Caut in $Folder_sursa"
if [ ! -d "$Folder_sursa" ]; then 
	echo "Error: folderul $Folder_sursa nu exita"
	exit 1
fi
mkdir -p bin tmp/obj
recursiv "$Folder_sursa"
local module_aux=""
for obj in tmp/obj/*.o; do
	[ -e "$obj" ] || continue
	local nume_obj=$(basename "$obj")
	if [[ "$nume_obj" != main_* ]]; then
		module_aux="$module_aux $obj"
	fi
done
local main=0
for main_obj in tmp/obj/main_*.o; do
	[ -e "$main_obj" ] || continue
	main=1
	local nume_main_obj=$(basename "$main_obj")
	local nume_exe="${nume_main_obj#main_}"
	nume_exe="${nume_exe%.o}"
	echo "[LINK] Executabil final: bin/$nume_exe"
	gcc $CFLAGS "$main_obj" $module_aux -o "bin/$nume_exe"
	if [ $? -ne 0 ]; then
		echo "Eroare la creare executabilul bin/$nume_exe!"
		exit 1
	fi
done
if [ $main -eq 0 ]; then
	echo "[INFO] Niciun fisier main_*.c gasit. Am compilat doar modulele aux"
fi
echo "Build complet"
}

function_run(){
echo "Start Run"
if [ "$2" != "--" ]; then
	echo "Error: lipseste '--'"
	echo "Folosire: $0 run -- nume_executabil [argumente...]"
	exit 1
fi

EXECUTABIL="$3"

if [ ! -x "bin/$EXECUTABIL" ]; then
	echo "Error: executabilul 'bin/$EXECUTABIL' nu exista sau nu are permisiune de rulare"
	exit 1
fi

shift 3
./bin/"$EXECUTABIL" "$@"
COD_IESIRE=$?
exit $COD_IESIRE
}

function_clean(){
echo "Clean"
rm -rf bin/* 2>/dev/null
rm -rf tmp/obj/* 2>/dev/null
echo "Fisierele generate de build au fost sterse din bin/ si tmp/obj/"
}

function_test(){
echo "Start Test"
mkdir -p reports
> reports/T2_tests.txt
Teste_picate=0
for test_script in $(find tests/ -type f -name "*.sh" 2>/dev/null); do
	echo "Rules testul: $test_script"
	chmod +x "$test_script"
	./"$test_script"
	if [ $? -eq 0 ]; then
		echo "PASS - $test_script" >> reports/T2_tests.txt
		echo " -> [PASS]"
	else
		echo "FAIL - $test_script" >> reports/T2_tests.txt
		echo " -> [FAIL]"
		Teste_picate=$((Teste_picate+1))
	fi
done
echo "Raportul a fost scris in reports/T2_tests.txt"
if [ "$Teste_picate" -gt 0 ]; then
	echo "$Teste_picate teste au picat"
	exit 1
else
	echo "Toate testele sunt trecute"
	exit 0
fi
}

comanda=$1

case "$comanda" in
	"init")
		function_init;;
	"build")
		function_build "$@";;
	"run")
		function_run "$@";;
	"clean")
		function_clean;;
	"test")
		function_test;;
	*)
		echo "error";;
esac

