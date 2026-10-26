#!/usr/bin/env python3
"""Analyze serial monitor logs from a Callingcard field trial (no dependencies)."""
import argparse
import json
import re
from pathlib import Path

INTEGER = re.compile(r"([a-z_]+)=(-?\d+)")
def analyze(lines):
    metrics={"wifi_cycles":0,"ble_cycles":0,"passive_windows":0,
             "wifi_timeouts":0,"ble_timeouts":0,"passive_failures":0,
             "blade_connects":0,"blade_disconnects":0,
             "blade_connected_status":0,"blade_notification_attempts_max":None,
             "ble_prequeue_dropped_max":None,"pipeline_dropped_max":None,
             "minimum_heap_bytes":None,"samples_with_heap":0,
             "candidate_log_lines":0}
    wifi_durations=[];ble_durations=[];connections=[]
    for line in lines:
        if "CYCLE mode=wifi count=" in line:
            metrics["wifi_cycles"]+=1
            fields=dict((key,int(value)) for key,value in INTEGER.findall(line))
            if "duration_ms" in fields:wifi_durations.append(fields["duration_ms"])
        if "CYCLE mode=ble unique=" in line:
            metrics["ble_cycles"]+=1
            fields=dict((key,int(value)) for key,value in INTEGER.findall(line))
            if "duration_ms" in fields:ble_durations.append(fields["duration_ms"])
            if "prequeue_dropped" in fields:
                v=fields["prequeue_dropped"]
                prev=metrics["ble_prequeue_dropped_max"]
                metrics["ble_prequeue_dropped_max"]=v if prev is None else max(v,prev)
        if "PASSIVE window=" in line:metrics["passive_windows"]+=1
        if "CYCLE mode=wifi timeout=1" in line:metrics["wifi_timeouts"]+=1
        if "CYCLE mode=ble timeout=1" in line:metrics["ble_timeouts"]+=1
        if "PASSIVE " in line and ("start_failed=1" in line or "channel_failed=" in line):
            metrics["passive_failures"]+=1
        if "BLADE connection_changed=1" in line:
            metrics["blade_connects"]+=1;connections.append("connected")
        if "BLADE connection_changed=0" in line:
            metrics["blade_disconnects"]+=1;connections.append("disconnected")
        if "BLADE connected=1 notify=" in line:
            metrics["blade_connected_status"]+=1
            match=re.search(r"attempts=(\d+)",line)
            if match:
                v=int(match.group(1))
                old=metrics["blade_notification_attempts_max"]
                metrics["blade_notification_attempts_max"]=v if old is None else max(old,v)
        if "DETECT candidate=" in line:metrics["candidate_log_lines"]+=1
        if "PIPE queue=" in line:
            match=re.search(r"dropped=(\d+)",line)
            if match:
                v=int(match.group(1))
                old=metrics["pipeline_dropped_max"]
                metrics["pipeline_dropped_max"]=v if old is None else max(old,v)
        match=re.search(r"\bheap=(\d+)",line)
        if match:
            v=int(match.group(1));metrics["samples_with_heap"]+=1
            old=metrics["minimum_heap_bytes"]
            metrics["minimum_heap_bytes"]=v if old is None else min(old,v)
    metrics["wifi_duration_ms_max"]=max(wifi_durations,default=None)
    metrics["ble_duration_ms_max"]=max(ble_durations,default=None)
    metrics["connection_events"]=connections
    metrics["limits"]="Counters reflect log coverage; lost serial lines/reboots can change cumulative values."
    metrics["notification_delivery"]="not measured: attempts are not acknowledged deliveries"
    metrics["gps_accuracy"]="not measured: requires independent ground truth"
    metrics["false_positive_rate"]="not measured: requires labeled field observations"
    return metrics

def main():
    p=argparse.ArgumentParser(description=__doc__)
    p.add_argument("serial_log",type=Path)
    p.add_argument("--output",type=Path,default=Path("field-health.json"))
    args=p.parse_args()
    with args.serial_log.open(encoding="utf-8",errors="replace") as stream:
        result=analyze(stream)
    args.output.write_text(json.dumps(result,indent=2)+"\n",encoding="utf-8")
    print(f"Wi-Fi={result['wifi_cycles']} BLE={result['ble_cycles']} passive={result['passive_windows']}; "
          f"disconnects={result['blade_disconnects']} min_heap={result['minimum_heap_bytes']}")
if __name__=="__main__":
    main()
