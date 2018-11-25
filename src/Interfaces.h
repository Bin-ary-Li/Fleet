#ifndef INTERFACES_H
#define INTERFACES_H

#include "Miscellaneous.h"

template<typename t_input, typename t_return>
class VirtualMachineState;

template<typename t_x, typename t_return>
class VirtualMachinePool;

template<typename t_input, typename t_output>
class Dispatchable {
public:
	// A dispatchable class is one that implements the dispatch rule we need in order to call/evaluate.
	// This is the interface that a Hypothesis requires
	virtual t_abort dispatch_rule(op_t op, VirtualMachinePool<t_input,t_output>* pool, VirtualMachineState<t_input,t_output>* vms)=0;
	
	// This loads a program into the stack. Short is passed here in case we have a factorized lexicon,
	// which for now is a pretty inelegant hack. 
	virtual void push_program(Opstack&, short)=0;
};


// Just a clean interface to define what kinds of operations MCMC requires
// This also defines class types for data
// This also defines interfaces for storing hypotheses (equality, hash, comparison, etc)
template<typename t_input, typename t_output>
class Bayesable {
public:
	
	// We'll define a vector of pairs of inputs and outputs
	// this may be used externally to define what data is
	
	typedef struct {
		t_input input;
		t_output output;
		double   reliability; // the noise probability (typically required)
	} t_datum; // single data point
	typedef std::vector<t_datum> t_data; 

	// log values for all
	double prior; 
	double likelihood;
	double posterior;    

	Bayesable() : prior(0.0), likelihood(0.0), posterior(0.0) {
		
	}
	
	Bayesable(const Bayesable& b) : prior(b.prior), likelihood(b.likelihood), posterior(b.posterior) {
	}

	
	virtual double compute_prior() = 0; 
	virtual double compute_single_likelihood(const t_datum& datum) = 0;
	virtual double compute_likelihood(const t_data& data) {
		// defaultly a sum over datums in data (e.g. assuming independence)
		likelihood = 0.0;
		for(auto d : data) {
			likelihood += compute_single_likelihood(d);
		}
		return likelihood;		
	}
	
	virtual double compute_posterior(const t_data& data) {
		
		// Always compute a prior
		prior = compute_prior();
		
		// if the prior is -inf, then 
		if(prior == -infinity) {
			likelihood = NaN;
			posterior = -infinity;
		}
		else {		
			likelihood = compute_likelihood(data);
			posterior = prior + likelihood;	
		}
		
		return posterior;
	}
	
	virtual size_t hash() const=0;
	
	virtual bool operator<(const Bayesable<t_input,t_output>& l) const {
		// when we implement this, we defaulty sort by posterior (so that TopN works)
		// But we need to be careful because std::set also uses this to determine equality
		// so we will only 
		// This first checks  postersios, then priors (to handle -inf likelihoods well)
		if(posterior < l.posterior) {
			return true;
		}
		else if(l.posterior < posterior) {
			return false;
		}
		else if(prior < l.prior) {
			return true;
		}
		else if(l.prior < prior) {
			return false;
		}
		else {
			// we could be equal, but we only want to say that if we are. 
			// otherwise this is decided by the hash function 
			return this->hash() < l.hash();
		}
	}
};



template<typename HYP, typename t_input, typename t_output>
class MCMCable : public Bayesable<t_input,t_output> {
public:
	double fb; // forward minus backward probability -- from when I was created (assumed to be set in propose)

	MCMCable() : fb(0.0) {
		
	}

	MCMCable(const MCMCable<HYP,t_input,t_output>& h) : Bayesable<t_input,t_output>(h), fb(0.0) {// don't copy FB
		
	}

	virtual HYP* copy() const = 0;
	virtual HYP* propose() const = 0; // create a new HYP that is a proposal (and sets the new one's fb)
	virtual HYP* restart() const = 0; // restart a new chain -- typically by sampling from the prior 
};



// This class is used by MCTS and allows us to incrementally search a hypothesis
template<typename HYP, typename t_input, typename t_output>
class Searchable {
public:
	virtual int  neighbors() const = 0; // how many neighbors do I have?
	virtual HYP* make_neighbor(int k) const = 0; // not const since its modified
	virtual bool is_evaluable() const = 0; // can I call this hypothesis? Note it might still have neighbors (a in factorized lexica)
	// NOTE: here there is both neighbors() and can_evaluate becaucse it's possible that I can evaluate something
	// that has neghbors but also can be evaluated
	
	// TODO: Should copy_and_complete be in here? Would allow us to take a partial and score it; but that's actually handled in playouts
	//       and maybe doesn't seem like it should necessarily be pat of searchable
	//       Maybe actually the evaluation function should be in here -- evaluate_state?
};


#endif
