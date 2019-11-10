
#include <string>

using S = std::string; // just for convenience

// These define a collection of CustomOps (must be a macro so we can define it if 
// this is left out) and these are ones whose instructions are processed by 
// dispatch_custom
#define CUSTOM_OPS op_PAIR

///~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
/// Set up some basic variables (which may get overwritten)
///~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

S alphabet = "01"; // the alphabet we use (possibly specified on command line)
S datastr  = "011,011011,011011011"; // the data, comma separated
const double strgamma = 0.99; // penalty on string length
const size_t MAX_LENGTH = 64; // longest strings cons will handle
	
///~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
/// These define all of the types that are used in the grammar.
/// This macro must be defined before we import Fleet.
///~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

#define FLEET_GRAMMAR_TYPES S,bool

///~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
/// This is a global variable that provides a convenient way to wrap our primitives
/// where we can pair up a function with a name, and pass that as a constructor
/// to the grammar. We need a tuple here because Primitive has a bunch of template
/// types to handle thee function it has, so each is actually a different type.
///~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

#include "Primitives.h"

std::tuple PRIMITIVES = {
//	Primitive("cons(%s,%s)",   +[](S a, S b) -> S { return a && b; }, 2.0), // NOTE: This is defined below
	Primitive("cdr(%s)",       +[](S s)      -> S { return (s.empty() ? S("") : s.substr(1,S::npos)); }),
	Primitive("car(%s)",       +[](S s)      -> S { return (s.empty() ? S("") : S(1,s.at(0))); }),
	Primitive("\u00D8",        +[]()         -> S { return  S(""); }),
	Primitive("(%s==%s)",      +[](S x, S y) -> bool { return x==y; }),
};

// Includes critical files. Also defines some variables (mcts_steps, explore, etc.) that get processed from argv 
#include "Fleet.h" 

class MyHypothesis : public LOTHypothesis<MyHypothesis,Node,S,S> {
public:
	using Super =  LOTHypothesis<MyHypothesis,Node,S,S>;
	MyHypothesis(Grammar* g, Node v)    : Super(g,v) {}
	MyHypothesis(Grammar* g)            : Super(g) {}
	MyHypothesis()                      : Super() {}
	
	double compute_single_likelihood(const t_datum& x) {		
		auto out = call(x.input, "<err>", this, 256, 256); //256, 256);
		
		// a likelihood based on the prefix probability -- we assume that we generate from the hypothesis
		// and then might glue on some number of additional strings, flipping a gamma-weighted coin to determine
		// how many to add
		double lp = -infinity;
		for(auto o : out.values()) { // add up the probability from all of the strings
			if(is_prefix(o.first, x.output)) {
				lp = logplusexp(lp, o.second + log(strgamma) + log((1.0-strgamma)/alphabet.length()) * (x.output.length() - o.first.length()));
			}
		}
		return lp;
	}
	
	vmstatus_t dispatch_custom(Instruction i, VirtualMachinePool<S,S>* pool, VirtualMachineState<S,S>& vms, Dispatchable<S,S>* loader ) {
		// This is a custom dispatch 
		assert(i.is<CustomOp>());
		assert(i.as<CustomOp>() == CustomOp::op_PAIR); // this is the only one we've implemented
		switch(i.as<CustomOp>()) {
			case CustomOp::op_PAIR: {
				// Above is one way to implement cons, but here we can do it faster by not removing it from the stack
				S y = vms.getpop<S>();
				
				// length check
				if(vms.stack<S>().topref().length() + y.length() > MAX_LENGTH) 
					return vmstatus_t::SIZE_EXCEPTION;

				vms.stack<S>().topref().append(y);
						
				return vmstatus_t::GOOD;
			}
			default: assert(false && "*** Should not get here -- must be an undefined CustomOp");
		}
	}
	
	void print(std::string prefix="") {
		assert(prefix == "");
		
		prefix  = "#\n#" +  this->call("", "<err>").string() + "\n"; 
//		extern TopN<MyHypothesis> top;
//		prefix += std::to_string(top.count(*this)) + "\t";
		
		Super::print(prefix); // print but prepend my top count
	}
};





////////////////////////////////////////////////////////////////////////////////////////////


int main(int argc, char** argv){ 
	using namespace std;
	
	// default include to process a bunch of global variables: mcts_steps, mcc_steps, etc
	auto app = Fleet::DefaultArguments("A simple, one-factor formal language learner");
	app.add_option("-a,--alphabet", alphabet, "Alphabet we will use"); 	// add my own args
	app.add_option("-d,--data",     datastr, "Comma separated list of input data strings");	
	CLI11_PARSE(app, argc, argv);
	
	Fleet_initialize(); // must happen afer args are processed since the alphabet is in the grammar
	
	// mydata stores the data for the inference model
	MyHypothesis::t_data mydata;
	
	// top stores the top hypotheses we have found
	TopN<MyHypothesis> top(ntop);
	
	// declare a grammar with our primitives
	Grammar grammar(PRIMITIVES);
	
	// Add the builtins we want
	grammar.add<S>         (BuiltinOp::op_X,            "x", 10.0); 
	grammar.add<S,bool,S,S>(BuiltinOp::op_IF,           "if(%s,%s,%s)"); 
	grammar.add<bool>      (BuiltinOp::op_FLIP,         "flip()"); 
	grammar.add<S,S>       (BuiltinOp::op_SAFE_RECURSE, "F(%s)"); 
	
	// here we create an alphabet op with an "arg" that stores the character (this is fater than alphabet.substring with i.arg as an index) 
	// here, op_ALPHABET converts arg to a string (and pushes it)
	for(size_t i=0;i<alphabet.length();i++) {
		grammar.add<S>   (BuiltinOp::op_ALPHABET, alphabet.substr(i,1), 10.0/alphabet.length(), (int)alphabet.at(i)); 
	}
	
	// we've said we will define and implement cons ourself
	grammar.add<S,S,S>   (CustomOp::op_PAIR, "pair(%s,%s)"); 
	
	//------------------
	// set up the data
	//------------------
	
	// we will parse the data from a comma-separated list of "data" on the command line
	for(auto di : split(datastr, ',')) {
		mydata.push_back( MyHypothesis::t_datum({S(""), di}) );
	}
	
	//------------------
	// Run
	//------------------
	
	MyHypothesis h0(&grammar);
//
//	tic();
//	MCMCChain thechain(h0, &mydata, top);
//	thechain.run(mcmc_steps, runtime);
//	tic();
	
	ParallelTempering samp(h0, &mydata, top, nchains, 1000.0);
	tic();
	samp.run(mcmc_steps, runtime, 1.0, 3.0); //30000);		
	tic();
	
	// Show the best we've found
	top.print();
	
	COUT "# Global sample count:" TAB FleetStatistics::global_sample_count ENDL;
	COUT "# Elapsed time:" TAB elapsed_seconds() << " seconds " ENDL;
	COUT "# Samples per second:" TAB FleetStatistics::global_sample_count/elapsed_seconds() ENDL;
	
}