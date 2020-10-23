#include <bluetooth/bluetooth.h>
#include <bluetooth/conn.h>
#include <bluetooth/gatt.h>

#include <drivers/uart.h>

#include <usb/usb_device.h>

// Parameters for BLE GAP advertising (explained in main())
static const bt_le_adv_param ADV_PARAMS[] = {
        BT_LE_ADV_PARAM_INIT(
                BT_LE_ADV_OPT_CONNECTABLE | BT_LE_ADV_OPT_USE_NAME,
                BT_GAP_ADV_FAST_INT_MIN_2, BT_GAP_ADV_FAST_INT_MAX_2,
                nullptr)
};

// https://github.com/adafruit/Adafruit_nRF52_Arduino/blob/master/libraries/Bluefruit52Lib/src/services/BLEUart.cpp
// UUID for the BLE UART interface on the Bluefruit app
// UUIDs are encoded in little-endian (LSBs first)
static bt_uuid_128 BLEUART_UUID_SERVICE = BT_UUID_INIT_128(
        0x9E, 0xCA, 0xDC, 0x24, 0x0E, 0xE5, 0xA9, 0xE0,
        0x93, 0xF3, 0xA3, 0xB5, 0x01, 0x00, 0x40, 0x6E
);
// The Central device can send messages to the nRF by
// writing to the RX characteristic
static bt_uuid_128 BLEUART_UUID_CHR_RXD = BT_UUID_INIT_128(
        0x9E, 0xCA, 0xDC, 0x24, 0x0E, 0xE5, 0xA9, 0xE0,
        0x93, 0xF3, 0xA3, 0xB5, 0x02, 0x00, 0x40, 0x6E
);
// The Central device is notified of messages being written
// from the nRF via the TX characteristic
static bt_uuid_128 BLEUART_UUID_CHR_TXD = BT_UUID_INIT_128(
        0x9E, 0xCA, 0xDC, 0x24, 0x0E, 0xE5, 0xA9, 0xE0,
        0x93, 0xF3, 0xA3, 0xB5, 0x03, 0x00, 0x40, 0x6E
);

K_FIFO_DEFINE(msg_recv);

// Data structure for a buffered message received from
// the BT Central device
struct buffered_msg {
    // FIFO housekeeping header
    void *fifo_header;

    // The message
    void *str;
    // The message length + \0
    int len;
};

// Write callback for the RXD characteristic
// Called whenever a Central device writes
static ssize_t write(struct bt_conn *conn,
                     const struct bt_gatt_attr *attr,
                     const void *buf, uint16_t len,
                     uint16_t offset, uint8_t flags) {
    buffered_msg *msg = (buffered_msg *) k_malloc(sizeof(*msg));
    if (!msg) {
        printk("Failed to allocate memory\n");
        return 0;
    }

    // Bluefruit doesn't add a '\0' terminator so allocate
    // 1 extra char just in case
    uint16_t strlen = len - offset;
    msg->len = strlen + 1;

    char *str = (char *) k_malloc(msg->len);
    if (!str) {
        printk("Failed to allocate memory\n");
        return 0;
    }

    memcpy(str, ((char *) buf) + offset, strlen);
    str[strlen] = '\0';

    msg->str = str;
    k_fifo_put(&msg_recv, msg);

    // Always write the requested number of bytes
    return strlen;
}

// Define the GATT service here
// This will automatically register 'serv'
BT_GATT_SERVICE_DEFINE(serv,
                       BT_GATT_PRIMARY_SERVICE(&BLEUART_UUID_SERVICE),
                       BT_GATT_CHARACTERISTIC((bt_uuid *) &BLEUART_UUID_CHR_RXD,
                                              BT_GATT_CHRC_WRITE | BT_GATT_CHRC_WRITE_WITHOUT_RESP,
                                              BT_GATT_PERM_WRITE,
                                              nullptr,
                                              write,
                                              (void *) ""),  // char[1] { '\0' }
                       BT_GATT_CHARACTERISTIC((bt_uuid *) &BLEUART_UUID_CHR_TXD,
                                              BT_GATT_CHRC_READ | BT_GATT_CHRC_NOTIFY,
                                              BT_GATT_PERM_READ,
                                              nullptr,
                                              nullptr,
                                              (void *) ""), // char[1] { '\0' }
                       BT_GATT_CCC(nullptr, BT_GATT_PERM_READ | BT_GATT_PERM_WRITE));


