DEBUG=@DEBUG@

MODDIR=${0%/*}
LOG_FILE="$MODDIR/service.log"
SERVER_LOG_FILE="$MODDIR/eid_hal_server.log"
exec >"$LOG_FILE" 2>&1

if [ ! -d "$MODDIR/system/vendor" ]; then
    echo "eID files not found! Exiting..."
    exit 0
fi


until [ "$(getprop sys.boot_completed)" -eq 1 ] ; do
  sleep 3
done


MAX_RETRIES=10
RETRY_DELAY=5
attempt=1

while [ $attempt -le $MAX_RETRIES ]; do
    if pgrep -f "eid_hal_server" >/dev/null 2>&1; then
        echo "eid_hal_server is already running"
        break
    fi
    echo "Starting eid_hal_server attempt $attempt/$MAX_RETRIES"
    /vendor/bin/hw/eid_hal_server >"$SERVER_LOG_FILE" 2>&1 &
    sleep $RETRY_DELAY
    attempt=$((attempt + 1))
done

if ! pgrep -f "eid_hal_server" >/dev/null 2>&1; then
    echo "Failed to start eid_hal_server after $MAX_RETRIES attempts"
    # run again and print exit code
    /vendor/bin/hw/eid_hal_server >"$SERVER_LOG_FILE" 2>&1
    echo "eid_hal_server exited with code $?"
else
    echo "eid_hal_server started successfully"
    echo "pid: $(pgrep -f "eid_hal_server")"
fi
