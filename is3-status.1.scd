is3-status(1)

# NAME
is3-status - Generates a status line for i3bar, swaybar

# SYNOPSIS
is3-status \[_FILE_\]

# OPTIONS
\[_FILE_\]
	Specifies an alternate configuration file path. By default, is3-status looks
	for configuration files in the following order:
	. ${IS3_STATUS_CONFIG}
	. ${XDG_CONFIG_HOME}/is3-status.conf
	. ~/.config/is3-status.conf
	. ~/.is3-status.conf
	. /etc/is3-status.conf

# DESCRIPTION
is3-status is a small program for generating a status bar for i3bar, swaybar
or similar programs which implements the i3bar protocol. It is designed to be
very efficient by issuing a very small number of system calls, as one generally
wants to update such a status line every second. This ensures that even under
high load, your status bar is updated correctly. Also, it saves a bit of energy
by not hogging your CPU as much as spawning the corresponding amount of shell
commands would.

# CONFIGURATION
The configuration file is an .ini file whose sections represents the modules.
The order of the sections is the order in is3-status's output. The default
section (before any is mentioned) is the general settings section.

Please note that the error checking is very basic. It is your responsibility
to check for error, using this man file. Inputting wrong values for the options
might output error in the good case, or in the worst case suddenly crush the
program. Full input checking is missing for improving performance.

Comment may appear as only a full comment line, and they must start with *#*
symbol.

Empty lines are ignored.

is3-status supports UTF-8 format output.

## EXAMPLE
file _~/.config/is3-status.conf_:
```
# this is a comment
# this section is the general settings section
interval = 2
color_good = #00FF00

[date]
format = %H:%M:%S

[eth]
interface = enp1s0
format_up = E: %4
format_down = E: down

[eth second]
interface = enp2s0
format_up = E: %4
```

## SECTIONS
Sections represents the different modules and theirs options. Every module can
appear as multiple instances, in which case they are differs in the instance
name. The correct format is:


	\[ _module_ _\[instance\]_ \]


Please note that the instance name is optional. It is required only in click
events. But is still recommended to make all instances different.

The order of the sections is the resulting order of the output (from left to
right).

