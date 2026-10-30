# CCA Linux App

Shows how an application might use the Cache Coherent Accelerator Linux driver

## Application Flow

- Open fd to device (get device context)
- Allocate space for input/output data via driver IOCTL
- Map input/output data buffer to userspace via mmap
- Fill input data buffers (optionally zero output buffers, typically handled by kernel on alloc)
- Submit job chain via IOCTL
- Wait for operations to complete and data to be written via IOCTL
- Read output data back and verify correctness
- Unmap and deallocate buffers
- Close fd to device and exit
