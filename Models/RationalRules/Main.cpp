// TODO:
//  See about having the primitives take references



// A simple example of a version of the RationalRules model. 
// This is primarily used as an example and for debugging MCMC
// My laptop gets around 200-300k samples per second

#include "assert.h"

// These define all of the types that are used in the grammar -- must be defined
// before we import Fleet
#define FLEET_GRAMMAR_TYPES bool,Object

// We need to define some structs to hold the object features
enum    class  Shape { Square, Triangle, Circle};
enum    class  Color { Red, Green, Blue};
typedef struct Object {
	Color color;
	Shape shape;
	
	// we must define this to compile because memoization requires sorting (but its only neded in op_MEM)
	//bool operator<(const Object& o) const { assert(false); }
} Object;


#include "Primitives.h"

// This is a global variable that provides a convenient way to wrap our primitives
// where we can pair up a function with a name, and pass that as a constructor
// to the grammar. We need a tuple here because Primitive has a bunch of template
// types to handle thee function it has, so each is actually a different type.
std::tuple PRIMITIVES = {
	Primitive("and(%s,%s)",    +[](bool a, bool b) -> bool { return a && b; }, 2.0),
	Primitive("or(%s,%s)",     +[](bool a, bool b) -> bool { return a || b; }),
	Primitive("not(%s)",       +[](bool a)         -> bool { return not a; }),
	// that + is really insane, but is needed to convert a lambda to a function pointer

	Primitive("red(%s)",       +[](Object x)       -> bool { return x.color == Color::Red; }),
	Primitive("green(%s)",     +[](Object x)       -> bool { return x.color == Color::Green; }),
	Primitive("blue(%s)",      +[](Object x)       -> bool { return x.color == Color::Blue; }),

	Primitive("square(%s)",    +[](Object x)       -> bool { return x.shape == Shape::Square; }),
	Primitive("triangle(%s)",  +[](Object x)       -> bool { return x.shape == Shape::Triangle; }),
	Primitive("circle(%s)",    +[](Object x)       -> bool { return x.shape == Shape::Circle; })
};

// Includes critical files. Also defines some variables (mcts_steps, explore, etc.) that get processed from argv 
#include "Fleet.h" 

/* Define a class for handling my specific hypotheses and data. Everything is defaulty a PCFG prior and 
 * regeneration proposals, but I have to define a likelihood */
class MyHypothesis : public LOTHypothesis<MyHypothesis,Node,Object,bool> {
public:
	using Super = LOTHypothesis<MyHypothesis,Node,Object,bool>;
	MyHypothesis(Grammar* g, Node n)   : Super(g, n) {}
	MyHypothesis(Grammar* g)           : Super(g) {}
	MyHypothesis()                     : Super() {}
	
	double compute_single_likelihood(const t_datum& x) {
		bool out = callOne(x.input, false);
		return out == x.output ? log(x.reliability + (1.0-x.reliability)/2.0) : log((1.0-x.reliability)/2.0);
	}
};

////////////////////////////////////////////////////////////////////////////////////////////

int main(int argc, char** argv){ 
	
	// default include to process a bunch of global variables: mcts_steps, mcc_steps, etc
	auto app = Fleet::DefaultArguments("Rational rules");
	CLI11_PARSE(app, argc, argv);
	Fleet_initialize(); // must happen afer args are processed since the alphabet is in the grammar
	
	//------------------
	// Basic setup
	//------------------
	
	// Define the grammar (default initialize using our primitives will add all those rules)	
	Grammar grammar(PRIMITIVES);
	
	// but we also have to add a rule for the BuiltinOp that access x, our argument
	grammar.add(Rule(grammar.nt<Object>(), BuiltinOp::op_X, "x", {}, 5.0));
	
	// mydata stores the data for the inference model
	MyHypothesis::t_data mydata;
	
	// top stores the top hypotheses we have found
	TopN<MyHypothesis> top;
	top.set_size(ntop); // set by above macro
	
	//------------------
	// set up the data
	//------------------
	
	mydata.push_back(   (MyHypothesis::t_datum){ (Object){Color::Red, Shape::Triangle}, true,  0.75 }  );
	mydata.push_back(   (MyHypothesis::t_datum){ (Object){Color::Red, Shape::Square},   false, 0.75 }  );
	mydata.push_back(   (MyHypothesis::t_datum){ (Object){Color::Red, Shape::Square},   false, 0.75 }  );
	
	//------------------
	// Actuall run
	//------------------
	
//	MyHypothesis h0(&grammar);
//	h0 = h0.restart();
//	MCMCChain chain(h0, &mydata, top);
//	tic();
//	chain.run(mcmc_steps,runtime);
//	tic();
	
	MyHypothesis h0(&grammar);
	h0 = h0.restart();
	ParallelTempering samp(h0, &mydata, top, 8, 1000.0);
	tic();
	samp.run(mcmc_steps, runtime, 0.2, 3.0); //30000);		
	tic();

	// Show the best we've found
	top.print();
		
	COUT "# Global sample count:" TAB FleetStatistics::global_sample_count ENDL;
	COUT "# Elapsed time:" TAB elapsed_seconds() << " seconds " ENDL;
	COUT "# Samples per second:" TAB FleetStatistics::global_sample_count/elapsed_seconds() ENDL;
	
}