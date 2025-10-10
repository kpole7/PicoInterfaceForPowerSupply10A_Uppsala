#!/usr/bin/env bash
SSH_CMD='ssh -X cyklotron@192.168.201.247'
gnome-terminal -- bash -lc "$SSH_CMD; exec bash"

