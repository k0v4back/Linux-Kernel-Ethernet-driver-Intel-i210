### Load driver

1. Check pci devices
```
lspci -nn
```

2. Show kernel driver which handling pci device
```
lspci -k
```

3. Unbind kernel driver 
```
sudo ip link set enp2s0 down
```
```
sudo sh -c 'echo 0000:02:00.0 > /sys/bus/pci/drivers/igb/unbind'
```

4. Load new driver
```
sudo insmod my_i210_driver.ko
```

5. Show log
```
dmesg | tail -n20
```