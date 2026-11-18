#!/bin/bash
echo "=== mido: storage probe ==="
connect -r
map -r
echo "--- MmcProbe ---"
MmcProbe.efi
echo "--- w0..w23 ---"
dmem 0x9F310000 0x60
echo "--- w24 page_reserved / w28..w31 bind markers ---"
dmem 0x9F310060 0x10
echo "--- w32 v4enh / w33 clksupply / w34 ctrlver / w35 baseclk ---"
dmem 0x9F310080 0x10
echo "=== done ==="
