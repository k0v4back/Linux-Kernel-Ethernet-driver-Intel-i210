### Build and Load driver

1. Clean and build
```
make clean
make all
```

2. Check modinfo
```
modinfo ./i210_driver.ko | grep -E 'filename|name|alias'
```

3. Check pci devices
```
lspci -nn
```

4. Show kernel driver which handling pci device
```
lspci -k
```

5. Unbind kernel driver 
```
sudo ip link set enp2s0 down
```
```
sudo sh -c 'echo 0000:02:00.0 > /sys/bus/pci/drivers/igb/unbind'
```

6. Load new driver
```
sudo insmod i210_driver.ko
```

7. Show log
```
dmesg | tail -n20
```

8. Set up interface
```
sudo ip link set eth0 up
```

9. Check ip route
```
ip route get 192.168.1.1
```

10. Through a specific interface
```
sudo ping -I eth0 192.168.1.1
```


### Indexing linux kernel and use compile commands at the project

1. Clone and compile linux kernel

2. Call `./scripts/clang-tools/gen_compile_commands.py`

3. Write to settings.json(~/.config/Code/User/settings.json)
```
{
    "clangd.arguments": [
        "-j", "8",
        "--compile-commands-dir=/home/k0v4_2/linux_kernel_for_vs_code",
        "--background-index",
        "--pch-storage=memory",
        "--header-insertion=never"
    ]
}
```

4. Restart clangd
```
Ctrl+Shift+P -> clangd: Restart language server
```


### Some functions

1. After `ip link set eth0 up`
``
eth0 -> net_device -> i210_netdev_ops -> i210_open
```

2. After `ip link set eth0 down`
``
eth0 -> net_device -> i210_netdev_ops -> i210_open
```