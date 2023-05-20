<<<<<<< HEAD
#include "front/lexical.h"

#include <map>
#include <cassert>
#include <string>

#define TODO assert(0 && "todo")

// #define DEBUG_DFA
#define DEBUG_SCANNER

std::string frontend::toString(State s)
{
    switch (s)
    {
    case State::Empty:
        return "Empty";
    case State::Ident:
        return "Ident";
    case State::IntLiteral:
        return "IntLiteral";
    case State::FloatLiteral:
        return "FloatLiteral";
    case State::op:
        return "op";
    default:
        assert(0 && "invalid State");
    }
    return "";
}

frontend::TokenType get_op_type(std::string s)
{
    if (s == "+")
        return frontend::TokenType::PLUS;
    if (s == "-")
        return frontend::TokenType::MINU;
    if (s == "*")
        return frontend::TokenType::MULT;
    if (s == "/")
        return frontend::TokenType::DIV;
    if (s == "%")
        return frontend::TokenType::MOD;
    if (s == "<")
        return frontend::TokenType::LSS;
    if (s == ">")
        return frontend::TokenType::GTR;
    if (s == ":")
        return frontend::TokenType::COLON;
    if (s == "=")
        return frontend::TokenType::ASSIGN;
    if (s == ";")
        return frontend::TokenType::SEMICN;
    if (s == ",")
        return frontend::TokenType::COMMA;
    if (s == "(")
        return frontend::TokenType::LPARENT;
    if (s == ")")
        return frontend::TokenType::RPARENT;
    if (s == "[")
        return frontend::TokenType::LBRACK;
    if (s == "]")
        return frontend::TokenType::RBRACK;
    if (s == "{")
        return frontend::TokenType::LBRACE;
    if (s == "}")
        return frontend::TokenType::RBRACE;
    if (s == "!")
        return frontend::TokenType::NOT;
    if (s == "<=")
        return frontend::TokenType::LEQ;
    if (s == ">=")
        return frontend::TokenType::GEQ;
    if (s == "==")
        return frontend::TokenType::EQL;
    if (s == "!=")
        return frontend::TokenType::NEQ;
    if (s == "&&")
        return frontend::TokenType::AND;
    if (s == "||")
        return frontend::TokenType::OR;
    std::cout << "Invalid Op:" << s << std::endl;
    assert(0 && "invalid  op");
}

frontend::TokenType get_keywords_type(std::string s)
{
    if (s == "const")
        return frontend::TokenType::CONSTTK;
    if (s == "void")
        return frontend::TokenType::VOIDTK;
    if (s == "int")
        return frontend::TokenType::INTTK;
    if (s == "float")
        return frontend::TokenType::FLOATTK;
    if (s == "if")
        return frontend::TokenType::IFTK;
    if (s == "else")
        return frontend::TokenType::ELSETK;
    if (s == "while")
        return frontend::TokenType::WHILETK;
    if (s == "continue")
        return frontend::TokenType::CONTINUETK;
    if (s == "break")
        return frontend::TokenType::BREAKTK;
    if (s == "return")
        return frontend::TokenType::RETURNTK;
    std::cout << "Invalid Keyword:" << s << std::endl;
    assert(0 && "invalid  Keyword");
}

std::set<std::string> frontend::keywords = {
    "const", "int", "float", "if", "else", "while", "continue", "break", "return", "void"};

frontend::DFA::DFA() : cur_state(frontend::State::Empty), cur_str() {}

frontend::DFA::~DFA() {}

