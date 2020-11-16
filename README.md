# reMap
A tool for reverse engineering the DRAM address mapping of Intel processors based on performance counters.
A description of the method for reverse engineering can be found in our paper:

Christian Helm, Soramichi Akiyama, Kenjiro Taura.
**Reliable Reverse Engineering of Intel DRAM Addressing Using Performance Counters.**
IEEE International Symposium on Modeling, Analysis, and Simulation of Computer and Telecommunication Systems (MASCOTS), November 2020. [PDF](http://www.soramichi.jp/pdf/MASCOTS2020.pdf)


It only works on processors that have the necessary IMC performance counters and currently only supports systems with at most 4 memory channels.
This tool requires root privilege and must be pinned to one socket.
To compile simply use make.

Typical usage: sudo numactl --cpubind=0 --membind=0 ./reMap

Command line options:
* -v Verbose output useful for debugging.
* -r Resolve the addressing function assuming multiple target address decoder (TAD) regions.
This mode requires access to configuration registers. It is only supported on Intel Haswell and Broadwell systems.
* -s <size in GB> Sets the size of the memory pool from which allocations are sampled. Default is 20 GB.
A bigger pool can help to cover more bits and reduce the number of unknown bits in the results.
* -n <number of samples> Sets the number of address samples that are collected. Default is 1000 samples.
An increased number can help to cover more bits and reduce the number of unknown bits in the results.
* -a <number of accesses> Sets the number of repeated accesses to each component. Default is 2000 accesses.
A higher number of accesses can improve reliability on noisy systems

Better results can be achieved using huge pages. They can be set up using hugeadm:

* sudo hugeadm --pool-pages-max 2m:40G                                                       
* sudo hugeadm --pool-pages-min 2m:1G                                                        
* sudo hugeadm --create-mounts 
* hugeadm --explain
