#!/bin/bash
gnome-terminal --title="RSTL master" -- bash -c \
        "cd ~/LocalSoftware/RSTL_protocol_master/RSTL_simulator && \
         ./RSTL_master /dev/ttyUSB0; \
         exec bash"