bool frontend::DFA::next(char input, Token &buf)
{
#ifdef DEBUG_DFA
#include <iostream>
    std::cout << "in state [" << toString(cur_state) << "], input = \'" << input << "\', str = " << cur_str << "\t";
#endif

    switch (cur_state)
    {
    // 当前状态为Empty
    case State::Empty:
        // 当前输入为一元操作符
        if (input == '+' || input == '-' || input == '*' || input == '/' || input == '%' || input == '<' || input == '>' || input == ':' || input == '=' || input == ';' || input == ',' || input == '(' || input == ')' || input == '[' || input == ']' || input == '{' || input == '}' || input == '!' || input == '&' || input == '|')
        {
            cur_state = State::op;
            cur_str = input;
            return false;
        }
        // 当前输入为字母
        else if ((input >= 'a' && input <= 'z') || (input >= 'A' && input <= 'Z') || (input == '_'))
        {
            cur_state = State::Ident;
            cur_str = input;
            return false;
        }
        // 当前输入为数字
        else if (input >= '0' && input <= '9')
        {
            cur_state = State::IntLiteral;
            cur_str = input;
            return false;
        }
        // 当前输入为.
        else if (input == '.')
        {
            cur_state = State::FloatLiteral;
            cur_str = input;
            return false;
        }
        // 当前输入为空格或者换行等
        else if (input == ' ' || input == '\n' || input == '\r' || input == '\t')
        {
            cur_state = State::Empty;
            return false;
        }
        break;
    // 当前状态为op
    case State::op:
        // 当前输入为二元操作符的第二个字符
        if ((cur_str == "<" && input == '=') || (cur_str == ">" && input == '=') || (cur_str == "=" && input == '=') || (cur_str == "!" && input == '=') || (cur_str == "&" && input == '&') || (cur_str == "|" && input == '|'))
        {
            cur_state = State::op;
            cur_str += input;
            return false;
        }
        // 当前输入为一元操作符
        else if (input == '+' || input == '-' || input == '*' || input == '/' || input == '%' || input == '<' || input == '>' || input == ':' || input == '=' || input == ';' || input == ',' || input == '(' || input == ')' || input == '[' || input == ']' || input == '{' || input == '}' || input == '!' || input == '&' || input == '|')
        {
            buf.type = get_op_type(cur_str);
            buf.value = cur_str;
            cur_state = State::op;
            cur_str = input;
            return true;
        }
        // 当前输入为字母
        else if ((input >= 'a' && input <= 'z') || (input >= 'A' && input <= 'Z') || (input == '_'))
        {
            buf.type = get_op_type(cur_str);
            buf.value = cur_str;
            cur_state = State::Ident;
            cur_str = input;
            return true;
        }
        // 当前输入为数字
        else if (input >= '0' && input <= '9')
        {
            buf.type = get_op_type(cur_str);
            buf.value = cur_str;
            cur_state = State::IntLiteral;
            cur_str = input;
            return true;
        }
        // 当前输入为.
        else if (input == '.')
        {
            buf.type = get_op_type(cur_str);
            buf.value = cur_str;
            cur_state = State::FloatLiteral;
            cur_str = input;
            return true;
        }
        // 当前输入为空格或者换行等
        else if (input == ' ' || input == '\n' || input == '\r' || input == '\t')
        {
            buf.type = get_op_type(cur_str);
            buf.value = cur_str;
            cur_state = State::Empty;
            cur_str = "";
            return true;
        }
        break;
    // 当前状态为Ident
    case State::Ident:
        // 当前输入为一元操作符
        if (input == '+' || input == '-' || input == '*' || input == '/' || input == '%' || input == '<' || input == '>' || input == ':' || input == '=' || input == ';' || input == ',' || input == '(' || input == ')' || input == '[' || input == ']' || input == '{' || input == '}' || input == '!' || input == '&' || input == '|')
        {
            if (keywords.find(cur_str) != keywords.end())
            {
                buf.type = get_keywords_type(cur_str);
                buf.value = cur_str;
            }
            else
            {
                buf.type = frontend::TokenType::IDENFR;
                buf.value = cur_str;
            }
            cur_state = State::op;
            cur_str = input;
            return true;
        }
        // 当前输入为字母
        else if ((input >= 'a' && input <= 'z') || (input >= 'A' && input <= 'Z') || (input == '_'))
        {
            cur_state = State::Ident;
            cur_str += input;
            return false;
        }
        // 当前输入为数字
        else if (input >= '0' && input <= '9')
        {
            cur_state = State::Ident;
            cur_str += input;
            return false;
        }
        // 当前输入为.
        else if (input == '.')
        {
            buf.type = get_op_type(cur_str);
            buf.value = cur_str;
            cur_state = State::FloatLiteral;
            cur_str = input;
            return true;
        }
        // 当前输入为空格或者换行等
        else if (input == ' ' || input == '\n' || input == '\r' || input == '\t')
        {
            if (keywords.find(cur_str) != keywords.end())
            {
                buf.type = get_keywords_type(cur_str);
                buf.value = cur_str;
            }
            else
            {
                buf.type = frontend::TokenType::IDENFR;
                buf.value = cur_str;
            }
            cur_state = State::Empty;
            cur_str = "";
            return true;
        }
        break;
    // 当前状态为IntLiteral
    case State::IntLiteral:
        // 当前输入为一元操作符
        if (input == '+' || input == '-' || input == '*' || input == '/' || input == '%' || input == '<' || input == '>' || input == ':' || input == '=' || input == ';' || input == ',' || input == '(' || input == ')' || input == '[' || input == ']' || input == '{' || input == '}' || input == '!' || input == '&' || input == '|')
        {
            buf.type = frontend::TokenType::INTLTR;
            buf.value = cur_str;
            cur_state = State::op;
            cur_str = input;
            return true;
        }
        // 当前输入为字母
        else if ((input >= 'a' && input <= 'z') || (input >= 'A' && input <= 'Z') || (input == '_'))
        {
            if (((cur_str == "0") && (input == 'B' || input == 'b' || input == 'X' || input == 'x')) || ((cur_str.size() >= 2) && (cur_str[1] == 'X' || cur_str[1] == 'x') && ((input >= 'a' && input <= 'f') || (input >= 'A' && input <= 'F'))))
            {
                cur_state = State::IntLiteral;
                cur_str += input;
                return false;
            }
            buf.type = frontend::TokenType::INTLTR;
            buf.value = cur_str;
            cur_state = State::Ident;
            cur_str = input;
            return true;
        }
        // 当前输入为数字
        else if (input >= '0' && input <= '9')
        {
            cur_state = State::IntLiteral;
            cur_str += input;
            return false;
        }
        // 当前输入为'.'
        else if (input == '.')
        {
            cur_state = State::FloatLiteral;
            cur_str += '.';
            return false;
        }
        // 当前输入为空格或者换行等
        else if (input == ' ' || input == '\n' || input == '\r' || input == '\t')
        {
            buf.type = frontend::TokenType::INTLTR;
            buf.value = cur_str;
            cur_state = State::Empty;
            cur_str = "";
            return true;
        }
        break;
    // 当前状态为FloatLiteral
    case State::FloatLiteral:
        // 当前输入为一元操作符
        if (input == '+' || input == '-' || input == '*' || input == '/' || input == '%' || input == '<' || input == '>' || input == ':' || input == '=' || input == ';' || input == ',' || input == '(' || input == ')' || input == '[' || input == ']' || input == '{' || input == '}' || input == '!' || input == '&' || input == '|')
        {
            buf.type = frontend::TokenType::FLOATLTR;
            buf.value = cur_str;
            cur_state = State::op;
            cur_str = input;
            return true;
        }
        // 当前输入为字母
        else if ((input >= 'a' && input <= 'z') || (input >= 'A' && input <= 'Z') || ((input == '_')))
        {
            buf.type = frontend::TokenType::FLOATLTR;
            buf.value = cur_str;
            cur_state = State::Ident;
            cur_str = input;
            return true;
        }
        // 当前输入为数字
        else if (input >= '0' && input <= '9')
        {
            cur_state = State::FloatLiteral;
            cur_str += input;
            return false;
        }
        // 当前输入为空格或者换行等
        else if (input == ' ' || input == '\n' || input == '\r' || input == '\t')
        {
            buf.type = frontend::TokenType::FLOATLTR;
            buf.value = cur_str;
            cur_state = State::Empty;
            cur_str = "";
            return true;
        }
        break;
    }

#ifdef DEBUG_DFA
    std::cout << "next state is [" << toString(cur_state) << "], next str = " << cur_str << "\t, ret = " << ret << std::endl;
#endif
}

