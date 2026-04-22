# dahdi_loop

A loopback DAHDI driver for testing on hosts without real DAHDI hardware. Spans are wired in pairs: data written to one end is read back from the other.

## Loading the module

```bash
modprobe dahdi_loop
```

## Module parameters

| Parameter | Type | Default | Description |
|---|---|---|---|
| `num_loops` | int | 1 | Number of loopback span pairs to create |
| `num_taps` | int | 1 | Number of tap span pairs to create |
| `debug` | int | 0 | Debug flags (bit 0: general, bit 1: ticks) |
| `alarm_sim_type` | uint | 9 | Alarm flags set when simulating an alarm |

## Simulating alarms

Use `dahdi_maint` with the `-i sim` option to toggle an alarm on a span:

```bash
# Set alarm on span 1
dahdi_maint -s 1 -i sim

# Clear alarm on span 1 (call again to toggle off)
dahdi_maint -s 1 -i sim
```

The alarm type is controlled by the `alarm_sim_type` parameter. The value is a bitmask of DAHDI alarm flags:

| Value | Alarm |
|---|---|
| 0x1 | `DAHDI_ALARM_LOS` — Loss of Signal |
| 0x4 | `DAHDI_ALARM_YELLOW` |
| 0x8 | `DAHDI_ALARM_RED` |
| 0x9 | `DAHDI_ALARM_RED` + `DAHDI_ALARM_LOS` (default) |

### Simulate a red alarm (default)

```bash
modprobe dahdi_loop
dahdi_maint -s 1 -i sim
```

### Simulate a yellow alarm

```bash
modprobe dahdi_loop alarm_sim_type=4
dahdi_maint -s 1 -i sim
```

### Change alarm type at runtime without reloading

```bash
echo 4 > /sys/module/dahdi_loop/parameters/alarm_sim_type
```
