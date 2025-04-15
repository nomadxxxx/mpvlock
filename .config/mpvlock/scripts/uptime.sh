#!/usr/bin/env bash
# Print system uptime in a human-readable format using /proc/uptime

if [[ -r /proc/uptime ]]; then
    s=$(< /proc/uptime)
    s=${s%% *}
    s=${s%.*}
else
    echo "Error: Uptime could not be determined." >&2
    exit 1
fi

d=$((s / 60 / 60 / 24))
h=$((s / 60 / 60 % 24))
m=$((s / 60 % 60))

parts=()
((d > 0)) && parts+=("$d day$([[ $d -ne 1 ]] && echo s)")
((h > 0)) && parts+=("$h hour$([[ $h -ne 1 ]] && echo s)")
((m > 0)) && parts+=("$m minute$([[ $m -ne 1 ]] && echo s)")

if ((${#parts[@]} == 0)); then
    uptime="$s seconds"
else
    if ((${#parts[@]} == 1)); then
        uptime="${parts[0]}"
    elif ((${#parts[@]} == 2)); then
        uptime="${parts[0]} and ${parts[1]}"
    else
        uptime="${parts[0]}, ${parts[1]} and ${parts[2]}"
    fi
fi

echo "uptime $uptime"
