#!/bin/bash 

KEY='/home/pranav/.ssh/id.pranav'
LOCATION_LOCAL=/home/pranav/Desktop/Courses/CSC\ 501/Projects/Project\ 2/code/
LOCATION_REMOTE_VCL=/home/pmgaikwa/csc501/
LOCATION_REMOTE_VBX=/home/pranav/Desktop/Project2/

USER_REMOTE_VCL=pmgaikwa
USER_REMOTE_VBX=pranav

# ADDR_REMOTE_VCL='152.46.17.30'
ADDR_REMOTE_VBX='10.152.7.207'
# ADDR_REMOTE_VBX='192.168.10.231'

# rsync -rltvz -e "ssh -o ConnectTimeout=10 -i ${KEY}" --progress "$LOCATION_LOCAL" "$USER_REMOTE_VCL"@"$ADDR_REMOTE_VCL":"$LOCATION_REMOTE_VCL"

rsync -O -rltvz -e "ssh -o ConnectTimeout=10 -i ${KEY}" --no-perms --progress "$LOCATION_LOCAL" "$USER_REMOTE_VBX"@"$ADDR_REMOTE_VBX":"$LOCATION_REMOTE_VBX"
