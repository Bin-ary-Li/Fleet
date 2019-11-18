#include <cmath>


const double sdscale = 1.0; // can change if we want
const size_t nsamples = 250; // how many per structure?
const size_t nstructs = 20; // print out all the samples from the top this many structures

///~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
/// These define all of the types that are used in the grammar.
/// This macro must be defined before we import Fleet.
///~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

using D = double; // this must match the below type (both can be changed to float if we want)
#define FLEET_GRAMMAR_TYPES D

// constants must be processed by the stack internal to each hypothesis!
#define CUSTOM_OPS op_Constant

///~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
/// Define magic primitives
///~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

#include "Primitives.h"
#include "Builtins.h"

std::tuple PRIMITIVES = {
	Primitive("(%s+%s)",    +[](D a, D b) -> D     { return a+b; }),
	Primitive("(%s-%s)",    +[](D a, D b) -> D     { return a-b; }),
	Primitive("(%s*%s)",    +[](D a, D b) -> D     { return a*b; }),
	Primitive("(%s/%s)",    +[](D a, D b) -> D     { return (b==0 ? 0 : a/b); }),
	Primitive("(%s^%s)",    +[](D a, D b) -> D     { return pow(a,b); }),
	Primitive("(-%s)",      +[](D a)          -> D { return -a; }),
	Primitive("exp(%s)",    +[](D a)          -> D { return exp(a); }),
	Primitive("log(%s)",    +[](D a)          -> D { return log(a); }),
	
	Primitive("1",          +[]()             -> D { return 1.0; }),
	
	Builtin::X<D>("x", 5.0)
};


#include "Fleet.h" 


class MyHypothesis : public LOTHypothesis<MyHypothesis,Node,D,D> {
	/* This class handles enumeration of the structure but critically does MCMC over the constants */
	
public:
	std::vector<D> constants;
	size_t         constant_idx; // in evaluation, this variable stores what constant we are in 
	
	using Super = LOTHypothesis<MyHypothesis,Node,D,D>;
	using Super::Super;
	
	double compute_single_likelihood(const t_datum& datum) {
		double fx = this->callOne(datum.input, NaN);
		if(std::isnan(fx)) return -infinity;
            
		return normal_lpdf( (fx-datum.output)/datum.reliability );		
	}
	
	size_t count_constants() const {
		size_t cnt=0;
		for(const auto& x : value) {
			cnt += x.rule->instr.is_a(CustomOp::op_Constant);
		}
		return cnt;
	}
	
	virtual double compute_constants_prior() const {
		double lp = 0.0;
		for(auto& c : constants) {
			lp += normal_lpdf(c);
		}
		return lp;
	}
	
	virtual double compute_prior() {
		this->prior = Super::compute_prior() + compute_constants_prior();
		return this->prior;
	}
	
	virtual D callOne(const D x, const D err) {
		// my own wrapper that zeros the constant_i counter
		constant_idx = 0;
		auto out = Super::callOne(x,err);
		assert(constant_idx == constants.size()); // just check
		return out;
	}
	
	
	virtual std::string __my_string_recurse(const Node* n, size_t& idx) const {
		// we need this to print strings -- its in a similar format to evaluation
		
		if(n->rule->instr.is_a(CustomOp::op_Constant)) {
			return "("+str(constants[idx++])+")";
		}
		else if(n->rule->N == 0) {
			return n->rule->format;
		}
		else {
			
			// strings are evaluated in right->left order so we have to 
			// use that here (since we use them to index idx)
			std::string childStrings[n->rule->N];
			
			// no, we evaluate left to right, but things are pushed in that order
			// so here we want to ru nright to left
			for(size_t i=0;i<n->rule->N;i++) {
//			for(int i=(int)n->rule->N-1;i>=0;i--) {
				childStrings[i] = __my_string_recurse(&n->child[i],idx);
			}
			
			std::string s = n->rule->format;
			for(size_t i=0;i<n->rule->N;i++) { // can't be size_t for counting down
				auto pos = s.find(ChildStr);
				assert(pos != std::string::npos); // must contain the ChildStr for all children all children
				s.replace(pos, ChildStr.length(), childStrings[i]);
			}
			
			return s;
		}
	}
	
