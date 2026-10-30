# Linux Device Tree for Cache Coherent Accelerator Design
This device tree demonstrates how to setup a device tree for Linux to map FPGA hardware registers and how to use Device Tree Specification properties to leverage FPGA features

## Summary of Key Device Tree Properties
- `interrupts` - Expose the interrupt connection to Linux, see the Device Tree Specification for more info on the fields. Cross-reference with HPS technical reference to find the corresponding interrupt ID (second element in the interrupt property for this device tree)
- `dma-coherent` - Necessary when performing coherent DMA transactions, allows Linux APIs to use correct path to allocate cacheable, write-back buffers
- `iommus` - Necessary when the SMMU is enabled in your design, cross-reference with Qsys in order to find the correct StreamID for your device (0xF in this design)