void frontend::DFA::reset()
{
    cur_state = State::Empty;
    cur_str = "";
}

frontend::Scanner::Scanner(std::string filename) : fin(filename)
{
    if (!fin.is_open())
    {
        assert(0 && "in Scanner constructor, input file cannot open");
    }
}

frontend::Scanner::~Scanner()
{
    fin.close();
}

std::string preproccess(std::string s)
{ // 删除注释
    std::string res = "";
    int state = 0; // 用来表示去除注释过程中不同的状态,初始状态为0
    for (int i = 0; i < int(s.length()); i++)
    { // 从s中一个字符一个字符的读
        switch (state)
        {
        case 0:
            if (s[i] == '/')
            {
                state = 1;
                res = res + s[i];
            }
            else if (s[i] == '\'')
            {
                state = 5;
                res = res + s[i];
            }
            else if (s[i] == '"')
            {
                state = 6;
                res = res + s[i];
            }
            else
            {
                state = 0;
                res = res + s[i];
            }
            break;

        case 1: //   /
            if (s[i] == '/')
            {
                state = 4;
                res = res.substr(0, res.length() - 1); // 去掉前面的'/'
            }
            else if (s[i] == '*')
            {
                state = 2;
                res = res.substr(0, res.length() - 1); // 去掉前面的'/'
            }
            else
            {
                state = 0;
                res = res + s[i];
            }
            break;

        case 2: //  /*...     多行注释状态：什么都不输出
            if (s[i] == '*')
            {
                state = 3;
            }
            else
            {
                state = 2;
            }
            break;

        case 3: //   /*...*
            if (s[i] == '/')
            {
                state = 0;
            }
            else
            {
                state = 2;
            }
            break;

        case 4: //   //...    单行注释状态：什么都不输出
            if (s[i] == '\n')
            { // 单行注释状态结束
                state = 0;
                res = res + s[i];
            }
            else
            {
                state = 4;
            }
            break;

        case 5: //   '    单引号内全都输出
            if (s[i] == '\'')
            { // 单引号状态结束
                state = 0;
                res = res + s[i];
            }
            else
            {
                state = 5;
                res = res + s[i];
            }
            break;

        case 6: //   "    双引号内全都输出
            if (s[i] == '"')
            { // 双引号状态结束
                state = 0;
                res = res + s[i];
            }
            else
            {
                state = 6;
                res = res + s[i];
            }
            break;
        }
    }
    return res;
}

