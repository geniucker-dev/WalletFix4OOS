DEBUG=@DEBUG@

MODDIR=${0%/*}

if [ ! -d "$MODDIR/eid" ]; then
    exit 0
fi

LIBPATH=/vendor/lib64:$MODDIR/eid/lib64
if [ -d "$MODDIR/eid/lib" ]; then
    LIBPATH=$LIBPATH:$MODDIR/eid/lib
fi

if ! pgrep -f "eid_hal_server" >/dev/null 2>&1;then
    LD_LIBRARY_PATH=$LIBPATH $MODDIR/eid/bin/hw/eid_hal_server &
fi
