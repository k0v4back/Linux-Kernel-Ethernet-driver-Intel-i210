# Intel I210 Linux Network Driver

## 1. Build and Load Driver

### Build

Clean and build the driver:

``` bash
make clean
make all
```

Check the generated module information:

``` bash
modinfo ./i210_driver.ko | grep -E 'filename|name|alias'
```

### Identify the PCI device

List PCI devices:

``` bash
lspci -nn
```

Find the kernel driver currently handling the Intel I210 device:

``` bash
lspci -k -s 02:00.0
```

Example:

``` text
02:00.0 Ethernet controller: Intel Corporation I210 Gigabit Network Connection
    Kernel driver in use: igb
    Kernel modules: igb
```

### Unbind the original driver

Before loading the custom driver, unbind the device from the original
`igb` driver:

``` bash
sudo sh -c 'echo 0000:02:00.0 > /sys/bus/pci/drivers/igb/unbind'
```

### Load the custom driver

``` bash
sudo insmod ./i210_driver.ko
```

Check kernel messages:

``` bash
sudo dmesg -w
```

Check whether the device is now handled by the custom driver:

``` bash
lspci -k -s 02:00.0
```

Check the network interface:

``` bash
ip link show
```

The driver registers the network interface as `eth0`.

### Unload the driver

Bring the interface down and unload the module:

``` bash
sudo ip link set eth0 down
sudo rmmod i210_driver
```

To restore the original driver:

``` bash
sudo modprobe igb
```

------------------------------------------------------------------------

## 2. Network Configuration and Monitoring

### Configure a static IP address

For testing, a static IPv4 address can be assigned directly:

``` bash
sudo ip addr add 10.123.45.1/24 dev eth0
```

Remove the address:

``` bash
sudo ip addr del 10.123.45.1/24 dev eth0
```

Check the interface configuration:

``` bash
ip addr show dev eth0
```

### NetworkManager configuration (optional)

If NetworkManager manages `eth0`, it may attempt DHCP configuration and
repeatedly change the interface state.

For testing, a static IPv4 profile can be configured:

``` bash
sudo nmcli connection modify "Wired connection 1" \
    ipv4.method manual \
    ipv4.addresses 10.123.45.1/24 \
    ipv4.gateway "" \
    ipv4.dns "" \
    ipv6.method link-local \
    connection.autoconnect yes
```

Activate the profile:

``` bash
sudo nmcli connection up "Wired connection 1"
```

Check the result:

``` bash
nmcli device status
ip -4 addr show dev eth0
```

Replace `"Wired connection 1"` with the actual NetworkManager connection
name if necessary.

### Monitor network events

Monitor IP address changes:

``` bash
ip -ts monitor address dev eth0
```

Monitor interface state changes:

``` bash
ip -ts monitor link dev eth0
```

Check interface statistics:

``` bash
ip -s link show dev eth0
```

Check link speed and duplex mode:

``` bash
sudo ethtool eth0
```

------------------------------------------------------------------------

## 3. Packet Transmission Tests

The driver can be tested using two computers connected through Ethernet.

-   **PC1 (Host):** A computer with a standard Linux Ethernet driver.
-   **PC2 (I210):** A computer running the custom Intel I210 driver.

The following IP configuration is used:

  Device       Interface     IPv4 address
  ------------ ------------- ------------------
  PC1 (Host)   `enp0s31f6`   `10.123.45.2/24`
  PC2 (I210)   `eth0`        `10.123.45.1/24`

### 3.1 ARP and ICMP test

First, configure the PC1 interface:

``` bash
sudo ip addr add 10.123.45.2/24 dev enp0s31f6
```

On PC1, start `tcpdump` to capture ARP and ICMP packets:

``` bash
sudo tcpdump -ni enp0s31f6 -e -vv 'arp or icmp'
```

On PC2, load the driver and configure the interface:

``` bash
sudo insmod ./i210_driver.ko
sudo ip link set eth0 up
sudo ip addr add 10.123.45.1/24 dev eth0
```

Send ICMP Echo Requests from PC2 to PC1:

