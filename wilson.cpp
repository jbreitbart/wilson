// compile with something like clang++ -g3 -std=c++14 wilson.cpp -Illvm/include -lclang

#include <iostream>
#include <string>
#include <vector>

#include <clang-c/Index.h>

// convert CXString to std::string
static inline std::string cx_to_std(const CXString &x) {
	auto temp = clang_getCString(x);
	if (temp == nullptr) {
		return "";
	}
	auto ret = std::string(temp);
	clang_disposeString(x);
	return ret;
}

// print CXString for the lazy
static std::ostream &operator<<(std::ostream &stream, const CXString &str) {
	stream << cx_to_std(str);
	return stream;
}

// type used to store everything that is important for a function
struct function {
	function(CXString _name, const std::vector<CXCursor> &_parameters, CXType _result) {
		name = cx_to_std(_name);

		for (auto p : _parameters) {
			auto name = clang_getCursorSpelling(p);
			auto name_str = cx_to_std(name);

			auto type = clang_getCursorType(p);
			auto type_str = cx_to_std(clang_getTypeSpelling(type));
			parameters.push_back(type_str + " " + name_str);
		}

		result = cx_to_std(clang_getTypeSpelling(_result));
	}

	std::string name;
	std::vector<std::string> parameters;
	std::string result;
};
std::ostream &operator<<(std::ostream &stream, const function &f) {
	stream << f.result << " " << f.name << "(";
	for (const auto &p : f.parameters) {
		stream << p << ", ";
	}
	stream << ")";
	return stream;
}

// global variable that is filled by the visitor function and printed later in main
static std::vector<function> functions;

// visitor funtion is called for every entry in the input header file that should be parsed
static CXChildVisitResult visitor(CXCursor c, CXCursor parent, CXClientData client_data) {
	// check file location and filter usr/include
	auto c_loc = clang_getCursorLocation(c);
	CXFile file;
	clang_getSpellingLocation(c_loc, &file, nullptr, nullptr, nullptr);
	auto file_str = cx_to_std(clang_getFileName(file));

	if (file_str.find("usr/include") != std::string::npos) {
		return CXChildVisit_Recurse;
	}

	// is the current cursor a function?
	if (clang_getCursorKind(c) == CXCursor_FunctionDecl) {
		// get every parameter and its type
		int nb_parameters = clang_Cursor_getNumArguments(c);
		std::vector<CXCursor> parameters;
		for (int i = 0; i < nb_parameters; ++i) {
			parameters.push_back(clang_Cursor_getArgument(c, i));
		}

		// get the return type of the function
		auto ret = clang_getCursorResultType(c);

		functions.emplace_back(clang_getCursorSpelling(c), parameters, ret);
	}

#if 0
	std::cout << "Cursor '" << clang_getCursorSpelling(c) << "' of kind '"
			  << clang_getCursorKindSpelling(clang_getCursorKind(c)) << " at " << clang_getFileName(file) << std::endl;
#endif
	return CXChildVisit_Recurse;
}

int main(int argc, char const *argv[]) {
	if (argc < 2) {
		std::cerr << "Filename missing." << std::endl;
		exit(-1);
	}
	std::string filename(argv[1]);

	CXIndex index = clang_createIndex(0, 0);
	CXTranslationUnit unit = nullptr;

	std::cout << "Parsing file " << filename << "... " << std::flush;

	auto err = clang_parseTranslationUnit2(index, filename.c_str(), nullptr, 0, nullptr, 0,
										   CXTranslationUnit_Incomplete, &unit);
	if (err != 0) {
		std::cerr << std::endl << "Unable to parse translation unit: " << err << std::endl;
		exit(-1);
	}

	std::cout << "successful" << std::endl;

	CXCursor cursor = clang_getTranslationUnitCursor(unit);
	// TODO pass global variable via pointer in last parameter of the following call
	clang_visitChildren(cursor, visitor, nullptr);

	for (const auto &f : functions) {
		std::cout << f << std ::endl;
	}

	clang_disposeTranslationUnit(unit);
	clang_disposeIndex(index);

	return 0;
}
