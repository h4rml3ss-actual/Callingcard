package org.callingcard.blade;

import android.Manifest;
import android.app.Activity;
import android.bluetooth.*;
import android.bluetooth.le.*;
import android.content.Context;
import android.content.pm.PackageManager;
import android.os.Build;
import android.os.Bundle;
import android.os.Handler;
import android.os.Looper;
import android.util.Log;
import android.widget.TextView;
import java.io.FileInputStream;
import java.security.GeneralSecurityException;
import java.security.SecureRandom;
import java.util.Arrays;
import java.util.UUID;

/**
 * Opt-in CSTP laboratory client. Explicitly launch this Activity via ADB.
 * The existing MainActivity and legacy Blade telemetry remain unchanged.
 * No BLE bond or native MITM downgrade: GATT carries only proofs/ciphertext.
 * Developer raw PSK in app-private filesDir is NOT production key protection.
 */
public final class CstpActivity extends Activity {
    private static final String TAG = "CallingcardCSTP";
    private static final UUID SERVICE = UUID.fromString("cabb1e00-ccad-4abc-a112-24fc00000100");
    private static final UUID CONTROL = UUID.fromString("cabb1e00-ccad-4abc-a112-24fc00000101");
    private static final UUID DATA = UUID.fromString("cabb1e00-ccad-4abc-a112-24fc00000102");
    private static final UUID CCCD = UUID.fromString("00002902-0000-1000-8000-00805f9b34fb");
    private final Handler handler = new Handler(Looper.getMainLooper());
    private final SecureRandom rng = new SecureRandom();
    private TextView display;
    private BluetoothLeScanner scanner;
    private BluetoothGatt gatt;
    private BluetoothGattCharacteristic control, data;
    private byte[] psk, transcript, sid, proofExpected;
    private CstpV1.Secrets receiveKeys;
    private long nextSequence;
    private boolean scanning, subscribed, authorized, started;
    private int subscriptionStep;
    private byte[] lastHello;
    private final Runnable timeout = () -> fail("CSTP handshake timeout");

