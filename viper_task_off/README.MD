# CPU Performance Formula

Collects hardware performance counters using `perf` and evaluates:

```
CPU_time = I_CPU × CPI × t_clock
Where: CPI = CPI_exec + m_CPU × AMAT
```
## Usage
```bash
sudo ./collect_metrics.sh <command>
```

## Output

 Creates `perf_metrics_<timestamp>.txt` with raw counters and calculated formula components.
