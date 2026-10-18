#!/bin/sh

echo '=== LAB-02: Test procfs & sysfs ==='

MOD_NAME="lab2_driver"
MOD_PATH="/lib/modules/6.1.90/lab2_driver.ko"
DEV_NODE="/dev/lab2"
PROC_FILE="/proc/lab2_info"
SYSFS="/sys/class/lab2_class/lab2"

# ============================================================
# 1. Check and load kernel module
# ============================================================

echo '[1] Checking kernel module...'

if grep -q "^${MOD_NAME} " /proc/modules; then
    echo "    $MOD_NAME already loaded."
else
    echo "    Loading $MOD_NAME..."

    if [ ! -f "$MOD_PATH" ]; then
        echo "ERROR: Module not found: $MOD_PATH"
        exit 1
    fi

    insmod "$MOD_PATH" || {
        echo "ERROR: Failed to load $MOD_NAME"
        exit 1
    }
fi

# ============================================================
# 2. Create device node if necessary
# ============================================================

echo '[2] Checking device node...'

if [ ! -c "$DEV_NODE" ]; then
    mknod "$DEV_NODE" c 240 0 || {
        echo "ERROR: Failed to create $DEV_NODE"
        exit 1
    }
fi

ls -l "$DEV_NODE"

# ============================================================
# 3. Test procfs before writing
# ============================================================

echo '[3] Reading procfs before write...'

if [ ! -r "$PROC_FILE" ]; then
    echo "ERROR: Cannot read $PROC_FILE"
    exit 1
fi

cat "$PROC_FILE" || exit 1

# ============================================================
# 4. Write test data to character device
# ============================================================

echo '[4] Writing test data to device...'

TEST_DATA="Embedded Linux Lab2 Test Data"

printf '%s\n' "$TEST_DATA" > "$DEV_NODE" || {
    echo "ERROR: Failed to write to $DEV_NODE"
    exit 1
}

# ============================================================
# 5. Test procfs after writing
# ============================================================

echo '[5] Reading procfs after write...'

cat "$PROC_FILE" || exit 1

# ============================================================
# 6. Test sysfs attributes
# ============================================================

echo '[6] Checking sysfs attributes...'

for attr in buffer_len open_count last_data; do
    if [ ! -r "$SYSFS/$attr" ]; then
        echo "ERROR: Missing sysfs attribute: $attr"
        exit 1
    fi
done

echo "buffer_len : $(cat "$SYSFS/buffer_len")"
echo "open_count : $(cat "$SYSFS/open_count")"
echo "last_data  : $(cat "$SYSFS/last_data")"

# ============================================================
# 7. Verify buffer length
# ============================================================

echo '[7] Verifying buffer length...'

EXPECTED_LEN=$(printf '%s\n' "$TEST_DATA" | wc -c | tr -d ' ')
ACTUAL_LEN=$(cat "$SYSFS/buffer_len") || exit 1

if [ "$ACTUAL_LEN" -ne "$EXPECTED_LEN" ]; then
    echo "FAIL: buffer_len=$ACTUAL_LEN, expected=$EXPECTED_LEN"
    exit 1
fi

echo "PASS: buffer_len=$ACTUAL_LEN"

# ============================================================
# 8. Verify stored data
# ============================================================

echo '[8] Verifying last_data...'

ACTUAL_DATA=$(cat "$SYSFS/last_data") || exit 1

if [ "$ACTUAL_DATA" != "$TEST_DATA" ]; then
    echo "FAIL: last_data mismatch"
    echo "Expected: [$TEST_DATA]"
    echo "Actual  : [$ACTUAL_DATA]"
    exit 1
fi

echo "PASS: last_data matches written data"

# ============================================================
# 9. Final result
# ============================================================

echo '========================================'
echo '=== LAB-02 procfs/sysfs TEST PASSED ==='
echo '========================================'

exit 0