# C DSM : distributed and shared memory system in C

This project was made in collaboration with Julien MATHET as school project.
This project contains several programs and handlers to run an external program, using the DSM library or not, and distribute its execution on different machines. Along with execution distribution, a virtual memory space is created and maintained between the different machines.


## Usage
The C DSM system is launched using the dsmexec executable. 
The structure is: ``dsmexec <machine_file> <external_program> <external_program args>`` whereas machine_file is a text file containing hostnames or IPs of others machines within the same network.

## Assessment

This 3-month project is now finished and was assessed by our teachers.
