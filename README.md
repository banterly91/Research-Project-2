# Research-Project-2
IPOP profiling results and source code of the implementations described in the paper

Most of the code was not developed by me. I only introduced functionalities described in the paper and mentioned in comments in the source files. The original project repository can be found [here](https://github.com/ipop-project). The project homepage can be found [here](http://ipop-project.org/).


###This repository contains the following source files:

####For selective security functionality:

- gvpn_controller.py - controller code for IPOP containing the implementation of selective security.
- ipoplib.py - code required by the controller. Also contains part of the implementation for selective security.
- tincanconnectionmanager.cc - IPOP-Tincan code containing the implementation of selective security.

 
	

####Reimplementation of packet forwarding:	
- packetioserial.c - The original implementation of the code handling packet forwarding in IPOP-Tap
- packetioparalel.c - Parallel implementation of the Producer-Consumer pattern without mutexes.  
- packetioparalelconditionalsignal.c - Parallel implementation using mutexes and conditional signals. Still needs work, race conditions are detected in some situations.
- packetioasinchronous.c - Asynchronous parallel implementation 

	
	
###This repository also contains the following profiling results:

####Oprofile results on the receiver side
- reciperf - profiling iperf over direct connections
- reciperfgraph - callgraph for the above
- reciperft - profiling iperf over IPOP connections
- reciperftgraph - callgraph for the above
- rectincan - profiling tincan
- rectincangraph - callgraph for the above
- recwide - system wide profiling without IPOP
- recwidegraph - callgraph for the above
- recwidet - system wide profiling with IPOP
- recwidetgraph - callgraph for the above
	
	
####Oprofile results on the sender side	
- sendiperf - profiling iperf over direct connections
- sendiperfgraph - callgraph for the above
- sendiperft - profiling iperf over IPOP connections
- sendiperftgraph - callgraph for the above
- sendtincan - profiling tincan
- sendtincangraph - callgraph for the above
- sendwide - system wide profiling without IPOP
- sendwidegraph - callgraph for the above
- sendwidet - system wide profiling with IPOP
- sendwidetgraph -callgraph for the above
	


####Zoom results on the receiver side:
- recvipop.zoom - system wide profiling with ipop
- recvnoipop.zoom - system wide profiling without IPOP


####Zoom results on the sender side:
* sendipop.zoom - system wide profiling with ipop
* sendnoipop.zoom - system wide profiling without IPOP

