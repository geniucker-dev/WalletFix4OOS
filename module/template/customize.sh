# shellcheck disable=SC2034

REPLACE="
/system/product/priv-app/KeKeUserCenter
"

SKIPUNZIP=1

DEBUG=@DEBUG@
SONAME=@SONAME@
SUPPORTED_ABIS="@SUPPORTED_ABIS@"

if [ "$BOOTMODE" ] && [ "$KSU" ]; then
  ui_print "- Installing from KernelSU app"
  ui_print "- KernelSU version: $KSU_KERNEL_VER_CODE (kernel) + $KSU_VER_CODE (ksud)"
  if [ "$(which magisk)" ]; then
    ui_print "*********************************************************"
    ui_print "! Multiple root implementation is NOT supported!"
    ui_print "! Please uninstall Magisk before installing Zygisk Next"
    abort    "*********************************************************"
  fi
elif [ "$BOOTMODE" ] && [ "$MAGISK_VER_CODE" ]; then
  ui_print "- Installing from Magisk app"
else
  ui_print "*********************************************************"
  ui_print "! Install from recovery is not supported"
  ui_print "! Please install from KernelSU or Magisk app"
  abort    "*********************************************************"
fi

VERSION=$(grep_prop version "${TMPDIR}/module.prop")
ui_print "- Installing $SONAME $VERSION"

# check architecture
support=false
for abi in $SUPPORTED_ABIS
do
  if [ "$ARCH" == "$abi" ]; then
    support=true
  fi
done
if [ "$support" == "false" ]; then
  abort "! Unsupported platform: $ARCH"
else
  ui_print "- Device platform: $ARCH"
fi

ui_print "- Extracting verify.sh"
unzip -o "$ZIPFILE" 'verify.sh' -d "$TMPDIR" >&2
if [ ! -f "$TMPDIR/verify.sh" ]; then
  ui_print "*********************************************************"
  ui_print "! Unable to extract verify.sh!"
  ui_print "! This zip may be corrupted, please try downloading again"
  abort    "*********************************************************"
fi
. "$TMPDIR/verify.sh"
extract "$ZIPFILE" 'customize.sh'  "$TMPDIR/.vunzip"
extract "$ZIPFILE" 'verify.sh'     "$TMPDIR/.vunzip"
extract "$ZIPFILE" 'sepolicy.rule' "$TMPDIR"

ui_print "- Extracting module files"
extract "$ZIPFILE" 'module.prop'     "$MODPATH"
extract "$ZIPFILE" 'post-fs-data.sh' "$MODPATH"
mv "$TMPDIR/sepolicy.rule" "$MODPATH"

ui_print "- Extracting apps"
# extract files in product
for file in $(unzip -l "$ZIPFILE" | awk '{print $4}' | grep "^system/product/" | grep -v "/$" | grep -v ".sha256"); do
  extract "$ZIPFILE" "$file" "$MODPATH"
done

ui_print "- Extracting device_map"
extract "$ZIPFILE" "device_map" "$TMPDIR"
MODEL=$(getprop ro.product.model)
ui_print "- Device model: $MODEL"
ORIGINAL_MODEL="$MODEL"
MODEL=$(grep -m1 "^$MODEL=" "$TMPDIR/device_map" | cut -d'=' -f2)

# if MODEL mapping exists, use mapped value
if [ -n "$MODEL" ]; then
  MODEL="$MODEL"
  ui_print "- Mapped device model: $MODEL"
  ui_print "- eID fix will be applied!"
  ui_print "- Extracting eID files for $MODEL"
  # extract files in product
  for file in $(unzip -l "$ZIPFILE" | awk '{print $4}' | grep "^odms/$MODEL/" | grep -v "/$" | grep -v ".sha256"); do
    extract "$ZIPFILE" "$file" "$MODPATH"
  done
  # mount as vendor partition for better compatibility
  mv "$MODPATH/odms/$MODEL/odm" "$MODPATH/system/vendor"
  rm -rf "$MODPATH/odms"
  # extract service.sh only eID fix supported
  extract "$ZIPFILE" 'service.sh'      "$MODPATH"
  # replace eid_hal_server placeholder in service.sh with the real file name
  REAL_EID_HAL_SERVER_FILENAME=$(basename "$(find "$MODPATH/system/vendor/bin" -name "*eid*" -type f | head -n 1)")
  sed -i "s|eid_hal_server|$REAL_EID_HAL_SERVER_FILENAME|g" "$MODPATH/service.sh"
  # replace odm in $MODPATH/system/vendor/etc files with vendor
  sed -i "s|/odm/|/vendor/|g" $(find "$MODPATH/system/vendor/etc" -type f)
  ui_print "- eID files for $MODEL extracted!"
else
  ui_print "- eID fix not supported on this model yet!"
  ui_print "- eID fix won't be applied!"
fi

HAS32BIT=false && [ $(getprop ro.product.cpu.abilist32) ] && HAS32BIT=true

mkdir "$MODPATH/zygisk"

if [ "$ARCH" = "x86" ] || [ "$ARCH" = "x64" ]; then
  if [ "$HAS32BIT" = true ]; then
    ui_print "- Extracting x86 libraries"
    extract "$ZIPFILE" "lib/x86/lib$SONAME.so" "$MODPATH/zygisk/" true
    mv "$MODPATH/zygisk/lib$SONAME.so" "$MODPATH/zygisk/x86.so"
  fi

  ui_print "- Extracting x64 libraries"
  extract "$ZIPFILE" "lib/x86_64/lib$SONAME.so" "$MODPATH/zygisk" true
  mv "$MODPATH/zygisk/lib$SONAME.so" "$MODPATH/zygisk/x86_64.so"
else
  if [ "$HAS32BIT" = true ]; then
    extract "$ZIPFILE" "lib/armeabi-v7a/lib$SONAME.so" "$MODPATH/zygisk" true
    mv "$MODPATH/zygisk/lib$SONAME.so" "$MODPATH/zygisk/armeabi-v7a.so"
  fi

  ui_print "- Extracting arm64 libraries"
  extract "$ZIPFILE" "lib/arm64-v8a/lib$SONAME.so" "$MODPATH/zygisk" true
  mv "$MODPATH/zygisk/lib$SONAME.so" "$MODPATH/zygisk/arm64-v8a.so"
fi

ui_print "- Setting permissions"
set_perm_recursive "$MODPATH/system/product" 0 0 0755 0644
if [ -d "$MODPATH/system/vendor" ]; then
  set_perm_recursive "$MODPATH/system/vendor" 0 0 0755 0644 u:object_r:vendor_file:s0
  set_perm_recursive "$MODPATH/system/vendor/bin" 0 0 0751 0755 u:object_r:vendor_file:s0
  set_perm "$MODPATH/system/vendor/bin/hw/$REAL_EID_HAL_SERVER_FILENAME" 0 2000 0755 0755 u:object_r:hal_eid_oplus_exec:s0
  set_perm_recursive "$MODPATH/system/vendor/etc" 0 0 0755 0644 u:object_r:vendor_configs_file:s0
fi
ls -laRZ "$MODPATH/system/vendor"