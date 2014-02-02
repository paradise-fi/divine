#include <divine/llvm/silk-parse.h>

using namespace divine::silk;

const std::string parse::Token::tokenName[] =
{
    "<invalid>", "<comment>", "<newline>", "<application>", "<identifier>", "<constant>",
    "map", "fold", ".", "|", "=",
    "(", ")",
    "if", "then", "else",
    "||", "&&", "not",
    "<=", "<", ">=", ">", "==", "!=",
    "+", "-", "*", "/", "%", "|", "&", "^", "<<", ">>"
};

const std::string parse::Token::frag[] = { "#", "\n", "and", "or", "true", "false" };
