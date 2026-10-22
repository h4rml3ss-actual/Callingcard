package org.callingcard.blade;

import android.Manifest;
import android.app.Activity;
import android.os.Bundle;
import android.os.Build;
import android.os.Handler;
import android.os.Looper;
import android.bluetooth.*;
import android.bluetooth.le.*;
import android.content.Context;
import android.content.pm.PackageManager;
import android.graphics.Color;
import android.view.Gravity;
import android.widget.TextView;
import android.widget.LinearLayout;
import java.util.Arrays;
import java.util.UUID;
import java.nio.charset.StandardCharsets;

public class MainActivity extends Activity {
    private static final UUID SERVICE = UUID.fromString("cabb1e00-ccad-4abc-a112-24fc00000001");
    private static final UUID STATUS = UUID.fromString("cabb1e00-ccad-4abc-a112-24fc00000002");
    private final Handler handler = new Handler(Looper.getMainLooper());
    private TextView display;
    private BluetoothLeScanner scanner;
    private BluetoothGatt gatt;
    private boolean scanning = false;
    private boolean active = false;
    private final ScanCallback discovery = new ScanCallback() {
        @Override public void onScanResult(int callbackType, ScanResult result) {
            BluetoothDevice device = result.getDevice();
            stopScan();
            if (!active || !canConnect()) return;
            show("Blade 2 HUD\nConnecting...");
            gatt = device.connectGatt(MainActivity.this, false, callback, BluetoothDevice.TRANSPORT_LE);
        }
        @Override public void onScanFailed(int errorCode) { show("BLE scan failed: " + errorCode); scanning = false; }
    };
    private final BluetoothGattCallback callback = new BluetoothGattCallback() {
        @Override public void onConnectionStateChange(BluetoothGatt client, int status, int state) {
            if (state == BluetoothProfile.STATE_CONNECTED) {
                show("Connected\nDiscovering telemetry...");
                if (canConnect()) client.discoverServices();
            } else if (state == BluetoothProfile.STATE_DISCONNECTED) {
                show("Disconnected");
                client.close();
                if (gatt == client) gatt = null;
                handler.postDelayed(() -> { if (active) startScan(); }, 2000);
            }
        }
        @Override public void onServicesDiscovered(BluetoothGatt client, int status) {
            if (status != BluetoothGatt.GATT_SUCCESS || !canConnect()) return;
            BluetoothGattService service = client.getService(SERVICE);
            BluetoothGattCharacteristic ch = service == null ? null : service.getCharacteristic(STATUS);
            if (ch == null) { show("Telemetry service missing"); return; }
            client.setCharacteristicNotification(ch, true);
            BluetoothGattDescriptor descriptor = ch.getDescriptor(UUID.fromString("00002902-0000-1000-8000-00805f9b34fb"));
            if (descriptor != null) {
                descriptor.setValue(BluetoothGattDescriptor.ENABLE_NOTIFICATION_VALUE);
                client.writeDescriptor(descriptor);
            }
            client.readCharacteristic(ch);
        }
        @Override public void onCharacteristicRead(BluetoothGatt client, BluetoothGattCharacteristic ch, int status) {
            if (status == BluetoothGatt.GATT_SUCCESS && STATUS.equals(ch.getUuid())) showPayload(ch.getValue());
        }
        @Override public void onCharacteristicChanged(BluetoothGatt client, BluetoothGattCharacteristic ch) {
            if (STATUS.equals(ch.getUuid())) showPayload(ch.getValue());
        }
    };
    @Override public void onCreate(Bundle state) {
        super.onCreate(state);
        display = new TextView(this);
        display.setTextColor(Color.rgb(114, 240, 186));
        display.setBackgroundColor(Color.BLACK);
        display.setTextSize(22);
        display.setGravity(Gravity.CENTER);
        display.setText("CALLINGCARD\nBlade 2 BLE proof of concept");
        setContentView(display);
        active = true;
        if (Build.VERSION.SDK_INT >= 31 &&
             (checkSelfPermission(Manifest.permission.BLUETOOTH_SCAN) != PackageManager.PERMISSION_GRANTED ||
              checkSelfPermission(Manifest.permission.BLUETOOTH_CONNECT) != PackageManager.PERMISSION_GRANTED)) {
            requestPermissions(new String[]{Manifest.permission.BLUETOOTH_SCAN, Manifest.permission.BLUETOOTH_CONNECT}, 7);
        } else if (Build.VERSION.SDK_INT < 31 && checkSelfPermission(Manifest.permission.ACCESS_FINE_LOCATION) != PackageManager.PERMISSION_GRANTED) {
            requestPermissions(new String[]{Manifest.permission.ACCESS_FINE_LOCATION}, 7);
        } else startScan();
    }
    @Override public void onRequestPermissionsResult(int code, String[] permissions, int[] result) {
        super.onRequestPermissionsResult(code, permissions, result);
        if (code == 7 && Arrays.stream(result).allMatch(v -> v == PackageManager.PERMISSION_GRANTED)) startScan();
        else show("Bluetooth permission required");
    }
    private boolean canConnect() {
        return Build.VERSION.SDK_INT < 31 || checkSelfPermission(Manifest.permission.BLUETOOTH_CONNECT) == PackageManager.PERMISSION_GRANTED;
    }
    private boolean canScan() {
        return Build.VERSION.SDK_INT < 31 || checkSelfPermission(Manifest.permission.BLUETOOTH_SCAN) == PackageManager.PERMISSION_GRANTED;
    }
    private void startScan() {
        if (!active || scanning || !canScan()) return;
        BluetoothManager manager = (BluetoothManager)getSystemService(Context.BLUETOOTH_SERVICE);
        BluetoothAdapter adapter = manager == null ? null : manager.getAdapter();
        if (adapter == null || !adapter.isEnabled()) { show("Enable Bluetooth on Blade 2"); return; }
        scanner = adapter.getBluetoothLeScanner();
        if (scanner == null) { show("BLE scanner unavailable"); return; }
        show("Searching for Callingcard...");
        scanning = true;
        scanner.startScan(Arrays.asList(new ScanFilter.Builder().setServiceUuid(new android.os.ParcelUuid(SERVICE)).build()),
                          new ScanSettings.Builder().setScanMode(ScanSettings.SCAN_MODE_LOW_LATENCY).build(), discovery);
        handler.postDelayed(() -> {
            if (scanning) { stopScan(); show("No Callingcard found\nRetrying..."); handler.postDelayed(this::startScan, 1500); }
        }, 12000);
    }
    private void stopScan() {
        if (scanning && scanner != null && canScan()) scanner.stopScan(discovery);
        scanning = false;
    }
    private void showPayload(byte[] payload) {
        if (payload == null) return;
        String[] p = new String(payload, StandardCharsets.UTF_8).split(",", -1);
        if (p.length != 6 || !p[0].equals("1")) { show("Unknown telemetry version"); return; }
        show("CALLINGCARD LINK\n\nWi-Fi: " + p[1] + "\nBLE: " + p[2] +
             "\nLoRa RX: " + p[3] + "\nGPS fix: " + (p[4].equals("1")?"YES":"NO") +
             "\nRecording: " + (p[5].equals("1")?"YES":"NO"));
    }
    private void show(String text) { runOnUiThread(() -> { if (active) display.setText(text); }); }
    @Override protected void onDestroy() {
        active = false;
        stopScan();
        handler.removeCallbacksAndMessages(null);
        if (gatt != null && canConnect()) { gatt.disconnect(); gatt.close(); gatt = null; }
        super.onDestroy();
    }
}
