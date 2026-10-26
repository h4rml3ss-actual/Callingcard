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
    // Temporary rollback: only use with cardputer-blade-details (unencrypted).
    private static final boolean REQUIRE_BOND = false;
    private static final UUID SERVICE = UUID.fromString("cabb1e00-ccad-4abc-a112-24fc00000001");
    private static final UUID STATUS = UUID.fromString("cabb1e00-ccad-4abc-a112-24fc00000002");
    private static final UUID DETAIL = UUID.fromString("cabb1e00-ccad-4abc-a112-24fc00000004");
    private static final UUID ALERT = UUID.fromString("cabb1e00-ccad-4abc-a112-24fc00000003");
    private static final UUID CCCD = UUID.fromString("00002902-0000-1000-8000-00805f9b34fb");
    private String lastAlert = "";
    private long lastAlertMs = -1;
    private int lastAlertSequence = -1;
    private int lastGeoSequence = -1;
    private String lastGeoSummary = "";
    private long geoHeaderAtMs = -1;
    private final String[][] evidenceChunks = new String[2][9];
    private final int[] evidenceTotals = new int[2];
    private void resetEvidence() {
        for(String[] parts:evidenceChunks) java.util.Arrays.fill(parts,null);
        evidenceTotals[0]=evidenceTotals[1]=0;
    }
    private void receiveEvidence(String[] parts) {
        if(parts.length!=6 || lastGeoSequence<0 || geoHeaderAtMs<0 ||
           SystemClock.elapsedRealtime()-geoHeaderAtMs>20000)return;
        try {
            int seq=Integer.parseInt(parts[1]);
            int index=Integer.parseInt(parts[3]);
            int total=Integer.parseInt(parts[4]);
            int field="I".equals(parts[2])?0:"R".equals(parts[2])?1:-1;
            String data=parts[5];
            if(seq!=lastGeoSequence||field<0||total<1||total>9||
               index<0||index>=total||data.length()<1||data.length()>8||
               !data.matches("[A-Za-z0-9:_\\\\.-]+"))return;
            if(evidenceTotals[field]!=0 && evidenceTotals[field]!=total)return;
            evidenceTotals[field]=total;
            evidenceChunks[field][index]=data;
            StringBuilder combined=new StringBuilder();
            for(int i=0;i<total;i++) {
                if(evidenceChunks[field][i]==null)return;
                combined.append(evidenceChunks[field][i]);
            }
            String evidence=combined.toString();
            if(field==0 && !evidence.matches("(?i)[0-9a-f]{2}(:[0-9a-f]{2}){5}"))
                return;
            Log.i(TAG,"evidence seq="+seq+" field="+parts[2]+" value="+evidence);
            // Retain the bounded completed field for current candidate HUD.
            lastAlert=lastGeoSummary+" / "+(field==0?"ID ":"RULE ")+evidence;
            graphical.alert=lastAlert;
            lastAlertMs=SystemClock.elapsedRealtime();
            renderHud();
            evidenceTotals[field]=0;
            java.util.Arrays.fill(evidenceChunks[field],null);
        } catch(NumberFormatException ignored) {}
    }
    private static final String[] GEO_TYPES =
        {"FLOCK","ALPR","AXON","CAMERA","TRACKER","META","SKIMMER"};
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
    private int lastPairingVariant = -1;
    private long bondStartedMs = -1;
    // Pairing variant IDs are stable in Android's BluetoothDevice implementation,
    // but several symbolic constants are hidden from public compile SDK stubs.
    // Diagnostic only: never use these values to submit or approve a PIN.
    private String pairingVariantName(int variant) {
        switch (variant) {
            case 0: return "PIN";
            case 1: return "PASSKEY_CONFIRMATION";
            case 2: return "PASSKEY_ENTRY";
            case 3: return "CONSENT";
            case 4: return "DISPLAY_PASSKEY";
            case 5: return "DISPLAY_PIN";
            case 6: return "OOB_CONSENT";
            default: return "UNKNOWN_" + variant;
        }
    }
    private final BroadcastReceiver pairingReceiver = new BroadcastReceiver() {
        @Override public void onReceive(Context context, Intent intent) {
            if (!REQUIRE_BOND || !BluetoothDevice.ACTION_PAIRING_REQUEST.equals(intent.getAction())) return;
            BluetoothDevice device = intent.getParcelableExtra(BluetoothDevice.EXTRA_DEVICE);
            if (device == null || pendingBond == null ||
                !device.getAddress().equals(pendingBond.getAddress())) return;
            lastPairingVariant = intent.getIntExtra(BluetoothDevice.EXTRA_PAIRING_VARIANT, -1);
            Log.i(TAG, "SMP pairing_request variant=" + lastPairingVariant +
                       " name=" + pairingVariantName(lastPairingVariant) +
                       " attempt=" + connectionAttempt +
                       " receiver_observed_only=1");
            // Do not consume, abort or answer the system pairing broadcast;
            // Vuzix's privileged handler owns PIN input.
        }
    };
    // All connection state lives on the main looper. A non-null GATT is the
    // single owner, including while connectGatt() is still connecting.
    private long connectionAttempt = 0;
    private boolean discoveryStarted = false;
    private final BroadcastReceiver bondReceiver = new BroadcastReceiver() {
        @Override public void onReceive(Context context, Intent intent) {
            if (!REQUIRE_BOND || !BluetoothDevice.ACTION_BOND_STATE_CHANGED.equals(intent.getAction())) return;
            BluetoothDevice device=intent.getParcelableExtra(BluetoothDevice.EXTRA_DEVICE);
            if(device==null || pendingBond==null || !device.getAddress().equals(pendingBond.getAddress())) return;
            int state=intent.getIntExtra(BluetoothDevice.EXTRA_BOND_STATE,BluetoothDevice.BOND_NONE);
            final int previous = intent.getIntExtra(
                BluetoothDevice.EXTRA_PREVIOUS_BOND_STATE, -1);
            String elapsed = bondStartedMs < 0 ? "unknown" :
                Long.toString(SystemClock.elapsedRealtime() - bondStartedMs);
            Log.i(TAG,"SMP bond state=" + state + " previous=" + previous +
                      " pairing_variant=" + pairingVariantName(lastPairingVariant) +
                      " elapsed_ms=" + elapsed + " attempt=" + connectionAttempt);
            if (state == BluetoothDevice.BOND_BONDING) {
                bondStartedMs = SystemClock.elapsedRealtime();
            }
            if(state==BluetoothDevice.BOND_BONDED) {
                bonding=false;
                bondFailed=false;
                if(linkConnected && gatt!=null) beginServiceDiscovery(gatt);
                // A bond broadcast must never create another GATT client.
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
            handler.post(() -> processScanResult(result));
        }
        private void processScanResult(ScanResult result) {
            if (!active || gatt != null || bonding || bondFailed || !scanning) return;
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
            if (REQUIRE_BOND && (bondFailed || bonding)) return;
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
            handler.post(() -> handleConnectionState(client, status, state));
        }
        @Override public void onMtuChanged(BluetoothGatt client, int mtu, int status) {
            Log.i(TAG, "negotiated MTU=" + mtu + " status=" + status);
            if (client == gatt && linkConnected && canConnect()) client.discoverServices();
        }
        @Override public void onServicesDiscovered(BluetoothGatt client, int status) {
            Log.i(TAG, "services discovered status=" + status);
            if (client != gatt || !linkConnected || status != BluetoothGatt.GATT_SUCCESS || !canConnect()) return;
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
            if (client != gatt || !linkConnected) return;
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
            if (client == gatt && linkConnected && status == BluetoothGatt.GATT_SUCCESS && STATUS.equals(ch.getUuid())) showPayload(ch.getValue());
        }
        @Override public void onCharacteristicChanged(BluetoothGatt client, BluetoothGattCharacteristic ch) {
            if (client != gatt || !linkConnected) return;
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
        if (REQUIRE_BOND) {
            registerReceiver(bondReceiver,new IntentFilter(BluetoothDevice.ACTION_BOND_STATE_CHANGED));
            registerReceiver(pairingReceiver,new IntentFilter(BluetoothDevice.ACTION_PAIRING_REQUEST));
            bondingRegistered=true;
        }
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
        if (!active || scanning || (REQUIRE_BOND && (bonding || bondFailed)) || gatt!=null || !canScan()) return;
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
    // Main-thread only: reject callbacks from a superseded GATT instance.
    private void handleConnectionState(BluetoothGatt client, int status, int state) {
        if (client != gatt || !active) {
            Log.w(TAG, "GATT stale_callback=1 state=" + state + " status=" + status);
            client.close();
            return;
        }
        Log.i(TAG, "GATT attempt=" + connectionAttempt + " state=" + state +
                   " status=" + status + " bond=" + client.getDevice().getBondState());
        if (state == BluetoothProfile.STATE_CONNECTED && status == BluetoothGatt.GATT_SUCCESS) {
            linkConnected = true;
            if (!canConnect()) return;
            final int bond = client.getDevice().getBondState();
            if (!REQUIRE_BOND) {
                Log.i(TAG, "GATT insecure_telemetry=1 no_bond_requested=1");
                beginServiceDiscovery(client);
            } else if (bond == BluetoothDevice.BOND_BONDED) {
                bonding = false;
                beginServiceDiscovery(client);
            } else if (bond == BluetoothDevice.BOND_BONDING) {
                bonding = true;
                Log.i(TAG, "SMP bonding_in_progress=1");
            } else if (!bonding && !bondFailed) {
                bonding = true;
                pendingBond = client.getDevice();
                Log.i(TAG, "SMP bond_request=1 attempt=" + connectionAttempt);
                show("PAIR ONCE: enter Cardputer PIN");
                if (!pendingBond.createBond()) {
                    bonding = false;
                    bondFailed = true;
                    show("PAIRING COULD NOT START - restart app");
                }
            }
        } else if (state == BluetoothProfile.STATE_DISCONNECTED || status != BluetoothGatt.GATT_SUCCESS) {
            Log.w(TAG, "GATT disconnected attempt=" + connectionAttempt + " status=" + status);
            linkConnected = false;
            discoveryStarted = false;
            lastTelemetryMs = -1;
            latestHud = "";
            lastAlert = "";
            lastAlertSequence = -1;
            show("CALLINGCARD LINK DISCONNECTED");
            gatt = null;
            client.close();
            pendingBond = null;
            bonding = false;
            if (!bondFailed) handler.postDelayed(() -> {
                if (active && gatt == null) startScan();
            }, 2000);
        }
    }
    private void beginServiceDiscovery(BluetoothGatt client) {
        if (!active || !canConnect() || client==null || client != gatt ||
            !linkConnected || discoveryStarted ||
            (REQUIRE_BOND && client.getDevice().getBondState() != BluetoothDevice.BOND_BONDED)) return;
        discoveryStarted = true;
        Log.i(TAG, "GATT discovery=1 require_bond=" + REQUIRE_BOND + " attempt=" + connectionAttempt);
        if (!client.requestMtu(100)) client.discoverServices();
    }
    private void connectPaired(BluetoothDevice device) {
        if (!active || !canConnect() || device == null || gatt != null ||
            (REQUIRE_BOND && (bonding || bondFailed))) {
            Log.i(TAG, "GATT duplicate_connect_ignored=1");
            return;
        }
        stopScan();
        linkConnected = false;
        discoveryStarted = false;
        show("CONNECTING BLE LINK...");
        pendingBond = device;
        final long attempt = ++connectionAttempt;
        Log.i(TAG, "GATT connect_attempt=" + attempt);
        // Keep the single returned client until its disconnect callback or
        // Activity teardown. No bond broadcast may replace it.
        gatt = device.connectGatt(this, false, callback, BluetoothDevice.TRANSPORT_LE);
        if (gatt == null) {
            Log.e(TAG, "GATT connectGatt_returned_null attempt=" + attempt);
            pendingBond = null;
            if (active) handler.postDelayed(this::startScan, 2000);
        }
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
            // Opt-in v2 stream: H=classified header, P=scaled GPS, T=GNSS UTC.
            // Sequence numbers prevent late geographic fragments attaching
            // to a different detection alert.
            if(parts.length==6 && parts[0].equals("X")) {
                receiveEvidence(parts);
                return;
            }
            if(parts.length==6 && parts[0].equals("H")) {
                try {
                    int seq=Integer.parseInt(parts[1]);
                    int kind=Integer.parseInt(parts[2]);
                    int rssi=Integer.parseInt(parts[4]);
                    if(seq<0||seq>=1000||kind<0||kind>=GEO_TYPES.length||
                       rssi< -127||rssi>20||
                       !Arrays.asList("L","M","H").contains(parts[3])||
                       !Arrays.asList("W","B").contains(parts[5]))return;
                    lastGeoSequence=seq;
                    geoHeaderAtMs=SystemClock.elapsedRealtime();
                    resetEvidence();
                    String confidence=parts[3].equals("L")?"LOW":parts[3].equals("M")?"MEDIUM":"HIGH";
                    String source=parts[5].equals("W")?"Wi-Fi":"BLE";
                    lastGeoSummary="POSSIBLE "+GEO_TYPES[kind]+" / "+confidence+
                        " / "+rssi+"dBm / "+source;
                    lastAlert=lastGeoSummary;
                    graphical.addDetection(GEO_TYPES[kind],confidence,rssi,source);
                    graphical.alert=lastAlert;
                    lastAlertMs=SystemClock.elapsedRealtime();
                    renderHud();
                } catch(NumberFormatException ignored) {}
                return;
            }
            if(parts.length==4 && parts[0].equals("P")) {
                try {
                    int seq=Integer.parseInt(parts[1]);
                    long latitude=Long.parseLong(parts[2]);
                    long longitude=Long.parseLong(parts[3]);
                    if(seq!=lastGeoSequence || latitude< -900000||latitude>900000||
                        longitude< -1800000||longitude>1800000)return;
                    lastAlert=lastGeoSummary+" / "+(latitude/10000.0)+","+(longitude/10000.0);
                    graphical.alert=lastAlert;
                    lastAlertMs=SystemClock.elapsedRealtime();
                    Log.i(TAG,"geo candidate "+lastAlert);
                    renderHud();
                } catch(NumberFormatException ignored) {}
                return;
            }
            if(parts.length==3 && parts[0].equals("T")) {
                try {
                    int seq=Integer.parseInt(parts[1]);
                    if(seq!=lastGeoSequence||!parts[2].matches("[0-9]{12}"))return;
                    Log.i(TAG,"geo candidate seq="+seq+" UTC YYMMDDHHMMSS="+parts[2]);
                } catch(NumberFormatException ignored) {}
                return;
            }
            if (parts.length == 5 && parts[0].equals("C")) {
                String type=parts[1], confidence=parts[2], source=parts[4];
                if (!Arrays.asList("FLOCK","ALPR","AXON","CAMERA","TRACKER","META","SKIMMER").contains(type)
                    || !Arrays.asList("L","M","H").contains(confidence)
                    || !Arrays.asList("W","B").contains(source)) return;
                try {
                    int rssi=Integer.parseInt(parts[3]);
                    if (rssi < -127 || rssi > 20) return;
                    String message="POSSIBLE " + type + " / " +
                        (confidence.equals("L")?"LOW":confidence.equals("M")?"MEDIUM":"HIGH") +
                        " / " + rssi + " dBm / " + (source.equals("W")?"Wi-Fi":"BLE");
                    lastAlert=message;
                    graphical.addDetection(type,
                        confidence.equals("L")?"LOW":confidence.equals("M")?"MEDIUM":"HIGH",
                        rssi,source.equals("W")?"Wi-Fi":"BLE");
                    graphical.alert=message;
                    lastAlertMs=SystemClock.elapsedRealtime();
                    Log.i(TAG,"candidate detection: "+message);
                    renderHud();
                } catch(NumberFormatException ignored) {}
                return;
            }
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
        if (!canConnect() || client != gatt || !linkConnected) return;
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
        if (bondingRegistered) {
            unregisterReceiver(bondReceiver);
            unregisterReceiver(pairingReceiver);
        }
        if (gatt != null && canConnect()) { gatt.disconnect(); gatt.close(); gatt = null; }
        super.onDestroy();
    }
}
