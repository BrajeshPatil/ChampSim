# ChampSim
ChampSim is a trace-based simulator for a microarchitecture study. This project extends ChampSim with support for actual memory data values in instruction traces and during simulation.

## Project Goals

- Extend the ChampSim trace format to record actual memory read and write data.
- Capture memory data during trace generation and reconstruct it during trace replay.
- Propagate data through the processor and cache request/response paths.
- Support reading data on loads and updating data on stores.

## Trace Format
The extended trace format retains the existing instruction metadata and appends the actual memory data associated with each instruction.

Each trace record consists of:
1. Fixed-size instruction record of type input_instr
2. Bytes read by the instruction.
3. Bytes written by the instruction.

## Download Dependencies

Initialize and install the dependencies:

```bash
git submodule update --init
vcpkg/bootstrap-vcpkg.sh
vcpkg/vcpkg install
```

## Compile

Configure ChampSim using a JSON configuration file:

```bash
./config.sh champsim_config.json
make
```

## Generate Traces
Download and Extract Intel Pin Tool
```
wget "https://software.intel.com/sites/landingpage/pintool/downloads/pin-external-4.4-99977-g54fbb8814-gcc-linux.tar.gz" -O pin.tar.gz && tar -xzf pin.tar.gz
```

Export Path to directory
```
export PIN_ROOT="/path/to/pin-external-4.4-99977-g54fbb8814-gcc-linux"
```

Verify version and presence of files
```
"$PIN_ROOT/pin" -version
ls "$PIN_ROOT/source/tools/Config"
```

Change directory to tracer/pin and build
```
cd tracer/pin
make
```

Verify Build
```
ls -lh obj-intel64/champsim_tracer.so
```

Generate a trace starting from main (Run from ChampSim Root)
```
"$PIN_ROOT/pin" -t tracer/pin/obj-intel64/champsim_tracer.so -o test.trace -start_symbol main -t <num_of_instructions> -- ./path/to/executable
```

# Run simulation
**Important:** Use a trace generated in the extended data-supported format.

```
$ bin/champsim --warmup-instructions 200000000 --simulation-instructions 500000000 ~/path/to/trace
```
The number of warmup and simulation instructions given will be the number of instructions retired. Note that the statistics printed at the end of the simulation include only the simulation phase.
