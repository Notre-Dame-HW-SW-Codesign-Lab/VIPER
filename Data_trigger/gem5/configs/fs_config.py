import m5
from m5.objects import *
from m5.objects import Cache
import argparse

# Argument Parser
parser = argparse.ArgumentParser(description="Gem5 Full-System Simulation")
parser.add_argument("--cmd", type=str, default=None, help="Path to the benchmark binary")
args = parser.parse_args()

# Define cache classes
class L1Cache(Cache):
    assoc = 8
    tag_latency = 2
    data_latency = 2
    response_latency = 2
    mshrs = 4
    tgts_per_mshr = 20

class L1ICache(L1Cache):
    size = '32KiB'

class L1DCache(L1Cache):
    size = '32KiB'

class L2Cache(Cache):
    size = '256KiB'
    assoc = 8
    tag_latency = 10
    data_latency = 10
    response_latency = 10
    mshrs = 16
    tgts_per_mshr = 12

class L3Cache(Cache):
    size = '8MiB'
    assoc = 16
    tag_latency = 30
    data_latency = 30 
    response_latency = 30
    mshrs = 32
    tgts_per_mshr = 20

# Create system
system = System()

# Set up a 64-bit X86 simulation
system.clk_domain = SrcClockDomain()
system.clk_domain.clock = '3GHz'  # CPU clock
system.clk_domain.voltage_domain = VoltageDomain()

system.mem_mode = 'timing'  # Use timing mode for memory
system.mem_ranges = [AddrRange('32GiB')]  # 32GB DDR4 memory

# Memory Bus
system.membus = SystemXBar()

# Create inter-cache buses
system.l1_to_l2bus = L2XBar()
system.l2_to_l3bus = L2XBar()

# Fix: Use DerivO3CPU() instead of X86O3CPU()
system.cpu = DerivO3CPU()  # Simpler out-of-order CPU
system.cpu.numThreads = 1

# Fix: Add Interrupt Controller
system.cpu.createInterruptController()
system.cpu.interrupts[0].pio = system.membus.mem_side_ports
system.cpu.interrupts[0].int_requestor = system.membus.cpu_side_ports
system.cpu.interrupts[0].int_responder = system.membus.mem_side_ports

# Instantiate L1 caches
system.cpu.icache = L1ICache()
system.cpu.dcache = L1DCache()

# Connect L1 Caches to CPU Ports
system.cpu.icache.cpu_side = system.cpu.icache_port
system.cpu.dcache.cpu_side = system.cpu.dcache_port

# Connect L1 Caches to L1-to-L2 Bus
system.cpu.icache.mem_side = system.l1_to_l2bus.cpu_side_ports
system.cpu.dcache.mem_side = system.l1_to_l2bus.cpu_side_ports

# Instantiate and connect L2 Cache
system.l2cache = L2Cache()
system.l2cache.cpu_side = system.l1_to_l2bus.mem_side_ports
system.l2cache.mem_side = system.l2_to_l3bus.cpu_side_ports

# Instantiate and connect L3 Cache
system.l3cache = L3Cache()
system.l3cache.cpu_side = system.l2_to_l3bus.mem_side_ports
system.l3cache.mem_side = system.membus.cpu_side_ports  # L3 to memory bus

# DDR4 Memory Controller
system.mem_ctrl = MemCtrl()
system.mem_ctrl.dram = DDR4_2400_8x8()
system.mem_ctrl.dram.range = system.mem_ranges[0]

system.mem_ctrl.dram.read_buffer_size = 1024  # Number of entries in read queue
system.mem_ctrl.dram.write_buffer_size =1024 # Number of entries in write queue

#system.mem_ctrl.dram.tCL = '45ns'  # Add 45ns to CAS Latency
#system.mem_ctrl.dram.tRCD = '683ns'  # Add 45ns to Row-to-Column Delay
#system.mem_ctrl.dram.tRP = '480ns'  # Add 45ns to Row Precharge Time



system.mem_ctrl.port = system.membus.mem_side_ports

# Connect CPU to Memory Bus
system.system_port = system.membus.cpu_side_ports

# Fix: Ensure correct workload initialization
if args.cmd:
    system.workload = SEWorkload.init_compatible(args.cmd)
    process = Process()
    process.executable = args.cmd
    process.cwd = "/"
    process.cmd = [args.cmd] + (args.args.split() if args.args else []) # Pass the binary as the command
    system.cpu.workload = process
    system.cpu.createThreads()  # Fix: Ensure CPU threads are created

# System Simulation
root = Root(full_system=False, system=system)  # Not full-system mode
m5.instantiate()

print("Starting simulation...")
exit_event = m5.simulate()

print(f"Exit @ tick {m5.curTick()} because {exit_event.getCause()}")

