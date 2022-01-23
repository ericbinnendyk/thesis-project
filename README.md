# thesis-project
The programming project I am working on for my master's thesis, about using reinforcement learning to improve network performance.

For now, this is just a backup of the actual working directory I use.

Prerequisites for running the code are C++ and Python 3 ZeroMQ libraries, Numpy, Tensorflow, and Keras.

NOTE: This code doesn't compile yet because ndnSIM compilation seems to be dependent on the code being split into submodules.

To compile the code, download Anju James and George Torres's original ndnQoS code at https://github.com/nsol-nmsu/ndnQoS. Then replace the directories src/ndnSIM/examples, src/ndnSIM/apps, and src/ndnSIM/NFD/daemon/fw with the ones in this code. Also, add train-RL-model.py and plot_network_performance.r to the ns-3 directory.

To run the code, enter the command "./waf --run=<simulation name>". Then, in separate terminals, run one instance of "python3 train-RL-model.py <ID>" for each router node with the QoS strategy installed, where <ID> is the ID of the node.

The file plot_network_performance.r can be run in RStudio to plot network performance metrics.
