# TaskAccelerator - gem5 DMA-Enabled Accelerator

A production-ready custom accelerator for gem5 that demonstrates DMA operations, register interfaces, and task offloading with comprehensive testing.

## ✨ Features

- **🚀 DMA Interface**: High-performance direct memory access for efficient data transfers
- **⚡ Multiple Task Types**: NOP, COPY, ADD, MULTIPLY, FILTER operations with extensible framework
- **📊 Memory-Mapped Registers**: Complete MMIO interface with 64-byte register space
- **🔔 Interrupt Support**: Completion notifications to CPU with configurable interrupts
- **🔧 Fully Integrated**: Seamless gem5 integration with automated build system
- **✅ Verified**: Comprehensive test suite validating all functionality

## 🎯 Quick Start

### 1. Enter Development Environment
```bash
make enter
```
This launches the gem5 container with all dependencies.

### 2. Build gem5 with TaskAccelerator
```bash
make build-gem5
```
Automatically copies accelerator files and builds gem5 with full integration.

### 3. Run Tests
```bash
# Run individual tests
make test-simple     # Basic validation
make test-register   # Register interface test
make test-dma        # DMA functionality test

# Or run all tests
make test-all
```

## 📁 Project Structure

### **Source Files** (Main Development)
```
accelerator_files/           # ← Edit these files for development
├── task_accelerator.cc      # Main C++ implementation
├── task_accelerator.hh      # Header with class definition
├── TaskAccelerator.py       # Python configuration interface
├── SConscript              # Build configuration
└── README.md               # Documentation
```

### **Test Programs**
```
├── simple_test.c            # Basic validation test
├── register_test.c          # Register interface test
├── dma_test.c              # DMA functionality test
├── test_accelerator.c       # Full accelerator test
├── test_config.py          # Basic gem5 configuration
└── dma_test_config.py      # DMA-specific configuration
```

### **Build System**
```
├── Makefile                # Automated build and test commands
├── .gitignore             # Git ignore rules for clean repo
└── README.md              # This file
```

## 🛠️ Available Make Commands

### **Environment**
- `make enter` - Enter gem5 development container
- `make help` - Show all available commands

### **Building**
- `make setup-accelerator` - Copy accelerator files to gem5 source
- `make build-gem5` - Build gem5 with TaskAccelerator (auto-copies files)
- `make clean-gem5` - Clean gem5 build files

### **Testing**
- `make test-simple` - Quick validation (accelerator loads properly)
- `make test-register` - Register interface test (with debug output)
- `make test-dma` - DMA functionality test (with debug output)
- `make test-all` - Run complete test suite
- `make clean` - Remove compiled test binaries

### **Development**
- `make native` - Compile test program for native architecture
- `make arm` - Cross-compile for ARM 32-bit
- `make aarch64` - Cross-compile for ARM 64-bit

## 🗺️ Register Map

| Offset | Name | Access | Description |
|--------|------|--------|-------------|
| 0x00 | TASK_CTRL | RW | Control: START(0x04), RESET(0x02), ENABLE(0x01) |
| 0x04 | TASK_STATUS | RO | Status: ERROR(0x08), COMPLETE(0x04), BUSY(0x02), IDLE(0x01) |
| 0x08 | TASK_CMD | RW | Command type (0-5: NOP, COPY, ADD, MULTIPLY, FILTER, TRANSFORM) |
| 0x0C | TASK_ARG0 | RW | First operation argument |
| 0x10 | TASK_ARG1 | RW | Second operation argument |
| 0x14 | TASK_ARG2 | RW | Third operation argument |
| 0x18 | SRC_ADDR_LOW | RW | Source DMA address (lower 32 bits) |
| 0x1C | SRC_ADDR_HIGH | RW | Source DMA address (upper 32 bits) |
| 0x20 | DST_ADDR_LOW | RW | Destination DMA address (lower 32 bits) |
| 0x24 | DST_ADDR_HIGH | RW | Destination DMA address (upper 32 bits) |
| 0x28 | DATA_SIZE | RW | Size of data to process (bytes) |
| 0x2C | DMA_CTRL | RW | DMA control register |
| 0x30 | DMA_STATUS | RO | DMA status (0x1 = active) |
| 0x34 | INT_ENABLE | RW | Interrupt enable mask |
| 0x38 | INT_STATUS | RW | Interrupt status (write 1 to clear) |

**Base Address**: `0x40000000` (configurable in test configurations)

## ⚙️ Task Commands

| Command | Value | Description | Arguments |
|---------|-------|-------------|-----------|
| CMD_NOP | 0x00 | No operation (test interface) | None |
| CMD_COPY | 0x01 | Memory copy | src→dst |
| CMD_ADD | 0x02 | Add constant to each 32-bit word | ARG0 = addend |
| CMD_MULTIPLY | 0x03 | Multiply each 32-bit word | ARG0 = multiplier |
| CMD_FILTER | 0x04 | Threshold filter on bytes | ARG0 = threshold |
| CMD_TRANSFORM | 0x05 | Reserved for future use | TBD |

## 🏗️ Architecture

The accelerator extends gem5's `DmaDevice` class with full PIO and DMA capabilities:

```
┌─────┐    ┌─────────────┐    ┌─────────────────┐
│ CPU │◄──►│ Memory Bus  │◄──►│ TaskAccelerator │
└─────┘    └─────────────┘    │                 │
               │               │  ┌─────────────┐ │
               │               │  │ Registers   │ │
               │               │  │ (64 bytes)  │ │
               │               │  └─────────────┘ │
               │               │                 │
               │               │  ┌─────────────┐ │
               ▼               │  │ DMA Engine  │ │
        ┌─────────────┐        │  └─────────────┘ │
        │   Memory    │◄───────┴─────────────────┘
        │ Controller  │
        └─────────────┘
```

### DMA Operation Flow
1. **Setup**: CPU writes task parameters to registers via PIO
2. **Start**: CPU writes CTRL_START to TASK_CTRL register
3. **DMA Read**: Accelerator DMAs source data to internal buffer
4. **Process**: Accelerator executes command on data
5. **DMA Write**: Accelerator DMAs results to destination
6. **Complete**: Accelerator sets COMPLETE status and triggers interrupt

## 🧪 Test Results

All tests validate different aspects of the accelerator:

### **test-simple**: Basic Integration ✅
```
✓ Accelerator creation and initialization
✓ Address range registration (0x40000000-0x40000040)
✓ Port connections and memory bus integration
✓ Clean simulation completion
```

### **test-register**: Register Interface ✅
```
✓ PIO read/write operations
✓ Register mapping and address decoding
✓ Debug output and tracing
✓ Error handling for invalid addresses
```

### **test-dma**: DMA Functionality ✅
```
✓ DMA read operations (81μs latency @ 1GHz)
✓ DMA write operations and completion events
✓ Memory interface with DDR3 controller
✓ Event scheduling and callback execution
```

## 🔧 Development Workflow

### **Editing Source Code**
1. Edit files in `accelerator_files/` directory
2. Run `make build-gem5` (automatically copies and builds)
3. Test with `make test-all`

### **Adding New Features**
1. Update `task_accelerator.hh` with new registers/commands
2. Implement logic in `task_accelerator.cc`
3. Add test cases to existing test programs
4. Verify with `make test-all`

### **Debugging**
```bash
# Enable accelerator debug output
make test-register  # Shows register access patterns

# Enable DMA debug output
make test-dma       # Shows DMA timing and memory operations

# Full debug (both accelerator and DMA)
include/gem5/build/X86/gem5.opt --debug-flags=TaskAccelerator,DMA config.py program
```

