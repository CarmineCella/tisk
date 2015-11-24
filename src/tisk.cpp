// tisk.cpp
//

#include "tisk.h"

using namespace std;

int main (int argc, char* argv[]) {
	try {
		AtomPtr env = Atom::make_environment (nullptr);

		if (argc == 1) {
			cout << "[tisk, v. 0.1]" << endl << endl;
			cout << "minimalistic scheme dialect" << endl;
			cout << "(c) 2015 www.carminecella.com" << endl << endl;

			init_env (env);
			repl (env);
		} else {
			for (int i = 1; i < argc; ++i) {
				load (argv[i], env);
			}
		}
	} catch (exception& e) {
		cout << "error: " << e.what () << endl;
	}
	return 0;
}

// eof
