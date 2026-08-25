import m5
from m5.objects import *
from m5.objects import Cache
import argparse
import shlex
import os
print("Current Working Directory:", os.getcwd())




# Argument Parser
parser = argparse.ArgumentParser(description="Gem5 SE Simulation")
parser.add_argument("--cmd", type=str, required=True, help="Path to the benchmark ELF binary")
parser.add_argument("--args", type=str, default="", help="Arguments for the benchmark (quoted string)")
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

system.clk_domain = SrcClockDomain()
system.clk_domain.clock = '3GHz'
system.clk_domain.voltage_domain = VoltageDomain()

system.mem_mode = 'timing'
system.mem_ranges = [AddrRange('32GiB')]

system.membus = SystemXBar()
system.l1_to_l2bus = L2XBar()
system.l2_to_l3bus = L2XBar()

#system.cpu = DerivO3CPU()
system.cpu = TimingSimpleCPU()
system.cpu.numThreads = 1

system.cpu.createInterruptController()
system.cpu.interrupts[0].pio = system.membus.mem_side_ports
system.cpu.interrupts[0].int_requestor = system.membus.cpu_side_ports
system.cpu.interrupts[0].int_responder = system.membus.mem_side_ports

system.cpu.icache = L1ICache()
system.cpu.dcache = L1DCache()

system.cpu.icache.cpu_side = system.cpu.icache_port
system.cpu.dcache.cpu_side = system.cpu.dcache_port

system.cpu.icache.mem_side = system.l1_to_l2bus.cpu_side_ports
system.cpu.dcache.mem_side = system.l1_to_l2bus.cpu_side_ports

system.l2cache = L2Cache()
system.l2cache.cpu_side = system.l1_to_l2bus.mem_side_ports
system.l2cache.mem_side = system.l2_to_l3bus.cpu_side_ports

system.l3cache = L3Cache()
system.l3cache.cpu_side = system.l2_to_l3bus.mem_side_ports
system.l3cache.mem_side = system.membus.cpu_side_ports

system.mem_ctrl = MemCtrl()
system.mem_ctrl.dram = DDR4_2400_8x8()
system.mem_ctrl.dram.range = system.mem_ranges[0]
system.mem_ctrl.dram.read_buffer_size = 1024
system.mem_ctrl.dram.write_buffer_size = 1024
system.mem_ctrl.port = system.membus.mem_side_ports

system.system_port = system.membus.cpu_side_ports

# Workload (SE mode)
system.workload = SEWorkload.init_compatible(args.cmd)
process = Process()
process.executable = args.cmd
process.cwd = os.getcwd()

argv = [args.cmd] + (shlex.split(args.args) if args.args else [])
process.cmd = argv

system.cpu.workload = process
system.cpu.createThreads()

root = Root(full_system=False, system=system)
m5.instantiate()

print("Starting simulation...")
exit_event = m5.simulate()
print(f"Exit @ tick {m5.curTick()} because {exit_event.getCause()}")

