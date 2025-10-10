#!/usr/bin/env bash
gnome-terminal -- bash -lc "ssh -t -X cyklotron@192.168.201.247 \"nohup cutecom >/dev/null 2>&1 </dev/null & exec bash -l\""

