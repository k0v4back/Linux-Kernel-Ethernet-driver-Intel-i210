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



### Some functions

1. After `ip link set eth0 up`
```
eth0 -> net_device -> i210_netdev_ops -> i210_open
```

2. After `ip link set eth0 down`
```
eth0 -> net_device -> i210_netdev_ops -> i210_open
```



### Fix the NetworkManager profile
For eth0 set static IPv4 in the NetworkManager, it does not delete the address after a failed DHCP
```
sudo nmcli connection modify "Wired connection 1" \
    ipv4.method manual \
    ipv4.addresses 10.123.45.1/24 \
    ipv4.gateway "" \
    ipv4.dns "" \
    ipv6.method link-local \
    connection.autoconnect yes
```

Activate the profile:
```
sudo nmcli connection up "Wired connection 1"
```

Check result:
```
nmcli device status
ip -4 addr show dev eth0
```



### Test monitoring
Monitoring address changes:
```
ip -ts monitor address dev eth0
```

Interface status monitoring:
```
ip -ts monitor link dev eth0
```



### Send ARP

ARP operation is visible in this test, computers are directly connected via an ethernet cable

PC2(local working computer):
1. Add addr to interface
```
sudo ip addr add 10.123.45.2/24 dev enp0s31f6
```

2. Start tcpdump
```
sudo tcpdump -eni enp0s31f6 'arp or ether host c4:62:37:03:18:ad'
```


PC1(compute with i210):
1. Insmod driver
```
sudo insmod i210_driver.ko
```

2. Add addr to interface eth0
```
sudo ip addr add 10.123.45.1/24 dev eth0
```

3. Ping PC2
```
ping -I eth0 10.123.45.2
```


At tcpdump PC2(request and reply):
```
//...
23:30:03.506754 c4:62:37:03:18:ad > ff:ff:ff:ff:ff:ff, ethertype ARP (0x0806), length 60: Request who-has 10.123.45.2 tell 10.123.45.1, length 46
23:30:03.506814 e4:b9:7a:56:48:46 > c4:62:37:03:18:ad, ethertype ARP (0x0806), length 42: Reply 10.123.45.2 is-at e4:b9:7a:56:48:46, length 28
//...
```



### Ping example
PC2:
```
$ sudo ip addr add 10.123.45.2/24 dev enp0s31f6

$ ip a                                                                   INT 
//...
2: enp0s31f6: <BROADCAST,MULTICAST,UP,LOWER_UP> mtu 1500 qdisc pfifo_fast state UP group default qlen 1000
    link/ether e4:b9:7a:56:48:46 brd ff:ff:ff:ff:ff:ff
    inet 10.123.45.2/24 scope global enp0s31f6
       valid_lft forever preferred_lft forever
//..

$ sudo tcpdump -ni enp0s31f6 -e -vv 'arp or icmp'                            07:19:33 PM
tcpdump: listening on enp0s31f6, link-type EN10MB (Ethernet), snapshot length 262144 bytes
19:19:35.965185 c4:62:37:03:18:ad > ff:ff:ff:ff:ff:ff, ethertype ARP (0x0806), length 60: Ethernet (len 6), IPv4 (len 4), Request who-has 10.123.45.2 tell 10.123.45.1, length 46
19:19:35.965233 e4:b9:7a:56:48:46 > c4:62:37:03:18:ad, ethertype ARP (0x0806), length 42: Ethernet (len 6), IPv4 (len 4), Reply 10.123.45.2 is-at e4:b9:7a:56:48:46, length 28
19:19:36.988410 c4:62:37:03:18:ad > ff:ff:ff:ff:ff:ff, ethertype ARP (0x0806), length 60: Ethernet (len 6), IPv4 (len 4), Request who-has 10.123.45.2 tell 10.123.45.1, length 46
19:19:36.988444 e4:b9:7a:56:48:46 > c4:62:37:03:18:ad, ethertype ARP (0x0806), length 42: Ethernet (len 6), IPv4 (len 4), Reply 10.123.45.2 is-at e4:b9:7a:56:48:46, length 28
19:19:36.988949 c4:62:37:03:18:ad > e4:b9:7a:56:48:46, ethertype IPv4 (0x0800), length 98: (tos 0x0, ttl 64, id 18716, offset 0, flags [DF], proto ICMP (1), length 84)
    10.123.45.1 > 10.123.45.2: ICMP echo request, id 5, seq 1, length 64
19:19:36.989058 e4:b9:7a:56:48:46 > c4:62:37:03:18:ad, ethertype IPv4 (0x0800), length 98: (tos 0x0, ttl 64, id 18450, offset 0, flags [none], proto ICMP (1), length 84)
    10.123.45.2 > 10.123.45.1: ICMP echo reply, id 5, seq 1, length 64
19:19:36.989293 c4:62:37:03:18:ad > e4:b9:7a:56:48:46, ethertype IPv4 (0x0800), length 98: (tos 0x0, ttl 64, id 18931, offset 0, flags [DF], proto ICMP (1), length 84)
    10.123.45.1 > 10.123.45.2: ICMP echo request, id 5, seq 2, length 64
19:19:36.989325 e4:b9:7a:56:48:46 > c4:62:37:03:18:ad, ethertype IPv4 (0x0800), length 98: (tos 0x0, ttl 64, id 18451, offset 0, flags [none], proto ICMP (1), length 84)
    10.123.45.2 > 10.123.45.1: ICMP echo reply, id 5, seq 2, length 64
19:19:37.990356 c4:62:37:03:18:ad > e4:b9:7a:56:48:46, ethertype IPv4 (0x0800), length 98: (tos 0x0, ttl 64, id 18964, offset 0, flags [DF], proto ICMP (1), length 84)
```

