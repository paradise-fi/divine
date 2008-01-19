#include <wibble/commandline/parser.h>
#include <iostream>

using namespace std;
using namespace wibble::commandline;

int withCommands(int argc, const char* argv[])
{
	// Main parser
	StandardParserWithMandatoryCommand parser(argv[0], "0.1", 1, "enrico@enricozini.org");
	parser.usage = "<command> [options and arguments]";
	parser.description = "Demo commandline parser";
	parser.longDescription = 
				"This program is a demo for the commandline parser.  It shows a parser with "
				"subcommands and various kinds of options.";

	// Grep subcommand
	Engine* grep = parser.addEngine("grep", "<pattern> [files...]",
				"Print lines matching the pattern",
				"Print all the lines of standard input or the given files that match "
				"the given pattern.");
	BoolOption* grep_invert = grep->add<BoolOption>("invert", 'v', "invert", "",
				"invert the match");
	StringOption* grep_output = grep->add<StringOption>("output", 'o', "output", "<file>",
				"write the output to the given file instead of standard output");

	// ls subcommand
	Engine* ls = parser.addEngine("ls", "[directory...]",
				"List files in a directory",
				"List all files found in the directories given as parameters to standard output.  "
				"if no directory is given, list the files in the current directory.");
	// sort option group
	OptionGroup* ls_sort = ls->addGroup("Options controlling the order of output");
	BoolOption* ls_sort_invert = ls_sort->add<BoolOption>("invert", 'r', "invert", "",
				"sort in inverted order");
	IntOption* ls_sort_field = ls_sort->add<IntOption>("field", 0, "field", "",
				"sort the given field (if the switch is omitted, 1 is assumed");
	// format option group
	OptionGroup* ls_format = ls->addGroup("Options controlling the format of output");
	BoolOption* ls_format_long = ls_format->add<BoolOption>("long", 'l', "long", "",
				"long output format with all the details");
	BoolOption* ls_format_inode = ls_format->add<BoolOption>("inode", 'i', "inode", "",
				"also output the file inode number");
	// other ls options
	BoolOption* ls_all = ls->add<BoolOption>("all", 'a', "all", "",
				"output all files, including the ones starting with a dot");

	try {
		if (parser.parse(argc, argv))
			cerr << "The parser handled the command for us." << endl;
		if (parser.foundCommand())
			cerr << "Selected command: " << parser.foundCommand()->name() << endl;
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
