# SDAccel command script.

# Define a solution name.
create_solution -name conv1 -dir FPGA -force

# Define the target platform of the application
add_device -vbnv xilinx:adm-pcie-7v3:1ddr:2.0

# Host source files.
add_files "main.cpp"

# Header files.
add_files "convolution.hpp"
set_property file_type "c header files" [get_files "convolution.hpp"]

add_files "util.hpp"
set_property file_type "c header files" [get_files "util.hpp"]

# Create the kernel.
create_kernel conv1 -type clc
add_files -kernel [get_kernels conv1] "conv1.cl"

# Define binary containers.
create_opencl_binary alpha
set_property region "OCL_REGION_0" [get_opencl_binary alpha]
create_compute_unit -opencl_binary [get_opencl_binary alpha] -kernel [get_kernels conv1] -name CONV1

# Compile the design for CPU based emulation.
compile_emulation -flow cpu -opencl_binary [get_opencl_binary alpha]

# Generate the system estimate report.
report_estimate

# Run the design in CPU emulation mode
run_emulation -flow cpu -args

build_system

package_system