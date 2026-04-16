#!/bin/bash

start_timestamp=$(date +%s)
start_human=$(date)

mkdir -p reports/fs reports/process reports/proc reports/pipeline 

ls -la > reports/fs/A1_ls_long.txt
find . -type f -name "*.sh" > reports/fs/A2_find_sh.txt
du -h -d 1 > reports/fs/A3_du_level1.txt

ps aux --sort=-%mem | head -n 11 > reports/process/B1_top_mem.txt
pstree -p > reports/process/B2_pstree.txt
sleep 60 & pgrep -l sleep > reports/process/B3_pgrep_sleep.txt

grep "model name" /proc/cpuinfo > reports/proc/C1_cpu_model.txt
grep -E "MemTotal|MemAvailable" /proc/meminfo > reports/proc/C2_mem_total_avail.txt
cut -f1 -d" " /proc/uptime > reports/proc/C3_uptime.txt

find . -type f | xargs du -b | sort -nr | head -5 > reports/pipeline/D1_top5_large_files.txt
ps -eo pid,comm --sort=-%mem | grep -v "PID" | uniq | head -6 > reports/pipeline/D2_top5_proc_mem_pid_name.txt
grep "\`" doc/T1_comenzi.md | sort | uniq | wc -l > reports/pipeline/D3_count_commands.txt

end_timestamp=$(date +%s)
end_human=$(date)

durata=$((end_timestamp - start_timestamp))

echo "Timp start: $start_human (Timestamp: $start_timestamp)" >> reports/T1_sumary.txt
echo "Timp end: $end_human (Timestamp: $end_timestamp)" >> reports/T1_sumary.txt
echo "Timp de rulare: $durata secunde" >> reports/T1_sumary.txt
echo "Cai create/verificate" >> reports/T1_sumary.txt
echo "reports/fs" >> reports/T1_sumary.txt
echo "reports/process" >> reports/T1_sumary.txt
echo "reports/proc" >> reports/T1_sumary.txt
echo "reports/pipeline" >> reports/T1_sumary.txt

echo "Audit complet"
