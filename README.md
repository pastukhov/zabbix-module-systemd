# Native Zabbix systemd monitoring

zabbix-module-systemd is a loadable Zabbix module that enables Zabbix to query
the systemd D-Bus API for native and granular system state monitoring.

## Download

The following packages are available:

- [Sources](http://s3.cavaliercoder.com/zabbix-contrib/release/zabbix-module-systemd-1.2.0.tar.gz)
- [Zabbix 3.2 - EL7 x86_64 RPM](http://s3.cavaliercoder.com/zabbix-contrib/rhel/7/x86_64/zabbix-module-systemd-1.2.0-1.x86_64.rpm)

## Install

```bash
$ ./configure --with-zabbix=/usr/src/zabbix-3.2.5
$ make
$ sudo make install
```

If you are using a packaged version of Zabbix, you may with to redirect the
installation directories as follows:

```bash
$ sudo make prefix=/usr sysconfdir=/etc libdir=/usr/lib64 install
```

## Available keys

Note: `systemd.cgroup.*` keys require cgroup accounting. The system default
for this setting may be controlled with `Default*Accounting` settings in
[systemd-system.conf](https://www.freedesktop.org/software/systemd/man/systemd-system.conf.html).

Example how to enable cgroup accounting:

```bash
$ sed -i -e "s/.*DefaultCPUAccounting=.*/DefaultCPUAccounting=yes/g" /etc/systemd/system.conf
$ sed -i -e "s/.*DefaultBlockIOAccounting=.*/DefaultBlockIOAccounting=yes/g" /etc/systemd/system.conf
$ sed -i -e "s/.*DefaultMemoryAccounting=.*/DefaultMemoryAccounting=yes/g" /etc/systemd/system.conf
$ systemctl daemon-reexec
```

| Key | Description |
| ------------------------------ | ----------- |
| **systemd[\<property\>]** | Return the given property of the systemd Manager interface. |
| **systemd.unit[unit,\<interface\>,\<property\>]** | Return the given property of the given interface of the given unit name. For a list of available unit interfaces and properties, see the [D-Bus API of systemd/PID 1](https://www.freedesktop.org/wiki/Software/systemd/dbus). |
| **systemd.unit.discovery[\<type\>]** | Discovery all known units of the given type (default: `all`). |
| **systemd.service.info[service,\<param\>]** | Query various service stats, similar to `service.info` on the Windows agent. |
| **systemd.service.discovery[]** | Discovery all known services. |
| **systemd.cgroup.cpu[\<unit\>,\<cmetric\>]** | **CPU metrics:**<br>**cmetric** - any available CPU metric in the pseudo-file cpuacct.stat/cpu.stat, e.g.: *system, user, total (current sum of system/user* or cgroup [throttling metrics](https://access.redhat.com/documentation/en-US/Red_Hat_Enterprise_Linux/6/html/Resource_Management_Guide/sec-cpu.html): *nr_throttled, throttled_time*<br>Note: CPU user/system/total metrics must be recalculated to % utilization value by Zabbix - *Delta (speed per second)*. |
| **systemd.cgroup.dev[\<unit\>,\<bfile\>,\<bmetric\>]** | **Blk IO metrics:**<br>**bfile** - cgroup blkio pseudo-file, e.g.: *blkio.io_merged, blkio.io_queued, blkio.io_service_bytes, blkio.io_serviced, blkio.io_service_time, blkio.io_wait_time, blkio.sectors, blkio.time, blkio.avg_queue_size, blkio.idle_time, blkio.dequeue, ...*<br>**bmetric** - any available blkio metric in selected pseudo-file, e.g.: *Total*. Option for selected block device only is also available e.g. *'8:0 Sync'* (quotes must be used in key parameter in this case)<br>Note: Some pseudo blkio files are available only if kernel config *CONFIG_DEBUG_BLK_CGROUP=y*. |
| **systemd.cgroup.mem[\<unit\>,\<mmetric\>]** | **Memory metrics:**<br>**mmetric** - any available memory metric in the pseudo-file memory.stat, e.g.: *cache, rss, mapped_file, pgpgin, pgpgout, swap, pgfault, pgmajfault, inactive_anon, active_anon, inactive_file, active_file, unevictable, hierarchical_memory_limit, hierarchical_memsw_limit, total_cache, total_rss, total_mapped_file, total_pgpgin, total_pgpgout, total_swap, total_pgfault, total_pgmajfault, total_inactive_anon, total_active_anon, total_inactive_file, total_active_file, total_unevictable*, Note: if you have problem with memory metrics, be sure that memory cgroup subsystem is enabled - kernel parameter: *cgroup_enable=memory* |
| **systemd.modver[]** | Version of the loaded systemd module. |

## Cgroup metrics

To support cgroup metrics you need to enable CPU/mem/... accounting in ``` /etc/systemd/system.conf ```

```
CPUAccounting=yes
BlockIOAccounting=yes
MemoryAccounting=yes
```
After that you need to reboot host or do ``` systemctl daemon-reexec && systemctl restart zabbix-agent ```

## Examples

```bash
# return the system architecture
$ zabbix_get -k systemd[Architecture]
x86-64

# discover all units - filtering for sockets
$ zabbix_get -k systemd.unit.discovery[socket]
{
  "data": [
    {
      "{#UNIT.NAME}": "dbus.socket",
      "{#UNIT.DESCRIPTION}": "D-Bus System Message Bus Socket",
      "{#UNIT.LOADSTATE}": "loaded",
      "{#UNIT.ACTIVESTATE}": "active",
      "{#UNIT.SUBSTATE}": "running",
      "{#UNIT.OBJECTPATH}": "/org/freedesktop/systemd1/unit/dbus_2esocket",
      "{#UNIT.FRAGMENTPATH}": "/usr/lib/systemd/system/dbus.socket",
      "{#UNIT.UNITFILESTATE}": "static"
    },
    ...
  ]
}

# return the location of a mount unit
$ zabbix_get -k systemd.unit[dev-mqueue.mount,Mount,Where]
/dev/mqueue

# return the number of open connections on a socket unit
$ zabbix_get -k systemd.unit[dbus.socket,Socket,NConnections]
1

# discover all services
$ zabbix_get -k systemd.service.discovery[service]
{
  "data": [
    {
      "{#SERVICE.TYPE}": "service",
      "{#SERVICE.NAME}": "dbus.service",
      "{#SERVICE.DISPLAYNAME}": "D-Bus System Message Bus",
      "{#SERVICE.PATH}": "/usr/lib/systemd/system/dbus.service",
      "{#SERVICE.STARTUPNAME}": "static"
    },
    ...
  ]
}

# return the state of a service as an integer
$ zabbix_get -k systemd.service.info[sshd]
0

# return the startup state of a service as an integer
$ zabbix_get -k systemd.service.info[sshd,startup]
0

# total cpu usage of dbus.service
$ zabbix_get -k systemd.cgroup.cpu[dbus.service,total]
44

# resident set size (RSS) mem usage of dbus.service
$ zabbix_get -k systemd.cgroup.mem[dbus.service,rss]
663552

# total queued iops of dbus.service
$ zabbix_get -s 127.0.0.1 -k systemd.cgroup.dev[dbus.service,blkio.io_queued,Total]
0
```

## SELinux

If you have configure SELinux in enforcing mode, you might see the following
error in your Zabbix logs, when attempting to use item keys from this module:

```
[systemd] org.freedesktop.DBus.Error.AccessDenied: SELinux policy denies access
```

This is because the SELinux policy that ships with RedHat/CentOS does not
explicitely allow the Zabbix agent to communicate with D-Bus. This package
includes an extension module to grant Zabbix only the permissions it requires
for read-only access.

After installing this package, the SELinux module can be enabled by running:

```bash
$ semodule -v -i /usr/share/selinux/packages/zabbix-module-systemd/libzbxsystemd.pp
```
