// tisk.h
//

#ifndef TISK_H
#define TISK_H

#include <iostream>
#include <vector>
#include <vector>
#include <string>
#include <sstream>
#include <fstream>
#include <stdexcept>
#include <memory>
#include <map>
#include <cstdlib>
#include <cmath>

using namespace std;

#define GARBAGE_COLLECTION

// TYPES and AST ---------------------------------------------------------------
struct Atom;
typedef double Real;
#ifdef GARBAGE_COLLECTION
	typedef std::shared_ptr<Atom> AtomPtr;
	#define CREATE(t)std::make_shared<Atom>(t);
#else
	typedef Atom* AtomPtr;
	#define CREATE(t) new Atom (t);
#endif
typedef std::map<std::string, AtomPtr> Dictionary;
typedef AtomPtr (*Action) (AtomPtr, AtomPtr);
std::ostream& print (AtomPtr a, std::ostream& out);
void error (const std::string& text, AtomPtr node);
enum AtomType {
	NUMBER,
	LIST,
	SYMBOL,
	STRING,
	OPERATOR,
	LAMBDA,
	MACRO,
	ENVIRONMENT
};
struct Atom {
	Atom (AtomType t) {
		type = t;
	}
	static AtomPtr make_number (Real v) {
		AtomPtr l = CREATE (NUMBER)
		l->val = v;
		return l;
	}
	static AtomPtr make_list () {
		AtomPtr l = CREATE (LIST)
		return l;
	}
	static AtomPtr make_symbol (const std::string& lexeme) {
		static Dictionary d;
		Dictionary::iterator it = d.find (lexeme);
		if (it != d.end()) return it->second;
		else {
			AtomPtr l = CREATE (SYMBOL)
			l->lexeme = lexeme;
			d[lexeme] = l;
			return l;
		}
	}
	static AtomPtr make_string (const std::string& lexeme) {
		AtomPtr l = CREATE (STRING)
		l->lexeme = lexeme;
		return l;
	}	
	static AtomPtr make_operator (Action a, int minargs = 0) {
		AtomPtr l = CREATE (OPERATOR)
		l->action = a;
		l->minargs = minargs;
		return l;
	}
	static AtomPtr make_lambda (AtomPtr body, AtomPtr args, AtomPtr env) {
		AtomPtr l = CREATE (LAMBDA)
		l->tail.push_back (body);
		l->tail.push_back (args);
		l->tail.push_back (env);
		return l;
	}
	static AtomPtr make_macro (AtomPtr body, AtomPtr args, AtomPtr env) {
		AtomPtr l = make_lambda (body, args, env);
		l->type = MACRO;
		return l;
	}
	static AtomPtr make_environment (AtomPtr outer) {
		AtomPtr l = CREATE (ENVIRONMENT)
		l->outer = outer;
		return l;
	}
	AtomType type;
	Real val;
	std::vector<AtomPtr> tail;
	std::string lexeme;
	Action action;
	unsigned minargs;
	AtomPtr outer;
};
AtomPtr define = Atom::make_symbol ("def");
AtomPtr quote = Atom::make_symbol ("quote");
AtomPtr lambda = Atom::make_symbol ("fn");
AtomPtr macro = Atom::make_symbol ("macro");
AtomPtr if_s = Atom::make_symbol ("if");
AtomPtr begin_s = Atom::make_symbol ("begin");
bool is_null (AtomPtr node) {
	return !node || node == nullptr;
}
// LEXING and PARSING ----------------------------------------------------------
bool is_number (const char* t) {
	std::stringstream dummy;
	dummy << t;
	Real v; dummy >> v;
	return dummy && dummy.eof ();
}
bool is_string (const std::string& t) {
	return t.find ("\"") != std::string::npos;
}
std::string next (std::istream& l) {
	std::stringstream token;
	while (!l.eof ()) {
		char c = l.get ();
		if (l.eof () || c <= 0) break;
		if (c == ';') {
			while (!l.eof ()) {
				c = l.get ();
				if (c == '\n' || c == '\r') break;
			}
			continue;
		} else if (c == '\"') {
			if (token.str ().size ()) {
				l.unget ();
				return token.str ();
			}
			else {
				while (!l.eof ()) {
					if (c == '\\') {
		             	c = l.get ();
	                    switch (c) {
	                        case 'n': token <<'\n'; break;
	                        case 'r': token <<'\r'; break;
	                        case 't': token <<'\t'; break;
	                        case '\"': token << "\""; c = 0; break;
	                        default: {
								error ("unsupported escape sequence in string", nullptr);
							}
	                    }
	                } else {
						token << c;	
					} 
					c = l.get();
					if (c == '\"') break;
				} 
				return token.str ();
			}
		} else if (c == ' ' || c == '\t' || c == '\r' || c == '\n') {
			if (token.str ().size ()) return token.str ();
			else continue;
		} else if (c == '(' || c == ')' || c == '\'') {
			if (token.str ().size ()) {
				l.unget ();
				return token.str ();
			} else {
				token << c;
				return token.str ();
			}
		} else {
			if (c) token << c;
		}
	}
	return token.str ();
}
AtomPtr read (std::istream& line) {
	std::string token = next (line);
	AtomPtr l = Atom::make_list ();
	if (!token.size ()) return l;
	if (token == "(") {
		while (!line.eof ()) {
			AtomPtr s = read (line);
			if (s->lexeme == ")") break;
			l->tail.push_back (s);
		}
	} else if (token == "\'") {
		l->tail.push_back (quote);
		l->tail.push_back (read (line)); 
	} else if (is_number (token.c_str ())) {
		l = Atom::make_number (atof (token.c_str ()));
	} else if (is_string (token)) {
		l = Atom::make_string (token.substr (1, token.size ()-1));
	} else {
		l = Atom::make_symbol (token);
	}
	return l;
}
// EVALUATION ------------------------------------------------------------------
AtomPtr assoc (AtomPtr symbol, AtomPtr env) {
	for (unsigned i = 0; i < env->tail.size () / 2; ++i) {
		if (env->tail[2 * i] == symbol) return env->tail[2 * i + 1];
	}
	if (!is_null (env->outer)) return assoc (symbol, env->outer);
	else {
		error ("unbound symbol ", symbol);
	}
	return Atom::make_list (); // not reached
}
AtomPtr extend (AtomPtr symbol, AtomPtr val, AtomPtr env) {
	for (unsigned i = 0; i < env->tail.size () / 2; ++i) {
		if (env->tail[2 * i] == symbol) {
			env->tail[2 * i + 1] = val;
			return val;
		}
	}
	env->tail.push_back (symbol); env->tail.push_back (val);
	return val;
}
AtomPtr args_check (AtomPtr node, unsigned ct) {
	if (node->tail.size () < ct) {
		error ("wrong number of arguments in ", node);
	}
	return node;
}
AtomPtr type_check (AtomPtr node, AtomType type) {
	if (node->type != type) {
		error ("invalid type for ", node);
	}
	return node;
}
int atom_eq (AtomPtr x, AtomPtr y) {
	if (x->type != y->type) { return 0; }
	switch (x->type) {
    case NUMBER: return (x->val == y->val);
    case SYMBOL: case STRING: return (x->lexeme == y->lexeme);
    case LIST: case ENVIRONMENT: {
		if (x->tail.size () != y->tail.size ()) { return 0; }
		for (unsigned i = 0; i < x->tail.size (); ++i) {
			if (!atom_eq (x->tail[i], y->tail[i])) { return 0; }
		}
		return 1;
	}
    case OPERATOR: return (x->action == y->action);
	case MACRO: case LAMBDA: return (atom_eq (x->tail[0], y->tail[0]) 
		&& atom_eq (x->tail[1], y->tail[1]));
	default:
		return 0;
	}
}
AtomPtr fn_eval (AtomPtr node, AtomPtr env) { return nullptr; } // not used
AtomPtr fn_apply (AtomPtr node, AtomPtr env) { return nullptr; } // not used
AtomPtr eval (AtomPtr node, AtomPtr env) {
	while (true) {
		if (is_null (node)) return node;
		if (node->type == SYMBOL) {
			return assoc (type_check (node, SYMBOL), env);
		}
		if (node->type != LIST || (node->type == LIST && node->tail.size () == 0)) {
			return node;
		}
		AtomPtr car = node->tail[0];
		if (car == define) {
			args_check (node, 3);
			return  extend (type_check (node->tail[1], SYMBOL),
				eval (node->tail[2], env), env);
		} else if (car == quote) {
			args_check (node, 2);
			return node->tail[1];
		} else if (car == lambda || car == macro) {
			args_check (node, 3);
			return car == lambda ? Atom::make_lambda (node->tail[2], node->tail[1], env) :
				Atom::make_macro (node->tail[2], node->tail[1], env);
		} else if (car == if_s) {
			args_check (node, 3);
			bool c = (bool) type_check(eval (node->tail[1], env), NUMBER)->val;
			if (c) {
				node = node->tail[2];
				continue;
			} else {
				if (node->tail.size () == 4) {
					node = node->tail[3];
					continue;
				} else return Atom::make_list ();
			}
		} else if (car == begin_s) {
			args_check (node, 2);
			for (unsigned i = 1; i < node->tail.size() - 1; ++i) {
				eval (node->tail[i], env); 
			}
			node = node->tail[node->tail.size () - 1];
			continue;
		}
		AtomPtr func = eval (car, env);
		AtomPtr args = Atom::make_list ();
		for (unsigned i = 1; i < node->tail.size (); ++i) {
			AtomPtr v = func->type == MACRO ? node->tail[i] : eval (node->tail[i], env);
			args->tail.push_back (v);
		}
		if (func->type == LAMBDA || func->type == MACRO) {
			AtomPtr nenv = Atom::make_environment (func->tail[2]);
			args_check (func->tail[1], args->tail.size ());
			for (unsigned i = 0; i < func->tail[1]->tail.size (); ++i) {
				extend (type_check (func->tail[1]->tail[i], SYMBOL),
					args->tail[i], nenv);
			}

			node = func->type == MACRO ? eval (func->tail[0], nenv) : func->tail[0];
			env = nenv;
			continue;
		}
		if (func->type == OPERATOR) {
			args_check (args, func->minargs);
			if (func->action == &fn_eval) {
				node = args->tail[0];
				continue;
			} 
			if (func->action == &fn_apply) {
				node= args;
				continue;
			}
			return func->action (args, env);
		}	
		error ("function expected in ", node);
	}
	return node;
}
// HELPERS ---------------------------------------------------------------------
void error (const std::string& text, AtomPtr node) {
	std::stringstream msg;
	msg << text; print (node, msg);
	throw std::runtime_error (msg.str ());
}
std::ostream& print (AtomPtr a, std::ostream& out) {
	if (is_null (a)) { return out; }
	if (a->type == NUMBER) {
		out << a->val;
	} else if (a->type == LIST || a->type == ENVIRONMENT) {
		if (a->type ==ENVIRONMENT) {
			out << "[environment: " << a << "]" << std::endl;
		}
		out << "(";
		for (unsigned i = 0; i < a->tail.size (); ++i) {
			print (a->tail[i], out);
			if (a->type == ENVIRONMENT && i % 2 != 0) out << std::endl;
			if (i != a->tail.size () - 1) out << " ";
		}
		out << ")";
	} else if (a->type == SYMBOL || a->type == STRING) {
		out << a->lexeme;
	} else if (a->type == OPERATOR) {
		out << "[operator: " << (std::hex) << a <<"]";
	} else if (a->type == LAMBDA || a->type == MACRO) {
		if (a->type == LAMBDA) out<< "[lambda: ";
		else out << "[macro: ";
		print (a->tail[1], out);
		print (a->tail[0], out); out << "]";
	}
	return out;
}
AtomPtr load (const std::string& filename, AtomPtr env) {
	std::ifstream in (filename.c_str ());
	if (!in.good ()) {
		std::stringstream m;
		m << "cannot open file " << filename;
			error (m.str (), nullptr);
	}	
	AtomPtr r = Atom::make_list ();
	while (!in.eof ()) {
		r = eval (read (in), env);
	}
	return r;
}

