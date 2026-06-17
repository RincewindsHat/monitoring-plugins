#set document(title: "Plugins Development Guidelines")
#set heading(numbering: "1.1.")

#outline()

#pagebreak()

= Preface

The purpose of these guidelines is to provide a reference for
the plugin developers and encourage the standardization of the
different kinds of plugins: C, shell, perl, python, etc.


Permission is granted to make and distribute verbatim copies of
this manual provided the copyright notice and this permission
notice are preserved on all copies.

The plugins themselves are copyrighted by their respective
authors.


= Development platform requirements

Monitoring Plugins are developed to the GNU standard, so any OS
which is supported by GNU should run the plugins. While the
requirements for compiling the Monitoring Plugins release are
very basic, developing from the Git repository requires
additional software to be installed. These are the minimum
levels of software required:

#table(
  columns: (auto, auto),
  align: horizon,
  table.header([*Tool*], [*Version*]),
  [ GNU make], [3.79 ],
  [ GNU automake], [1.9.2 ],
  [ GNU autoconf], [2.59],
  [ GNU m4], [1.4.2],
  [ GNU libtool], [1.5 ],
)

To compile from Git, after you have cloned the repository, run:

```
tools/setup
./configure
make
make install
```

= Plugin Output for Nagios

You should always print something to `STDOUT` that tells if the
service is working or why it is failing. Try to keep the output
short - probably less than 80 characters. Remember that you
ideally would like the entire output to appear in a pager
message, which will get chopped off after a certain length.

As Nagios does not capture stderr output, you should only
output to `STDOUT` and not print to `STDERR`.

== Print only one line of text

Starting with version 3, Nagios will process plugins' multiline
output, which should be formatted as:

```
SERVICE STATUS: First line of output | First part of performance data
Any number of subsequent lines of output, but note that buffers may have a limited size | Second part of performance data, which
may have continuation lines, too
```

Note, however, that the default configs still do not include
the output's continuation lines into the notifications sent
when Nagios notifies contacts about potential problems. Thus,
keep your output short and to the point.

Output should be in the format:

```
SERVICE STATUS: Information text
```

However, note that this is not a requirement of the API, so you
cannot depend on this being an accurate reflection of the
status of the service - the status should always be determined
by the return code.

== Verbose output

Use the `-v` flag for verbose output. You should allow multiple
`-v` options for additional verbosity, up to a maximum of 3. The
standard type of output should be:

#table(
  columns: (auto, auto),
  align: horizon,
  table.header([*Verbosity Level*], [*Purpose*]),
  [0], [Single line, minimal output. Summary],
  [1], [Single line, additional information (eg list processes that fail)],

  [2], [Multi line, configuration debug output (eg ps command used)],
  [3], [Lots of detail for plugin problem diagnosis],
)

== Screen Output

The plugin should print the diagnostic and just the usage part
of the help message. A well written plugin would then have
`--help` as a way to get the verbose help.

Code and output should try to respect the 80x25 size of a crt
(remember when fixing stuff in the server room!)

== Plugin Return Codes

The return codes below are based on the POSIX spec of returning
a positive value. Netsaint prior to v0.0.7 supported non-POSIX
compliant return code of "-1" for unknown. Nagios supports
POSIX return codes by default.

Note: Some plugins will on occasion print on `STDOUT` that an
error occurred and error code is 138 or 255 or some such
number. These are usually caused by plugins using system
commands and having not enough checks to catch unexpected
output. Developers should include a default catch-all for
system command output that returns an UNKNOWN return code.

#table(
  columns: (auto, auto, auto),
  align: horizon,
  table.header(
    [*Return code*], [*Status Text*], [*Meaning*],
    [0],
    [OK],
    [The plugin was able to check the service and it appeared to be functioning properly],

    [1],
    [Warning],
    [The plugin was able to check the service, but it appeared to be
      above some "warning" threshold or did not appear to be working
      properly],

    [2],
    [Critical],
    [The plugin detected that either the service was not running or
      it was above some \"critical\" threshold],

    [3],
    [Unknown],
    [Invalid command line arguments were supplied to the plugin or
      low-level failures internal to the plugin (such as unable to
      fork, or open a tcp socket) that prevent it from performing the
      specified operation. Higher-level errors (such as name
      resolution errors, socket timeouts, etc) are outside of the
      control of plugins and should generally NOT be reported as
      UNKNOWN states.],
  ),
)

The `--help` or `--version` output should also result in Unknown
state.

== Threshold and ranges

A range is defined as a start and end point (inclusive) on a
numeric scale (possibly negative or positive infinity).

A threshold is a range with an alert level (either warning or
critical). Use the set_thresholds(thresholds \*, char \*, char \*)
function to set the thresholds.

The theory is that the plugin will do some sort of check which
returns back a numerical value, or metric, which is then
compared to the warning and critical thresholds. Use the
get_status(double, thresholds \*) function to compare the value
against the thresholds.

This is the generalized format for ranges:

```
[@]start:end
```

Notes:

+ start ≤ end
+ start and ":" is not required if start=0
+ if range is of format "start:" and end is not specified,
  assume end is infinity
+ to specify negative infinity, use "~"
+ alert is raised if metric is outside start and end range
  (inclusive of endpoints)
+ if range starts with "@", then alert if inside this range
  (inclusive of endpoints)

Note: Not all plugins are coded to expect ranges in this format
yet. There will be some work in providing multiple metrics.

#figure(
  table(
    columns: (auto, auto),
    align: horizon,
    table.header([*Range definition*], ["Generate an alert if ..."]),
    [10], [< 0 or > 10, (outside the range of {0 .. 10})],
    [10:], [< 10, (outside {10 .. ∞})],

    [~:10], [> 10, (outside the range of {-∞ .. 10})],
    [10:20], [< 10 or > 20, (outside the range of {10 .. 20})],

    [\@10:20], [≥ 10 and ≤ 20, (inside the range of {10 .. 20})],
  ),
  caption: "Example ranges",
)


#table(
  columns: (auto, auto),
  align: horizon,
  table.header([*Command line*], [Meaning]),
  [check_stuff -w10 -c20],
  [Critical if \"stuff\" is over 20, else warn if over 10 (will be critical if \"stuff\" is less than 0)],

  [check_stuff -w~:10 -c~:20], [Same as above. Negative \"stuff\" is OK],
  [check_stuff -w10: -c20],
  [Critical if \"stuff\" is over 20, else warn if \"stuff\" is below 10 (will be critical if \"stuff\" is less than 0)],

  [check_stuff -c1:], [Critical if \"stuff\" is less than 1],
  [check_stuff -w~:0 -c10],
  [Critical if \"stuff\" is above 10; Warn if \"stuff\" is above zero (will be critical if "stuff" is less than 0)],

  [check_stuff -c5:6], [Critical if \"stuff\" is less than 5 or more than 6],

  [check_stuff -c\@10:20],
  [OK if stuff is less than 10 or higher than 20, otherwise critical)],
)

== Performance data

Nagios 3 and newer will concatenate the parts following a "|"
in a) the first line output by the plugin, and b) in the second
to last line, into a string it passes to whatever performance
data processing it has configured. (Note that it currently does
not insert additional whitespace between both, so the plugin
needs to provide some to prevent the last pair of a) and the
first of b) getting run together.) Please refer to the Nagios
documentation for information on how to configure such
processing. However, it is the responsibility of the plugin
writer to ensure the performance data is in a "Nagios Plugins"
format. This is the expected format:

```
'label'=value[UOM];[warn];[crit];[min];[max]
```

Notes:

