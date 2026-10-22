#!/usr/bin/env python3
"""Build a self-contained, offline HTML viewer from Callingcard JSONL sessions."""
import argparse
import html
import json
from pathlib import Path
try:
    from tools.analyze_sessions import read_observations, coordinates
except ModuleNotFoundError:
    from analyze_sessions import read_observations, coordinates

# Bounded visualization rows; scans all records but charts at most this many.
MAX_POINTS = 12000

PAGE = r"""<!doctype html>
<html lang="en"><head><meta charset="utf-8">
<meta name="viewport" content="width=device-width, initial-scale=1">
<meta http-equiv="Content-Security-Policy" content="default-src 'none'; script-src 'unsafe-inline'; style-src 'unsafe-inline'; img-src data:">
<title>Callingcard — Offline Survey Viewer</title>
<style>
:root{color-scheme:dark;font-family:system-ui,sans-serif;background:#09151a;color:#e4f3f4}
body{max-width:1100px;margin:20px auto;padding:0 18px}
h1{font-size:1.5rem;color:#80edc7}.panel{background:#12262d;border:1px solid #2e5158;border-radius:10px;padding:15px;margin:12px 0}
.controls{display:flex;flex-wrap:wrap;gap:14px;align-items:center}
label{display:flex;gap:5px;align-items:center}input[type=search]{min-width:160px}select,input{background:#16333b;color:#fff;border:1px solid #56777b;border-radius:4px;padding:6px}
svg{width:100%;height:230px;background:#0b1b22;border-radius:8px}
small{color:#aec2c4}.legend{display:flex;gap:20px;flex-wrap:wrap}
.wifi{color:#7cecaa}.ble{color:#78cff9}.lora{color:#ffd27b}
#map{height:300px}.tip{min-height:1.5em;color:#d2e5e7}table{border-collapse:collapse;width:100%}th,td{padding:6px;text-align:left;border-bottom:1px solid #375158}
</style></head><body><h1>CALLINGCARD / OFFLINE SURVEY</h1>
<p>Local HTML report · charted samples are observations, not unique radios, RF spectrum, or coverage guarantees.</p>
<div class="panel controls"><label>Session <select id="session"></select></label>
<label><input type="checkbox" class="radio" value="wifi" checked>Wi-Fi</label>
<label><input type="checkbox" class="radio" value="ble" checked>BLE</label>
<label><input type="checkbox" class="radio" value="lora" checked>LoRa</label>
<label>Name contains <input id="search" type="search" placeholder="SSID or BLE name" autocomplete="off"></label>
<label>Min RSSI (dBm) <input id="minRssi" type="number" min="-130" max="30" placeholder="All" style="width:75px"></label>
<label><input id="onlyLocated" type="checkbox">GNSS fix only</label>
<label>Timeline bin <select id="bins"><option value="5">5 sec</option><option value="15">15 sec</option><option value="60">60 sec</option></select></label></div>
<div class="panel"><h2>Selected session</h2><div id="stats"></div></div>
<div class="panel"><h2>Observation activity over uptime</h2><svg id="timeline" viewBox="0 0 900 230" role="img" aria-label="Per-radio observation count timeline"></svg><div class="tip" id="timelineTip"></div></div>
<div class="panel"><h2>RSSI over uptime (dBm)</h2><svg id="rssi" viewBox="0 0 900 230" role="img" aria-label="RSSI scatterplot"></svg><div class="tip" id="rssiTip"></div></div>
<div class="panel"><h2>Location-tagged observations — relative position plot</h2>
<p><small>Local coordinates only; this is NOT a street map. Longitude increases rightward, latitude upward. Missing GNSS coordinates are not plotted.</small></p>
<svg id="map" viewBox="0 0 900 300" role="img" aria-label="Latitude longitude observation scatterplot"></svg><div class="tip" id="mapTip"></div><div class="tip" id="bounds"></div></div>
<p><small>No external scripts, tiles, network connections, or embedded third-party services. This file contains potentially sensitive wireless names and precise positions; store and share carefully.</small></p>
<script id="data" type="application/json">__DATA__</script>
<script>
'use strict';
const records=JSON.parse(document.getElementById('data').textContent);
const colors={wifi:'#7cecaa',ble:'#78cff9',lora:'#ffd27b'};
const select=document.getElementById('session');
const files=[...new Set(records.map(r=>r.file))];
files.forEach(f=>{const o=document.createElement('option');o.value=f;o.textContent=f;select.appendChild(o)});
function chosen(){
const permitted=new Set([...document.querySelectorAll('.radio:checked')].map(e=>e.value));
const name=document.getElementById('search').value.trim().toLowerCase();
const minText=document.getElementById('minRssi').value.trim();
const minRssi=minText===''?null:Number(minText);
const located=document.getElementById('onlyLocated').checked;
return records.filter(r=>r.file===select.value && permitted.has(r.source) &&
 (!name||r.label.toLowerCase().includes(name)) &&
 (minRssi===null||(Number.isFinite(r.rssi)&&r.rssi>=minRssi)) &&
 (!located||r.lat!==null));
}
const svgNS='http://www.w3.org/2000/svg';
function element(svg,tag,attrs){let e=document.createElementNS(svgNS,tag);Object.entries(attrs).forEach(([k,v])=>e.setAttribute(k,String(v)));svg.appendChild(e);return e}
function clear(id, height){const svg=document.getElementById(id);svg.replaceChildren();element(svg,'line',{x1:45,y1:height-30,x2:880,y2:height-30,stroke:'#6a8993'});element(svg,'line',{x1:45,y1:20,x2:45,y2:height-30,stroke:'#6a8993'});return svg}
function extent(vals){let a=Math.min(...vals),b=Math.max(...vals);if(!vals.length)return [0,1];if(a===b)return [a-1,b+1];return [a,b]}
function plotXY(id,items,xFn,yFn,height,geo=false){
const svg=clear(id,height), tip=document.getElementById(id+'Tip');tip.textContent='';
if(!items.length){tip.textContent='No matching records.';if(geo)document.getElementById('bounds').textContent='No valid coordinates in current selection.';return}
const xs=items.map(xFn),ys=items.map(yFn),[x0,x1]=extent(xs),[y0,y1]=extent(ys);
if(geo){
const fmt=x=>Number(x).toFixed(6);
document.getElementById('bounds').textContent='Coordinate bounds: lat '+fmt(Math.min(...ys))+' to '+fmt(Math.max(...ys))+
' | lon '+fmt(Math.min(...xs))+' to '+fmt(Math.max(...xs))+' (WGS84 degrees; no basemap)';
for(let i=1;i<5;i++){
const x=45+i*825/5,y=height-30-i*(height-50)/5;
element(svg,'line',{x1:x,y1:20,x2:x,y2:height-30,stroke:'#28424b','stroke-dasharray':'3 4'});
element(svg,'line',{x1:45,y1:y,x2:880,y2:y,stroke:'#28424b','stroke-dasharray':'3 4'});
}
}
items.forEach(r=>{const x=45+(xFn(r)-x0)/(x1-x0)*825,y=height-30-(yFn(r)-y0)/(y1-y0)*(height-50);
const circle=element(svg,'circle',{cx:x,cy:y,r:3,fill:colors[r.source],opacity:0.72});
circle.addEventListener('mouseenter',()=>{tip.textContent=r.source+' / '+(r.utc||'UTC unavailable')+' / RSSI '+r.rssi+' dBm'+(geo?' / '+r.lat+', '+r.lon:'')+' / '+r.label});
});
}
function timeline(items){
const svg=clear('timeline',230), tip=document.getElementById('timelineTip');tip.textContent='';
if(!items.length){tip.textContent='No matching observations.';return}
const bin=+document.getElementById('bins').value*1000;
const buckets=new Map();for(const r of items){const n=Math.floor(r.t/bin);const key=n+'|'+r.source;buckets.set(key,(buckets.get(key)||0)+1)}
const times=items.map(r=>Math.floor(r.t/bin)),low=Math.min(...times),high=Math.max(...times);
const max=Math.max(1,...buckets.values()), span=Math.max(1,high-low+1),bw=Math.min(18,825/span/3);
for(const [key,count] of buckets){const [n,source]=key.split('|'),index=['wifi','ble','lora'].indexOf(source);
const x=45+(+n-low)*825/span+index*bw,y=200-(count/max)*175;
const rect=element(svg,'rect',{x,y,width:Math.max(1,bw-1),height:200-y,fill:colors[source]});
rect.addEventListener('mouseenter',()=>tip.textContent=source+': '+count+' observations in uptime '+(+n*bin/1000)+'s bin');
}}
function render(){
const r=chosen();let counts={wifi:0,ble:0,lora:0},utc=0,pos=0;
r.forEach(v=>{counts[v.source]++;if(v.utc)utc++;if(v.lat!==null)pos++});
document.getElementById('stats').textContent='Observations: '+r.length+' | Wi-Fi: '+counts.wifi+' | BLE: '+counts.ble+' | LoRa: '+counts.lora+' | UTC: '+utc+' | Positioned: '+pos;
timeline(r);
plotXY('rssi',r.filter(v=>Number.isFinite(v.rssi)),v=>v.t,v=>v.rssi,230);
plotXY('map',r.filter(v=>v.lat!==null),v=>v.lon,v=>v.lat,300,true);
}
select.addEventListener('change',render);
document.querySelectorAll('.radio,#bins,#onlyLocated').forEach(x=>x.addEventListener('change',render));
document.querySelectorAll('#search,#minRssi').forEach(x=>x.addEventListener('input',render));
render();
</script></body></html>"""


