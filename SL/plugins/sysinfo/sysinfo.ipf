:userdoc.
.* 
.* Main help panel
.* ---------------
:h1 res=20801.DosQuerySysInfo()
:p.
The following list gives the ordinal index, name, and description of the system
variables.
:dl compact tsize=4.
:dt.1
:dd.QSV_MAX_PATH_LENGTH
:p.Maximum length, in bytes, of a path name&per.
:edl.
:dl compact tsize=4.
:dt.2
:dd.QSV_MAX_TEXT_SESSIONS
:p.Maximum number of text sessions&per.
:edl.
:dl compact tsize=4.
:dt.3
:dd.QSV_MAX_PM_SESSIONS
:p.Maximum number of PM sessions&per.
:edl.
:dl compact tsize=4.
:dt.4
:dd.QSV_MAX_VDM_SESSIONS
:p.Maximum number of DOS sessions&per.
:edl.
:dl compact tsize=4.
:dt.5
:dd.QSV_BOOT_DRIVE
:p.Drive from which the system was started (1 means drive A, 2 means drive B,
and so on)&per.
:edl.
:dl compact tsize=4.
:dt.6
:dd.QSV_DYN_PRI_VARIATION
:p.Dynamic priority variation flag (0 means absolute priority, 1 means dynamic 
priority)&per.
:edl.
:dl compact tsize=4.
:dt.7
:dd.QSV_MAX_WAIT
:p.Maximum wait in seconds&per.
:edl.
:dl compact tsize=4.
:dt.8
:dd.QSV_MIN_SLICE
:p.Minimum time slice in milliseconds&per.
:edl.
:dl compact tsize=4.
:dt.9
:dd.QSV_MAX_SLICE
:p.Maximum time slice in milliseconds&per.
:edl.
:dl compact tsize=4.
:dt.10
:dd.QSV_PAGE_SIZE
:p.Memory page size in bytes&per. This value is 4096 for the 80386 processor
&per.
:edl.
:dl compact tsize=4.
:dt.11
:dd.QSV_VERSION_MAJOR
:p.Major version number (see note below)&per.
:edl.
:dl compact tsize=4.
:dt.12
:dd.QSV_VERSION_MINOR
:p.Minor version number (see note below)&per.
:edl.
:dl compact tsize=4.
:dt.13
:dd.QSV_VERSION_REVISION
:p.Revision number (see note below)&per.
:edl.
:dl compact tsize=4.
:dt.14
:dd.QSV_MS_COUNT
:p.Value of a 32-bit, free-running millisecond counter&per. This value is zero 
when the system is started&per.
:edl.
:dl compact tsize=4.
:dt.15
:dd.QSV_TIME_LOW
:p.Low-order 32 bits of the time in seconds since January 1, 1970 at 0 00 00
&per.
:edl.
:dl compact tsize=4.
:dt.16
:dd.QSV_TIME_HIGH
:p.High-order 32 bits of the time in seconds since January 1, 1970 at 0 00 00
&per.
:edl.
:dl compact tsize=4.
:dt.17
:dd.QSV_TOTPHYSMEM
:p.Total number of bytes of physical memory in the system&per.
:edl.
:dl compact tsize=4.
:dt.18
:dd.QSV_TOTRESMEM
:p.Total number of bytes of resident memory in the system&per.
:edl.
:dl compact tsize=4.
:dt.19
:dd.QSV_TOTAVAILMEM
:p.Maximum number of bytes of memory that can be allocated by all processes in 
the system&per. This number is advisory and is not guaranteed, since system 
conditions change constantly&per.
:edl.
:dl compact tsize=4.
:dt.20
:dd.QSV_MAXPRMEM
:p.Maximum number of bytes of memory that this process can allocate in its 
private arena&per. This number is advisory and is not guaranteed, since system 
conditions change constantly&per.
:edl.
:dl compact tsize=4.
:dt.21
:dd.QSV_MAXSHMEM
:p.Maximum number of bytes of memory that a process can allocate in the shared 
arena&per. This number is advisory and is not guaranteed, since system conditions 
change constantly&per.
:edl.
:dl compact tsize=4.
:dt.22
:dd.QSV_TIMER_INTERVAL
:p.Timer interval in tenths of a millisecond&per.
:edl.
:dl compact tsize=4.
:dt.23
:dd.QSV_MAX_COMP_LENGTH
:p.Maximum length, in bytes, of one component in a path name&per.
:edl.
:dl compact tsize=4.
:dt.24
:dd.QSV_FOREGROUND_FS_SESSION
:p.Session ID of the current foreground full-screen session&per. Note that this 
only applies to full-screen sessions&per. The Presentation Manager session 
(which displays Vio-windowed, PM, and windowed DOS Sessions) is full-screen 
session ID 1&per.
:edl.
:dl compact tsize=4.
:dt.25
:dd.QSV_FOREGROUND_PROCESS
:p.Process ID of the current foreground process&per.
:edl.
:dl compact tsize=4.
:dt.26
:dd.QSV_NUMPROCESSORS
:p.Number of processors in the machine&per.
:edl.
:dl compact tsize=4.
:dt.27
:dd.QSV_MAXHPRMEM
:p.Maximum amount of free space in process&apos.s high private arena&per. 
Because system conditions change constantly, this number is advisory and is not 
guaranteed&per.  In addition, this number does not indicate the largest single 
memory object you can allocate because the arena may be fragmented&per. 
:edl.
:dl compact tsize=4.
:dt.28
:dd.QSV_MAXHSHMEM
:p.Maximum amount of free space in process&apos.s high shared arena&per. Because 
system conditions change constantly, this number is advisory and is not guaranteed
&per. In addition, this number does not indicate the largest single memory object you 
can allocate because the arena may be fragmented&per.
:edl.
:dl compact tsize=4.
:dt.29
:dd.QSV_MAXPROCESSES
:p.Maximum number of concurrent processes supported&per.
:edl.
:dl compact tsize=4.
:dt.30
:dd.QSV_VIRTUALADDRESSLIMIT
:p.Size of the user&apos.s address space in megabytes (that is, the value of 
the rounded VIRTUALADDRESSLIMIT)&per.
:edl.
:dl compact tsize=4.
:dt.33
:dd. 
:p.OS/4 kernel svn revision&per.
:edl.

:p.:hp2.Note&colon. :ehp2.Major, minor and revision numbers for versions of OS/2 operating 
system are described below&per.
:cgraphic.
:color fc=default.:color bc=default.                Major           Minor           Revision
OS/2 2&per.0        20              00              0
OS/2 2&per.1        20              10              0
OS/2 2&per.11       20              11              0
OS/2 3&per.0        20              30              0
OS/2 4&per.0        20              40              0
OS/2 4&per.5        20              45              0
:ecgraphic.

:euserdoc.
