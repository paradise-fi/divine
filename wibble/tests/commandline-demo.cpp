#include <wibble/commandline/parser.h>
#include <config.h>
#include <iostream>

using namespace std;
using namespace wibble::commandline;

int withCommands(int argc, const char* argv[])
{
	MemoryManager cleaner;

	// Main parser
	Engine* e = cleaner.add(new Engine("", "<command> [options and arguments]",
				"Demo commandline parser",
				"This program is a demo for the commandline parser.  It shows a parser with "
				"subcommands and various kinds of options."));

	// Grep subcommand
	Engine* grep = cleaner.add(e->createEngine("grep", "<pattern> [files...]",
				"Print lines matching the pattern",
				"Print all the lines of standard input or the given files that match "
				"the given pattern."));
	BoolOption* grep_invert = cleaner.add(grep->create<BoolOption>("invert", 'v', "invert", "",
				"invert the match"));
	StringOption* grep_output = cleaner.add(grep->create<StringOption>("output", 'o', "output", "<file>",
				"write the output to the given file instead of standard output"));

	// ls subcommand
	Engine* ls = cleaner.add(e->createEngine("ls", "[directory...]",
				"List files in a directory",
				"List all files found in the directories given as parameters to standard output.  "
				"if no directory is given, list the files in the current directory."));
	// sort option group
	OptionGroup* ls_sort = cleaner.add(ls->create("Options controlling the order of output"));
	BoolOption* ls_sort_invert = cleaner.add(ls_sort->create<BoolOption>("invert", 'r', "invert", "",
				"sort in inverted order"));
	IntOption* ls_sort_field = cleaner.add(ls_sort->create<IntOption>("field", 0, "field", "",
				"sort the given field (if the switch is omitted, 1 is assumed"));
	// format option group
	OptionGroup* ls_format = cleaner.add(ls->create("Options controlling the format of output"));
	BoolOption* ls_format_long = cleaner.add(ls_format->create<BoolOption>("long", 'l', "long", "",
				"long output format with all the details"));
	BoolOption* ls_format_inode = cleaner.add(ls_format->create<BoolOption>("inode", 'i', "inode", "",
				"also output the file inode number"));
	// other ls options
	BoolOption* ls_all = cleaner.add(ls->create<BoolOption>("all", 'a', "all", "",
				"output all files, including the ones starting with a dot"));

	StandardParserWithMandatoryCommand parser(*e, argv[0], VERSION, 1, "enrico@enricozini.org");
	try {
		if (parser.parse(argc, argv))
			cerr << "The parser handled the command for us." << endl;
		if (e->foundCommand())
			cerr << "Selected command: " << e->foundCommand()->name() << endl;
		else
			cerr << "No command selected." << endl;
		cerr << "Option values:" << endl;
		cerr << "  help: " << parser.help->boolValue() << endl;
		cerr << "  version: " << parser.version->boolValue() << endl;
		cerr << "  manpage: " << parser.manpage->boolValue() << endl;
		cerr << "  grep/invert: " << grep_invert->boolValue() << endl;
		cerr << "  grep/output: " << grep_output->stringValue() << endl;
		cerr << "  ls/sort/invert: " << ls_sort_invert->boolValue() << endl;
		cerr << "  ls/sort/field: " << ls_sort_field->intValue() << endl;
		cerr << "  ls/format/long: " << ls_format_long->boolValue() << endl;
		cerr << "  ls/format/inode: " << ls_format_inode->boolValue() << endl;
		cerr << "  ls/all: " << ls_all->boolValue() << endl;
		return 0;
	} catch (wibble::exception::BadOption& e) {
		cerr << e.fullInfo() << endl;
		parser.outputHelp(cerr);
		return 1;
	}
	return 1;
}

void usage(ostream& out, const string& argv0)
{
	out << "Usage: " << argv0 << " {commands|switches}" << endl;
}

int main(int argc, const char* argv[])
{
	try {
		if (argc == 1)
		{
			usage(cout, argv[0]);
			return 0;
		}
		if (string(argv[1]) == "commands")
			return withCommands(argc - 1, argv + 1);
		//else if (string(argv[1]) == "switches")
			//return withoutCommands(argc - 1, argv + 1);
		else {
			usage(cerr, argv[0]);
			return 1;
		}
		return 0;
	} catch (wibble::exception::Generic& e) {
		cerr << e.type() << ": " << e.fullInfo() << endl;
		return 1;
	} catch (std::exception& e) {
		cerr << e.what() << endl;
		return 1;
	}
}

// vim:set ts=4 sw=4:
