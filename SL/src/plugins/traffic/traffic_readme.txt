Sourece files of module:

traffic.c       Main interface functions of the module.
properties.c    Properties dialog for the module.
filter.c        Filter editor (rules of counting traffic).
tracewin.c      Trace window - packets passing through the node.
defflt.c        Creating an initial sample filter.
iptrace.c	The core of tracing network packets.
statcnt.c	Filter nodes management.


1. Traffic module
2. Filter
2.1. An example of the filter
2.2. Filter editor
3. Properties


1. Traffic module

In this list, you can see the output of the traffic filter. Rules statistics
calculations are defined in the filter editor that is called by the
"Filter..." entry of the context menu.

2. Filter

The filter is a treelike structure, each node of this structure sets parameters
for definite packets and accumulates statistics. A correctly configured filter
provides all required information. That depends on reasonableness of the
filter, how much useful and sufficient information will stand out.

Traffic counting is performed for each node and the traffic direction.
Data such as protocols, addresses and ports of a source and the receiver
is taken from the new packet and is checked by the filter, on corresponding 
nodes it changes the statistics "directly". 
Then the data of a source and receiver on various places is repeatedly checked 
by the filter, on corresponding nodes the "return" statistics changes. 
Thus, each rule has two counters of bytes: sent and received.

In the study of your filter it can be useful viewing real-time data about the
packages fall into a specific node. To do this, select the item "Trace" from
the context menu of any node.

2.1. An example of the filter.

Let's see a simple task of calculation of traffic the Internet-gate having
mail service (smtp) and a proxy-server. It is required to count statistics
for each user of a proxy-server and the traffic coming for a mail service 
from the Internet. The address of a local network has a Iš address of 192.168.33.0 
and a subnet mask of 255.255.255.0. The address of a gateway in a local network (lan1) -
is 192.168.33.1, the public address of the host on the Internet - (lan0)
195.72.224.229, port for SMTP - 25, port of the proxy-server - 8080. 
Local machine IPs (users): 192.168.33.2, 192.168.33.3 and 192.168.33.4.
The schematically rules of our sample filter will be as follows:

lan0 - (network interface to the Internet)

  1. Name: "Internet:SMTP"
     Source: any, Receiver: 195.72.224.229
     TCP port of the source: any, the receiver: 25

lan1 - (network interface to the local network)

  2. Name: "My proxy"
     Source: 192.168.33.0/255.255.255.0, Receiver: 192.168.33.1
     TCP port of the source: any, the receiver: 8080

    2.1. Name: "Proxy: Boss"
         Source: 192.168.33.2
         If the node has a match  - do not check subsequent nodes.

    2.2. Name: "Proxy: workstation A"
         Source: 192.168.33.3
         If the node has a match - do not check subsequent nodes.

    2.3. Name: "Proxy: workstation B"
         Source: 192.168.33.4

As we can see, the node 2 has enclosed nodes, it is made to not specify the
address and port of destination for each proxy-server user: the enclosed nodes
will be looked through only if conditions of a parental node were satisfied. It
not only simplifies the task of nodes, but also raises the productivity of
the filter: in general the nodes of users will not be looked through if the 
packet is not for a proxy-server on the internal network.

2.2. Filter editor

Filter is one or more of the trees in which the root node is the network
interface. Filter nodes are displayed on the left side of the window.

Properties of the current node except the name are displayed and edited in the
right pane of the filter editor window. Top-level nodes (network interfaces)
have no properties.

Create, move, and delete nodes executed by buttons located at the bottom of the
window.

Name.

Name of the node does not influence the filter, however it is used for 
the statistics display in the two-levels structure in the main window list.
The node list is made up of groups. The group name is specified in the node
name before the colon. So, the following evident structure will be given
to the user in our example:

  [-] Internet
        SMTP
  [-] Proxy
        Boss
        workstation A
        workstation B

If name ends with colon (node 2 "My proxy:" from the example) the statistics of the
given node is not displayed in main window.

To change the node name hold CTRL and click or press ENTER on the node, alternative you 
can also use the context menu item "Rename".

Stop scan nodes at this level.

In the filter editor node there is a checkbox "Stop scan nodes at this level". When
it is checked and the node for new packet was triggered subsequent nodes will not be
tested with this packet. Children of triggered nodes will be tested anyway.

IP-address.

The IP-address in the lists of a source addresses and destination addresses can
be specified in the following notation:

  specific address: 192.168.1.2
  address/mask: 192.168.1.0/255.255.255.0
  address/mask bits number: 192.168.1.0/24
  range: 192.168.1.10 - 192.168.1.20

Entries of an address list are merged by the logical condition OR. Lists are 
checked from the beginning to the end. For performance reasons, it makes sense 
to address most frequent positioned closer to the top. The order of entries can 
be changed by the <+> plus key and <-> minus key on the numpad section of the 
keyboard. An empty list matches any address.

Ports.

The ports of UDP and TCP protocols are listed separated by commas and may
contain ranges, for example:
  80,81,8000-8080.
An empty list matches any port. These fields are only available for the TCP and
UDP protocols.

3. Properties

For a group of nodes which have a given way to count the total rates are displayed
in details view.

  Maximum -  maximum send or receive rate of all nodes in the group.
  Sum of rates - accumulated rates values of all nodes in the group.
  Average values - Average values of rates of all nodes in the group.