    private void info(String value) { handler.post(() -> { if (!isFinishing()) display.setText(value); }); }
    private boolean permitted() {
        return Build.VERSION.SDK_INT < 31 ||
             (checkSelfPermission(Manifest.permission.BLUETOOTH_SCAN) == PackageManager.PERMISSION_GRANTED &&
              checkSelfPermission(Manifest.permission.BLUETOOTH_CONNECT) == PackageManager.PERMISSION_GRANTED);
    }
    private void clearSession() {
        authorized=false; subscribed=false; subscriptionStep=0; nextSequence=0;
        control=null;data=null;lastHello=null;
        if(transcript!=null) Arrays.fill(transcript,(byte)0);
        if(proofExpected!=null) Arrays.fill(proofExpected,(byte)0);
        transcript=null;proofExpected=null;sid=null;receiveKeys=null;
        handler.removeCallbacks(timeout);
    }
    private void fail(String reason) {
        Log.w(TAG,reason);
        info("CSTP SECURE LINK\n"+reason+"\nNo telemetry authorized");
        clearSession();
        if (gatt!=null && permitted()) { BluetoothGatt old=gatt;gatt=null;old.disconnect();old.close(); }
    }
    @Override public void onCreate(Bundle state) {
        super.onCreate(state);
        display=new TextView(this);
        display.setTextSize(20);display.setTextColor(0xff86edc1);
        display.setBackgroundColor(0xff000000);display.setPadding(18,18,18,18);
        setContentView(display);
        info("CSTP laboratory client\nChecking provisioning...");
        // Debug/developer-only import: push exactly 32 raw random bytes via
        // run-as from trusted ADB. Never place the key in APK/assets/logcat.
        psk=new byte[32];
        try(FileInputStream in=openFileInput("cstp.psk")) {
            if(in.read(psk)!=32 || in.read()!=-1) throw new IllegalArgumentException("PSK length");
            int any=0;for(byte b:psk)any|=b;
            if(any==0)throw new IllegalArgumentException("zero PSK");
        } catch(Exception ex) {
            Arrays.fill(psk,(byte)0);psk=null;
            info("CSTP NOT PROVISIONED\nNo pairing or plaintext fallback");
            return;
        }
        if (!permitted()) {
            if(Build.VERSION.SDK_INT>=31) requestPermissions(new String[]{Manifest.permission.BLUETOOTH_SCAN,Manifest.permission.BLUETOOTH_CONNECT},51);
            else fail("Bluetooth permissions missing");
        } else scan();
    }
    @Override public void onRequestPermissionsResult(int request,String[] permissions,int[] grants) {
        super.onRequestPermissionsResult(request,permissions,grants);
        if(request==51 && permitted())scan();else fail("Bluetooth permissions denied");
    }
    private final ScanCallback discovery=new ScanCallback() {
        @Override public void onScanResult(int type,ScanResult result) {
            ScanRecord record=result.getScanRecord();
            if(record==null || record.getServiceUuids()==null ||
               !record.getServiceUuids().contains(new android.os.ParcelUuid(SERVICE)))return;
            handler.post(() -> {
                if(!scanning || gatt!=null || !permitted())return;
                stopScan();
                info("CSTP connecting (untrusted transport)...");
                gatt=result.getDevice().connectGatt(CstpActivity.this,false,callbacks,BluetoothDevice.TRANSPORT_LE);
                if(gatt==null)fail("connectGatt returned null");
            });
        }
        @Override public void onScanFailed(int code) {handler.post(() -> fail("BLE scan failed: "+code));}
    };
    private void scan() {
        if(started || psk==null || !permitted())return;
        BluetoothManager manager=(BluetoothManager)getSystemService(Context.BLUETOOTH_SERVICE);
        BluetoothAdapter adapter=manager==null?null:manager.getAdapter();
        scanner=adapter==null?null:adapter.getBluetoothLeScanner();
        if(scanner==null){fail("BLE unavailable");return;}
        started=true;scanning=true;
        info("CSTP searching for secure-only service...");
        scanner.startScan(null,new ScanSettings.Builder().setScanMode(ScanSettings.SCAN_MODE_LOW_LATENCY).build(),discovery);
    }
    private void stopScan() {
        if(scanning && scanner!=null && permitted())scanner.stopScan(discovery);
        scanning=false;
    }
    private final BluetoothGattCallback callbacks=new BluetoothGattCallback() {
        @Override public void onConnectionStateChange(BluetoothGatt client,int status,int state){
            handler.post(() -> {
                if(client!=gatt) {client.close();return;}
                if(status!=BluetoothGatt.GATT_SUCCESS || state==BluetoothProfile.STATE_DISCONNECTED) {
                    fail("BLE disconnected status="+status);return;
                }
                if(state==BluetoothProfile.STATE_CONNECTED) {
                    info("CSTP discovering isolated GATT...");
                    if(!client.requestMtu(100))client.discoverServices();
                }
            });
        }
        @Override public void onMtuChanged(BluetoothGatt client,int mtu,int status){
            handler.post(() -> {if(client==gatt)client.discoverServices();});
        }
        @Override public void onServicesDiscovered(BluetoothGatt client,int status) {
            handler.post(() -> {
                if(client!=gatt || status!=BluetoothGatt.GATT_SUCCESS){fail("Secure service discovery failed");return;}
                BluetoothGattService service=client.getService(SERVICE);
                control=service==null?null:service.getCharacteristic(CONTROL);
                data=service==null?null:service.getCharacteristic(DATA);
                if(control==null || data==null){fail("Secure-only service unavailable");return;}
                subscriptionStep=0;
                subscribe(client,control);
            });
        }
        @Override public void onDescriptorWrite(BluetoothGatt client,BluetoothGattDescriptor descriptor,int status) {
            handler.post(() -> {
                if(client!=gatt)return;
                if(status!=BluetoothGatt.GATT_SUCCESS){fail("CSTP notify subscription rejected: "+status);return;}
                if(subscriptionStep==0){subscriptionStep=1;subscribe(client,data);}
                else if(subscriptionStep==1){
                    subscriptionStep=2;
                    subscribed=true;
                    if(!client.readCharacteristic(control))fail("HELLO read could not start");
                    handler.postDelayed(timeout,30000);
                }
            });
        }
        @Override public void onCharacteristicRead(BluetoothGatt client,BluetoothGattCharacteristic characteristic,int status) {
            byte[] bytes=characteristic.getValue();
            handler.post(() -> {
                if(client!=gatt || characteristic.getUuid()==null ||
                   !CONTROL.equals(characteristic.getUuid()))return;
                if(status!=BluetoothGatt.GATT_SUCCESS){fail("HELLO read failed");return;}
                processHello(bytes);
            });
        }
        @Override public void onCharacteristicChanged(BluetoothGatt client,BluetoothGattCharacteristic characteristic) {
            byte[] bytes=characteristic.getValue();
            handler.post(() -> {
                if(client!=gatt || bytes==null)return;
                if(CONTROL.equals(characteristic.getUuid()))processProof(bytes);
                else if(DATA.equals(characteristic.getUuid()))processData(bytes);
            });
        }
    };
    private void subscribe(BluetoothGatt client,BluetoothGattCharacteristic characteristic) {
        if(!client.setCharacteristicNotification(characteristic,true)){fail("notification registration failed");return;}
        BluetoothGattDescriptor descriptor=characteristic.getDescriptor(CCCD);
        if(descriptor==null){fail("secure CCCD missing");return;}
        descriptor.setValue(BluetoothGattDescriptor.ENABLE_NOTIFICATION_VALUE);
        if(!client.writeDescriptor(descriptor))fail("secure CCCD write failed");
    }
    private void processHello(byte[] hello) {
        if(!subscribed || transcript!=null || hello==null || hello.length!=34 ||
           hello[0]!=1 || hello[1]!=1){fail("invalid or duplicate HELLO");return;}
        lastHello=Arrays.copyOf(hello,hello.length);
        byte[] card=Arrays.copyOfRange(hello,2,34);
        byte[] blade=new byte[32];rng.nextBytes(blade);
        try {
            transcript=CstpV1.transcript(card,blade);
            sid=CstpV1.sessionId(transcript);
            proofExpected=CstpV1.proof(psk,transcript,false);
            receiveKeys=CstpV1.derive(psk,transcript,CstpV1.C2B);
            byte[] msg=new byte[66];msg[0]=2;msg[1]=1;
            System.arraycopy(blade,0,msg,2,32);
            System.arraycopy(CstpV1.proof(psk,transcript,true),0,msg,34,32);
            control.setValue(msg);
            info("CSTP verifying Cardputer proof...");
            if(!gatt.writeCharacteristic(control))fail("CLIENT_PROOF write failed");
        } catch(Exception e){fail("CSTP HELLO cryptographic failure");}
    }
    private void processProof(byte[] message) {
        if(transcript==null || authorized || message.length!=34 || message[0]!=3 || message[1]!=1) {
            fail("unexpected SERVER_PROOF");return;
        }
        if(!java.security.MessageDigest.isEqual(proofExpected,Arrays.copyOfRange(message,2,34))){
            fail("SERVER_PROOF authentication failed");return;
        }
        authorized=true;handler.removeCallbacks(timeout);
        info("CSTP authenticated\nWaiting for encrypted status...");
        Log.i(TAG,"CSTP authenticated=1");
    }
    private void processData(byte[] frame) {
        if(!authorized || receiveKeys==null){fail("unauthorized ciphertext received");return;}
        try {
            byte[] plain=CstpV1.open(receiveKeys,CstpV1.C2B,sid,nextSequence,frame);
            if(frame[2]!=0x10)throw new GeneralSecurityException("unsupported type");
            String value=new String(plain,java.nio.charset.StandardCharsets.UTF_8);
            if(!value.matches("1,[0-9]{1,3},[0-9]{1,3},[0-9]{1,3},[01],[01]"))
                throw new GeneralSecurityException("invalid status");
            if(nextSequence==Long.MAX_VALUE)throw new GeneralSecurityException("sequence exhausted");
            ++nextSequence;
            String[] fields=value.split(",");
            info("CSTP AUTHENTICATED / ENCRYPTED\nWi-Fi: "+fields[1]+
                 "\nBLE: "+fields[2]+"\nLoRa: "+fields[3]+
                 "\nGPS fix: "+fields[4]+"\nSD recording: "+fields[5]);
        } catch(Exception e){fail("CSTP ciphertext rejected (tag/sequence/schema)");}
    }
    @Override protected void onDestroy(){
        stopScan();clearSession();started=false;
        if(gatt!=null && permitted()){BluetoothGatt old=gatt;gatt=null;old.disconnect();old.close();}
        if(psk!=null)Arrays.fill(psk,(byte)0);
        handler.removeCallbacksAndMessages(null);
        super.onDestroy();
    }
}
