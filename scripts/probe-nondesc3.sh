#!/bin/bash
cd $HOME/edk2-msm
F=Common/edk2/MdeModulePkg/Bus/Pci/NonDiscoverablePciDeviceDxe/NonDiscoverablePciDeviceDxe.c
sed -n '55,130p' "$F" | cat -n
