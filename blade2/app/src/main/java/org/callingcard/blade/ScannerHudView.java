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
    static final class DetectionItem {
        final String type, confidence, source;
        final int rssi;
        final long timestamp;
        DetectionItem(String t,String c,int r,String src,long when) {
            type=t;confidence=c;rssi=r;source=src;timestamp=when;
        }
    }
    private final ArrayDeque<DetectionItem> detections=new ArrayDeque<>();
    private int detectionSelection=0;
    private boolean detectionDetail=false;
    void addDetection(String type,String confidence,int rssi,String source) {
        long now=android.os.SystemClock.elapsedRealtime();
        // Bounded transient on-device history, newest first; no addresses or GPS.
        if(detections.size()>=24)detections.removeLast();
        detections.addFirst(new DetectionItem(type,confidence,rssi,source,now));
        detectionSelection=0;
        invalidate();
    }
    final ArrayDeque<Item>[] items;
    // Keep raw Wi-Fi sightings for the 120-second channel histogram.
    final ArrayDeque<Item> wifiSamples = new ArrayDeque<>();
    int page=0, wifi=0, ble=0, lora=0;
    // Observation ages are based on receipt time, not proof of RF disappearance.
    private static final long RECENT_WIFI_MS=120000;
    private static final long RECENT_BLE_MS=180000;
    private static final long RETAIN_MS=900000;
    private boolean showOld=true;
    private String selectedKey=null;
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
        // Prune old rows without changing the independent histogram sample window.
        Iterator<Item> expired=q.iterator();
        while(expired.hasNext()) {
            if(now-expired.next().timestamp>RETAIN_MS) expired.remove();
        }
        ArrayDeque<Integer> prior=null;
        for(Item old:q) {
            if(index!=2 && name!=null && !name.equals("<unnamed BLE>") &&
               !name.equals("<hidden>") && old.name.equals(name) &&
               (index==1 || old.channel==channel)) {prior=old.history;break;}
        }
        if(index==2 && !q.isEmpty()) prior=q.peekLast().history;
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
    void next(int delta) { if(detailOpen)return; page=(page+delta+5)%5; selection=0;selectedKey=null;invalidate(); }
    boolean toggleOld() {
        if(page==0||page==4||detailOpen)return false;
        showOld=!showOld;
        selection=0;selectedKey=null;invalidate();return true;
    }
    private long recentThreshold() {return page==1?RECENT_WIFI_MS:page==2?RECENT_BLE_MS:RETAIN_MS;}
    private String identityKey(Item item) {
        return (page==1?"W:"+item.channel+":":page==2?"B:":"L:")+item.name;
    }
    private ArrayList<Item> newestFirst() {
        ArrayList<Item> result=new ArrayList<>();
        if(page==0||page==4)return result;
        long now=android.os.SystemClock.elapsedRealtime();
        for(Item item:items[page-1]) {
            if(now-item.timestamp>RETAIN_MS)continue;
            if(!showOld && now-item.timestamp>recentThreshold())continue;
            result.add(0,item);
        }
        return result;
    }
    boolean moveSelection(int delta) {
        if(page==4) {
            if(detectionDetail)return false;
            detectionSelection=Math.max(0,Math.min(detections.size()-1,detectionSelection+delta));
            invalidate();return true;
        }
        if(page==0||detailOpen)return false;
        ArrayList<Item> list=newestFirst();
        if(list.isEmpty())return true;
        selection=Math.max(0,Math.min(list.size()-1,selection+delta));
        selectedKey=identityKey(list.get(selection));
        invalidate();return true;
    }
    boolean openSelected() {
        if(page==4) {
            if(detections.isEmpty())return true;
            detectionDetail=true;invalidate();return true;
        }
        if(page==0||detailOpen)return false;
        ArrayList<Item> list=newestFirst();
        if(list.isEmpty())return true;
        selection=Math.min(selection,list.size()-1);
        selectedItem=list.get(selection);selectedSource=page;detailOpen=true;
        selectedKey=identityKey(selectedItem);
        invalidate();return true;
    }
    boolean back() {
        if(detectionDetail){detectionDetail=false;invalidate();return true;}
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
        txt(c,"<  "+new String[]{"OVERVIEW","Wi-Fi","BLE","LoRa","DETECTIONS"}[page]+"  >",30,96,26,white);
        p.setColor(dim);c.drawRect(26,112,454,114,p);
        if(page==4) {
            txt(c,"CANDIDATES / "+detections.size(),28,147,22,white);
            if(detections.isEmpty())txt(c,"No classified detections",28,211,19,dim);
            else {
                int idx=0;
                for(DetectionItem d:detections) {
                    if(idx==detectionSelection && detectionDetail) {
                        txt(c,"POSSIBLE "+d.type,28,204,25,Color.YELLOW);
                        txt(c,"CONFIDENCE "+d.confidence,28,251,20,white);
                        txt(c,"SIGNAL "+d.rssi+" dBm",28,298,20,green);
                        txt(c,"SOURCE "+d.source,28,341,20,white);
                        long age=(android.os.SystemClock.elapsedRealtime()-d.timestamp)/1000;
                        txt(c,"SEEN "+age+"s ago",28,384,19,dim);
                        txt(c,"Candidate, not identity proof",28,434,15,Color.YELLOW);
                        break;
                    }
                    ++idx;
                }
                if(!detectionDetail) {
                    int start=Math.max(0,detectionSelection-5);idx=0;
                    for(DetectionItem d:detections) {
                        if(idx>=start && idx<start+6) {
                            int y=191+(idx-start)*46;
                            if(idx==detectionSelection) {
                                p.setColor(Color.rgb(48,88,68));c.drawRoundRect(21,y-23,454,y+13,5,5,p);
                            }
                            String name="POSSIBLE "+d.type;
                            txt(c,name,28,y,19,Color.YELLOW);
                            txt(c,d.confidence+"  "+d.rssi+"dBm",28,y+19,14,dim);
                        }
                        idx++;
                    }
                }
            }
            c.restore();return;
        }
        if(detailOpen && selectedItem!=null) {
            Item item=selectedItem;
            txt(c,"OBSERVATION DETAIL",28,149,24,green);
            txt(c,"SOURCE "+new String[]{"Wi-Fi","BLE","LoRa"}[selectedSource-1],28,195,23,white);
            boolean uncertain=selectedSource!=3 &&
                (item.name.equals("<hidden>") || item.name.equals("<unnamed BLE>") || item.name.isEmpty());
            txt(c,uncertain?"IDENTITY UNKNOWN":"NAME-BASED MATCH",28,238,16,Color.YELLOW);
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
            boolean recent=selectedSource==1?age*1000<RECENT_WIFI_MS:
                           selectedSource==2?age*1000<RECENT_BLE_MS:age*1000<RETAIN_MS;
            txt(c,(recent?"RECENT ":"OLD ")+age+"s  SAMPLES "+item.history.size(),28,446,15,recent?green:Color.YELLOW);

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
            if(page!=3) txt(c,showOld?"ALL (UP toggles)":"RECENT (UP toggles)",27,page==1?377:167,14,dim);
            if(selectedKey!=null) {
                for(int i=0;i<recent.size();i++)
                    if(identityKey(recent.get(i)).equals(selectedKey)) {selection=i;break;}
            }
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
                long ageMs=android.os.SystemClock.elapsedRealtime()-item.timestamp;
                boolean old=ageMs>(page==1?RECENT_WIFI_MS:page==2?RECENT_BLE_MS:RETAIN_MS);
                txt(c,name,27,y,nameSize,old?dim:white);
                String extra=page==1?"ch"+item.channel+" "+item.rssi+"dBm":
                    page==2?item.rssi+"dBm":"RX";
                txt(c,extra,320,y,page==1?15:18,old?dim:green);
            }
        }
        if(!alert.isEmpty() && page==0) txt(c,alert.length()>30?alert.substring(0,30):alert,28,453,18,Color.YELLOW);
        c.restore();
    }
}
