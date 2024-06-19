# pi-fanctl

Kernel driver for pwm control of cooling fans targeting Raspberry Pi boards with
64-bit cpu architectures.

This projects aims to offer the necessary software for cooling Raspberry Pis in
order to enable their full performance potential / prevent throttling for
overclocked chips, while allowing enough flexibility regarding the actual
setup (fan type and curve).

Features include:

- easy configuration
- up to 8-point fan curves with temperature hysteresis
- the possibility to control even 2-pin fans with few extra components
  (a transistor and a flyback diode)
- very low resource usage and efficient implementation using inline assembly,
  bit operations and NEON instructions

### Dependencies & Compatibility

The table below contains the supported boards with the tested Linux
distributions / kernel versions so far:

| Board                    | Distribution                                            | Kernels  |
|:------------------------:| ------------------------------------------------------- | -------- |
| Raspberry Pi 4           | Raspberry Pi OS (64-bit) based on Debain 12 (bookworm)  | 6.1, 6.6 |
| Raspberry Pi 5           | Raspberry Pi OS (64-bit) based on Debain 12 (bookworm)  | 6.1, 6.6 |

However, the project may be compatible with other distributions targeting Pi boards,
such as Ubuntu, provided that they have installed:

- `git` (optional, only for cloning the repository)
- `make`
- `python` (version 3.10 or newer)
- `dtc`
- `stress-ng` (optional, only for testing)
- the usual requirements for kernel modules compilation

**Note**: The project should be also compatible with Raspberry Pi 3 boards, but
no tests have been made so far.

### Installation

The installation process requires the source code; it can be obtained from the
GitHub release page or alternatively by cloning the repository:

```shell
git clone https://github.com/andreibiu/pi-fanctl
cd pi-fanctl
```

It is recommended to test the kernel driver on the actual system before
installing it by running the top-level script `fanctl` as follows:

```shell
chmod +x fanctl
sudo ./fanctl -t
```

This ensures that the driver compiles successfully and works as expected by running
a small stress test using `stress-ng` and by printing some debug info in the
kernel logs (can be viewed with `dmesg -w`). This mode is also useful for
customizing the fan curve, as described in the **Configuration** section.

If everything works as expected, the driver can be installed just by running:

```shell
sudo ./fanctl -i
```

All the required steps are done automatically, including device tree blob generation
and updating the boot configuration.

Uninstalling is just as easy (any changes in system configuration are also reverted):

```shell
sudo fanctl -u
```

**Note:** Those actions will require the system to be restarted.
**Note:** The uninstall process will also remove the config file created during
install; make sure to back up its content beforehand

### Configuration

All driver options are specified in a configuration file named `fanctl.cfg`. It
contains a list of key-value pairs separated by `=` sign, each on an individual
line and it is located in the following places:

- `config/fanctl.cfg` in the source directory (sample config also used in testing mode)
- `/usr/local/etc/fanctl.cfg` after the driver is installed

After installing, the config can be updated with any text editor and then applied
with:

```shell
sudo fanctl -c
```

The configuration options (where italic names in angular brackets should be
replaced with actual values, bold text is a mandatory part of the syntax) are:

| Key                     | Value format                                             | Value range                                                                               | Description                                                                                                                                |
|:-----------------------:|:--------------------------------------------------------:|:-----------------------------------------------------------------------------------------:| ------------------------------------------------------------------------------------------------------------------------------------------ |
| __MODE__                | *\<VALUE\>*                                              | VALUE: `gpio` or `fanh`                                                                   | Describes the mode of operation regarding the hardware setup, using either GPIO pins<sup>1</sup> or the board's fan header<sup>2</sup>     |
| __DELAY__               | *\<VALUE\>*__ms__                                        | -                                                                                         | The delay in milliseconds between two successive calls of the control algorithm                                                            |
| __PWM_FREQ__            | *\<VALUE\>*__Hz__                                        | -                                                                                         | The frequency in hertz of the pwm control signal                                                                                           |
| __PWM_POL__             | *\<VALUE\>*                                              | VALUE: `dir` or `inv`                                                                     | The polarity of the pwm signal, inverted mode swaps the voltage values of the two pwm states (on/off)                                      |
| __POINT\___*\<NUMBER\>* | *\<VALUE_1\>*__C,__*\<VALUE_2\>*__C,__*\<VALUE_3\>*__%__ | NUMBER: 1 to 8 <br> VALUE_1: -128 to 127 <br> VALUE_2: -128 to 127 <br> VALUE_3: 1 to 100 | Describes each fan curve's point in terms of temperature (celsius), temperature hysteresis (celsius), and fan speed percentage<sup>3</sup> |

**NOTE<sup>1</sup>**: In GPIO mode the driver uses for pwm the ***12(GPIO18)***
pin and default channel active on it:
- for Pi 4, the ***pwm 0*** of the ***pwm chip 0***
- for Pi 5, the ***pwm 2*** pf the ***pwm chip 2***

The exact location on the board of the used pin can be found using the `pinout`
command.  
**NOTE<sup>2</sup>**: The fan header mode is available only for Pi 5 boards and
uses the same pwm channel / chip combination as the official driver. In addition,
the board's device tree is automatically modified in order to disable the
official driver, but it is restored after uninstall.  
**NOTE<sup>3</sup>**: There are some additional constrains on the values of the curve points:

 - the fan curve must have at least one point
 - the fan curve can have less than 8 (maximum) points, but must be continuous
 - the fan curve should be strictly ascending (in terms of fan speeds and
   temperatures, including the ones obtained by applying the temperature
   hysteresis values)
 - the difference between the temperature and the temperature hysteresis must not
   be out of temperature range

All those criteria are checked by the script that parses the configuration file, so
the risk of invalid configurations is minimal.

**NOTE:** The sample config will not work for every fan setup. For example a
3/4-header fan that is directly connected to the board's pwm pin most probably
needs an *inverted* pwm polarity, while setups with external circuits may need
a *direct* (aka non-inverted) polarity.

### Additional commands

All driver script commands can be obtained by typing:

```shell
sudo fanctl -h
```