+ space separated list of label/value pairs
+ label can contain any characters except the equals sign or
  single quote (')
+ the single quotes for the label are optional. Required if
  spaces are in the label
+ label length is arbitrary, but ideally the first 19
  characters are unique (due to a limitation in RRD). Be
  aware of a limitation in the amount of data that NRPE
  returns to Nagios
+ to specify a quote character, use two single quotes
+ warn, crit, min or max may be null (for example, if the
  threshold is not defined or min and max do not apply).
  Trailing unfilled semicolons can be dropped
+ min and max are not required if UOM=%
+ value, min and max in class [-0-9.]. Must all be the same
  UOM. value may be a literal "U" instead, this would
  indicate that the actual value couldn't be determined
+ warn and crit are in the range format (see Section 2.5).
  Must be the same UOM
+ UOM (unit of measurement) is one of:
  a. no unit specified - assume a number (int or float) of
  things (eg, users, processes, load averages)
  b. s - seconds (also us, ms)
  c. % - percentage
  d. B - bytes (also KB, MB, TB)
  e. c - a continuous counter (such as bytes transmitted on
  an interface)

It is up to third party programs to convert the Monitoring
Plugins performance data into graphs.

= System Commands and Auxiliary Files

== Don't execute system commands without specifying their full path

Don't use exec(), popen(), etc. to execute external commands
without explicitly using the full path of the external program.

Doing otherwise makes the plugin vulnerable to hijacking by a
trojan horse earlier in the search path. See the main plugin
distribution for examples on how this is done.

== Use spopen() if external commands must be executed

If you have to execute external commands from within your
plugin and you're writing it in C, use the spopen() function
that Karl DeBisschop has written.

The code for spopen() and spclose() is included with the core
plugin distribution.

== Don't make temp files unless absolutely required

If temp files are needed, make sure that the plugin will fail
cleanly if the file can't be written (e.g., too few file
handles, out of disk space, incorrect permissions, etc.) and
delete the temp file when processing is complete.

== Don't be tricked into following symlinks

If your plugin opens any files, take steps to ensure that you
are not following a symlink to another location on the system.

== Validate all input

use routines in utils.c or utils.pm and write more as needed

= Perl Plugins

Perl plugins are coded a little more defensively than other
plugins because of embedded Perl. When configured as such,
embedded Perl Nagios (ePN) requires stricter use of the some of
Perl's features. This section outlines some of the steps needed
to use ePN effectively.

+ Do not use BEGIN and END blocks since they will be called
  only once (when Nagios starts and shuts down) with Embedded
  Perl (ePN). In particular, do not use BEGIN blocks to
  initialize variables.
+ To use utils.pm, you need to provide a full path to the
  module in order for it to work.
  e.g.
  use lib "/usr/local/nagios/libexec";
  use utils qw(...);

+ Perl scripts should be called with "-w"
+ All Perl plugins must compile cleanly under "use strict" -
  i.e. at least explicitly package names as in `$main::x` or
  predeclare every variable.
  Explicitly initialize each variable in use. Otherwise with
  caching enabled, the plugin will not be recompiled each
  time, and therefore Perl will not reinitialize all the
  variables. All old variable values will still be in effect.
+ Do not use >DATA< handles (these simply do not compile
  under ePN).
+ Do not use global variables in named subroutines. This is
  bad practise anyway, but with ePN the compiler will report
  an error "<global_var> will not stay shared ..". Values
  used by subroutines should be passed in the argument list.
+ If writing to a file (perhaps recording performance data)
  explicitly close it. The plugin never calls exit; that is
  caught by p1.pl, so output streams are never closed.
+ As in Section 5 all plugins need to monitor their runtime,
  specially if they are using network resources. Use of the
  alarm is recommended noting that some Perl modules (eg LWP)
  manage timers, so that an alarm set by a plugin using such
  a module is overwritten by the module. (workarounds are
  cunning (TM) or using the module timer) Plugins may import
  a default time out (`$TIMEOUT`) from utils.pm.
+ Perl plugins should import %ERRORS from utils.pm and then
  `exit $ERRORS{'OK'}` rather than "exit 0"

= Runtime Timeouts

