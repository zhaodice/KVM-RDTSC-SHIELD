#!/bin/sh
cd $(pwd)
make
cp ./build/KVM-RDTSC-SHIELD.ko /root/KVM-RDTSC-SHIELD.ko
rmmod KVM-RDTSC-SHIELD
insmod /root/KVM-RDTSC-SHIELD.ko tsc_scale=100 tsc_reset=800
echo 1 > /dev/rdtsc
