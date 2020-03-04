:userdoc.
.* 
.* 1. Main help panel
.* ------------------
:h1 res=20801.Traffic module
:p.
In this list, you can see the output of the traffic filter. Rules statistics
calculations are defined in the :link reftype=hd res=20804.filter editor:elink.
that is called by the :hp1.Filter...:ehp1. entry of the context menu.
.* 
.* 2. Filter
.* ---------
:h1 res=20802.Filter
:p.
The filter is a treelike structure, each node of this structure sets parameters
for definite packets and accumulates statistics. A correctly configured filter
provides all required information. That depends on reasonableness of the
filter, how much useful and sufficient information will stand out.
:p.
Traffic counting is performed for each node and the traffic direction.
Data such as protocols, addresses and ports of a source and the receiver
is taken from the new packet and is checked by the filter, on corresponding 
nodes it changes the statistics "directly". 
Then the data of a source and receiver on various places is repeatedly checked 
by the filter, on corresponding nodes the "return" statistics changes. 
Thus, each rule has two counters of bytes&colon. sent and received.
:p.
In the study of your filter it can be useful viewing real-time data about the
packages fall into a specific node. To do this, select the item
:hp1.Trace:ehp1. from the context menu of any node.
.*
.* 2.1. An example of the filter
.* -----------------------------
:h2 res=20803.An example of the filter
:p.
Let's see a simple task of calculation of traffic the Internet-gate having
mail service (smtp) and a proxy-server. It is required to count statistics
for each user of a proxy-server and the traffic coming for a mail service 
from the Internet. The address of a local network has a IP address of 192.168.33.0 
and a subnet mask of 255.255.255.0. The address of a gateway in a local network (lan1) -
is 192.168.33.1, the public address of the host on the Internet - (lan0)
195.72.224.229, port for SMTP - 25, port of the proxy-server - 8080. 
Local machine IPs (users)&colon. 192.168.33.2, 192.168.33.3 and 192.168.33.4.
The schematically rules of our sample filter will be as follows&colon.
:sl.
:li.:hp2.lan0:ehp2. :hp1.network interface to the Internet:ehp1.
:dl compact tsize=2.
:dt.1.
:dd.
:dl compact tsize=25.
:dt. Name                     :dd.Internet&colon.SMTP
:dt. Source                   :dd.:hp1.any:ehp1.
:dt. Receiver                 :dd.195.72.224.229
:dt. TCP port of the source   :dd.:hp1.any:ehp1.
:dt. TCP port of the receiver :dd.25
:edl.
:edl.
:li.:hp2.lan1:ehp2. :hp1.network interface to the local network:ehp1.
:dl compact tsize=2.
:dt.2.
:dd.
:dl compact tsize=25.
:dt. Name                     :dd.My proxy&colon.
:dt. Source                   :dd.192.168.33.0/255.255.255.0
:dt. Receiver                 :dd.192.168.33.1
:dt. TCP port of the source   :dd.:hp1.any:ehp1.
:dt. TCP port of the receiver :dd.8080
:dt.:dd.
:edl.
:dl compact tsize=2.
:dt.2.1.
:dd.
:dl compact tsize=25.
:dt. Name                     :dd.Proxy&colon.Boss
:dt. Source                   :dd.192.168.33.2
:dt. If the node has a match  :dd.do not check subsequent nodes.
:edl.
:dt.2.2.
:dd.
:dl compact tsize=25.
:dt. Name                     :dd.Proxy&colon.workstation A
:dt. Source                   :dd.192.168.33.3
:dt. If the node has a match  :dd.do not check subsequent nodes.
:edl.
:dt.2.3.
:dd.
:dl compact tsize=25.
:dt. Name                     :dd.Proxy&colon.workstation B
:dt. Source                   :dd.192.168.33.4
:edl.
:edl.
:edl.
:esl.
:p.
As we can see, the node 2. has enclosed nodes (2.1, 2.2 and 2.3), it is made to
not specify the address and port of destination for each proxy-server
user&colon. the enclosed nodes will be looked through only if conditions of a
parental node were satisfied. It not only simplifies the task of nodes, but
also raises the productivity of the filter&colon. in general the nodes of users
will not be looked through if the packet is not for a proxy-server on the
internal network.
.*
.* 2.2. Filter editor
.* ------------------
:h2 res=20804.Filter editor
:p.
:link reftype=hd res=20802.Filter:elink. is one or more of the trees in which the root node is the network
interface. Filter nodes are displayed on the left side of the window.
:p.
Properties of the current node except the name are displayed and edited in the
right pane of the filter editor window. Top-level nodes (network interfaces)
have no properties.
:p.
Create, move, and delete nodes executed by buttons located at the bottom of the
window.
:p.
:hp2.Name.:ehp2.
:p.
Name of the node does not influence the filter, however it is used for 
the statistics display in the two-levels structure in the main window list.
The node list is made up of groups. The group name is specified in the node
name before the colon. So, the following evident structure will be given
to the user in our :link reftype=hd res=20803.example:elink.&colon.
:sl compact.
:li.  [-] Internet
:sl.
:li.        SMTP
:esl.
:li.  [-] Proxy
:sl compact.
:li.        Boss
:li.        workstation A
:li.        workstation B
:esl.
:esl.
:p.
If name ends with colon (node 2 "My proxy&colon." from the 
:link reftype=hd res=20803.example:elink.) the statistics of the given node is
not displayed in main window.
:p.
To change the node name hold CTRL and click or press ENTER on the node,
alternative you can also use the context menu item :hp1.Rename:ehp1..
:p.
:hp2.Stop scan nodes at this level.:ehp2.
:p.
In the filter editor node there is a checkbox :hp1.Stop scan nodes at this
level:ehp1.. When it is checked and the node for new packet was triggered
subsequent nodes will not be tested with this packet. Children of triggered
nodes will be tested anyway.
:p.
:hp2.IP-address.:ehp2.
:p.
The IP-address in the lists of a source addresses and destination addresses can
be specified in the following notation&colon.
:dl compact tsize=30.
:dthd.:hp2.Format:ehp2.
:ddhd.:hp2.Example:ehp2.
:dt.specific address
:dd.192.168.1.2
:dt.address/mask
:dd.192.168.1.0/255.255.255.0
:dt.address/mask bits number
:dd.192.168.1.0/24
:dt.range
:dd.192.168.1.10 - 192.168.1.20
:edl.
:p.
Entries of an address list are merged by the logical condition OR. Lists are 
checked from the beginning to the end. For performance reasons, it makes sense 
to address most frequent positioned closer to the top. The order of entries can 
be changed by the <+> (plus) key and <-> (minus) key on the numpad section of
the keyboard. An empty list matches any address.
:p.
:hp2.Ports.:ehp2.
:p.
The ports of UDP and TCP protocols are listed separated by commas and may
contain ranges, for example&colon.
:sl compact.
:li.80,81,8000-8080.
:esl.
:p.
An empty list matches any port. These fields are only available for the TCP and
UDP protocols.
.*
.* 3. Properties
.* -------------
:h1 res=20805.Properties
:p.
For a group of nodes which have a given way to count the total rates are displayed
in details view.
:dl compact tsize=19.
:dt.Maximum        :dd.Maximum send or receive rate of all nodes in the group.
:dt.Sum of rates   :dd.Accumulated rates values of all nodes in the group.
:dt.Average values :dd.Average values of rates of all nodes in the group.
:edl.
:euserdoc.