Plugins have a very limited runtime - typically 10 sec. As a
result, it is very important for plugins to maintain internal
code to exit if runtime exceeds a threshold.

All plugins should timeout gracefully, not just networking
plugins. For instance, df may lock if you have automounted
drives and your network fails - but on first glance, who'd
think df could lock up like that. Plus, it should just be more
error resistant to be able to time out rather than consume
resources.

== Use DEFAULT_SOCKET_TIMEOUT

All network plugins should use DEFAULT_SOCKET_TIMEOUT to
timeout

== Add alarms to network plugins

If you write a plugin which communicates with another networked
host, you should make sure to set an alarm() in your code that
prevents the plugin from hanging due to abnormal socket
closures, etc. Nagios takes steps to protect itself against
unruly plugins that timeout, but any plugins you create should
be well behaved on their own.

= Plugin Options

A well written plugin should have --help as a way to get
verbose help. Code and output should try to respect the 80x25
size of a crt (remember when fixing stuff in the server room!)

== Option Processing

For plugins written in C, we recommend the C standard getopt
library for short options. Getopt_long is always available.

For plugins written in Perl, we recommend Getopt::Long module.

Positional arguments are strongly discouraged.

There are a few reserved options that should not be used for
other purposes:

-V version (--version)
-h help (--help)
-t timeout (--timeout)
-w warning threshold (--warning)
-c critical threshold (--critical)
-H hostname (--hostname)
-v verbose (--verbose)

In addition to the reserved options above, some other standard
options are:

-C SNMP community (--community)
-a authentication password (--authentication)
-l login name (--logname)
-p port or password (--port or --passwd/--password)mo
nitors operational
-u url or username (--url or --username)

Look at check_pgsql and check_procs to see how I currently
think this can work. Standard options are:

The option -V or --version should be present in all plugins.
For C plugins it should result in a call to print_revision, a
function in utils.c which takes two character arguments, the
command name and the plugin revision.

The -? option, or any other unparsable set of options, should
print out a short usage statement. Character width should be 80
and less and no more that 23 lines should be printed (it should
display cleanly on a dumb terminal in a server room).

The option -h or --help should be present in all plugins. In C
plugins, it should result in a call to print_help (or
equivalent). The function print_help should call
print_revision, then print_usage, then should provide detailed
help. Help text should fit on an 80-character width display,
but may run as many lines as needed.

The option -v or --verbose should be present in all plugins.
The user should be allowed to specify -v multiple times to
increase the verbosity level, as described in Table 1.

The exit code for version information or help should be UNKNOWN
(3).

== Plugins with more than one type of threshold, or with threshold ranges

Old style was to do things like -ct for critical time and -cv
for critical value. That goes out the window with POSIX getopt.
The allowable alternatives are:

1. long options like -critical-time (or -ct and -cv, I
  suppose).
2. repeated options like `check_load -w 10 -w 6 -w 4 -c 16 -c
       10 -c 10`
3. for brevity, the above can be expressed as `check_load -w
       10,6,4 -c 16,10,10`
4. ranges are expressed with colons as in `check_procs -C
       httpd -w 1:20 -c 1:30` which will warn above 20 instances,
  and critical at 0 and above 30
5. lists are expressed with commas, so Jacob's check_nmap uses
  constructs like '-p 1000,1010,1050:1060,2000'
6. If possible when writing lists, use tokens to make the list
  easy to remember and non-order dependent - so check_disk
  uses '-c 10000,10%' so that it is clear which is the
  percentage and which is the KB values (note that due to my
  own lack of foresight, that used to be '-c 10000:10%' but
  such constructs should all be changed for consistency,
  though providing reverse compatibility is fairly easy).

As always, comments are welcome - making this consistent
without a host of long options was quite a hassle, and I would
suspect that there are flaws in this strategy.

= Test cases

Tests are the best way of knowing if the plugins work as
expected. Please create and update test cases where possible.

To run a test, from the top level directory, run "make test".
This will run all the current tests and report an overall
success rate.

