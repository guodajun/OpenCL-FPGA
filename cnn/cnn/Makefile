
CLC = sdaccel
CC = touch

KERNELDIR = kernel
OUTPUTDIR = FPGA

TARGETS = host
TARGETS += conv1_baseline conv1_tile conv1
TARGETS += conv1_multi_cu
TARGETS += pool2
TARGETS += conv3_tile
TARGETS += conv5_tile
TARGETS += full6
TARGETS += rbf7
TARGETS += lenet5 lenet5_mcu lenet5_final

TCLS = $(addsuffix .tcl, $(TARGETS))
OBJS = $(addsuffix .o, $(TARGETS))

TCLFILES = $(addprefix, $(KERNELDIR)/, $(TCLS))
OBJFILES = $(addprefix, $(OUTPUTDIR)/, $(OBJS))

BINDIR = pkg/pcie
BIN = $(BINDIR)/alpha.xclbin
HOST = ./$(OUTPUTDIR)/host/$(BINDIR)/host.exe


all: $(TCLFILES) $(OBJS)

$(OBJS): $(TCLFILES)
	$(CLC) $(KERNELDIR)/$(@:.o=.tcl)
	$(CC) $(OUTPUTDIR)/$@

$(TARGETS): $(OUTPUTDIR)/host.o
	$(HOST) $(KERNELDIR)/$@.xml $(KERNELDIR)/result_$@.xml $(OUTPUTDIR)/$@/$(BIN)
	
clean: 
	rm -rf $(addprefix $(OUTPUTDIR)/, $(TARGETS))
	rm $(OBJFILES)