## MODULE: date
The module outputs the current time in the requested timezone and format.

	*format = *_[str]_: the output format. Uses the same format as the \`date\`
	command. Aborts if unset.

	*timezone = *_[str]_: the requested timezone. Uses local timezone if unset.

## MODULE: eth
The module outputs the current status of the network interface and it's IP
address.

	*interface = *_[str]_: the wanted network interface.

	*format_up = *_[str]_: the output format for when the interface is up. Use
	*\%4* for the IPv4 address, *\%6* for the IPv6 address, and *\%a* or *\%A*
	for one of IPv4 or IPv6 address. The default is *\%a*

	*format_down = *_[str]_: the output format for when the interface is down.
	Uses the same format as *format_up*. If unset uses *format_up*.

## MODULE: volume_alsa
The module outputs the current volume level of an ALSA device.

	*device = *_[str]_: the name of the ALSA device. The default is "default".

	*mixer = *_[str]_: the mixer name of the ALSA device. The default is
	"Master".

	*mixer_idx = *_[int]_: the mixer id of the ALSA device. The default is 0.

	*format = *_[str]_: the output format. Use *\%v* or *\%V* for the volume
	level value. Aborts if unset.

	*format_muted = *_[str]_: the output format when device is muted. Uses the
	same rules as *format*. If unset uses *format*.

	*wheel_step = *_[int]_: the amount in percentage to step when using the
	wheel to change the volume.

The module handles those mouse events:
. Wheel click - mute/unmute the device.
. Wheel step - add or decrease in *wheel_step* for every step with the wheel.

## MODULE: disk_usage
The module outputs the current filesystem usage.

	*format = *_[str]_: the output format. Aborts if unset. Possible
	placeholders are:

[[ *\%t*
:- Total storage size of filesystem.
|  *\%a*
:- Total available storage size to use.
|  *\%f*
:- Total free storage size.
|  *\%u*
:- Total used storage size.
|  *\%A*
:- Available storage percentage.
|  *\%F*
:- Free storage percentage.
|  *\%U*
:- Used storage percentage.


	*path = *_[str]_: mountpoint of the wanted filesystem. By default uses
	root ("/").

	*use_decimal = *_[0|1]_: use decimal suffix output for the total size
	variant. For example if set to "1", 1GiB = 1000MiB, and if set to "0",
	1GB = 1024MB. By default is set to "0", meaning using binary suffixes.

## MODULE: x11_language
The module outputs the current xkb keyboard layout used in the Xorg session.

	*display = *_[str]_: the Xorg display used. If unset uses *$DISPLAY*
	environment variable, and fallbacks to *:0*.

	*language1 = *_[str]_: the first state of language.

	*language2 = *_[str]_: the second state of language.

If *CAPS LOCK* is on, the text will be all uppercase text. If *NUM LOCK* is
off, the field's color will be *degraded*.

## MODULE: cpu_temperature
The module outputs the current CPU temperature.

	*path = *_[str]_: the path to the thermal Linux directory. This directory
	must have a file with *temp* name whose content is the current temperature
	in milli Celsius degrees. The default value is:
	*/sys/class/thermal/thermal_zone0/*.

	*format = *_[str]_: the output format. Aborts if unset. Possible
	placeholders are: *\%c* for Celsius degrees and *\%f* for Fahrenheit
	degrees.

	*high_threshold = *_[int]_: threshold in Celsius degrees starting from
	which the field would be colored *bad*.

## MODULE: memory
The module outputs the current RAM memory usage.

	*format = *_[str]_: the output format. Aborts if unset. Possible
	placeholders are:

[[ *\%t*
:- Total amount of RAM memory.
|  *\%a*
:- Total available RAM memory to use.
|  *\%f*
:- Total free RAM memory.
|  *\%s*
:- Total shared used RAM memory.
|  *\%u*
:- Total used RAM memory.
|  *\%A*
:- Available memory percentage.
|  *\%F*
:- Free memory percentage.
|  *\%S*
:- Shared used memory percentage.
|  *\%U*
:- Used memory percentage.


	*use_decimal = *_[0|1]_: use decimal suffix output for the total size
	variant. For example if set to "1", 1GiB = 1000MiB, and if set to "0",
	1GB = 1024MB. By default is set to "0", meaning using binary suffixes.

	*use_method_classical = *_[0|1]_: changes the way used RAM memory is
	calculated. When set to "1", _gnome-system-monitor_ algorithm is used, and
	if set to "0", matches _free_ command. By default is set to "0", which is
	the more standard way.

	*threshold_degraded = *_[str]_: formatted threshold for free RAM memory,
	starting from which the field is *degraded* colored. The format is an integer
	with a suffix, for example "5MB", "10%", "4GiB". Accepted suffixes are _%_,
	_KB_, _KiB_, _MB_, _MiB_, _GB_, _GiB_, _TB_, _TiB_. If no suffix is given, it
	assumes it is in bytes.

	*threshold_critical = *_[str]_: formatted threshold for free RAM memory,
	like *threshold_degraded*, but colors the field in *critical*.

## MODULE: run_watch
The module outputs the status of a running process.

	*path = *_[str]_: path to pid file (file which contains pid of the wanted
	process). Please note the path can't contain globals.

The module send an empty signal to the specified process. The module will
reread the file every *interval*, so you the process can restart and update the
pid file.

## MODULE: systemd_watch
The module outputs the status of a systemd service.

	*service = *_[str]_: the name of the systemd service. For example
	"polkit.service". Aborts if unset.

	*use_user_bus = *_[0|1]_: if set to "1", uses the user session (the same as
	calling "systemctl --user status"), and if set to "0", uses system session
	(the same as calling "systemctl status"). By default uses system session.

; TODO: add sub sections ("## <name>") for:
;  battery
;  mpris
;  sway_language