// FUNCTORS --------------------------------------------------------------------
#define MAKE_BINOP(op,name, unit)									\
	AtomPtr name (AtomPtr n, AtomPtr env) {							\
		if (n->tail.size () == 1) {									\
			return Atom::make_number(unit							\
				op (type_check (n->tail[0], NUMBER)->val));			\
		}															\
		AtomPtr c = n;												\
		Real s = (type_check (n->tail[0], NUMBER)->val);			\
		for (unsigned i = 1; i < n->tail.size (); ++i) {			\
			s = s op type_check (n->tail[i], NUMBER)->val;			\
		}															\
		return Atom::make_number (s);								\
	}																\

MAKE_BINOP(+,fn_add,0);
MAKE_BINOP(*,fn_mul,1);
MAKE_BINOP(-,fn_sub,0);
MAKE_BINOP(/,fn_div,1);

#define MAKE_CMPOP(f,o) \
	AtomPtr f (AtomPtr node, AtomPtr env) { \
		Real v = 0; \
		for (unsigned i = 0; i < node->tail.size () - 1; ++i) { \
			if (type_check (node->tail[i], NUMBER)->val o node->tail[i + 1]->val) v = 1; \
			else return Atom::make_number (0); \
		} \
 		return Atom::make_number (v); \
	} \

