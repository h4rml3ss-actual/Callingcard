package org.callingcard.blade;

import android.Manifest;
import android.app.Activity;
import android.bluetooth.*;
import android.bluetooth.le.*;
import android.content.*;
import android.content.pm.PackageManager;
import android.os.*;
import android.util.Log;
import android.widget.TextView;
import java.util.UUID;

/** Isolated PR60 experiment: trigger SMP by reading a protected GATT value. */
public final class GattAuthTestActivity extends Activity {
    private static final String TAG="CallingcardGattAuth";
    private static final UUID SERVICE=UUID.fromString("cabb1e00-ccad-4abc-a112-24fc00000001");
    private static final UUID STATUS=UUID.fromString("cabb1e00-ccad-4abc-a112-24fc00000002");
    private final Handler main=new Handler(Looper.getMainLooper());
    private BluetoothLeScanner scanner;
    private BluetoothGatt gatt;
    private BluetoothGattCharacteristic statusCharacteristic;
    private TextView view;
    private boolean scanning=false, pendingRead=false, bonded=false, active=false;
    private long startedMs=0;
    private int variant=-1;
    private void log(String msg) {
        Log.i(TAG,msg);
        if (view!=null && active) view.setText("Callingcard GATT auth test\n"+msg);
    }
    private boolean permits() {
        return Build.VERSION.SDK_INT<31 ||
            (checkSelfPermission(Manifest.permission.BLUETOOTH_SCAN)==PackageManager.PERMISSION_GRANTED &&
             checkSelfPermission(Manifest.permission.BLUETOOTH_CONNECT)==PackageManager.PERMISSION_GRANTED);
    }
    private final BroadcastReceiver receiver=new BroadcastReceiver() {
        @Override public void onReceive(Context context,Intent intent) {
            BluetoothDevice device=intent.getParcelableExtra(BluetoothDevice.EXTRA_DEVICE);
            if(device==null || gatt==null || !device.getAddress().equals(gatt.getDevice().getAddress()))return;
            if(BluetoothDevice.ACTION_PAIRING_REQUEST.equals(intent.getAction())) {
                variant=intent.getIntExtra(BluetoothDevice.EXTRA_PAIRING_VARIANT,-1);
                log("PAIRING_REQUEST variant="+variant+" (observed only)");
            } else if(BluetoothDevice.ACTION_BOND_STATE_CHANGED.equals(intent.getAction())) {
                final int state=intent.getIntExtra(BluetoothDevice.EXTRA_BOND_STATE,-1);
                log("bond_state="+state+" elapsed_ms="+(SystemClock.elapsedRealtime()-startedMs));
                if(state==BluetoothDevice.BOND_BONDED) {
                    bonded=true;
                    // Do not claim success until protected GATT read succeeds.
                    // If the read is already pending, wait for its callback.
                    if(!pendingRead) main.postDelayed(()->readProtected("after_bond"),500);
                }
                if(state==BluetoothDevice.BOND_NONE && variant>=0)
                    log("bond_failed variant="+variant+" (no retry/no security fallback)");
            }
        }
    };
    @Override public void onCreate(Bundle b) {
        super.onCreate(b);
        view=new TextView(this);view.setTextSize(19);view.setPadding(20,20,20,20);
        setContentView(view);active=true;
        IntentFilter filter=new IntentFilter(BluetoothDevice.ACTION_PAIRING_REQUEST);
        filter.addAction(BluetoothDevice.ACTION_BOND_STATE_CHANGED);
        registerReceiver(receiver,filter);
        if(!permits()) {
            if(Build.VERSION.SDK_INT>=31)requestPermissions(
                new String[]{Manifest.permission.BLUETOOTH_SCAN,Manifest.permission.BLUETOOTH_CONNECT},60);
            else log("Bluetooth permission missing");
        } else scan();
    }
    @Override public void onRequestPermissionsResult(int request,String[] names,int[] grants) {
        super.onRequestPermissionsResult(request,names,grants);
        if(request==60 && permits())scan();else log("Bluetooth permission denied");
    }
    private final ScanCallback scans=new ScanCallback(){
        @Override public void onScanResult(int type,ScanResult result) {
            ScanRecord record=result.getScanRecord();
            if(record==null || record.getServiceUuids()==null ||
                !record.getServiceUuids().contains(new android.os.ParcelUuid(SERVICE)))return;
            main.post(()->{
                if(!active || gatt!=null || !scanning || !permits())return;
                stopScan();
                log("connect: NO explicit createBond()");
                startedMs=SystemClock.elapsedRealtime();
                gatt=result.getDevice().connectGatt(GattAuthTestActivity.this,false,callbacks,BluetoothDevice.TRANSPORT_LE);
                if(gatt==null)log("connectGatt failed");
            });
        }
        @Override public void onScanFailed(int code) {main.post(()->log("scan failed code="+code));}
    };
    private void scan() {
        if(!active || scanning || gatt!=null || !permits())return;
        BluetoothManager manager=(BluetoothManager)getSystemService(Context.BLUETOOTH_SERVICE);
        BluetoothAdapter adapter=manager==null?null:manager.getAdapter();
        scanner=adapter==null?null:adapter.getBluetoothLeScanner();
        if(scanner==null){log("Bluetooth unavailable");return;}
        scanning=true;log("Scanning for isolated NimBLE protected service...");
        scanner.startScan(null,new ScanSettings.Builder().setScanMode(ScanSettings.SCAN_MODE_LOW_LATENCY).build(),scans);
    }
    private void stopScan() {
        if(scanning && scanner!=null && permits())scanner.stopScan(scans);
        scanning=false;
    }
    private final BluetoothGattCallback callbacks=new BluetoothGattCallback(){
        @Override public void onConnectionStateChange(BluetoothGatt client,int code,int state) {
            main.post(()->{
                if(client!=gatt)return;
                log("GATT state="+state+" status="+code+" bond="+client.getDevice().getBondState());
                if(state==BluetoothProfile.STATE_CONNECTED && code==BluetoothGatt.GATT_SUCCESS) {
                    if(!client.discoverServices())log("discoverServices could not start");
                } else if(state==BluetoothProfile.STATE_DISCONNECTED || code!=BluetoothGatt.GATT_SUCCESS){
                    pendingRead=false;statusCharacteristic=null;
                    gatt=null;client.close();log("Disconnected; experiment ended");
                }
            });
        }
        @Override public void onServicesDiscovered(BluetoothGatt client,int result) {
            main.post(()->{
                if(client!=gatt)return;
                if(result!=BluetoothGatt.GATT_SUCCESS){log("Discovery failed "+result);return;}
                BluetoothGattService service=client.getService(SERVICE);
                statusCharacteristic=service==null?null:service.getCharacteristic(STATUS);
                if(statusCharacteristic==null){log("Protected status missing");return;}
                readProtected("initial");
            });
        }
        @Override public void onCharacteristicRead(BluetoothGatt client,BluetoothGattCharacteristic characteristic,int result) {
            main.post(()->{
                if(client!=gatt || statusCharacteristic!=characteristic)return;
                pendingRead=false;
                int state=client.getDevice().getBondState();
                Log.i(TAG,"protected_read status="+result+" bond="+state+
                          " elapsed_ms="+(SystemClock.elapsedRealtime()-startedMs));
                if(result==BluetoothGatt.GATT_SUCCESS && state==BluetoothDevice.BOND_BONDED) {
                    log("SUCCESS: bonded + protected GATT read, value withheld");
                } else {
                    log("protected_read status="+result+" bond="+state+
                        " (not authenticated; no plaintext fallback)");
                }
            });
        }
    };
    private void readProtected(String trigger) {
        if(!active || gatt==null || statusCharacteristic==null || pendingRead)return;
        if(!permits()){log("read permission denied");return;}
        pendingRead=true;
        log("protected_read_request="+trigger+" bond="+gatt.getDevice().getBondState());
        if(!gatt.readCharacteristic(statusCharacteristic)) {
            pendingRead=false;log("readCharacteristic failed to start");
        }
    }
    @Override protected void onDestroy() {
        active=false;stopScan();main.removeCallbacksAndMessages(null);
        unregisterReceiver(receiver);
        if(gatt!=null && permits()){BluetoothGatt old=gatt;gatt=null;old.disconnect();old.close();}
        super.onDestroy();
    }
}
