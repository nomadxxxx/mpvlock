#!/usr/bin/env bash

# --- User Configuration ---
LOCATION_ID="a751f48de5f0b151b90f37cf211024153234be9ce3860154f811c40ddd3426ef"
WEATHERCOM_LOCALE="en-GB"  # Use en-AU or en-GB for celsius, en-US for fahrenheit

# Weather icons mapping
declare -A WEATHER_ICONS=(
    [sunnyDay]="󰖙"
    [clearNight]="󰖔"
    [cloudyFoggyDay]=""
    [cloudyFoggyNight]=""
    [rainyDay]=""
    [rainyNight]=""
    [snowyIcyDay]=""
    [snowyIcyNight]=""
    [severe]=""
    [default]=""
)

CACHE_DIR="$HOME/.config/mpvlock"
CACHE_FILE="$CACHE_DIR/weather"
mkdir -p "$CACHE_DIR"

URL="https://weather.com/$WEATHERCOM_LOCALE/weather/today/l/$LOCATION_ID"
HTML_CONTENT=$(curl -s --max-time 10 "$URL")

# Extract city name (just the city, not the state/country)
CITY=$(echo "$HTML_CONTENT" | grep -oP '<h1 class="CurrentConditions--location--[^"]+">\K[^<]+' | head -1 | cut -d',' -f1 | xargs)

# Extract temperature (number only)
TEMP_NUM=$(echo "$HTML_CONTENT" | grep -oP '<span data-testid="TemperatureValue"[^>]*>\K\d+' | head -1)

# Determine unit based on locale
if [[ "$WEATHERCOM_LOCALE" =~ en-US ]]; then
    TEMP_UNIT="f"
else
    TEMP_UNIT="c"
fi

# Extract status code from regionHeader class (fallback to phrase if needed)
STATUS_CODE=$(echo "$HTML_CONTENT" | grep -oP 'id="regionHeader"[^>]*class="[^"]*"' | grep -oE 'WeatherHeader--condition--\w+' | cut -d- -f4)
if [[ -z "$STATUS_CODE" ]]; then
    PHRASE=$(echo "$HTML_CONTENT" | grep -oP '<div data-testid="wxPhrase"[^>]*>\K[^<]+' | head -1)
    case "$PHRASE" in
        *Clear*) STATUS_CODE="sunnyDay" ;;
        *Cloud*) STATUS_CODE="cloudyFoggyDay" ;;
        *Rain*) STATUS_CODE="rainyDay" ;;
        *Snow*) STATUS_CODE="snowyIcyDay" ;;
        *Night*) STATUS_CODE="clearNight" ;;
        *Fog*) STATUS_CODE="cloudyFoggyNight" ;;
        *Severe*) STATUS_CODE="severe" ;;
        *) STATUS_CODE="default" ;;
    esac
fi

ICON="${WEATHER_ICONS[$STATUS_CODE]:-${WEATHER_ICONS[default]}}"

if [[ -n "$CITY" && -n "$TEMP_NUM" && -n "$ICON" ]]; then
    OUTPUT="$CITY, ${TEMP_NUM}${TEMP_UNIT} $ICON"
else
    OUTPUT="No Weather Data"
fi

echo "$OUTPUT"
echo "$OUTPUT" > "$CACHE_FILE"
