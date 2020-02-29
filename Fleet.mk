# Include directories
FLEET_INCLUDE=-I../../src/ -I../../src/Inference -I../../src/Hypotheses -I../../src/VirtualMachine -I../../src/Statistics

# Some standard/default flags
FLEET_FLAGS=-std=gnu++17 -Wall -fdiagnostics-color=auto -Wimplicit-fallthrough  -Wall -Wextra -Wextra-semi -Wpedantic -Wvla -Wnull-dereference -Wswitch-enum -Wno-unused-parameter -Wno-vla -fvar-tracking-assignments -Wduplicated-cond -Wduplicated-branches -Wsuggest-override -pthread -march=native -ftemplate-backtrace-limit=0 -fstack-protector-strong --param max-inline-insns-recursive=100000

## TODO: ADD  -Werror 
