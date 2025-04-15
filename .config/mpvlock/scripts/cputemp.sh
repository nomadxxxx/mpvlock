#!/usr/bin/env bash

CACHE_DIR="$HOME/.config/mpvlock"
CACHE_FILE="$CACHE_DIR/cputemp"
mkdir -p "$CACHE_DIR"

# Try lm-sensors first
if command -v sensors &>/dev/null; then
    # Get the first valid CPU temperature (integer only)
    CPU_TEMP=$(sensors | grep -E 'Package id 0:|Tctl:|Tdie:|Core 0:|Core 1:|Core 2:|Core 3:' | grep -oE '\+?[0-9]+(\.[0-9]+)?°C' | head -1 | grep -oE '[0-9]+')
    # Only use the first number found
    CPU_TEMP=$(echo "$CPU_TEMP" | head -1)
    if [[ "$CPU_TEMP" =~ ^[0-9]+$ && "$CPU_TEMP" -gt 0 ]]; then
        if ((CPU_TEMP >= 80)); then ICON="🔥"
        elif ((CPU_TEMP >= 60)); then ICON="🌡️"
        else ICON="❄️"; fi
        echo "CPU temp, ${CPU_TEMP}°C $ICON" | tee "$CACHE_FILE"
        exit 0
    fi
fi

# Fallback: /sys/class/thermal/thermal_zone*/temp
for zone in /sys/class/thermal/thermal_zone*/temp; do
    ZONE_TYPE_FILE="${zone%/temp}/type"
    if [[ -f "$ZONE_TYPE_FILE" ]]; then
        ZONE_TYPE=$(cat "$ZONE_TYPE_FILE")
        if [[ "$ZONE_TYPE" =~ cpu|x86_pkg_temp|Tdie|Tctl|core ]]; then
            RAW_TEMP=$(cat "$zone")
            CPU_TEMP=$((RAW_TEMP / 1000))
            if [[ "$CPU_TEMP" -gt 0 ]]; then
                if ((CPU_TEMP >= 80)); then ICON="🔥"
                elif ((CPU_TEMP >= 60)); then ICON="🌡️"
                else ICON="❄️"; fi
                echo "CPU temp, ${CPU_TEMP}°C $ICON" | tee "$CACHE_FILE"
                exit 0
            fi
        fi
    fi
done

echo "CPU temp, No Data" | tee "$CACHE_FILE"