MAKE_CMPOP(fn_less,<);
MAKE_CMPOP(fn_lesseq,<=);
MAKE_CMPOP(fn_greater,>);
MAKE_CMPOP(fn_greatereq,>=);

#define MAKE_SINGOP(f,o) \
	AtomPtr f (AtomPtr node, AtomPtr env) { \
		Real v = type_check (node->tail[0], NUMBER)->val; \
 		return Atom::make_number (o (v)); \
	} \

MAKE_SINGOP(fn_sqrt, sqrt);
MAKE_SINGOP(fn_sin, sin);
MAKE_SINGOP(fn_cos, cos);
MAKE_SINGOP(fn_log, log);
MAKE_SINGOP(fn_abs, fabs);
MAKE_SINGOP(fn_exp, exp);

AtomPtr fn_list (AtomPtr node, AtomPtr env) {
	return node;
}
AtomPtr fn_head (AtomPtr node, AtomPtr env) {
	AtomPtr o = type_check (node->tail[0], LIST);
	if (!o->tail.size ()) return Atom::make_list ();
	else return o->tail[0];
}
AtomPtr fn_tail (AtomPtr node, AtomPtr env) {
	AtomPtr l = Atom::make_list ();
	AtomPtr o = type_check (node->tail[0], LIST);
	if (o->tail.size () < 2) return Atom::make_list ();
	for (unsigned i = 1; i < o->tail.size (); ++i) {
		l->tail.push_back (o->tail[i]);
	}
	return l;
}
AtomPtr fn_env (AtomPtr node, AtomPtr env) {
	return env;
}
AtomPtr fn_read (AtomPtr node, AtomPtr env) {
	return read (std::cin);
}
AtomPtr fn_load (AtomPtr node, AtomPtr env) {
	std::string name = type_check (node->tail[0], STRING)->lexeme;
	return load (name, env);
}
AtomPtr fn_eqp (AtomPtr node, AtomPtr env) {
	return Atom::make_number (atom_eq (node->tail[0], node->tail[1]));	
}
AtomPtr fn_display (AtomPtr node, AtomPtr env) {
	for(unsigned i = 0; i < node->tail.size (); ++i) {
		print (node->tail[i], std::cout);
	}
	return node;
}
AtomPtr fn_cat (AtomPtr node, AtomPtr env) {
	std::stringstream tmp;
	for (unsigned i = 0; i < node->tail.size (); ++i) {
		tmp << type_check (node->tail[i], STRING)->lexeme;
	}
	return Atom::make_string (tmp.str ());	
}
AtomPtr fn_substr (AtomPtr node, AtomPtr env) {
	return Atom::make_string (
		type_check (node->tail[0], STRING)->lexeme.substr(
			type_check (node->tail[1], NUMBER)->val,
			type_check (node->tail[2], NUMBER)->val));	
}
AtomPtr fn_find (AtomPtr node, AtomPtr env) {
	std::string s = type_check (node->tail[0], STRING)->lexeme;
	std::string f = type_check (node->tail[1], STRING)->lexeme;
	return Atom::make_number ((Real) (long) s.find (f));	
}
// INTERFACE -------------------------------------------------------------------
void repl (AtomPtr env) {
	while (true) {
		std::cout << ">> ";
		std::string line;
		try {
			print (eval (read (std::cin), env), std::cout) << std::endl;
		} catch (std::exception& e) {
			std::cout << "error: " << e.what () << std::endl;
		}
	}
}
void init_env (AtomPtr env) {
	// lists and environments	
	extend (Atom::make_symbol ("list"), Atom::make_operator (fn_list), env);
	extend (Atom::make_symbol ("head"), Atom::make_operator (fn_head, 1), env);
	extend (Atom::make_symbol ("tail"), Atom::make_operator (fn_tail, 1), env);
	extend (Atom::make_symbol ("env"), Atom::make_operator (fn_env), env);
	extend (Atom::make_symbol ("eval"), Atom::make_operator (fn_eval, 1), env);
	extend (Atom::make_symbol ("apply"), Atom::make_operator (fn_apply, 2), env);
	// maths
	extend (Atom::make_symbol ("+"), Atom::make_operator (fn_add), env);
	extend (Atom::make_symbol ("-"), Atom::make_operator (fn_sub), env);
	extend (Atom::make_symbol ("*"), Atom::make_operator (fn_mul), env);
	extend (Atom::make_symbol ("/"), Atom::make_operator (fn_div), env);
	extend (Atom::make_symbol ("<"), Atom::make_operator (fn_less), env);
	extend (Atom::make_symbol ("<="), Atom::make_operator (fn_lesseq), env);
	extend (Atom::make_symbol (">"), Atom::make_operator (fn_greater), env);
	extend (Atom::make_symbol (">="), Atom::make_operator (fn_greatereq), env);
	extend (Atom::make_symbol ("=="), Atom::make_operator (fn_eqp, 2), env);
	extend (Atom::make_symbol ("sin"), Atom::make_operator (fn_sin, 1), env);
	extend (Atom::make_symbol ("cos"), Atom::make_operator (fn_cos, 1), env);
	extend (Atom::make_symbol ("sqrt"), Atom::make_operator (fn_sqrt, 1), env);
	extend (Atom::make_symbol ("log"), Atom::make_operator (fn_log, 1), env);
	extend (Atom::make_symbol ("abs"), Atom::make_operator (fn_abs, 1), env);
	extend (Atom::make_symbol ("exp"), Atom::make_operator (fn_exp, 1), env);
	// I/O
	extend (Atom::make_symbol ("read"), Atom::make_operator (fn_read), env);
	extend (Atom::make_symbol ("load"), Atom::make_operator (fn_load, 1), env);
	extend (Atom::make_symbol ("display"), Atom::make_operator (fn_display), env);
	// strings
	extend (Atom::make_symbol ("cat"), Atom::make_operator (fn_cat), env);
	extend (Atom::make_symbol ("substr"), Atom::make_operator (fn_substr, 3), env);
	extend (Atom::make_symbol ("find"), Atom::make_operator (fn_find, 2), env);
}

#endif // TISK_H

// eof