static bt_conn *g_conn = nullptr;

// Connection initialization callback - see CONN_CALLBACKS
void connected(bt_conn *conn, uint8_t err) {
    if (err) {
        printk("connected() called, but error was detected\n");
        return;
    }

    g_conn = bt_conn_ref(conn);
    printk("Connection established\n");
}

// Connection disconnect callback - see CONN_CALLBACKS
void disconnected(bt_conn *conn, uint8_t reason) {
    bt_conn_unref(conn);
    g_conn = nullptr;

    printk("Connection dropped\n");
}

// Parameter update request callback - see CONN_CALLBACKS
bool param_update_req(bt_conn *conn, bt_le_conn_param *param) {
    // Always accept update
    return true;
}

// Parameter post-update callback - see CONN_CALLBACKS
void param_updated(bt_conn *conn, uint16_t interval,
                   uint16_t latency, uint16_t timeout) {
    // No-op
}

// Parameters for BLE connection management
static bt_conn_cb CONN_CALLBACKS = {
        .connected = connected,
        .disconnected = disconnected,
        .le_param_req = param_update_req,
        .le_param_updated = param_updated
};

int main() {
    // Obtain the USB device and enable to allow console
    // access with the ACM UART driver
    const device *dev =
            device_get_binding(CONFIG_UART_CONSOLE_ON_DEV_NAME);
    if (usb_enable(nullptr)) {
        return 1;
    }

    // Wait for a serial monitor to connect
    uint32_t dtr = 0;
    while (!dtr) {
        if (uart_line_ctrl_get(dev, UART_LINE_CTRL_DTR, &dtr)) {
            return 1;
        }
    }

    // Allow 1 second for serial monitor initialize
    k_sleep(K_SECONDS(1));
    printk("Beginning initialization...\n");

    // Enable BT driver
    // cb = nullptr, no callback and enable synchronously
    if (bt_enable(nullptr)) {
        printk("Failed to enable BT (error code returned)\n");
        return 1;
    }

    printk("bt_enable() succeeded\n");

    // Begin GAP advertising
    // Set the advertising parameters to the value defined
    // by the BT_LE_ADV_CONN_NAME macro (C++ issue prevents
    // this from being compiled)
    // Set advertisement and scan response data to null
    // (we don't use this)
    if (bt_le_adv_start(ADV_PARAMS,
                        nullptr, 0,
                        nullptr, 0)) {
        printk("Failed to enable GAP advertising\n");
        return 1;
    }

    printk("bt_le_adv_start() succeeded\n");

    // Register connection callback
    bt_conn_cb_register(&CONN_CALLBACKS);
    printk("bt_conn_cb_register() succeeded\n");

    int counter = 0;
    while (true) {
        k_sleep(K_SECONDS(1));

        // Check to make sure the connection is active
        if (!g_conn) {
            continue;
        }

        /* buffered_msg *msg = (buffered_msg *) k_fifo_get(&msg_recv, K_NO_WAIT);
        if (!msg) {
            continue;
        }

        // Perform a write to the TXD characteristic
        int notify_ec = bt_gatt_notify(g_conn, &serv.attrs[4],
                                       msg->str, msg->len);
        k_free(msg->str);
        k_free(msg);

        if (notify_ec) {
            printk("bt_gatt_notify() failed");
            return 1;
        } */

        char data_line[256] = {0};
        snprintk(data_line, 256, "%d,%d\n", counter++, counter = -counter);
        if (bt_gatt_notify(g_conn, &serv.attrs[4],
                           data_line, strlen(data_line))) {
            printk("bt_gatt_notify() failed\n");
            return 1;
        }
    }

    return 0;
}
