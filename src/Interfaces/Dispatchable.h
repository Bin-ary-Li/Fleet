#pragma once 

template<typename t_input, typename t_return>
class VirtualMachineState;

template<typename t_x, typename t_return>
class VirtualMachinePool;

template<typename t_input, typename t_output>
class Dispatchable {
public:
	// A dispatchable class is one that implements the dispatch rule we need in order to call/evaluate.
	// This is the interface that a Hypothesis requires
	virtual abort_t dispatch_rule(Instruction i, VirtualMachinePool<t_input,t_output>* pool, VirtualMachineState<t_input,t_output>& vms,
                                  Dispatchable<t_input,t_output>* loader )=0;
	
	// This loads a program into the stack. Short is passed here in case we have a factorized lexicon,
	// which for now is a pretty inelegant hack. 
	virtual void push_program(Program&, short)=0;
};

