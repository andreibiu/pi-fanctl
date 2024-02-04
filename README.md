# pi-fanctl

Kernel driver for pwm control of cooling fans targeting Raspberry Pi boards.

This projects aims to offer the necessary software for cooling Raspberry Pis in
order to enable their full performance potential (especially useful for overclocked
chips), while allowing enough flexibility regarding the actual setup.
Features include:
 - up to 8-point fan curves with temperature hysteresis
 - the possibility to use even 2-pin fans with few extra components (a transistor
   and a flyback diode)
 - efficient implementation using NEON instructions

**Note:** The driver uses the pwm channel *0* and the GPIO pin *18*.

### Dependencies & Compatibility

The project was developed and tested on Raspberry Pi 4 running the latest
Raspberry Pi OS 64-bit version, but any other 64-bit Linux distribution should
that has a `python` version `3.8` or newer and the necessary dependencies for
module and device tree compilation (`gcc`, `make`, `dtc`, kernel headers matching
the running kernel version) should work as long as it also provides the
`pwm-overlay` shipped by Raspberry Pi OS.

**Note**: Raspberry Pi 5 comes with official cooling fan support. At the moment
I don't have any data about how this project would stack up compared to it, but
I expect that it would be relatively easy to add support for the 5's fan header
in `pi-fanctl` if it has some advantages over the official cooling software.

### Setup

The source code can be obtained from the GitHub release page or alternatively
by cloning the repository:
```shell
git clone https://github.com/andreibiu/pi-fanctl
cd pi-fanctl
```

Before installing the kernel driver, it is recommended to test it first on the
actual system by running the top-level script `fanctl` as follows:
```shell
chmod +x fanctl
sudo ./fanctl -t
```

This ensures that the driver compiles successfully and works as expected by running
a small stress test using `stress-ng` program (it should be manually installed)
and by printing some debug info in the kernel logs. This mode is also useful for
customizing the fan curve, as described in the **Configuration** section.

If everything works as expected, the driver can be installed by running:
```shell
sudo ./fanctl
```
and uninstalled with:
```shell
sudo fanctl -u
```

**Note:** The script is designed to add/remove automatically the necessary lines
in the boot configuration file and reboot the system if the actions above finish.

### Configuration

The driver can be configured at compile-time by editing the `fanctl.cfg` file.
It contains a list of key-value pairs separated by `=` sign, each on an individual
line. The configurable values are:

 - the run period: the time between successive runs of the algorithm that checks
   the temperature and updates the fan speed accordingly:  
   **P =** *<value_ms>*  
   this configuration is optional and has a default value of *1000* if it is not
   provided in the file
 - the fan curve: described by a list of consecutive points starting from *1*,
   each specifying the temperature, the temperature hysteresis and the corresponding
   fan speed to be applied:  
   *<point_id>* **=** *<temp_celsius>* **,** *<temp_hyst_celsius>* **,** *<fan_percentage>*  
   the corresponding fan speed (*0* to *100*) is applied when the measured
   temperature is equal or greater that the target one (ranging from *-128*C
   to *127*C) and changed back only when it decreases under the target minus the
   hysteresis (ranging from *0*C to *127*C); the fan curve configuration is
   mandatory and should have between one to eight points

**Note:** The names between *< >* should be replaced with actual values.  
**Note:** The `fanctl.cfg` file comes with a sample configuration that can serve
as a starting point.  
**Note:** There are some additional constrains on the values of the curve points.
For example the difference between the temperature of point *N* and *N-1* must
be strictly greater than the hysteresis of the point *N*. The config parser
script stops the compilation in such cases to prevent unexpected behavior at run
time. However, most configurations that make sense in practice should work without
any problem.