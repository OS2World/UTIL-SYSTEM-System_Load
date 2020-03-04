:userdoc.
.* 
.* Main help panel
.* ---------------
:h1 res=20801.Processors
:p.
The list displays all available processors on the system. 
From the context menu you can manually set the status of a processor to 
ONLINE or OFFLINE except the first one. Please note that other programs
like CPUGOV or ACPIDAEMON can set the status automatically. 
In the module "Processes" you can check for the presence of these processes.
.* 
.* Properties help
.* ---------------
:h1 res=20802.Properties
:p.
:hp2.Temperature.:ehp2.
The temperature can be obtained from various sources.
:ul.
:li.To obtain the information from Intel processors sensors the driver RDMSR$ is required.
If access to the sensors on your processor is supported and the driver is not
installed you will be prompted to install the driver automatically, In case you
want to install it manually add the following line to the config.sys "DEVICE=C&colon.&bsl.PROGRAMS&bsl.SL&bsl.RDMSR.SYS"
(where "C&colon.&bsl.PROGRAMS&bsl.SL&bsl." is the path to SystemLoad on your disk).
If data can be read from the temperature sensor they will be displayed near each
CPU in the list.
:li.Another source of information is Advanced Configuration and Power Interface
(ACPI) which contains information about so-called Thermal Zones of the
motherboard. This requires the correct path in the ACPI table. The path will 
be determined automatically if the installed hardware is supported. Custom values can be
specified by the user. If the data from the Termal Zone is available 
the temperature will be displayed above the CPU load graph.
:eul.
:p.
The switch :hp2.Show real frequency:ehp2. controls the display of the CPU clock
calculated real frequency.
.* --------------------
:euserdoc.
