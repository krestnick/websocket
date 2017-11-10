A video says more like a thousand pictures http://vimeo.com/242098558

The demo setup is a Standard PIC32MZ Embedded Connectivity with FPU (EF) Starter Kit: Part Number: DM320007

Datasheet: http://ww1.microchip.com/downloads/en/DeviceDoc/70005230B.pdf

Order at: https://octopart.com/search?q=%20DM320007&start=0

A USB cable Mini B for programming and power supply and a RJ45, which is switched from the PIC board directly to the PC. No switch or network necessary. (If necessary, another mini B for UART debug issues (is chic, but also goes without))

You can see the standard website from the Harmony TCPIP Stack. So you can see how an embedded webserver looks like.
Extended with a so-called Websocket. This can be used to create an asynchronous connection between a Javascript based web interface and the embedded client. 

It is shown how a data stream generated in the PIC (at first only a noise) and is transmitted to the browser and displayed there graphically (Flot with JQuery). Pressing the switch on the board turns the noise into a ramp.

An online oscilloscope, so to speak.

Simple setup with a standard tool. Would have to work everywhere.

The IP address is the AutoIP address 169.254.97.179 with the network mask 255.255.0.0

Also, the NetBIOS name http://MCHPBOARD_E would have to work in the browser immediately.

This would have a Windows computer can connect directly immediately if no DHCP is involved.
If the board is plugged into a network with DHCP then the PIC fetches an address from the DHCP. Then also works http://MCHPBOARD_E
