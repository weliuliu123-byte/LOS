#!/bin/sh
echo '=== LAB-02: Device Driver Test ==='

# Kiểm tra an toàn bằng /proc/modules thay vì lsmod
if grep -q lab2_driver /proc/modules; then
    echo '[1] lab2_driver already loaded in kernel.'
else
    echo '[1] Loading lab2_driver...'
    MOD_PATH="/lib/modules/6.1.90/lab2_driver.ko"
    [ ! -f "$MOD_PATH" ] && MOD_PATH="/lib/modules/lab2_driver.ko"
    insmod "$MOD_PATH" || { echo 'ERROR: insmod failed!'; exit 1; }
fi

echo '[2] Loaded modules:'
lsmod | grep lab2

echo '[3] Creating device node...'
mknod /dev/lab2 c 240 0 2>/dev/null || echo '  (node already exists)'
ls -la /dev/lab2

echo '[4] Writing to device...'
echo 'Hello from userspace, LAB-02!' > /dev/lab2

echo '[5] Reading from device...'
DATA=$(cat /dev/lab2)
echo "  Read back: [$DATA]"

echo '[6] Multiple write/read test...'
for i in 1 2 3; do
    echo "Message_$i" > /dev/lab2
    cat /dev/lab2
done

echo '[7] Kernel messages (dmesg):'
dmesg | grep lab2 | tail -10

echo '=== Test PASSED ==='
