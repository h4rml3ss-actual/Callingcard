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
        final ArrayDeque<Integer> history;
        Item(String n,int c,int r,long t,ArrayDeque<Integer> prior) {
            name=n;channel=c;rssi=r;timestamp=t;
            history=prior==null?new ArrayDeque<>():new ArrayDeque<>(prior);
            if(history.size()>=24)history.removeFirst();
            history.addLast(r);
        }
    }
    final ArrayDeque<Item>[] items;
    // Keep raw Wi-Fi sightings for the 120-second channel histogram.
    final ArrayDeque<Item> wifiSamples = new ArrayDeque<>();
    int page=0, wifi=0, ble=0, lora=0;
    private int selection=0;
    private boolean detailOpen=false;
    private Item selectedItem=null;
    private int selectedSource=0;
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
        long now=android.os.SystemClock.elapsedRealtime();
        ArrayDeque<Item> q=items[index];
        ArrayDeque<Integer> prior=null;
        for(Item old:q) {
            if(index!=2 && name!=null && !name.equals("<unnamed BLE>") &&
               !name.equals("<hidden>") && old.name.equals(name) &&
               (index==1 || old.channel==channel)) {prior=old.history;break;}
        }
        Item updated=new Item(name,channel,rssi,now,prior);
        if(index==0) {
            while(!wifiSamples.isEmpty() && now-wifiSamples.peekFirst().timestamp>120000)
                wifiSamples.removeFirst();
            if(wifiSamples.size()>=256) wifiSamples.removeFirst();
            wifiSamples.addLast(updated);
        }
        // Same SSID/channel is one displayed Wi-Fi row; same advertised BLE name
        // is one row because our BLE wire payload has no device address.
        // Placeholder/unnamed advertisements are not uniquely identifiable.
        if(index!=2 && name!=null && !name.isEmpty() &&
                !name.equals("<unnamed BLE>") && !name.equals("<hidden>")) {
            Iterator<Item> it=q.iterator();
            while(it.hasNext()) {
                Item old=it.next();
                if(old.name.equals(name) && (index==1 || old.channel==channel)) it.remove();
            }
        }
        if(q.size()>=32) q.removeFirst();
        q.addLast(updated);
        if(detailOpen && selectedItem!=null && selectedSource==index+1 &&
            selectedItem.name.equals(name) && (index!=0||selectedItem.channel==channel))
            selectedItem=updated;
        invalidate();
    }
    void next(int delta) { if(detailOpen)return; page=(page+delta+4)%4; selection=0;invalidate(); }
    private ArrayList<Item> newestFirst() {
        ArrayList<Item> result=new ArrayList<>();
        if(page==0)return result;
        for(Item item:items[page-1]) result.add(0,item);
        return result;
    }
    boolean moveSelection(int delta) {
        if(page==0||detailOpen)return false;
        ArrayList<Item> list=newestFirst();
        if(list.isEmpty())return true;
        selection=Math.max(0,Math.min(list.size()-1,selection+delta));
        invalidate();return true;
    }
    boolean openSelected() {
        if(page==0||detailOpen)return false;
        ArrayList<Item> list=newestFirst();
        if(list.isEmpty())return true;
        selection=Math.min(selection,list.size()-1);
        selectedItem=list.get(selection);selectedSource=page;detailOpen=true;
        invalidate();return true;
    }
    boolean back() {
        if(!detailOpen)return false;
        detailOpen=false;selectedItem=null;invalidate();return true;
    }
    private void txt(Canvas c, String s,float x,float y,float size,int color) {
        p.setColor(color);p.setStrokeWidth(1);p.setStyle(Paint.Style.FILL);
        p.setTypeface(android.graphics.Typeface.MONOSPACE);p.setTextSize(size);c.drawText(s,x,y,p);
    }
    private void drawSignalTrend(Canvas c,Item item,float x,float y,float width,float height,int green,int dim) {
        p.setColor(dim);p.setStyle(Paint.Style.STROKE);p.setStrokeWidth(1);
        c.drawRect(x,y,x+width,y+height,p);
        int count=item.history.size(),i=0;float lastX=0,lastY=0;
        for(int rssi:item.history) {
            float px=x+i*width/Math.max(1,count-1);
            float py=y+height-(Math.max(-110,Math.min(-20,rssi))+110)*height/90f;
            if(i>0) {p.setColor(green);p.setStrokeWidth(2);c.drawLine(lastX,lastY,px,py,p);}
            lastX=px;lastY=py;i++;
        }
        p.setStyle(Paint.Style.FILL);
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
        if(detailOpen && selectedItem!=null) {
            Item item=selectedItem;
            txt(c,"OBSERVATION DETAIL",28,149,24,green);
            txt(c,"SOURCE "+new String[]{"Wi-Fi","BLE","LoRa"}[selectedSource-1],28,195,23,white);
            txt(c,"NAME (received)",28,238,18,dim);
            String n=item.name==null?"":item.name;
            for(int pos=0;pos<n.length()&&pos<60;pos+=19)
                txt(c,n.substring(pos,Math.min(n.length(),pos+19)),28,270+(pos/19)*26,20,white);
            if(selectedSource==1)txt(c,"CHANNEL "+item.channel,285,345,17,white);
            txt(c,"RSSI "+item.rssi+" dBm",28,345,22,white);
            if(selectedSource==3 && item.name.startsWith("SNR:")) {
                try {
                    int t=Integer.parseInt(item.name.substring(4));
                    txt(c,"SNR "+(t/10.0f)+" dB",28,374,18,white);
                } catch(NumberFormatException ignored) {}
            }
            drawSignalTrend(c,item,28,380,420,44,green,dim);
            long age=Math.max(0,(android.os.SystemClock.elapsedRealtime()-item.timestamp)/1000);
            txt(c,"SEEN "+age+"s   SAMPLES "+item.history.size(),28,446,15,dim);

            c.restore();return;
        }
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
            for(Item item:wifiSamples) if(item.channel>=1 && item.channel<=14 && now-item.timestamp<120000) counts[item.channel-1]++;
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
            ArrayList<Item> recent=newestFirst();
            int maxLines=page==1?3:6;
            selection=Math.max(0,Math.min(selection,Math.max(0,recent.size()-1)));
            int windowStart=Math.max(0,selection-maxLines+1);
            for(int row=0;row<maxLines && windowStart+row<recent.size();row++) {
                int index=windowStart+row;
                Item item=recent.get(index);
                int y=page==1?398+row*23:186+row*37;
                if(index==selection) {
                    p.setColor(Color.rgb(48,88,68));
                    c.drawRoundRect(21,y-(page==1?18:25),454,y+5,5,5,p);
                }
                String name=item.name==null?"":item.name;
                float nameSize=page==1?17:20;
                p.setTypeface(android.graphics.Typeface.MONOSPACE);p.setTextSize(nameSize);
                int maxWidth=page==1?275:260;
                while(name.length()>1 && p.measureText(name)>maxWidth) name=name.substring(0,name.length()-1);
                if(!name.equals(item.name) && name.length()>1) name=name.substring(0,name.length()-1)+"…";
                txt(c,name,27,y,nameSize,white);
                String extra=page==1?"ch"+item.channel+" "+item.rssi+"dBm":
                    page==2?item.rssi+"dBm":"RX";
                txt(c,extra,320,y,page==1?15:18,green);
            }
        }
        if(!alert.isEmpty()) txt(c,alert.length()>30?alert.substring(0,30):alert,28,453,18,Color.YELLOW);
        c.restore();
    }
}