PC1(I210):
```
$ sudo insmod i210_driver.ko 
$ sudo ip link set eth0 up
$ sudo ip addr add 10.123.45.1/24 dev eth0
$ ping -I eth0 -c 10 10.123.45.2
PING 10.123.45.2 (10.123.45.2) from 10.123.45.1 eth0: 56(84) bytes of data.
64 bytes from 10.123.45.2: icmp_seq=1 ttl=64 time=1024 ms
64 bytes from 10.123.45.2: icmp_seq=2 ttl=64 time=0.907 ms
64 bytes from 10.123.45.2: icmp_seq=3 ttl=64 time=1014 ms
64 bytes from 10.123.45.2: icmp_seq=4 ttl=64 time=1024 ms
64 bytes from 10.123.45.2: icmp_seq=5 ttl=64 time=1024 ms
64 bytes from 10.123.45.2: icmp_seq=6 ttl=64 time=1024 ms
64 bytes from 10.123.45.2: icmp_seq=7 ttl=64 time=1024 ms
64 bytes from 10.123.45.2: icmp_seq=8 ttl=64 time=0.367 ms
```



### Iperf3 example
PC1(I210):
```
$ sudo insmod i210_driver.ko 
$ sudo ip link set eth0 up
$ sudo ip addr add 10.123.45.1/24 dev eth0
$ iperf3 -c 10.123.45.2 -t 10
Connecting to host 10.123.45.2, port 5201
[  5] local 10.123.45.1 port 45976 connected to 10.123.45.2 port 5201
[ ID] Interval           Transfer     Bitrate         Retr  Cwnd
[  5]   0.00-1.00   sec  29.0 MBytes   243 Mbits/sec    0    134 KBytes       
[  5]   1.00-2.04   sec  30.0 MBytes   243 Mbits/sec    0    134 KBytes       
[  5]   2.04-3.03   sec  28.8 MBytes   243 Mbits/sec    0    134 KBytes       
[  5]   3.03-4.04   sec  28.8 MBytes   240 Mbits/sec    0    134 KBytes       
[  5]   4.04-5.03   sec  28.8 MBytes   243 Mbits/sec    0    134 KBytes       
[  5]   5.03-6.01   sec  27.5 MBytes   234 Mbits/sec    0    297 KBytes       
[  5]   6.01-7.01   sec  28.8 MBytes   243 Mbits/sec    0    355 KBytes       
[  5]   7.01-8.04   sec  30.0 MBytes   243 Mbits/sec    0    355 KBytes       
[  5]   8.04-9.03   sec  28.8 MBytes   243 Mbits/sec    0    355 KBytes       
[  5]   9.03-10.02  sec  28.8 MBytes   243 Mbits/sec    0    355 KBytes       
- - - - - - - - - - - - - - - - - - - - - - - - -
[ ID] Interval           Transfer     Bitrate         Retr
[  5]   0.00-10.02  sec   289 MBytes   242 Mbits/sec    0             sender
[  5]   0.00-10.02  sec   289 MBytes   242 Mbits/sec                  receiver
```

PC2:
```
iperf3 -s                                                              INT 08:17:59 PM
-----------------------------------------------------------
Server listening on 5201 (test #1)
-----------------------------------------------------------
Accepted connection from 10.123.45.1, port 45962
[  5] local 10.123.45.1 port 53120 connected to 10.123.45.2 port 5201
[ ID] Interval           Transfer     Bitrate         Retr  Cwnd
[  5]   0.00-1.00   sec   114 MBytes   953 Mbits/sec    1    242 KBytes       
[  5]   1.00-2.00   sec   112 MBytes   941 Mbits/sec    0    256 KBytes       
[  5]   2.00-3.00   sec   112 MBytes   942 Mbits/sec    0    259 KBytes       
[  5]   3.00-4.00   sec   112 MBytes   937 Mbits/sec    0    260 KBytes       
[  5]   4.00-5.00   sec   113 MBytes   946 Mbits/sec    0    262 KBytes       
[  5]   5.00-6.00   sec   112 MBytes   941 Mbits/sec    0    264 KBytes       
[  5]   6.00-7.00   sec   112 MBytes   940 Mbits/sec    0    264 KBytes       
[  5]   7.00-8.00   sec   112 MBytes   942 Mbits/sec    0    264 KBytes       
[  5]   8.00-9.00   sec   112 MBytes   943 Mbits/sec    0    393 KBytes       
[  5]   9.00-10.00  sec   113 MBytes   944 Mbits/sec    0    393 KBytes       
- - - - - - - - - - - - - - - - - - - - - - - - -
[ ID] Interval           Transfer     Bitrate         Retr
[  5]   0.00-10.00  sec  1.10 GBytes   943 Mbits/sec    1             sender
[  5]   0.00-10.00  sec  1.10 GBytes   941 Mbits/sec                  receiver
- - - - - - - - - - - - - - - - - - - - - - - - -
[ ID] Interval           Transfer     Bitrate
[  5]   0.00-10.02  sec   289 MBytes   242 Mbits/sec                  receiver
-----------------------------------------------------------
Server listening on 5201 (test #2)
-----------------------------------------------------------
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
