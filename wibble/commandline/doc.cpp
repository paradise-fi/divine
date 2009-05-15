#include <wibble/commandline/doc.h>
#include <wibble/text/wordwrap.h>
#include <locale.h>
#include <errno.h>
#include <cstdlib>
#include <set>
#include <cstdio>
#ifdef _WIN32
#include <ctime>
#endif

using namespace std;

namespace wibble {
namespace commandline {


class HelpWriter
{
	// Width of the console
	std::ostream& out;
	int m_width;

public:
	HelpWriter(std::ostream& out);

	// Write 'size' spaces to out
	void pad(size_t size);

	// Output an item from a list.  The first bulletsize columns will be used to
	// output bullet, the rest will have text, wordwrapped and properly aligned
	void outlist(const std::string& bullet, size_t bulletsize, const std::string& text);

	void outstring(const std::string& str);
};

HelpWriter::HelpWriter(std::ostream& out) : out(out)
{
	char* columns = getenv("COLUMNS");
	m_width = columns ? atoi(columns) : 80;
}

void HelpWriter::pad(size_t size)
{
	for (size_t i = 0; i < size; i++) out << " ";
}

void HelpWriter::outlist(const std::string& bullet, size_t bulletsize, const std::string& text)
{
	text::WordWrap wrapper(text);
	size_t rightcol = m_width - bulletsize;

	out << bullet;
	pad(bulletsize - bullet.size());
	out << wrapper.get(rightcol);
	out << endl;

	while (wrapper.hasData())
	{
		pad(bulletsize);
		out << wrapper.get(rightcol);
		out << endl;
	}
}

void HelpWriter::outstring(const std::string& str)
{
	text::WordWrap wrapper(str);

	while (wrapper.hasData())
	{
		out << wrapper.get(m_width);
		out << endl;
	}
}

void Help::outputOptions(ostream& out, HelpWriter& writer, const Engine& p)
{
	// Compute size of option display
	size_t maxLeftCol = 0;
	for (vector<OptionGroup*>::const_iterator i = p.groups().begin();
			i != p.groups().end(); i++)
	{
		if ((*i)->hidden) continue;
		for (vector<Option*>::const_iterator j = (*i)->options.begin();
				j != (*i)->options.end(); j++)
		{
			if ((*j)->hidden) continue;
			size_t w = (*j)->fullUsage().size();
			if (w > maxLeftCol)
				maxLeftCol = w;
		}
	}
	for (vector<Option*>::const_iterator j = p.options().begin();
			j != p.options().end(); j++)
	{
		if ((*j)->hidden) continue;
		size_t w = (*j)->fullUsage().size();
		if (w > maxLeftCol)
			maxLeftCol = w;
	}

	if (maxLeftCol)
	{
		// Output the options
		out << endl;
		out << "Options are:" << endl;
		for (vector<OptionGroup*>::const_iterator i = p.groups().begin();
				i != p.groups().end(); i++)
		{
			if ((*i)->hidden) continue;
			if (!(*i)->description.empty())
			{
				out << endl;
				writer.outstring((*i)->description + ":");
				out << endl;
			}
			for (vector<Option*>::const_iterator j = (*i)->options.begin();
					j != (*i)->options.end(); j++)
			{
				if ((*j)->hidden) continue;
				writer.outlist(" " + (*j)->fullUsage(), maxLeftCol + 3, (*j)->description);
			}
		}
		if (!p.options().empty())
		{
			out << endl;
			writer.outstring("Other options:");
			out << endl;
			for (vector<Option*>::const_iterator j = p.options().begin();
					j != p.options().end(); j++)
			{
				if ((*j)->hidden) continue;
				writer.outlist(" " + (*j)->fullUsage(), maxLeftCol + 3, (*j)->description);
			}
		}
	}
}

void Help::outputVersion(std::ostream& out)
{
	out << m_app << " version " << m_ver << endl;
}

void Help::outputHelp(std::ostream& out, const Engine& p)
{
	HelpWriter writer(out);

	if (!p.commands().empty())
	{
		// Dig informations from p
		const std::vector<Engine*>& commands = p.commands();

		// Compute the maximum length of alias names
		size_t maxAliasSize = 0;
		for (vector<Engine*>::const_iterator i = commands.begin();
				i != commands.end(); i++)
		{
			if ((*i)->hidden) continue;
			const string& str = (*i)->primaryAlias;
			if (maxAliasSize < str.size())
				maxAliasSize = str.size();
		}

		out << "Usage: " << m_app << " [options] " << p.usage << endl;
		out << endl;
		writer.outstring("Description: " + p.description);
		out << endl;
		out << "Commands are:" << endl;
		out << endl;

		// Print the commands
		for (vector<Engine*>::const_iterator i = commands.begin();
				i != commands.end(); i++)
		{
			if ((*i)->hidden) continue;
			string aliases;
			const vector<string>& v = (*i)->aliases;
			if (!v.empty())
			{
				aliases += "  May also be invoked as ";
				for (vector<string>::const_iterator j = v.begin();
						j != v.end(); j++)
					if (j == v.begin())
						aliases += *j;
					else
						aliases += " or " + *j;
				aliases += ".";
			}

			writer.outlist(" " + (*i)->primaryAlias, maxAliasSize + 3, (*i)->description + "." + aliases);
		}
	} else {
 		// FIXME the || m_app == thing is a workaround...
		if (p.primaryAlias.empty() || m_app == p.primaryAlias)
			out << "Usage: " << m_app << " [options] " << p.usage << endl;
		else
			out << "Usage: " << m_app << " [options] " << p.primaryAlias << " [options] " << p.usage << endl;
		out << endl;

		if (!p.aliases.empty())
		{
			out << "Command aliases: ";
			for (vector<string>::const_iterator i = p.aliases.begin();
					i != p.aliases.end(); i++)
				if (i == p.aliases.begin())
					out << *i;
				else
					out << ", " << *i;
			out << "." << endl;
			out << endl;
		}
		writer.outstring("Description: " + p.description);
	}

	if (p.hasOptions())
		outputOptions(out, writer, p);

	out << endl;
}

static string toupper(const std::string& str)
{
	string res;
	for (size_t i = 0; i < str.size(); i++)
		res += ::toupper(str[i]);
	return res;
}

static string man_date()
{
	time_t tnow = time(0);
	struct tm* now = gmtime(&tnow);
	char buf[20];
	const char* oldlocale = setlocale(LC_TIME, "C");
	string res(buf, strftime(buf, 20, "%B %d, %Y", now));
	setlocale(LC_TIME, oldlocale);
	return res;
}

void Manpage::outputParagraph(std::ostream& out, const std::string& str)
{
	for (size_t i = 0; i < str.size(); i++)
		switch (str[i])
		{
			case '-':
				out << "\\-";
				break;
			case '\n':
				out << "\n.br\n";
				break;
			default:
				out << str[i];
		}
	out << '\n';
}

void Manpage::outputOption(std::ostream& out, const Option* o)
{
	out << ".TP" << endl;
	out << ".B " << o->fullUsageForMan() << endl;
	out << o->description << "." << endl;
}

void Manpage::runHooks(std::ostream& out, const std::string& section, where where)
{
	for (std::vector<Hook>::const_iterator i = hooks.begin();
			i != hooks.end(); i++)
		if (i->section == section && i->placement == where)
			out << i->text;
}

void Manpage::startSection(std::ostream& out, const std::string& name)
{
	runHooks(out, name, BEFORE);
	out << ".SH " << name << endl;
	runHooks(out, name, BEGINNING);
	lastSection = name;
}

void Manpage::endSection(std::ostream& out)
{
	runHooks(out, lastSection, END);
	lastSection.clear();
}

void Manpage::outputOptions(std::ostream& out, const Engine& p)
{
	for (vector<OptionGroup*>::const_iterator i = p.groups().begin();
			i != p.groups().end(); i++)
	{
		if ((*i)->hidden) continue;
		if (!(*i)->description.empty())
			out << endl << (*i)->description << ":" << endl;
		for (vector<Option*>::const_iterator j = (*i)->options.begin();
				j != (*i)->options.end(); ++j)
		{
			if ((*j)->hidden) continue;
			outputOption(out, *j);
		}
		out << ".PP" << endl;
	}

	if (!p.options().empty())
	{
		out << endl;
		out << "Other options:" << endl;
		for (vector<Option*>::const_iterator j = p.options().begin();
				j != p.options().end(); ++j)
		{
			if ((*j)->hidden) continue;
			outputOption(out, *j);
		}
	}
}

void Manpage::output(std::ostream& out, const Engine& p)
{
	// Manpage header
	out << ".TH " << toupper(m_app) << " " << m_section << " \"" << man_date() << "\" \"" << m_ver << "\"" << endl;

	startSection(out, "NAME");
	out << p.name() << " \\- " << p.description << endl;
	endSection(out);

	startSection(out, "SYNOPSIS");
	out << "\\fB" << p.name() << "\\fP [options] " << p.usage << endl;
	endSection(out);

	startSection(out, "DESCRIPTION");
	if (!p.longDescription.empty())
		outputParagraph(out, p.longDescription);
	endSection(out);

	if (!p.commands().empty())
	{
		const vector<Engine*>& commands = p.commands();

		startSection(out, "COMMANDS");
		out << "\\fB" << p.name() << "\\fP accepts a non-switch argument, that indicates what is the operation that should be performed:" << endl;
		for (vector<Engine*>::const_iterator i = commands.begin();
				i != commands.end(); i++)
		{
			if ((*i)->hidden) continue;
			out << ".TP" << endl;
			out << "\\fB" << (*i)->primaryAlias << "\\fP";

			const vector<string>& v = (*i)->aliases;
			for (vector<string>::const_iterator j = v.begin(); j != v.end(); j++)
				out << " or \\fB" << *j << "\\fP";

			out << " " << (*i)->usage << endl;
			out << ".br" << endl;
			if ((*i)->longDescription.empty())
				outputParagraph(out, (*i)->description);
			else
				outputParagraph(out, (*i)->longDescription);
		}
		endSection(out);
	}

	startSection(out, "OPTIONS");
	out << "This program follows the usual GNU command line syntax, with long options starting with two dashes (`\\-')." << endl << endl;
	if (!p.commands().empty())
		out << "Every one of the commands listed above has its own set of options.  To keep this manpage readable, all the options are presented together.  Please refer to \"\\fB" << p.name() << "\\fP help \\fIcommand\\fP\" to see which options are accepted by a given command." << endl;

	// Output the general options
	outputOptions(out, p);

	// Output group-specific options
	if (!p.commands().empty())
	{
		const vector<Engine*>& commands = p.commands();
		for (vector<Engine*>::const_iterator i = commands.begin();
				i != commands.end(); i++)
		{
			if ((*i)->hidden) continue;
			out << "\\fBOptions for command " << (*i)->primaryAlias << "\\fP" << endl;
			out << ".br" << endl;
			outputOptions(out, **i);
		}
	}
	endSection(out);

	startSection(out, "AUTHOR");
	out << "\\fB" << p.name() << "\\fP is maintained by " << m_author << "." << endl << endl;
	out << "This manpage has been automatically generated by the " << m_app << " program." << endl;
	endSection(out);
}

static string readline(FILE* in)
{
	string res;
	int c;
	while ((c = getc(in)) != EOF && c != '\n')
		res += c;
	return res;
}

void Manpage::readHooks(const std::string& file)
{
	FILE* in = fopen(file.c_str(), "r");
	if (!in) throw exception::File(file, "opening for reading");
	string section;
	commandline::Manpage::where placement = commandline::Manpage::BEFORE;
	string text;
	while (!feof(in))
	{
		string line(readline(in));
		if (line.empty())
			continue;
		if (line[0] == '|')
		{
			text += line.substr(1) + "\n";
		}
		else if (isalpha(line[0]))
		{
			if (!section.empty())
			{
				addHook(section, placement, text);
				text.clear();
			}
			size_t sep = line.find(' ');
			if (sep == string::npos)
			{
				fclose(in);
				throw exception::Consistency("expected two words in line: " + line);
			}
			section = line.substr(0, sep);
			string w(line, sep+1);
			if (w == "before")
			{
				placement = commandline::Manpage::BEFORE;
			} else if (w == "beginning") {
				placement = commandline::Manpage::BEGINNING;
			} else if (w == "end") {
				placement = commandline::Manpage::END;
			} else {
				fclose(in);
				throw exception::Consistency("expected 'before', 'beginning' or 'end' in line: " + line);
			}
		}
	}
	if (!section.empty())
		addHook(section, placement, text);
	fclose(in);
}

}
}


// vim:set ts=4 sw=4:
