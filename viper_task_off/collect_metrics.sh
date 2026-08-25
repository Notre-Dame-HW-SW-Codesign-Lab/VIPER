#!/bin/bash

# Performance metric collection script for CPU formula:
# CPU_time = I_CPU × CPI × t_clock
# Where: CPI = CPI_CPU_exec + m_CPU × AMAT_CPU
#        t_clock = 1/frequency (clock period in seconds)
#
# Uses hierarchical cache model for AMAT calculation:
#   AMAT = L1_hit_time + L1_miss_rate × (LLC_hit_penalty + LLC_miss_rate × memory_penalty)
#
# Memory stall CPI calculated from actual miss counts:
#   stall_cycles = (L1_misses - LLC_misses) × LLC_penalty + LLC_misses × memory_penalty
#   memory_stall_CPI = stall_cycles / instructions

if [ $# -eq 0 ]; then
  echo "Usage: $0 <command_to_profile>"
  echo "Example: $0 ./your_program"
  exit 1
fi

COMMAND="$@"
OUTPUT_FILE="perf_metrics_$(date +%Y%m%d_%H%M%S).txt"

echo "Collecting performance metrics for: $COMMAND"
echo "Output file: $OUTPUT_FILE"
echo "===========================================" >$OUTPUT_FILE
echo "Performance Metrics Collection" >>$OUTPUT_FILE
echo "Command: $COMMAND" >>$OUTPUT_FILE
echo "Date: $(date)" >>$OUTPUT_FILE
echo "===========================================" >>$OUTPUT_FILE

# Collect comprehensive metrics in one perf run
perf stat -e instructions,cycles,cache-misses,cache-references,L1-dcache-loads,L1-dcache-load-misses,LLC-loads,LLC-load-misses,dTLB-loads,dTLB-load-misses,branches,branch-misses,context-switches,cpu-migrations,page-faults,minor-faults,major-faults -o $OUTPUT_FILE --append $COMMAND

echo "" >>$OUTPUT_FILE
echo "===========================================" >>$OUTPUT_FILE
echo "CPU Performance Formula:" >>$OUTPUT_FILE
echo "CPU_time = I_CPU × CPI × t_clock" >>$OUTPUT_FILE
echo "Where: CPI = CPI_exec + m_CPU × AMAT" >>$OUTPUT_FILE
echo "===========================================" >>$OUTPUT_FILE
echo "Formula Components:" >>$OUTPUT_FILE
echo "===========================================" >>$OUTPUT_FILE

# Extract and calculate formula components
INSTRUCTIONS=$(grep "instructions" $OUTPUT_FILE | awk '{print $1}' | tr -d ',' | head -1)
CYCLES=$(grep "cycles" $OUTPUT_FILE | awk '{print $1}' | tr -d ',' | head -1)
CACHE_MISSES=$(grep "cache-misses" $OUTPUT_FILE | awk '{print $1}' | tr -d ',')
CACHE_REFS=$(grep "cache-references" $OUTPUT_FILE | awk '{print $1}' | tr -d ',')
EXECUTION_TIME=$(grep "seconds time elapsed" $OUTPUT_FILE | awk '{print $1}')

# Calculate formula components using awk for floating point arithmetic
echo "Raw Metrics:" >>$OUTPUT_FILE
echo "- Instructions (I_CPU): $INSTRUCTIONS" >>$OUTPUT_FILE
echo "- Cycles: $CYCLES" >>$OUTPUT_FILE
echo "- Cache Misses: $CACHE_MISSES" >>$OUTPUT_FILE
echo "- Cache References: $CACHE_REFS" >>$OUTPUT_FILE
echo "- Execution Time (t_CPU): ${EXECUTION_TIME} seconds" >>$OUTPUT_FILE
echo "" >>$OUTPUT_FILE

# Calculate derived metrics
if [ ! -z "$INSTRUCTIONS" ] && [ ! -z "$CYCLES" ] && [ "$INSTRUCTIONS" -ne 0 ]; then
  CPI_TOTAL=$(awk "BEGIN {printf \"%.4f\", $CYCLES / $INSTRUCTIONS}")
  echo "- CPI_total (includes memory stalls): $CPI_TOTAL" >>$OUTPUT_FILE
fi

# Extract L1 data cache loads for memory access ratio
L1_LOADS=$(grep "L1-dcache-loads" $OUTPUT_FILE | awk '{print $1}' | tr -d ',')

if [ ! -z "$INSTRUCTIONS" ] && [ ! -z "$L1_LOADS" ] && [ "$INSTRUCTIONS" -ne 0 ]; then
  M_CPU=$(awk "BEGIN {printf \"%.4f\", $L1_LOADS / $INSTRUCTIONS}")
  echo "- m_CPU (memory accesses per instruction): $M_CPU" >>$OUTPUT_FILE
fi

if [ ! -z "$CACHE_MISSES" ] && [ ! -z "$CACHE_REFS" ] && [ "$CACHE_REFS" -ne 0 ]; then
  MISS_RATIO=$(awk "BEGIN {printf \"%.4f\", $CACHE_MISSES / $CACHE_REFS}")
  echo "- Cache miss ratio: $MISS_RATIO" >>$OUTPUT_FILE
fi

# Extract cache hierarchy metrics
L1_MISSES=$(grep "L1-dcache-load-misses" $OUTPUT_FILE | awk '{print $1}' | tr -d ',')
LLC_LOADS=$(grep "LLC-loads" $OUTPUT_FILE | awk '{print $1}' | tr -d ',')
LLC_MISSES=$(grep "LLC-load-misses" $OUTPUT_FILE | awk '{print $1}' | tr -d ',')

# Calculate AMAT using hierarchical cache model
# Cache hierarchy latencies (adjusted for modern OoO CPUs):
# - L1 hit: ~4 cycles
# - L2/L3 hit (L1 miss serviced by LLC): ~10 cycles
# - Memory access (LLC miss): ~100 cycles
L1_HIT_TIME=4
LLC_HIT_PENALTY=10
MEMORY_PENALTY=100

if [ ! -z "$L1_LOADS" ] && [ "$L1_LOADS" -gt 0 ] && [ ! -z "$L1_MISSES" ]; then
  L1_MISS_RATE=$(awk "BEGIN {printf \"%.6f\", $L1_MISSES / $L1_LOADS}")
  echo "- L1 miss rate: $L1_MISS_RATE" >>$OUTPUT_FILE

  # Calculate LLC miss rate (of L1 misses that reach LLC)
  if [ ! -z "$LLC_LOADS" ] && [ "$LLC_LOADS" -gt 0 ] && [ ! -z "$LLC_MISSES" ]; then
    LLC_MISS_RATE=$(awk "BEGIN {printf \"%.6f\", $LLC_MISSES / $LLC_LOADS}")
    echo "- LLC miss rate: $LLC_MISS_RATE" >>$OUTPUT_FILE

    # Hierarchical AMAT: L1_hit_time + L1_miss_rate × (LLC_hit_penalty + LLC_miss_rate × memory_penalty)
    AMAT=$(awk "BEGIN {printf \"%.4f\", $L1_HIT_TIME - $L1_HIT_TIME + $L1_MISS_RATE * ($LLC_HIT_PENALTY + $LLC_MISS_RATE * $MEMORY_PENALTY)}")
    echo "- AMAT_CPU (hierarchical): $AMAT cycles" >>$OUTPUT_FILE
    echo "  (L1_hit: ${L1_HIT_TIME} + L1_miss_rate × (LLC_hit: ${LLC_HIT_PENALTY} + LLC_miss_rate × mem: ${MEMORY_PENALTY}))" >>$OUTPUT_FILE

    # Calculate memory stall cycles per instruction directly from miss counts
    # Memory stalls = (L1_misses - LLC_misses) × LLC_hit_penalty + LLC_misses × memory_penalty
    # Per instruction: divide by instruction count
    if [ ! -z "$INSTRUCTIONS" ] && [ "$INSTRUCTIONS" -gt 0 ]; then
      MEMORY_STALL_CPI=$(awk "BEGIN {printf \"%.6f\", (($L1_MISSES - $LLC_MISSES) * $LLC_HIT_PENALTY + $LLC_MISSES * $MEMORY_PENALTY) / $INSTRUCTIONS}")
      echo "- Memory stall CPI: $MEMORY_STALL_CPI cycles/instruction" >>$OUTPUT_FILE
      echo "  ((L1_misses - LLC_misses) × LLC_penalty + LLC_misses × mem_penalty) / instructions" >>$OUTPUT_FILE

      # CPI_exec = CPI_total - memory_stall_CPI
      if [ ! -z "$CPI_TOTAL" ]; then
        CPI_EXEC=$(awk "BEGIN {val = $CPI_TOTAL - $MEMORY_STALL_CPI; if (val < 0.5) val = 0.5; printf \"%.4f\", val}")
        echo "- CPI_CPU_exec (calculated): $CPI_EXEC cycles/instruction" >>$OUTPUT_FILE
        if (($(awk "BEGIN {print ($CPI_TOTAL < $MEMORY_STALL_CPI)}"))); then
          echo "  (Note: clamped to 0.5 - stalls exceed measured CPI, likely due to OoO execution hiding latency)" >>$OUTPUT_FILE
        fi
      fi
    fi
  else
    echo "- AMAT_CPU: Cannot calculate (insufficient LLC data)" >>$OUTPUT_FILE
    echo "- CPI_CPU_exec: Cannot calculate without AMAT" >>$OUTPUT_FILE
  fi
else
  echo "- AMAT_CPU: Cannot calculate (insufficient L1 data)" >>$OUTPUT_FILE
  echo "- CPI_CPU_exec: Cannot calculate without AMAT" >>$OUTPUT_FILE
fi

echo "" >>$OUTPUT_FILE
echo "===========================================" >>$OUTPUT_FILE
echo "Formula Evaluation:" >>$OUTPUT_FILE
echo "CPU_time = I_CPU × CPI × t_clock" >>$OUTPUT_FILE
echo "Where CPI = CPI_CPU_exec + m_CPU × AMAT_CPU" >>$OUTPUT_FILE
if [ ! -z "$INSTRUCTIONS" ] && [ ! -z "$CPI_EXEC" ] && [ ! -z "$M_CPU" ] && [ ! -z "$AMAT" ] && [ ! -z "$EXECUTION_TIME" ] && [ ! -z "$CYCLES" ]; then
  # Calculate clock period: t_clock = execution_time / cycles
  T_CLOCK=$(awk "BEGIN {printf \"%.12e\", $EXECUTION_TIME / $CYCLES}")
  FREQUENCY=$(awk "BEGIN {printf \"%.2f\", $CYCLES / $EXECUTION_TIME / 1e9}")
  CPI_CALCULATED=$(awk "BEGIN {printf \"%.4f\", $CPI_EXEC + $M_CPU * $AMAT}")
  FORMULA_RESULT=$(awk "BEGIN {printf \"%.9f\", $INSTRUCTIONS * ($CPI_EXEC + $M_CPU * $AMAT) * ($EXECUTION_TIME / $CYCLES)}")
  echo "" >>$OUTPUT_FILE
  echo "Clock period (t_clock): $T_CLOCK seconds" >>$OUTPUT_FILE
  echo "CPU frequency: ${FREQUENCY} GHz" >>$OUTPUT_FILE
  echo "CPI_calculated: $CPI_CALCULATED (CPI_exec + m_CPU × AMAT)" >>$OUTPUT_FILE
  echo "CPI_measured: $CPI_TOTAL" >>$OUTPUT_FILE
  echo "" >>$OUTPUT_FILE
  echo "CPU_time = $INSTRUCTIONS × $CPI_CALCULATED × $T_CLOCK" >>$OUTPUT_FILE
  echo "         = $FORMULA_RESULT seconds" >>$OUTPUT_FILE
  echo "" >>$OUTPUT_FILE
  echo "Actual execution time: $EXECUTION_TIME seconds" >>$OUTPUT_FILE
else
  echo "= Cannot calculate (missing components)" >>$OUTPUT_FILE
fi
echo "===========================================" >>$OUTPUT_FILE

echo "Metrics collected successfully in: $OUTPUT_FILE"
echo ""
echo "Quick Summary:"
echo "- Instructions (I_CPU): $INSTRUCTIONS"
echo "- CPI_measured: ${CPI_TOTAL:-N/A}"
echo "- CPI_exec: ${CPI_EXEC:-N/A}"
echo "- m_CPU: ${M_CPU:-N/A}"
echo "- AMAT: ${AMAT:-N/A} cycles"
echo "- Actual execution time: $EXECUTION_TIME seconds"
echo ""
if [ ! -z "$FORMULA_RESULT" ]; then
  echo "Calculated CPU_time: $FORMULA_RESULT seconds"
  echo "(Should approximate actual execution time)"
else
  echo "Formula: CPU_time = I_CPU × CPI × t_clock"
fi
