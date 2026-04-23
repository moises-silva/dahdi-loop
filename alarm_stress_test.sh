#!/bin/bash

ALARM_SIM_TYPE_PATH=/sys/module/dahdi_loop/parameters/alarm_sim_type
ALARM_TYPE=yellow
SLEEP_MS=1000
ALARMS_ACTIVE=0

declare -A ALARM_TYPES=(
    [los]=1
    [yellow]=4
    [red]=8
    [red-los]=9
)

usage() {
    echo "Usage: $0 [-a <alarm-type>] [-t <ms>] [-h]"
    echo ""
    echo "Options:"
    echo "  -a <alarm-type>  Alarm type to simulate (default: yellow)"
    echo "                   Named types: ${!ALARM_TYPES[*]}"
    echo "                   Raw numeric values are also accepted (e.g. -a 4)"
    echo "  -t <ms>          Time in milliseconds to sleep between alarm on/off (default: 1000)"
    echo "  -h               Show this help message"
    exit "${1:-1}"
}

while getopts "a:t:h" opt; do
    case $opt in
        a) ALARM_TYPE="$OPTARG" ;;
        t) SLEEP_MS="$OPTARG" ;;
        h) usage 0 ;;
        *) usage ;;
    esac
done

if ! [[ $SLEEP_MS =~ ^[0-9]+$ ]] || [ "$SLEEP_MS" -lt 1 ]; then
    echo "Invalid sleep time: $SLEEP_MS (must be a positive integer in milliseconds)"
    usage
fi
SLEEP_SEC=$(awk "BEGIN {printf \"%.3f\", $SLEEP_MS / 1000}")

if [[ -v ALARM_TYPES[$ALARM_TYPE] ]]; then
    ALARM_VALUE=${ALARM_TYPES[$ALARM_TYPE]}
elif [[ $ALARM_TYPE =~ ^[0-9]+$ ]]; then
    ALARM_VALUE=$ALARM_TYPE
else
    echo "Unknown alarm type: $ALARM_TYPE"
    usage
fi

echo "$ALARM_VALUE" > "$ALARM_SIM_TYPE_PATH" || {
    echo "Failed to write to $ALARM_SIM_TYPE_PATH — is dahdi_loop loaded?"
    exit 1
}

cleanup() {
    echo ""
    echo "Interrupted. Clearing alarms..."
    if [ "$ALARMS_ACTIVE" -eq 1 ]; then
        dahdi_maint -s 1 -i sim
        dahdi_maint -s 2 -i sim
    fi
    exit 0
}

trap cleanup INT

echo "Starting $ALARM_TYPE alarm stress test on spans 1 and 2 (alarm_sim_type=$ALARM_VALUE, sleep=${SLEEP_MS}ms). Press CTRL+C to stop."

while true; do
    ALARMS_ACTIVE=1
    dahdi_maint -s 1 -i sim
    dahdi_maint -s 2 -i sim
    sleep "$SLEEP_SEC"
    ALARMS_ACTIVE=0
    dahdi_maint -s 1 -i sim
    dahdi_maint -s 2 -i sim
    sleep "$SLEEP_SEC"
done