	virtual std::string string() const { 
		// Here we need to simulate the stack used in evaluation in order to get the 
		// order of the constants and operators right 
		// I think we could do this by iterating through the kids in the right way..
		// NOTE: This only works here becaus e
		
		if(!value.is_complete()) return structure_string(); // don't fill in constants if we aren't complete
		
		size_t idx = 0;
		return  std::string("\u03BBx.") +  __my_string_recurse(&value,idx);
	}
	
	virtual std::string structure_string() const {
		return Super::string();
	}
	
	vmstatus_t dispatch_custom(Instruction i, VirtualMachinePool<D,D>* pool, VirtualMachineState<D,D>* vms, Dispatchable<D,D>* loader ) {
		switch(i.as<CustomOp>()) {
			case CustomOp::op_Constant: {
				assert(this->constant_idx < this->constants.size() && this->constant_idx >= 0); 
				vms->push(this->constants.at(constant_idx++));
				break;
			}			
			default: assert(0); 
		}
		return vmstatus_t::GOOD;
	}
	
	/// *****************************************************************************
	/// Change equality to include equality of constants
	/// *****************************************************************************
	
	virtual bool operator==(const MyHypothesis& h) const {
		// equality requires our constants to be equal 
		if(! this->Super::operator==(h) ) return false;
		
		assert(constants.size() == h.constants.size()); // has to be if we passed the previous test
		for(size_t i=0;i<constants.size();i++){
			if(constants[i] != h.constants[i]) return false;
		}
		return true;
	}

	virtual size_t hash() const {
		// hash includes constants so they are only ever equal if constants are equal
		size_t h = Super::hash();
		for(size_t i=0;i<constants.size();i++) {
			hash_combine(h, i, constants[i]);
		}
		return h;
	}
	
	/// *****************************************************************************
	/// Implement MCMC moves as changes to constants
	/// *****************************************************************************
	
	virtual std::pair<MyHypothesis,double> propose() const {
		// Our proposals will either be to constants, or entirely from the prior
		
		if(flip()){
			MyHypothesis ret = *this;
			
			// now add to all that I have
			for(size_t i=0;i<ret.constants.size();i++) { 
				ret.constants[i] += 0.1*random_cauchy(); // symmetric, not counting in fb
			}
			return std::make_pair(ret, 0.0);
		}
		else {
			auto [h, fb] = Super::propose(); // a proposal to structure
			
			h.randomize_constants(); // with random constants
			
			return std::make_pair(h, fb + h.compute_constants_prior() - this->compute_constants_prior());
		}
	}
	
	virtual void randomize_constants() {
		constants.resize(count_constants());
		for(size_t i=0;i<constants.size();i++) {
			constants[i] = random_cauchy();
		}
	}

	virtual MyHypothesis restart() const {
		MyHypothesis ret = Super::restart(); // may reset my structure
		ret.randomize_constants();
		return ret;
	}
	
	virtual MyHypothesis copy_and_complete() const {
		auto ret = Super::copy_and_complete();
		ret.constants.resize(ret.count_constants());
		ret.randomize_constants();
		return ret;
	}
	
	virtual MyHypothesis make_neighbor(int k) const {
		auto ret = Super::make_neighbor(k);
		ret.constants.resize(ret.count_constants());
		ret.randomize_constants();
		return ret;
	}
	
	void print() {
		Super::print("\t"+Q(this->structure_string()));
	}
	
};

////////////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////

// Define these types -- they are needed below
using t_data  = MyHypothesis::t_data;
using t_datum = MyHypothesis::t_datum;
#include "Data.h"
#include "Polynomial.h"

std::map<std::string,ReservoirSample<MyHypothesis>> master_samples; // master set of samples
std::mutex master_sample_lock;

////////////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////


void callback(MyHypothesis& h) {
	if(h.posterior == -infinity) return;
	
	auto ss = h.structure_string();
	std::lock_guard guard(master_sample_lock);
	if(!master_samples.count(ss)) { // create this if it doesn't exist
		master_samples.emplace(ss,nsamples);
		master_samples[ss].unique = true;
	}
	
	// and add it
	master_samples[ss] << h;
}


////////////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////

