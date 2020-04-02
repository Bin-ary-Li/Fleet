include ../../Fleet.mk

all:
	g++-9 Main.cpp -o main -lm -O2 $(FLEET_FLAGS) $(FLEET_INCLUDE)

debug:
	g++-9 Main.cpp -o main -lm -g $(FLEET_FLAGS) $(FLEET_INCLUDE)
profiled:
	g++-9 Main.cpp -o main -lm -O2 -pg -g $(FLEET_FLAGS) $(FLEET_INCLUDE)
	