``` bash
ping -I eth0 -c 10 10.123.45.2
```

The packet capture on PC1 shows the following sequence:

1.  PC2 sends an ARP Request to resolve the MAC address of
    `10.123.45.2`.
2.  PC1 responds with an ARP Reply.
3.  PC2 sends an ICMP Echo Request.
4.  PC1 responds with an ICMP Echo Reply.

Example `tcpdump` output:

``` text
c4:62:37:03:18:ad > ff:ff:ff:ff:ff:ff,
    ethertype ARP (0x0806):
    Request who-has 10.123.45.2 tell 10.123.45.1

e4:b9:7a:56:48:46 > c4:62:37:03:18:ad,
    ethertype ARP (0x0806):
    Reply 10.123.45.2 is-at e4:b9:7a:56:48:46

c4:62:37:03:18:ad > e4:b9:7a:56:48:46,
    ethertype IPv4 (0x0800):
    10.123.45.1 > 10.123.45.2: ICMP echo request

e4:b9:7a:56:48:46 > c4:62:37:03:18:ad,
    ethertype IPv4 (0x0800):
    10.123.45.2 > 10.123.45.1: ICMP echo reply
```

This test verifies ARP processing, Ethernet frame transmission and
reception, and ICMP packet delivery through the Linux network stack.

### 3.2 TCP throughput test (iperf3)

`iperf3` is used to measure TCP throughput between the two computers.

On PC1 (Host), start the server:

``` bash
iperf3 -s
```

On PC2 (I210), run the client:

``` bash
iperf3 -c 10.123.45.2 -t 10
```

Example output from PC2 (I210):

``` text
Connecting to host 10.123.45.2, port 5201

[ ID] Interval           Transfer     Bitrate         Retr
[  5]   0.00-1.00 sec    113 MBytes   949 Mbits/sec    1
[  5]   1.00-2.00 sec    112 MBytes   942 Mbits/sec    0
[  5]   2.00-3.00 sec    112 MBytes   938 Mbits/sec    0
[  5]   3.00-4.00 sec    112 MBytes   941 Mbits/sec    0
[  5]   4.00-5.00 sec    112 MBytes   943 Mbits/sec    0
[  5]   5.00-6.00 sec    112 MBytes   941 Mbits/sec    0
[  6]   6.00-7.00 sec    113 MBytes   947 Mbits/sec    0
[  7]   7.00-8.00 sec    112 MBytes   941 Mbits/sec    0
[  8]   8.00-9.00 sec    112 MBytes   941 Mbits/sec    0
[  9]   9.00-10.00 sec   112 MBytes   939 Mbits/sec    0

[ ID] Interval           Transfer     Bitrate
[  5]   0.00-10.00 sec   1.10 GBytes  943 Mbits/sec  sender
[  5]   0.00-10.00 sec   1.10 GBytes  941 Mbits/sec  receiver
```

The test achieved approximately **941 Mbit/s TCP throughput**.

This result demonstrates that the driver can handle a continuous TCP
data stream at a rate close to the practical throughput limit of Gigabit
Ethernet.

------------------------------------------------------------------------

## 4. Linux Kernel Indexing with clangd

The Linux kernel source can be indexed by `clangd` to provide code
navigation, symbol lookup, and completion when developing the driver.

### 4.1 Clone and configure the kernel

Clone the Linux kernel repository and configure/build the kernel.

### 4.2 Generate compilation database

From the Linux kernel source directory, run:

``` bash
python3 scripts/clang-tools/gen_compile_commands.py
```

This generates `compile_commands.json`, which contains compilation
commands for kernel source files.

### 4.3 Configure clangd in VS Code

Open the VS Code user settings file:

``` bash
~/.config/Code/User/settings.json
```

Add the following configuration:

``` json
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

Replace the compilation database path with the actual Linux kernel
source directory.

### 4.4 Restart clangd

In VS Code, open the command palette:

``` text
Ctrl+Shift+P -> clangd: Restart language server
```

After indexing, clangd can navigate Linux kernel definitions and
references directly from the driver source code.