== Test cases for plugins

These use perl's Test::More. To do a one time test, run "cd
plugins && perl t/check_disk.t".

There will sometimes be failures seen in this output which are
known failures that need to be fixed. As long as the return
code is 0, it will be reported as "test pass". (If you have a
fix so that the specific test passes, that will be gratefully
received!)

If you want a summary test, run: "cd plugins && prove
t/check_disk.t". This runs the test in a summary format.

For a good and amusing tutorial on using Test::More, see this
link

== Testing the C library functions

We use the libtap library, which gives perl's TAP (Test
Anything Protocol) output. This is used by the FreeBSD team for
their regression testing.

To run tests using the libtap library, download the latest tar
ball and extract. There is a problem with tap-1.01 where
pthread support doesn't appear to work properly on non-FreeBSD
systems. Install with 'CPPFLAGS="-UHAVE_LIBPTHREAD" ./configure
&& make && make check && make install'.

When you run Monitoring Plugins' configure, it will look for
the tap library and will automatically setup the tests. Run
"make test" to run all the tests.

= Coding guidelines

See GNU Coding standards for general guidelines. Some parts of
the guidelines (namely and explicitly the coding style) are not
seen as best practice by us.

== C coding

You should use the type "bool" and its values "true" and
"false" instead of the "int" type for booleans.

== Crediting sources

If you have copied a routine from another source, make sure the
license from your source allows this. Add a comment referencing
the ACKNOWLEDGEMENTS file, where you can put more detail about
the source.

For contributed code, do not add any named credits in the
source code - contributors should be added into the THANKS.in
file instead.

== Commit Messages

If the change is due to a contribution, please quote the
contributor's name and, if applicable, add the GitHub Issue
Tracker number. Don't forget to update the THANKS.in file.

If you have a change that is useful for noting in the next
release, please update the NEWS file.

All commits will be written to a ChangeLog at release time.

= Submission of new plugins and patches

== Patches

If you have a bug patch, please supply a unified or context
diff against the version you are using. For new features,
please supply a diff against the Git "master" branch.

Patches should be submitted via GitHub's Pull requests for
Monitoring Plugins

Submission of a patch implies that the submmitter acknowledges
that they are the author of the code (or have permission from
the author to release the code) and agree that the code can be
released under the GPL. The copyright for the changes will then
revert to the Monitoring Plugins Development Team - this is
required so that any copyright infringements can be
investigated quickly without contacting a huge list of
copyright holders. Credit will always be given for any patches
through a THANKS file in the distribution.

== Contributed plugins

Plugins that have been contributed to the project and
distributed with the Monitoring Plugins files are held in the
contrib/ directory and are not installed by default. These
plugins are not officially supported by the team. The current
policy is that these plugins should be owned and maintained by
the original contributor, preferably hosted on Icinga Exchange.

If patches or bugs are raised to an contributed plugin, we will
start communications with the original contributor, but seek to
remove the plugin from our distribution.

The aim is to distribute only code that the Monitoring Plugins
team are responsible for.

== New plugins

If you would like others to use your plugins, please add it to
the official 3rd party plugin repository, Icinga Exchange.

We are not accepting requests for inclusion of plugins into our
distribution at the moment, but when we do, these are the
minimum requirements:

1. Include copyright and license information in all files.
  Copyright must be solely granted to the Monitoring Plugins
  Development Team
2. The standard command options are supported (--help,
  --version, --timeout, --warning, --critical)
3. It is determined to be not redundant (for instance, we
  would not add a new version of check_disk just because
  someone had provide a plugin that had perf checking - we
  would incorporate the features into an existing plugin)
4. One of the developers has had the time to audit the code
  and declare it ready for core
5. It should also follow code format guidelines, and use
  functions from utils (perl or c or sh) rather than using
  its own
6. Includes patches to configure.in if required (via the
  EXTRAS list if it will only work on some platforms)
7. If possible, please submit a test harness. Documentation on
  sample tests coming soon