int main(int argc, char** argv){ 
	// default include to process a bunch of global variables: mcts_steps, mcc_steps, etc
	auto app = Fleet::DefaultArguments("Symbolic regression");
	CLI11_PARSE(app, argc, argv);
	Fleet_initialize();

	//------------------
	// Set up the grammar
	//------------------
	
	Grammar grammar(PRIMITIVES);
	grammar.add<double>(CustomOp::op_Constant, "C", 5.0);	
	
	//------------------
	// set up the data
	//------------------

	t_data mydata = load_data_file(input_path.c_str()); // "data-sources/Science/COBE/data.txt");
 
 	//------------------
	// Run
	//------------------
 
	MyHypothesis h0(&grammar);
	MCTSNode m(explore, h0, callback, &mydata);
	tic();
	m.parallel_search(nthreads, mcts_steps, runtime);
	tic();
	
	m.print(tree_path.c_str());

//	std::function cb = callback;
//	ChainPool samp(h0, &mydata, cb, nchains, true);
//	tic();
//	samp.run(mcmc_steps, runtime);
//	tic();

	// set up a paralle tempering object
//	ParallelTempering<MyHypothesis> samp(h0, &mydata, callback, 8, 1000.0, false);
//	tic();
//	samp.run(mcmc_steps, runtime, .25, 3.00); 
//	tic();

//	MyHypothesis h0(&grammar);
//	h0 = h0.restart();
//	MCMCChain<MyHypothesis> samp(h0, &mydata, callback);
//	tic();
//	samp.run(mcmc_steps, runtime); //30000);		
//	tic();

	
	//------------------
	// Postprocessing to show
	//------------------

	// and print out structures that have their top N
	double cutoff = -infinity;
	std::vector<double> structureScores;
	for(auto& q: master_samples)  {
		structureScores.push_back(q.second.max().posterior);
	}
	
	double maxscore = -infinity;
	if(structureScores.size() > 0) { 	// figure out the n'th best (NOTE: there are smarter ways to do this)
		std::sort(structureScores.begin(), structureScores.end()); // sort low to high
		maxscore = structureScores[structureScores.size()-1];
		if(structureScores.size() > nstructs) cutoff = structureScores[structureScores.size()-nstructs-1];
		else                                  cutoff = structureScores[0];
	}
	
	// figure out the structure normalizer
	double Z = -infinity;
	for(auto s : structureScores) {
		if(s >= cutoff) Z = logplusexp(Z, s); // accumulate all of this -- TODO: Should we only be adding up ones over the cutoff?
	}
	COUT "# Cutting off at " << cutoff ENDL;
	
	// And display!
	COUT "structure\tstructure.max\testimated.posterior\tposterior\tprior\tlikelihood\tf0\tf1\tpolynomial.degree\th\tparseable.h" ENDL;
	for(auto& m : master_samples) {
		if(m.second.max().posterior >= cutoff) {

			// find the normalizer for this structure
			double sz = -infinity;
			for(auto& h : m.second) sz = logplusexp(sz, h.posterior);
			
			for(auto h : m.second) {
				COUT QQ(h.structure_string()) TAB 
				     m.second.max().posterior TAB 
					 ( (h.posterior-sz) + (m.second.max().posterior-Z)) TAB // b/c I am using reservoir sampling, I am just one out of the size
					 h.posterior TAB h.prior TAB h.likelihood TAB
					 h.callOne(0.0, NaN) TAB h.callOne(1.0, NaN) TAB 
					 get_polynomial_degree(h.value) TAB 
					 Q(h.string()) TAB Q(h.parseable()) ENDL;
			}
		}
	}
	COUT "# **** REMINDER: These are not printed in order! ****" ENDL;
	
	
	COUT "# Global sample count:" TAB FleetStatistics::global_sample_count ENDL;
	COUT "# MCTS tree size:" TAB m.size() ENDL;	
	COUT "# Elapsed time:" TAB elapsed_seconds() << " seconds " ENDL;
	COUT "# Samples per second:" TAB FleetStatistics::global_sample_count/elapsed_seconds() ENDL;
	COUT "# VM ops per second:" TAB FleetStatistics::vm_ops/elapsed_seconds() ENDL;
	COUT "# Max score: " TAB maxscore ENDL;
	COUT "# MCTS steps per second:" TAB m.statistics.N/elapsed_seconds() ENDL;	
}