std::vector<frontend::Token> frontend::Scanner::run()
{
    std::vector<Token> ret;
    Token tk;
    DFA dfa;
    std::string s = "", temp;
    while (getline(fin, temp)) // dasdadawfeafaf
    {
        s = s + temp;
        s = s + '\n';
    }
    s = preproccess(s); // delete comments
    for (auto c : s)
    {
        if (dfa.next(c, tk))
        {
            ret.push_back(tk);
        }
    }
    return ret;
#ifdef DEBUG_SCANNER
#include <iostream>
    std::cout << "token: " << toString(tk.type) << "\t" << tk.value << std::endl;
#endif
=======
#include "front/lexical.h"

#include <map>
#include <cassert>
#include <string>

#define TODO assert(0 && "todo")

// #define DEBUG_DFA
#define DEBUG_SCANNER

std::string frontend::toString(State s)
{
    switch (s)
    {
    case State::Empty:
        return "Empty";
    case State::Ident:
        return "Ident";
    case State::IntLiteral:
        return "IntLiteral";
    case State::FloatLiteral:
        return "FloatLiteral";
    case State::op:
        return "op";
    default:
        assert(0 && "invalid State");
    }
    return "";
}

frontend::TokenType get_op_type(std::string s)
{
    if (s == "+")
        return frontend::TokenType::PLUS;
    if (s == "-")
        return frontend::TokenType::MINU;
    if (s == "*")
        return frontend::TokenType::MULT;
    if (s == "/")
        return frontend::TokenType::DIV;
    if (s == "%")
        return frontend::TokenType::MOD;
    if (s == "<")
        return frontend::TokenType::LSS;
    if (s == ">")
        return frontend::TokenType::GTR;
    if (s == ":")
        return frontend::TokenType::COLON;
    if (s == "=")
        return frontend::TokenType::ASSIGN;
    if (s == ";")
        return frontend::TokenType::SEMICN;
    if (s == ",")
        return frontend::TokenType::COMMA;
    if (s == "(")
        return frontend::TokenType::LPARENT;
    if (s == ")")
        return frontend::TokenType::RPARENT;
    if (s == "[")
        return frontend::TokenType::LBRACK;
    if (s == "]")
        return frontend::TokenType::RBRACK;
    if (s == "{")
        return frontend::TokenType::LBRACE;
    if (s == "}")
        return frontend::TokenType::RBRACE;
    if (s == "!")
        return frontend::TokenType::NOT;
    if (s == "<=")
        return frontend::TokenType::LEQ;
    if (s == ">=")
        return frontend::TokenType::GEQ;
    if (s == "==")
        return frontend::TokenType::EQL;
    if (s == "!=")
        return frontend::TokenType::NEQ;
    if (s == "&&")
        return frontend::TokenType::AND;
    if (s == "||")
        return frontend::TokenType::OR;
    std::cout << "Invalid Op:" << s << std::endl;
    assert(0 && "invalid  op");
}

frontend::TokenType get_keywords_type(std::string s)
{
    if (s == "const")
        return frontend::TokenType::CONSTTK;
    if (s == "void")
        return frontend::TokenType::VOIDTK;
    if (s == "int")
        return frontend::TokenType::INTTK;
    if (s == "float")
        return frontend::TokenType::FLOATTK;
    if (s == "if")
        return frontend::TokenType::IFTK;
    if (s == "else")
        return frontend::TokenType::ELSETK;
    if (s == "while")
        return frontend::TokenType::WHILETK;
    if (s == "continue")
        return frontend::TokenType::CONTINUETK;
    if (s == "break")
        return frontend::TokenType::BREAKTK;
    if (s == "return")
        return frontend::TokenType::RETURNTK;
    std::cout << "Invalid Keyword:" << s << std::endl;
    assert(0 && "invalid  Keyword");
}

std::set<std::string> frontend::keywords = {
    "const", "int", "float", "if", "else", "while", "continue", "break", "return", "void"};

frontend::DFA::DFA() : cur_state(frontend::State::Empty), cur_str() {}

frontend::DFA::~DFA() {}

bool frontend::DFA::next(char input, Token &buf)
{
#ifdef DEBUG_DFA
#include <iostream>
    std::cout << "in state [" << toString(cur_state) << "], input = \'" << input << "\', str = " << cur_str << "\t";
#endif

    switch (cur_state)
    {
    // 当前状态为Empty
    case State::Empty:
        // 当前输入为一元操作符
        if (input == '+' || input == '-' || input == '*' || input == '/' || input == '%' || input == '<' || input == '>' || input == ':' || input == '=' || input == ';' || input == ',' || input == '(' || input == ')' || input == '[' || input == ']' || input == '{' || input == '}' || input == '!' || input == '&' || input == '|')
        {
            cur_state = State::op;
            cur_str = input;
            return false;
        }
        // 当前输入为字母
        else if ((input >= 'a' && input <= 'z') || (input >= 'A' && input <= 'Z') || (input == '_'))
        {
            cur_state = State::Ident;
            cur_str = input;
            return false;
        }
        // 当前输入为数字
        else if (input >= '0' && input <= '9')
        {
            cur_state = State::IntLiteral;
            cur_str = input;
            return false;
        }
        // 当前输入为.
        else if (input == '.')
        {
            cur_state = State::FloatLiteral;
            cur_str = input;
            return false;
        }
        // 当前输入为空格或者换行等
        else if (input == ' ' || input == '\n' || input == '\r' || input == '\t')
        {
            cur_state = State::Empty;
            return false;
        }
        break;
    // 当前状态为op
    case State::op:
        // 当前输入为二元操作符的第二个字符
        if ((cur_str == "<" && input == '=') || (cur_str == ">" && input == '=') || (cur_str == "=" && input == '=') || (cur_str == "!" && input == '=') || (cur_str == "&" && input == '&') || (cur_str == "|" && input == '|'))
        {
            cur_state = State::op;
            cur_str += input;
            return false;
        }
        // 当前输入为一元操作符
        else if (input == '+' || input == '-' || input == '*' || input == '/' || input == '%' || input == '<' || input == '>' || input == ':' || input == '=' || input == ';' || input == ',' || input == '(' || input == ')' || input == '[' || input == ']' || input == '{' || input == '}' || input == '!' || input == '&' || input == '|')
        {
            buf.type = get_op_type(cur_str);
            buf.value = cur_str;
            cur_state = State::op;
            cur_str = input;
            return true;
        }
        // 当前输入为字母
        else if ((input >= 'a' && input <= 'z') || (input >= 'A' && input <= 'Z') || (input == '_'))
        {
            buf.type = get_op_type(cur_str);
            buf.value = cur_str;
            cur_state = State::Ident;
            cur_str = input;
            return true;
        }
        // 当前输入为数字
        else if (input >= '0' && input <= '9')
        {
            buf.type = get_op_type(cur_str);
            buf.value = cur_str;
            cur_state = State::IntLiteral;
            cur_str = input;
            return true;
        }
        // 当前输入为.
        else if (input == '.')
        {
            buf.type = get_op_type(cur_str);
            buf.value = cur_str;
            cur_state = State::FloatLiteral;
            cur_str = input;
            return true;
        }
        // 当前输入为空格或者换行等
        else if (input == ' ' || input == '\n' || input == '\r' || input == '\t')
        {
            buf.type = get_op_type(cur_str);
            buf.value = cur_str;
            cur_state = State::Empty;
            cur_str = "";
            return true;
        }
        break;
    // 当前状态为Ident
    case State::Ident:
        // 当前输入为一元操作符
        if (input == '+' || input == '-' || input == '*' || input == '/' || input == '%' || input == '<' || input == '>' || input == ':' || input == '=' || input == ';' || input == ',' || input == '(' || input == ')' || input == '[' || input == ']' || input == '{' || input == '}' || input == '!' || input == '&' || input == '|')
        {
            if (keywords.find(cur_str) != keywords.end())
            {
                buf.type = get_keywords_type(cur_str);
                buf.value = cur_str;
            }
            else
            {
                buf.type = frontend::TokenType::IDENFR;
                buf.value = cur_str;
            }
            cur_state = State::op;
            cur_str = input;
            return true;
        }
        // 当前输入为字母
        else if ((input >= 'a' && input <= 'z') || (input >= 'A' && input <= 'Z') || (input == '_'))
        {
            cur_state = State::Ident;
            cur_str += input;
            return false;
        }
        // 当前输入为数字
        else if (input >= '0' && input <= '9')
        {
            cur_state = State::Ident;
            cur_str += input;
            return false;
        }
        // 当前输入为.
        else if (input == '.')
        {
            buf.type = get_op_type(cur_str);
            buf.value = cur_str;
            cur_state = State::FloatLiteral;
            cur_str = input;
            return true;
        }
        // 当前输入为空格或者换行等
        else if (input == ' ' || input == '\n' || input == '\r' || input == '\t')
        {
            if (keywords.find(cur_str) != keywords.end())
            {
                buf.type = get_keywords_type(cur_str);
                buf.value = cur_str;
            }
            else
            {
                buf.type = frontend::TokenType::IDENFR;
                buf.value = cur_str;
            }
            cur_state = State::Empty;
            cur_str = "";
            return true;
        }
        break;
    // 当前状态为IntLiteral
    case State::IntLiteral:
        // 当前输入为一元操作符
        if (input == '+' || input == '-' || input == '*' || input == '/' || input == '%' || input == '<' || input == '>' || input == ':' || input == '=' || input == ';' || input == ',' || input == '(' || input == ')' || input == '[' || input == ']' || input == '{' || input == '}' || input == '!' || input == '&' || input == '|')
        {
            buf.type = frontend::TokenType::INTLTR;
            buf.value = cur_str;
            cur_state = State::op;
            cur_str = input;
            return true;
        }
        // 当前输入为字母
        else if ((input >= 'a' && input <= 'z') || (input >= 'A' && input <= 'Z') || (input == '_'))
        {
            if (((cur_str == "0") && (input == 'B' || input == 'b' || input == 'X' || input == 'x')) || ((cur_str.size() >= 2) && (cur_str[1] == 'X' || cur_str[1] == 'x') && ((input >= 'a' && input <= 'f') || (input >= 'A' && input <= 'F'))))
            {
                cur_state = State::IntLiteral;
                cur_str += input;
                return false;
            }
            buf.type = frontend::TokenType::INTLTR;
            buf.value = cur_str;
            cur_state = State::Ident;
            cur_str = input;
            return true;
        }
        // 当前输入为数字
        else if (input >= '0' && input <= '9')
        {
            cur_state = State::IntLiteral;
            cur_str += input;
            return false;
        }
        // 当前输入为'.'
        else if (input == '.')
        {
            cur_state = State::FloatLiteral;
            cur_str += '.';
            return false;
        }
        // 当前输入为空格或者换行等
        else if (input == ' ' || input == '\n' || input == '\r' || input == '\t')
        {
            buf.type = frontend::TokenType::INTLTR;
            buf.value = cur_str;
            cur_state = State::Empty;
            cur_str = "";
            return true;
        }
        break;
    // 当前状态为FloatLiteral
    case State::FloatLiteral:
        // 当前输入为一元操作符
        if (input == '+' || input == '-' || input == '*' || input == '/' || input == '%' || input == '<' || input == '>' || input == ':' || input == '=' || input == ';' || input == ',' || input == '(' || input == ')' || input == '[' || input == ']' || input == '{' || input == '}' || input == '!' || input == '&' || input == '|')
        {
            buf.type = frontend::TokenType::FLOATLTR;
            buf.value = cur_str;
            cur_state = State::op;
            cur_str = input;
            return true;
        }
        // 当前输入为字母
        else if ((input >= 'a' && input <= 'z') || (input >= 'A' && input <= 'Z') || ((input == '_')))
        {
            buf.type = frontend::TokenType::FLOATLTR;
            buf.value = cur_str;
            cur_state = State::Ident;
            cur_str = input;
            return true;
        }
        // 当前输入为数字
        else if (input >= '0' && input <= '9')
        {
            cur_state = State::FloatLiteral;
            cur_str += input;
            return false;
        }
        // 当前输入为空格或者换行等
        else if (input == ' ' || input == '\n' || input == '\r' || input == '\t')
        {
            buf.type = frontend::TokenType::FLOATLTR;
            buf.value = cur_str;
            cur_state = State::Empty;
            cur_str = "";
            return true;
        }
        break;
    }

#ifdef DEBUG_DFA
    std::cout << "next state is [" << toString(cur_state) << "], next str = " << cur_str << "\t, ret = " << ret << std::endl;
#endif
}

void frontend::DFA::reset()
{
    cur_state = State::Empty;
    cur_str = "";
}

frontend::Scanner::Scanner(std::string filename) : fin(filename)
{
    if (!fin.is_open())
    {
        assert(0 && "in Scanner constructor, input file cannot open");
    }
}

frontend::Scanner::~Scanner()
{
    fin.close();
}

std::string preproccess(std::string s)
{ // 删除注释
    std::string res = "";
    int state = 0; // 用来表示去除注释过程中不同的状态,初始状态为0
    for (int i = 0; i < int(s.length()); i++)
    { // 从s中一个字符一个字符的读
        switch (state)
        {
        case 0:
            if (s[i] == '/')
            {
                state = 1;
                res = res + s[i];
            }
            else if (s[i] == '\'')
            {
                state = 5;
                res = res + s[i];
            }
            else if (s[i] == '"')
            {
                state = 6;
                res = res + s[i];
            }
            else
            {
                state = 0;
                res = res + s[i];
            }
            break;

        case 1: //   /
            if (s[i] == '/')
            {
                state = 4;
                res = res.substr(0, res.length() - 1); // 去掉前面的'/'
            }
            else if (s[i] == '*')
            {
                state = 2;
                res = res.substr(0, res.length() - 1); // 去掉前面的'/'
            }
            else
            {
                state = 0;
                res = res + s[i];
            }
            break;

        case 2: //  /*...     多行注释状态：什么都不输出
            if (s[i] == '*')
            {
                state = 3;
            }
            else
            {
                state = 2;
            }
            break;

        case 3: //   /*...*
            if (s[i] == '/')
            {
                state = 0;
            }
            else
            {
                state = 2;
            }
            break;

        case 4: //   //...    单行注释状态：什么都不输出
            if (s[i] == '\n')
            { // 单行注释状态结束
                state = 0;
                res = res + s[i];
            }
            else
            {
                state = 4;
            }
            break;

        case 5: //   '    单引号内全都输出
            if (s[i] == '\'')
            { // 单引号状态结束
                state = 0;
                res = res + s[i];
            }
            else
            {
                state = 5;
                res = res + s[i];
            }
            break;

        case 6: //   "    双引号内全都输出
            if (s[i] == '"')
            { // 双引号状态结束
                state = 0;
                res = res + s[i];
            }
            else
            {
                state = 6;
                res = res + s[i];
            }
            break;
        }
    }
    return res;
}

std::vector<frontend::Token> frontend::Scanner::run()
{
    std::vector<Token> ret;
    Token tk;
    DFA dfa;
    std::string s = "", temp;
    while (getline(fin, temp)) // dasdadawfeafaf
    {
        s = s + temp;
        s = s + '\n';
    }
    s = preproccess(s); // delete comments
    for (auto c : s)
    {
        if (dfa.next(c, tk))
        {
            ret.push_back(tk);
        }
    }
    return ret;
#ifdef DEBUG_SCANNER
#include <iostream>
    std::cout << "token: " << toString(tk.type) << "\t" << tk.value << std::endl;
#endif
>>>>>>> ffd4636 (58/58 拿下)
}