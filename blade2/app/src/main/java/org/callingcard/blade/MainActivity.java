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
import android.content.BroadcastReceiver;
import android.content.Intent;
import android.content.IntentFilter;
import android.content.pm.PackageManager;
import android.graphics.Color;
import android.view.Gravity;
import android.widget.TextView;
import android.widget.LinearLayout;
import android.graphics.Typeface;
import android.os.SystemClock;
import android.view.KeyEvent;
import android.util.Log;
import java.util.Arrays;
import java.util.UUID;
import java.nio.charset.StandardCharsets;

public class MainActivity extends Activity {
    private static final String TAG = "CallingcardBlade";
    private static final UUID SERVICE = UUID.fromString("cabb1e00-ccad-4abc-a112-24fc00000001");
    private static final UUID STATUS = UUID.fromString("cabb1e00-ccad-4abc-a112-24fc00000002");
    private static final UUID DETAIL = UUID.fromString("cabb1e00-ccad-4abc-a112-24fc00000004");
    private static final UUID ALERT = UUID.fromString("cabb1e00-ccad-4abc-a112-24fc00000003");
    private static final UUID CCCD = UUID.fromString("00002902-0000-1000-8000-00805f9b34fb");
    private String lastAlert = "";
    private long lastAlertMs = -1;
    private int lastAlertSequence = -1;
    private final Handler handler = new Handler(Looper.getMainLooper());
    private TextView display;
    private ScannerHudView graphical;
    private BluetoothLeScanner scanner;
    private BluetoothGatt gatt;
    private BluetoothDevice pendingBond;
    private volatile boolean linkConnected = false;
    private boolean bonding = false;
    private boolean bondFailed = false; // No repeated PIN prompts after a failed enrollment.
    private boolean bondingRegistered = false;
    private final BroadcastReceiver bondReceiver = new BroadcastReceiver() {
        @Override public void onReceive(Context context, Intent intent) {
            if (!BluetoothDevice.ACTION_BOND_STATE_CHANGED.equals(intent.getAction())) return;
            BluetoothDevice device=intent.getParcelableExtra(BluetoothDevice.EXTRA_DEVICE);
            if(device==null || pendingBond==null || !device.getAddress().equals(pendingBond.getAddress())) return;
            int state=intent.getIntExtra(BluetoothDevice.EXTRA_BOND_STATE,BluetoothDevice.BOND_NONE);
            Log.i(TAG,"bond state="+state);
            if(state==BluetoothDevice.BOND_BONDED) {
                bonding=false;
                bondFailed=false;
                if(linkConnected && gatt!=null) beginServiceDiscovery(gatt);
                else if (gatt==null) connectPaired(device);
            } else if(state==BluetoothDevice.BOND_NONE) {
                bonding=false;pendingBond=null;
                bondFailed=true;
                show("PAIRING FAILED - restart app to retry");
            }
        }
    };
    private boolean scanning = false;
    private boolean active = false;
    private long lastTelemetryMs = -1;
    private int previousWifi = -1, previousBle = -1, previousLora = -1;
    private String latestHud = "";
    private final Runnable freshnessTicker = new Runnable() {
        @Override public void run() {
            if (!active) return;
            if (lastTelemetryMs >= 0) renderHud();
            handler.postDelayed(this, 1000);
        }
    };
    private final ScanCallback discovery = new ScanCallback() {
        @Override public void onScanResult(int callbackType, ScanResult result) {
            BluetoothDevice device = result.getDevice();
            ScanRecord scanRecord = result.getScanRecord();
            String name = scanRecord == null ? null : scanRecord.getDeviceName();
            boolean uuidMatch = scanRecord != null && scanRecord.getServiceUuids() != null &&
                                scanRecord.getServiceUuids().contains(new android.os.ParcelUuid(SERVICE));
            boolean nameMatch = "Callingcard".equals(name);
            Log.i(TAG, "scan name=" + name + " uuidMatch=" + uuidMatch + " rssi=" + result.getRssi());
            if (!uuidMatch && !nameMatch) return;
            stopScan();
            if (!active || !canConnect()) return;
            show("Blade 2 HUD\nConnecting...");
            // Never bypass bond enrollment: connecting unbonded triggered repeated
            // system PIN prompts and raced the separate bond receiver.
            if (bondFailed || bonding) return;
            pendingBond=device;
            // Open the BLE physical link once, then initiate bonding from the
            // connected state. On some Android BLE stacks createBond() before
            // GATT connection never reaches a usable encrypted session.
            connectPaired(device);
        }
        @Override public void onScanFailed(int errorCode) { Log.e(TAG, "scan failed " + errorCode); show("BLE scan failed: " + errorCode); scanning = false; }
    };
    private final BluetoothGattCallback callback = new BluetoothGattCallback() {
        @Override public void onConnectionStateChange(BluetoothGatt client, int status, int state) {
            Log.i(TAG, "connection state=" + state + " status=" + status + " bond=" + client.getDevice().getBondState());
            if (state == BluetoothProfile.STATE_CONNECTED) {
                show("Connected\nDiscovering telemetry...");
                if (canConnect()) {
                    // Request a larger ATT MTU before service discovery so names fit.
                    if (!client.requestMtu(100)) client.discoverServices();
                }
            } else if (state == BluetoothProfile.STATE_DISCONNECTED) {
                Log.w(TAG,"BLE disconnected, gattStatus="+status);
                handler.post(() -> { lastTelemetryMs = -1; latestHud = ""; lastAlert = ""; lastAlertSequence = -1; show("CALLINGCARD\\nLINK DISCONNECTED"); });
                client.close();
                if (gatt == client) gatt = null;
                handler.postDelayed(() -> { if (active) startScan(); }, 2000);
            }
        }
        @Override public void onMtuChanged(BluetoothGatt client, int mtu, int status) {
            Log.i(TAG, "negotiated MTU=" + mtu + " status=" + status);
            if (canConnect()) client.discoverServices();
        }
        @Override public void onServicesDiscovered(BluetoothGatt client, int status) {
            Log.i(TAG, "services discovered status=" + status);
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
            // CCCD writes are asynchronous; alert subscription follows status descriptor callback.
        }
        @Override public void onDescriptorWrite(BluetoothGatt client, BluetoothGattDescriptor descriptor, int status) {
            if (status != BluetoothGatt.GATT_SUCCESS) { Log.w(TAG, "notification descriptor write failed " + status); return; }
            if (STATUS.equals(descriptor.getCharacteristic().getUuid()) && canConnect()) {
                BluetoothGattService service = client.getService(SERVICE);
                BluetoothGattCharacteristic alert = service == null ? null : service.getCharacteristic(ALERT);
                if (alert == null) { Log.i(TAG, "legacy firmware: alert characteristic absent"); subscribeDetails(client); return; }
                client.setCharacteristicNotification(alert, true);
                BluetoothGattDescriptor cccd = alert.getDescriptor(CCCD);
                if (cccd != null) {
                    cccd.setValue(BluetoothGattDescriptor.ENABLE_NOTIFICATION_VALUE);
                    client.writeDescriptor(cccd);
                }
            } else if (ALERT.equals(descriptor.getCharacteristic().getUuid())) {
                subscribeDetails(client);
            }
        }
        @Override public void onCharacteristicRead(BluetoothGatt client, BluetoothGattCharacteristic ch, int status) {
            if (status == BluetoothGatt.GATT_SUCCESS && STATUS.equals(ch.getUuid())) showPayload(ch.getValue());
        }
        @Override public void onCharacteristicChanged(BluetoothGatt client, BluetoothGattCharacteristic ch) {
            if (STATUS.equals(ch.getUuid())) showPayload(ch.getValue());
            else if (ALERT.equals(ch.getUuid())) showAlert(ch.getValue());
            else if (DETAIL.equals(ch.getUuid())) showDetail(ch.getValue());
        }
    };
    @Override public void onCreate(Bundle state) {
        super.onCreate(state);
        display = new TextView(this);
        display.setTextColor(Color.rgb(114, 240, 186));
        display.setBackgroundColor(Color.BLACK);
        display.setTextSize(20);
        display.setTypeface(Typeface.MONOSPACE);
        display.setPadding(20, 18, 20, 18);
        display.setGravity(Gravity.CENTER);
        display.setText("CALLINGCARD\nBlade 2 BLE proof of concept");
        graphical = new ScannerHudView(this);
        setContentView(graphical);
        active = true;
        registerReceiver(bondReceiver,new IntentFilter(BluetoothDevice.ACTION_BOND_STATE_CHANGED));
        bondingRegistered=true;
        handler.post(freshnessTicker);
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
        if (code == 7 && result.length > 0 && Arrays.stream(result).allMatch(v -> v == PackageManager.PERMISSION_GRANTED)) startScan();
        else show("Bluetooth permission required");
    }
    private boolean canConnect() {
        return Build.VERSION.SDK_INT < 31 || checkSelfPermission(Manifest.permission.BLUETOOTH_CONNECT) == PackageManager.PERMISSION_GRANTED;
    }
    private boolean canScan() {
        return Build.VERSION.SDK_INT < 31 || checkSelfPermission(Manifest.permission.BLUETOOTH_SCAN) == PackageManager.PERMISSION_GRANTED;
    }
    private void startScan() {
        if (!active || scanning || bonding || bondFailed || gatt!=null || !canScan()) return;
        BluetoothManager manager = (BluetoothManager)getSystemService(Context.BLUETOOTH_SERVICE);
        BluetoothAdapter adapter = manager == null ? null : manager.getAdapter();
        if (adapter == null || !adapter.isEnabled()) { show("Enable Bluetooth on Blade 2"); return; }
        scanner = adapter.getBluetoothLeScanner();
        if (scanner == null) { show("BLE scanner unavailable"); return; }
        show("Searching for Callingcard...");
        scanning = true;
        Log.i(TAG, "starting unfiltered diagnostic scan");
        scanner.startScan(null,
                          new ScanSettings.Builder().setScanMode(ScanSettings.SCAN_MODE_LOW_LATENCY).build(), discovery);
        handler.postDelayed(() -> {
            if (scanning) { stopScan(); show("No Callingcard found\nRetrying..."); handler.postDelayed(this::startScan, 1500); }
        }, 12000);
    }
    private void beginServiceDiscovery(BluetoothGatt client) {
        if (!active || !canConnect() || client==null || !linkConnected) return;
        // ATT characteristic permissions remain MITM-protected on Cardputer.
        if (!client.requestMtu(100)) client.discoverServices();
    }
    private void connectPaired(BluetoothDevice device) {
        if(!active || !canConnect())return;
        if(gatt!=null) {gatt.close();gatt=null;}
        linkConnected=false;
        show("CONNECTING BLE LINK...");
        pendingBond=device;
        gatt=device.connectGatt(this,false,callback,BluetoothDevice.TRANSPORT_LE);
    }
    private void stopScan() {
        if (scanning && scanner != null && canScan()) scanner.stopScan(discovery);
        scanning = false;
    }
    private void showPayload(byte[] payload) {
        if (payload == null) return;
        final String wire = new String(payload, StandardCharsets.UTF_8);
        handler.post(() -> {
            String[] p = wire.split(",", -1);
            if (p.length != 6 || !p[0].equals("1")) { show("Unsupported telemetry"); return; }
            try {
                int wifi = Integer.parseInt(p[1]), ble = Integer.parseInt(p[2]), lora = Integer.parseInt(p[3]);
                if (wifi < 0 || wifi > 999 || ble < 0 || ble > 999 || lora < 0 || lora > 999 ||
                    (!p[4].equals("0") && !p[4].equals("1")) ||
                    (!p[5].equals("0") && !p[5].equals("1"))) throw new NumberFormatException();
                String w = previousWifi >= 0 && wifi != previousWifi ? "*" : " ";
                String b = previousBle >= 0 && ble != previousBle ? "*" : " ";
                String l = previousLora >= 0 && lora != previousLora ? "*" : " ";
                previousWifi = wifi; previousBle = ble; previousLora = lora;
                graphical.wifi=wifi; graphical.ble=ble; graphical.lora=lora;
                graphical.gps=p[4].equals("1"); graphical.recording=p[5].equals("1");
                latestHud = "Wi-Fi " + String.format(java.util.Locale.US, "%3d", wifi) + " " + w +
                    "\nBLE   " + String.format(java.util.Locale.US, "%3d", ble) + " " + b +
                    "\nLoRa  " + String.format(java.util.Locale.US, "%3d", lora) + " " + l +
                    "\nGPS   " + (p[4].equals("1") ? "FIX" : "NO FIX") +
                    "\nSD    " + (p[5].equals("1") ? "REC" : "IDLE");
                lastTelemetryMs = SystemClock.elapsedRealtime();
                renderHud();
            } catch (NumberFormatException ex) {
                Log.w(TAG, "invalid telemetry status");
            }
        });
    }
    private void showAlert(byte[] raw) {
        if (raw == null) return;
        final String wire = new String(raw, StandardCharsets.UTF_8);
        handler.post(() -> {
            String[] parts = wire.split(",", -1);
            if (parts.length != 3 || !parts[0].equals("E")) return;
            try {
                int sequence = Integer.parseInt(parts[1]);
                if (sequence < 0 || sequence > 999 || sequence == lastAlertSequence) return;
                String source;
                switch (parts[2]) {
                    case "W": source = "Wi-Fi observation"; break;
                    case "B": source = "BLE observation"; break;
                    case "L": source = "LoRa reception"; break;
                    default: return;
                }
                lastAlertSequence = sequence;
                lastAlert = source;
                graphical.alert = source;
                lastAlertMs = SystemClock.elapsedRealtime();
                Log.i(TAG, "passive event alert: " + source);
                renderHud();
            } catch (NumberFormatException ignored) { }
        });
    }
    private void renderHud() {
        if (!active || lastTelemetryMs < 0) return;
        long age = SystemClock.elapsedRealtime() - lastTelemetryMs;
        String health = age < 3000 ? "LIVE" : age < 10000 ? "DELAYED" : "STALE";
        graphical.health = health;
        graphical.invalidate();
        display.setText("CALLINGCARD  " + health + "\n" +
            "--------------------\n" + latestHud + "\n\n" +
            (age < 3000 ? "LINK ACTIVE" : "LAST UPDATE " + (age/1000) + "s AGO") +
            (lastAlertMs >= 0 && SystemClock.elapsedRealtime() - lastAlertMs < 8000
                ? "\nALERT: " + lastAlert : ""));
        display.setTextColor(age < 3000 ? Color.rgb(114,240,186) :
                             age < 10000 ? Color.YELLOW : Color.RED);
    }
    private void subscribeDetails(BluetoothGatt client) {
        if (!canConnect()) return;
        BluetoothGattService service=client.getService(SERVICE);
        BluetoothGattCharacteristic ch=service==null?null:service.getCharacteristic(DETAIL);
        if(ch==null) { Log.i(TAG,"legacy firmware: detail characteristic absent"); return; }
        client.setCharacteristicNotification(ch,true);
        BluetoothGattDescriptor descriptor=ch.getDescriptor(CCCD);
        if(descriptor!=null) {
            descriptor.setValue(BluetoothGattDescriptor.ENABLE_NOTIFICATION_VALUE);
            client.writeDescriptor(descriptor);
        }
    }
    private void showDetail(byte[] raw) {
        if(raw==null) return;
        final String wire = new String(raw,StandardCharsets.UTF_8);
        handler.post(() -> {
            String[] v=wire.split(",",5);
            if(v.length!=5 || !v[0].equals("D") || v[1].length()!=1) return;
            char source=v[1].charAt(0);
            if(source!='W' && source!='B' && source!='L') return;
            try {
                int channel=Integer.parseInt(v[2]),rssi=Integer.parseInt(v[3]);
                if(channel<0 || channel>196 || rssi>20 || rssi< -127) return;
                graphical.add(source,channel,rssi,v[4].replace("\\n"," ").replace("\\r"," "));
            } catch(NumberFormatException ignored) {}
        });
    }
    @Override public boolean onKeyDown(int keyCode, KeyEvent event) {
        if(keyCode==KeyEvent.KEYCODE_DPAD_RIGHT) { graphical.next(1); return true; }
        if(keyCode==KeyEvent.KEYCODE_DPAD_LEFT) { graphical.next(-1); return true; }
        if(keyCode==KeyEvent.KEYCODE_MENU) return graphical.toggleOld();
        if(keyCode==KeyEvent.KEYCODE_DPAD_DOWN) return graphical.moveSelection(1);
        if(keyCode==KeyEvent.KEYCODE_DPAD_UP) return graphical.moveSelection(-1);
        if(keyCode==KeyEvent.KEYCODE_DPAD_CENTER || keyCode==KeyEvent.KEYCODE_ENTER)
            return graphical.openSelected();
        if(keyCode==KeyEvent.KEYCODE_BACK && graphical.back()) return true;
        return super.onKeyDown(keyCode,event);
    }
    @Override public void onBackPressed() {
        if(graphical.back()) return;
        super.onBackPressed();
    }
    private void show(String text) { runOnUiThread(() -> {
        if (active) { display.setText(text); if(graphical!=null) { graphical.health=text; graphical.invalidate(); } }
    }); }
    @Override protected void onDestroy() {
        active = false;
        stopScan();
        handler.removeCallbacksAndMessages(null);
        if (bondingRegistered) unregisterReceiver(bondReceiver);
        if (gatt != null && canConnect()) { gatt.disconnect(); gatt.close(); gatt = null; }
        super.onDestroy();
    }
}
