package org.callingcard.blade;

import android.content.Context;
import android.graphics.Canvas;
import android.graphics.Color;
import android.graphics.Paint;
import android.view.View;
import java.util.ArrayDeque;
import java.util.ArrayList;
import java.util.Iterator;

/** Square-display graphical HUD. Only passive observations from the Cardputer. */
public final class ScannerHudView extends View {
    static final class Item {
        final String name; final int channel, rssi; final long timestamp;
        Item(String n, int c, int r, long t) { name=n;channel=c;rssi=r;timestamp=t; }
    }
    final ArrayDeque<Item>[] items;
    int page=0, wifi=0, ble=0, lora=0;
    boolean gps=false, recording=false;
    String health="SEARCHING", alert="";
    private final Paint p = new Paint(3);
    @SuppressWarnings("unchecked")
    ScannerHudView(Context context) {
        super(context);
        items = (ArrayDeque<Item>[])new ArrayDeque<?>[3];
        for (int i=0;i<3;i++) items[i]=new ArrayDeque<>();
        setBackgroundColor(Color.BLACK);
    }
    void add(char source, int channel, int rssi, String name) {
        int index = source=='W'?0:source=='B'?1:source=='L'?2:-1;
        if(index<0) return;
        ArrayDeque<Item> q=items[index];
        if(q.size()>=32) q.removeFirst();
        q.addLast(new Item(name,channel,rssi,android.os.SystemClock.elapsedRealtime()));
        invalidate();
    }
    void next(int delta) { page=(page+delta+4)%4;invalidate(); }
    private void txt(Canvas c, String s,float x,float y,float size,int color) {
        p.setColor(color);p.setStrokeWidth(1);p.setStyle(Paint.Style.FILL);
        p.setTypeface(android.graphics.Typeface.MONOSPACE);p.setTextSize(size);c.drawText(s,x,y,p);
    }
    private int tint() {
        return "LIVE".equals(health)?Color.rgb(95,237,167):
            "STALE".equals(health)?Color.RED:Color.YELLOW;
    }
    @Override protected void onDraw(Canvas original) {
        super.onDraw(original);
        float scale=Math.min(getWidth()/480f,getHeight()/480f);
        Canvas c=original;
        c.save();c.translate((getWidth()-480*scale)/2,(getHeight()-480*scale)/2);c.scale(scale,scale);
        int green=Color.rgb(95,237,167), white=Color.rgb(224,238,233), dim=Color.rgb(126,162,149);
        p.setColor(Color.rgb(20,47,37));c.drawRoundRect(10,10,470,470,15,15,p);
        txt(c,"CALLINGCARD",27,52,30,green);
        txt(c,health,320,52,22,tint());
        txt(c,"<  "+new String[]{"OVERVIEW","Wi-Fi","BLE","LoRa"}[page]+"  >",30,96,26,white);
        p.setColor(dim);c.drawRect(26,112,454,114,p);
        if(page==0) {
            txt(c,"Wi-Fi "+wifi,35,166,32,green);
            txt(c,"BLE   "+ble,35,212,32,green);
            txt(c,"LoRa  "+lora,35,258,32,green);
            txt(c,"GPS "+(gps?"FIX":"NO FIX"),35,313,25,white);
            txt(c,"SD  "+(recording?"RECORDING":"IDLE"),35,351,25,white);
        } else if(page==1) {
            txt(c,"2.4 GHz CHANNEL ACTIVITY",28,148,20,white);
            int[] counts=new int[14];
            long now=android.os.SystemClock.elapsedRealtime();
            for(Item item:items[0]) if(item.channel>=1 && item.channel<=14 && now-item.timestamp<120000) counts[item.channel-1]++;
            int max=1;for(int count:counts) max=Math.max(max,count);
            for(int i=0;i<14;i++) {
                float x=28+i*30;
                float h=120f*counts[i]/max;
                p.setColor(i<11?green:dim);c.drawRect(x,310-h,x+19,310,p);
                txt(c,Integer.toString(i+1),x,334,15,white);
            }
            txt(c,"Samples / last 120s (not RF spectrum)",26,366,17,dim);
        } else {
            txt(c,page==2?"RECENT BLE ADVERTISEMENTS":"RECENT LoRa RX",27,148,22,white);
        }
        if(page>0) {
            ArrayDeque<Item> q=items[page-1];
            ArrayList<Item> recent=new ArrayList<>(q);
            int maxLines=page==1?3:6, row=0;
            for(int i=recent.size()-1;i>=0 && row<maxLines;i--,row++) {
                Item item=recent.get(i);
                String name=item.name.length()>15?item.name.substring(0,15):item.name;
                int y=page==1?398+row*23:186+row*37;
                txt(c,name,27,y,page==1?18:22,white);
                String extra=page==1?"ch"+item.channel+" "+item.rssi+"dBm":
                    page==2?item.rssi+"dBm":"RX";
                txt(c,extra,292,y,page==1?17:19,green);
            }
        }
        if(!alert.isEmpty()) txt(c,alert.length()>30?alert.substring(0,30):alert,28,453,18,Color.YELLOW);
        c.restore();
    }
}