def build_records(paths, strict=False, limit=MAX_POINTS):
    # Evenly bound large sessions via simple deterministic stride in a second pass.
    total = sum(1 for _ in read_observations(paths, strict))
    stride = max(1, (total + limit - 1) // limit)
    result = []
    for index, (path, obj) in enumerate(read_observations(paths, strict)):
        if index % stride:
            continue
        loc = coordinates(obj)
        uptime = obj.get("uptime_ms")
        rssi = obj.get("rssi_dbm")
        if not isinstance(uptime, (int, float)) or isinstance(uptime, bool):
            continue
        if not isinstance(rssi, (int, float)) or isinstance(rssi, bool):
            rssi = None
        if isinstance(rssi, float) and not (-100000 < rssi < 100000):
            rssi = None
        result.append({"file": path.name, "source": obj["source"], "t": uptime,
                       "rssi": rssi, "utc": obj.get("utc") if isinstance(obj.get("utc"), str) else None,
                       "lat": loc[0] if loc else None, "lon": loc[1] if loc else None,
                       "label": str(obj.get("label") or "")[:64]})
    return result, total, stride


def render_html(records):
    # Avoid terminating the embedded JSON script or allowing HTML injection.
    payload = json.dumps(records, ensure_ascii=True, allow_nan=False)
    payload = payload.replace("<", "\\u003c").replace(">", "\\u003e").replace("&", "\\u0026")
    return PAGE.replace("__DATA__", payload)


def main(argv=None):
    ap = argparse.ArgumentParser(description="Generate self-contained offline Callingcard HTML viewer")
    ap.add_argument("inputs", type=Path, nargs="+")
    ap.add_argument("--output", type=Path, required=True)
    ap.add_argument("--strict", action="store_true")
    args = ap.parse_args(argv)
    for item in args.inputs:
        if not item.is_file():
            ap.error("missing file: "+str(item))
    try:
        records,total,stride=build_records(args.inputs,args.strict)
        args.output.write_text(render_html(records),encoding="utf-8")
    except (OSError,ValueError) as exc:
        ap.exit(1,"error: "+str(exc)+"\n")
    print("Wrote "+str(args.output)+" using "+str(len(records))+" of "+str(total)+" records (stride "+str(stride)+")")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
