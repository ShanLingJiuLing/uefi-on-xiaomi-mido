#!/bin/bash
set -e
P=http://192.168.137.1:7897
git config --global http.proxy $P
git config --global https.proxy $P
git config --global core.compression 0
git config --global http.postBuffer 524288000
echo "===== git proxy configured ====="
git config --global --get http.proxy
cd $HOME
if [ -d edk2-msm/.git ]; then
  echo "already cloned"
else
  echo "===== cloning edk2-msm (shallow, recursive) ====="
  time git clone --depth 1 --recurse-submodules --shallow-submodules \
    https://github.com/edk2-porting/edk2-msm edk2-msm 2>&1 | tail -30
fi
echo "===== result ====="
ls -la $HOME/edk2-msm | head -20
du -sh $HOME/edk2-msm 2>/dev/null
df -h / | tail -1
