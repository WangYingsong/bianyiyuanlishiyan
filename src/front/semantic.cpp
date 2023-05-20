<<<<<<< HEAD
#include "front/semantic.h"
#include <iostream>
#include <cassert>

using ir::Function;
using ir::Instruction;
using ir::Operand;
using ir::Operator;

#define TODO assert(0 && "TODO");

#define GET_CHILD_PTR(node, type, index)                     \
    auto node = dynamic_cast<type *>(root->children[index]); \
    assert(node);
#define ANALYSIS(node, type, index)                          \
    auto node = dynamic_cast<type *>(root->children[index]); \
    assert(node);                                            \
    analysis##type(node, buffer);
#define COPY_EXP_NODE(from, to)              \
    to->is_computable = from->is_computable; \
    to->v = from->v;                         \
    to->t = from->t;

map<std::string, ir::Function *> *frontend::get_lib_funcs()
{
    static map<std::string, ir::Function *> lib_funcs = {
        {"getint", new Function("getint", Type::Int)},
        {"getch", new Function("getch", Type::Int)},
        {"getfloat", new Function("getfloat", Type::Float)},
        {"getarray", new Function("getarray", {Operand("arr", Type::IntPtr)}, Type::Int)},
        {"getfarray", new Function("getfarray", {Operand("arr", Type::FloatPtr)}, Type::Int)},
        {"putint", new Function("putint", {Operand("i", Type::Int)}, Type::null)},
        {"putch", new Function("putch", {Operand("i", Type::Int)}, Type::null)},
        {"putfloat", new Function("putfloat", {Operand("f", Type::Float)}, Type::null)},
        {"putarray", new Function("putarray", {Operand("n", Type::Int), Operand("arr", Type::IntPtr)}, Type::null)},
        {"putfarray", new Function("putfarray", {Operand("n", Type::Int), Operand("arr", Type::FloatPtr)}, Type::null)},
    };
    return &lib_funcs;
}

void frontend::SymbolTable::add_scope()
{
    ScopeInfo scope_info;
    scope_info.cnt = 0;
    scope_info.name = "block " + std::to_string(scope_stack.size());
    scope_stack.push_back(scope_info);
}
void frontend::SymbolTable::exit_scope()
{
    scope_stack.pop_back();
}

string frontend::SymbolTable::get_scoped_name(string id) const
{
    for (auto it = scope_stack.rbegin(); it != scope_stack.rend(); it++)
    {
        auto scope = *it;
        if (scope.table.find(id) != scope.table.end())
        {
            return scope.name + "_" + id;
        }
    }
}

Operand frontend::SymbolTable::get_operand(string id) const
{
    for (int i = scope_stack.size() - 1; i >= 0; i--)
    {
        auto find_result = scope_stack[i].table.find(id);
        if (find_result != scope_stack[i].table.end())
            return find_result->second.operand;
    }
}

frontend::STE frontend::SymbolTable::get_ste(string id) const
{
    for (int i = scope_stack.size() - 1; i >= 0; i--)
    {
        auto find_result = scope_stack[i].table.find(id);
        if (find_result != scope_stack[i].table.end())
        {
            // std::cout << "get_ste: " << id << " " << find_result->second.operand.name << std::endl;
            return find_result->second;
        }
    }
}

frontend::Analyzer::Analyzer() : tmp_cnt(0), symbol_table()
{
    // add lib functions
    auto lib_funcs = get_lib_funcs();
    for (auto &func : *lib_funcs)
    {
        symbol_table.functions[func.first] = func.second;
    }
}

ir::Program frontend::Analyzer::get_ir_program(CompUnit *root)
{
    // printf("get_ir_program\n");
    symbol_table.add_scope();
    ir::Function *global_fun = new ir::Function("global", Type::null);
    symbol_table.functions["global"] = global_fun;
    // auto lib_funcs = *get_lib_funcs();
    // for (auto it = lib_funcs.begin(); it != lib_funcs.end(); it++)
    // symbol_table.functions[it->first] = it->second;
    ir::Program program;
    analysisCompUnit(root, program);
    global_fun->addInst(new Instruction(ir::Operand(),
                                        ir::Operand(),
                                        ir::Operand(), ir::Operator::_return));
    program.addFunction(*global_fun);
    for (auto it = symbol_table.scope_stack[0].table.begin(); it != symbol_table.scope_stack[0].table.end(); it++)
    {
        if (it->second.dimension.size() != 0)
        {
            int arr_len = 1;
            for (int i = 0; i < it->second.dimension.size(); i++)
                arr_len *= it->second.dimension[i];
            program.globalVal.push_back({{symbol_table.get_scoped_name(it->second.operand.name), it->second.operand.type}, arr_len});
        }
        else
        {
            if (it->second.operand.type == Type::FloatLiteral)
                program.globalVal.push_back({{symbol_table.get_scoped_name(it->second.operand.name), Type::Float}});
            else if (it->second.operand.type == Type::IntLiteral)
                program.globalVal.push_back({{symbol_table.get_scoped_name(it->second.operand.name), Type::Int}});
            else
                program.globalVal.push_back({{symbol_table.get_scoped_name(it->second.operand.name), it->second.operand.type}});
        }
    }
    for (Function &func : program.functions)
        if (func.returnType == Type::null)
        {
            func.addInst(new Instruction(ir::Operand(),
                                         ir::Operand(),
                                         ir::Operand(),
                                         ir::Operator::_return));
        }
    return program;
}

// CompUnit -> (Decl | FuncDef) [CompUnit]
void frontend::Analyzer::analysisCompUnit(CompUnit *root, ir::Program &program)
{
    // printf("analysisCompUnit\n");
    if (Decl *decl = dynamic_cast<Decl *>(root->children[0]))
    {
        vector<ir::Instruction *> buffer;
        analysisDecl(decl, buffer);
        for (Instruction *inst : buffer)
            symbol_table.functions["global"]->addInst(inst);
    }
    else if (FuncDef *funcdef = dynamic_cast<FuncDef *>(root->children[0]))
    {
        ir::Function *func = new Function();
        cur_func = func;
        analysisFuncDef(funcdef, *func);
        program.addFunction(*func);
    }
    if (root->children.size() > 1)
        analysisCompUnit(dynamic_cast<CompUnit *>(root->children[1]), program);
}

// BType -> 'int' | 'float'
void frontend::Analyzer::analysisBType(BType *root)
{
    Term *bt = dynamic_cast<Term *>(root->children[0]);
    root->t = bt->token.type == TokenType::INTTK ? Type::Int : Type::Float;
}
// Decl -> ConstDecl | VarDecl
void frontend::Analyzer::analysisDecl(Decl *root, vector<ir::Instruction *> &buffer)
{
    if (root->children[0]->type == NodeType::CONSTDECL)
    {
        ANALYSIS(constdecl, ConstDecl, 0);
    }
    else if (root->children[0]->type == NodeType::VARDECL)
    {
        ANALYSIS(vardecl, VarDecl, 0);
    }
}

// ConstDecl -> 'const' BType ConstDef { ',' ConstDef } ';'
void frontend::Analyzer::analysisConstDecl(ConstDecl *root, vector<ir::Instruction *> &buffer)
{
    // std::cout << "constdecl begin" << std::endl;
    GET_CHILD_PTR(btype, BType, 1);
    Term *bt = dynamic_cast<Term *>(btype->children[0]);
    root->t = bt->token.type == TokenType::INTTK ? Type::Int : Type::Float;
    ANALYSIS(constdef, ConstDef, 2);
    for (int i = 3; i < root->children.size(); i = i + 2)
    {
        GET_CHILD_PTR(term, Term, i);
        if (term->token.type == TokenType::SEMICN)
            break;
        ANALYSIS(constdef, ConstDef, i + 1);
    }
    // std::cout << "constdecl end" << std::endl;
}

// ConstDef -> Ident { '[' ConstExp ']' } '=' ConstInitVal
void frontend::Analyzer::analysisConstDef(ConstDef *root, vector<ir::Instruction *> &buffer)
{
    ConstDecl *parent = dynamic_cast<ConstDecl *>(root->parent);
    Type ident_type = parent->t == Type::Int ? Type::IntLiteral : Type::FloatLiteral;
    GET_CHILD_PTR(ident, Term, 0);
    string Ident_name = ident->token.value;
    STE ste;
    Operand ident_operand = Operand(Ident_name, ident_type);
    if (root->children.size() > 1)
    {
        GET_CHILD_PTR(term_0, Term, 1);
        if (term_0->token.type == TokenType::LBRACK)
        {
            if (ident_operand.type == Type::IntLiteral)
                ident_operand.type = Type::IntPtr;
            else if (ident_operand.type == Type::FloatLiteral)
                ident_operand.type = Type::FloatPtr;
            ANALYSIS(constexp_1, ConstExp, 2);
            ste.dimension.push_back(std::stoi(constexp_1->v));
            if (root->children.size() > 4)
            {
                GET_CHILD_PTR(term_1, Term, 4);
                if (term_1->token.type == TokenType::LBRACK)
                {
                    ANALYSIS(constexp_2, ConstExp, 5);
                    ste.dimension.push_back(std::stoi(constexp_2->v));
                }
            }
        }
    }
    ste.operand = ident_operand;
    symbol_table.scope_stack.back().table[Ident_name] = ste;
    string ident_after_rename = symbol_table.get_scoped_name(Ident_name);

    GET_CHILD_PTR(term_0, Term, 1);
    // const int/float a = ...;
    if (term_0->token.type == TokenType::ASSIGN)
    {
        ANALYSIS(constinitval, ConstInitVal, 2);
        string initval_value = constinitval->v;
        if (ident_type == Type::IntLiteral && constinitval->t == Type::FloatLiteral)
            initval_value = std::to_string((int)std::stof(initval_value));
        else if (ident_type == Type::FloatLiteral && constinitval->t == Type::IntLiteral)
            initval_value = std::to_string((float)std::stoi(initval_value));
        symbol_table.scope_stack.back().table[Ident_name].literal_value = initval_value;
    }
    else if (term_0->token.type == TokenType::LBRACK)
    {
        int ident_array_dim = ste.dimension.size();
        int allco_size;
        allco_size = ident_array_dim == 1 ? ste.dimension[0] : ste.dimension[0] * ste.dimension[1];
        if (symbol_table.scope_stack.size() > 1)
            buffer.push_back(new Instruction(Operand(std::to_string(allco_size), Type::IntLiteral),
                                             Operand(),
                                             Operand(ident_after_rename, ident_operand.type),
                                             Operator::alloc));

        // int/float a[3]={1,2,3};
        // int/float a[2][3]={1,2,3,4,5,6};

        ANALYSIS(constinitval, ConstInitVal, root->children.size() - 1);
        string const_list = constinitval->v;
        Type const_type = constinitval->t;
        string temp_const = "";
        int j = 0;
        for (int i = 0; i < constinitval->v.size(); i++)
        {
            if (constinitval->v[i] == ',')
            {
                buffer.push_back(new Instruction(Operand(symbol_table.get_scoped_name(Ident_name), ident_operand.type),
                                                 Operand(std::to_string(j), Type::IntLiteral),
                                                 Operand(temp_const, ident_type),
                                                 Operator::store));
                j++;
                temp_const = "";
            }
            else
                temp_const += const_list[i];
        }
    }
}

// ConstInitVal -> ConstExp | '{' [ ConstInitVal { ',' ConstInitVal } ] '}'
void frontend::Analyzer::analysisConstInitVal(ConstInitVal *root, vector<ir::Instruction *> &buffer)
{
    if (root->children.size() == 1)
    {
        ANALYSIS(constexp, ConstExp, 0);
        root->v = constexp->v;
        root->t = constexp->t;
    }
    else
    {
        ConstDef *const_def = dynamic_cast<ConstDef *>(root->parent);
        ConstDecl *const_decl = dynamic_cast<ConstDecl *>(const_def->parent);
        string const_list = "";
        for (int i = 1; i < root->children.size() - 1; i += 2)
        {
            ANALYSIS(constinitval_i, ConstInitVal, i);
            string constinitval_v = constinitval_i->v;
            Type constinitval_t = constinitval_i->t;
            const_list = const_list + constinitval_v + ",";
        }
        root->v = const_list;
        root->t = const_decl->t;
    }
}

// VarDecl -> BType VarDef { ',' VarDef } ';'
void frontend::Analyzer::analysisVarDecl(VarDecl *root, vector<ir::Instruction *> &buffer)
{
    // std::cout << "analysisVarDecl begin" << std::endl;
    GET_CHILD_PTR(btype, BType, 0);
    Term *bt = dynamic_cast<Term *>(btype->children[0]);
    root->t = bt->token.type == TokenType::INTTK ? Type::Int : Type::Float;
    ANALYSIS(vardef, VarDef, 1);
    for (int i = 2; i < root->children.size(); i += 2)
    {
        GET_CHILD_PTR(term, Term, i);
        if (term->token.type == TokenType::SEMICN)
            break;
        ANALYSIS(vardef, VarDef, i + 1);
    }
    // std::cout << "analysisVarDecl end" << std::endl;
}

// VarDef -> Ident { '[' ConstExp ']' } [ '=' InitVal ]
void frontend::Analyzer::analysisVarDef(VarDef *root, vector<ir::Instruction *> &buffer)
{
    // std::cout << "VarDef begin" << std::endl;
    VarDecl *parent = dynamic_cast<VarDecl *>(root->parent);
    Type ident_type = parent->t;
    GET_CHILD_PTR(ident, Term, 0);
    string Ident_name = ident->token.value;
    STE ste;
    Operand ident_operand = Operand(Ident_name, ident_type);
    if (root->children.size() > 1)
    {
        // std::cout << "VarDef has more than one child begin" << std::endl;
        GET_CHILD_PTR(term_0, Term, 1);
        if (term_0->token.type == TokenType::LBRACK)
        {
            if (ident_operand.type == Type::Int)
                ident_operand.type = Type::IntPtr;
            else if (ident_operand.type == Type::Float)
                ident_operand.type = Type::FloatPtr;
            ANALYSIS(constexp_1, ConstExp, 2);
            ste.dimension.push_back(std::stoi(constexp_1->v));
            if (root->children.size() > 4)
            {
                GET_CHILD_PTR(term_1, Term, 4);
                if (term_1->token.type == TokenType::LBRACK)
                {
                    ANALYSIS(constexp_2, ConstExp, 5);
                    ste.dimension.push_back(std::stoi(constexp_2->v));
                }
            }
        }
        // std::cout << "VarDef has more than one child end" << std::endl;
    }
    ste.operand = ident_operand;
    symbol_table.scope_stack.back().table[Ident_name] = ste;
    string ident_after_rename = symbol_table.get_scoped_name(Ident_name);

    // int/float a;
    if (root->children.size() == 1)
    {
        if (ident_operand.type == Type::Int)
            buffer.push_back(new Instruction(Operand("0", Type::IntLiteral),
                                             Operand(),
                                             Operand(ident_after_rename, Type::Int),
                                             Operator::def));
        else if (ident_operand.type == Type::Float)
            buffer.push_back(new Instruction(Operand("0.0", Type::FloatLiteral),
                                             Operand(),
                                             Operand(ident_after_rename, Type::Float),
                                             Operator::fdef));
    }
    else
    {
        GET_CHILD_PTR(term_0, Term, 1);
        // int/float a = ...;
        if (term_0->token.type == TokenType::ASSIGN)
        {
            ANALYSIS(initval, InitVal, 2);
            string initval_value = initval->v;
            if (ident_type == Type::Int)
            {
                if (initval->t == Type::IntLiteral)
                    buffer.push_back(new Instruction(Operand(initval_value, Type::IntLiteral),
                                                     Operand(),
                                                     Operand(ident_after_rename, Type::Int),
                                                     Operator::def));
                else if (initval->t == Type::Int)
                    buffer.push_back(new Instruction(Operand(initval_value, Type::Int),
                                                     Operand(),
                                                     Operand(ident_after_rename, Type::Int),
                                                     Operator::def));
                else if (initval->t == Type::FloatLiteral)
                {
                    string temp_float = "temp" + std::to_string(tmp_cnt++);
                    buffer.push_back(new Instruction(Operand(initval_value, Type::FloatLiteral),
                                                     Operand(),
                                                     Operand(temp_float, Type::Float),
                                                     Operator::fdef));
                    buffer.push_back(new Instruction(Operand(temp_float, Type::Float),
                                                     Operand(),
                                                     Operand(ident_after_rename, Type::Int),
                                                     Operator::cvt_f2i));
                }
                else if (initval->t == Type::Float)
                    buffer.push_back(new Instruction(Operand(initval_value, Type::Float),
                                                     Operand(),
                                                     Operand(ident_after_rename, Type::Int),
                                                     Operator::cvt_f2i));
            }
            else if (ident_type == Type::Float)
            {
                if (initval->t == Type::FloatLiteral)
                    buffer.push_back(new Instruction(Operand(initval_value, Type::FloatLiteral),
                                                     Operand(),
                                                     Operand(ident_after_rename, Type::Float),
                                                     Operator::fdef));
                else if (initval->t == Type::Float)
                    buffer.push_back(new Instruction(Operand(initval_value, Type::Float),
                                                     Operand(),
                                                     Operand(ident_after_rename, Type::Float),
                                                     Operator::fdef));
                else if (initval->t == Type::IntLiteral)
                {
                    string temp_int = "temp" + std::to_string(tmp_cnt++);
                    buffer.push_back(new Instruction(Operand(initval_value, Type::IntLiteral),
                                                     Operand(),
                                                     Operand(temp_int, Type::Int),
                                                     Operator::def));
                    buffer.push_back(new Instruction(Operand(temp_int, Type::IntLiteral),
                                                     Operand(),
                                                     Operand(ident_after_rename, Type::Float),
                                                     Operator::cvt_i2f));
                }
                else if (initval->t == Type::Int)
                    buffer.push_back(new Instruction(Operand(initval_value, Type::Int),
                                                     Operand(),
                                                     Operand(ident_after_rename, Type::Float),
                                                     Operator::cvt_i2f));
            }
        }
        else if (term_0->token.type == TokenType::LBRACK)
        {
            int ident_array_dim = ste.dimension.size();
            int allco_size;
            allco_size = ident_array_dim == 1 ? ste.dimension[0] : ste.dimension[0] * ste.dimension[1];
            if (symbol_table.scope_stack.size() > 1)
                buffer.push_back(new Instruction(Operand(std::to_string(allco_size), Type::IntLiteral),
                                                 Operand(),
                                                 Operand(ident_after_rename, ident_operand.type),
                                                 Operator::alloc));
            // int/float a[10];
            // 申请空间后什么都不做

            if (root->children.size() == 6 || root->children.size() == 9)
            {
                ANALYSIS(initval, InitVal, root->children.size() - 1);
                string temp_var_list = initval->v;
                // //int/float a[10]={};
                // inr/float a[10][10]={};
                if (temp_var_list == "")
                {
                    for (int i = 0; i < allco_size; i++)
                        buffer.push_back(new Instruction(Operand(ident_after_rename, ident_operand.type),
                                                         Operand(std::to_string(i), Type::IntLiteral),
                                                         Operand(ident_type == Type::Int ? "0" : "0.0", ident_type == Type::Int ? Type::IntLiteral : Type::FloatLiteral),
                                                         Operator::store));
                }
                // int/float a[3]={1,2,3};
                // int/float a[2][3]={1,2,3,4,5,6};
                else
                {
                    Type var_type = initval->t;
                    string temp_var_name = "";
                    int j = 0;
                    for (int i = 0; i < initval->v.size(); i++)
                    {
                        if (initval->v[i] == ',')
                        {
                            buffer.push_back(new Instruction(Operand(ident_after_rename, ident_operand.type),
                                                             Operand(std::to_string(j), Type::IntLiteral),
                                                             Operand(temp_var_name, ident_type),
                                                             Operator::store));
                            j++;
                            temp_var_name = "";
                        }
                        else
                            temp_var_name += temp_var_list[i];
                    }
                    if (j < allco_size - 1)
                    {
                        for (int i = j; i < allco_size; i++)
                            buffer.push_back(new Instruction(Operand(ident_after_rename, ident_operand.type),
                                                             Operand(std::to_string(i), Type::IntLiteral),
                                                             Operand(ident_type == Type::Int ? "0" : "0.0", ident_type == Type::Int ? Type::IntLiteral : Type::FloatLiteral),
                                                             Operator::store));
                    }
                }
            }
        }
    }
    // std::cout << "vardef end" << std::endl;
}

// ConstExp -> AddExp
void frontend::Analyzer::analysisConstExp(ConstExp *root, vector<ir::Instruction *> &buffer)
{
    // std::cout << "analysisConstExp" << std::endl;
    ANALYSIS(addexp, AddExp, 0);
    COPY_EXP_NODE(addexp, root);
}

// InitVal -> Exp | '{' [ InitVal { ',' InitVal } ] '}'
void frontend::Analyzer::analysisInitVal(InitVal *root, vector<ir::Instruction *> &buffer)
{
    // std::cout << "analysisInitVal " << root->children.size() << std::endl;
    if (root->children.size() == 1)
    {
        ANALYSIS(exp, Exp, 0);
        COPY_EXP_NODE(exp, root);
    }
    else if (root->children.size() == 2)
        root->v = "";
    else
    {
        VarDef *var_def = dynamic_cast<VarDef *>(root->parent);
        VarDecl *var_decl = dynamic_cast<VarDecl *>(var_def->parent);
        string var_name_list = "";
        for (int i = 1; i < root->children.size() - 1; i += 2)
        {
            ANALYSIS(initval_i, InitVal, i);
            string initval_v = initval_i->v;
            Type initval_t = initval_i->t;
            if (var_decl->t == Type::Int)
            {
                if (initval_t == Type::Int)
                    var_name_list = var_name_list + initval_i->v + ",";
                else if (initval_t == Type::Float)
                {
                    string temp_int = "temp" + std::to_string(tmp_cnt++);
                    buffer.push_back(new Instruction(Operand(initval_v, Type::Float),
                                                     Operand(),
                                                     Operand(temp_int, Type::Int),
                                                     Operator::cvt_f2i));
                    var_name_list = var_name_list + temp_int + ",";
                }
                else if (initval_t == Type::IntLiteral)
                {
                    string temp_int = "temp" + std::to_string(tmp_cnt++);
                    buffer.push_back(new Instruction(Operand(initval_v, Type::IntLiteral),
                                                     Operand(),
                                                     Operand(temp_int, Type::Int),
                                                     Operator::def));
                    var_name_list = var_name_list + temp_int + ",";
                }
                else if (initval_t == Type::FloatLiteral)
                {
                    string temp_int = "temp" + std::to_string(tmp_cnt++);
                    buffer.push_back(new Instruction(Operand(std::to_string(int(std::stof(initval_v))), Type::IntLiteral),
                                                     Operand(),
                                                     Operand(temp_int, Type::Int),
                                                     Operator::def));
                    var_name_list = var_name_list + temp_int + ",";
                }
            }
            else if (var_decl->t == Type::Float)
            {
                if (initval_t == Type::Float)
                    var_name_list = var_name_list + initval_i->v + ",";
                else if (initval_t == Type::Int)
                {
                    string temp_float = "temp" + std::to_string(tmp_cnt++);
                    buffer.push_back(new Instruction(Operand(initval_v, Type::Int),
                                                     Operand(),
                                                     Operand(temp_float, Type::Float),
                                                     Operator::cvt_i2f));
                    var_name_list = var_name_list + temp_float + ",";
                }
                else if (initval_t == Type::IntLiteral)
                {
                    string temp_float = "temp" + std::to_string(tmp_cnt++);
                    buffer.push_back(new Instruction(Operand(initval_v, Type::IntLiteral),
                                                     Operand(),
                                                     Operand(temp_float, Type::Float),
                                                     Operator::fdef));
                    var_name_list = var_name_list + temp_float + ",";
                }
                else if (initval_t == Type::FloatLiteral)
                {
                    string temp_float = "temp" + std::to_string(tmp_cnt++);
                    buffer.push_back(new Instruction(Operand(std::to_string(float(std::stoi(initval_v))), Type::FloatLiteral),
                                                     Operand(),
                                                     Operand(temp_float, Type::Float),
                                                     Operator::fdef));
                    var_name_list = var_name_list + temp_float + ",";
                }
            }
        }
        root->v = var_name_list;
        root->t = var_decl->t;
    }
}

// Exp -> AddExp
void frontend::Analyzer::analysisExp(Exp *root, vector<ir::Instruction *> &buffer)
{
    // std::cout << "analysisExp" << std::endl;
    ANALYSIS(addexp, AddExp, 0);
    COPY_EXP_NODE(addexp, root);
}

// AddExp -> MulExp { ('+' | '-') MulExp }
void frontend::Analyzer::analysisAddExp(AddExp *root, vector<ir::Instruction *> &buffer)
{
    // std::cout << "analysisAddExp begin" << std::endl;
    ANALYSIS(mulexp_0, MulExp, 0);
    COPY_EXP_NODE(mulexp_0, root);
    for (int i = 1; i < root->children.size(); i = i + 2)
    {
        GET_CHILD_PTR(term, Term, i);
        ANALYSIS(mulexp_i, MulExp, i + 1);
        string op = term->token.value;
        string temp_result = "temp" + std::to_string(tmp_cnt++);
        if (mulexp_i->t == Type::Float && root->t == Type::IntLiteral)
        {
            buffer.push_back(new Instruction(Operand(std::to_string(float(std::stoi(root->v))), Type::FloatLiteral),
                                             Operand(),
                                             Operand(temp_result, Type::Float),
                                             Operator::fdef));
            buffer.push_back(new Instruction(Operand(temp_result, Type::Float),
                                             Operand(mulexp_i->v, Type::Float),
                                             Operand(temp_result, Type::Float),
                                             op == "+" ? Operator::fadd : Operator::fsub));
            root->v = temp_result;
            root->t = Type::Float;
        }
        else if (mulexp_i->t == Type::Float && root->t == Type::FloatLiteral)
        {
            buffer.push_back(new Instruction(Operand(root->v, Type::FloatLiteral),
                                             Operand(),
                                             Operand(temp_result, Type::Float),
                                             Operator::fdef));
            buffer.push_back(new Instruction(Operand(temp_result, Type::Float),
                                             Operand(mulexp_i->v, Type::Float),
                                             Operand(temp_result, Type::Float),
                                             op == "+" ? Operator::fadd : Operator::fsub));
            root->v = temp_result;
            root->t = Type::Float;
        }
        else if (mulexp_i->t == Type::Float && root->t == Type::Int)
        {
            buffer.push_back(new Instruction(Operand(root->v, Type::Int),
                                             Operand(),
                                             Operand(temp_result, Type::Float),
                                             Operator::cvt_i2f));
            buffer.push_back(new Instruction(Operand(temp_result, Type::Float),
                                             Operand(mulexp_i->v, Type::Float),
                                             Operand(temp_result, Type::Float),
                                             op == "+" ? Operator::fadd : Operator::fsub));
            root->v = temp_result;
            root->t = Type::Float;
        }
        else if (mulexp_i->t == Type::Float && root->t == Type::Float)
        {
            buffer.push_back(new Instruction(Operand(root->v, Type::Float),
                                             Operand(mulexp_i->v, Type::Float),
                                             Operand(temp_result, Type::Float),
                                             op == "+" ? Operator::fadd : Operator::fsub));
            root->v = temp_result;
        }
        else if (mulexp_i->t == Type::FloatLiteral && root->t == Type::IntLiteral)
        {
            if (op == "+")
                root->v = std::to_string(std::stoi(root->v) + std::stof(mulexp_i->v));
            else if (op == "-")
                root->v = std::to_string(std::stoi(root->v) - std::stof(mulexp_i->v));
            root->t = Type::FloatLiteral;
        }
        else if (mulexp_i->t == Type::FloatLiteral && root->t == Type::FloatLiteral)
        {
            if (op == "+")
                root->v = std::to_string(std::stof(root->v) + std::stof(mulexp_i->v));
            else if (op == "-")
                root->v = std::to_string(std::stof(root->v) - std::stof(mulexp_i->v));
        }
        else if (mulexp_i->t == Type::FloatLiteral && root->t == Type::Int)
        {
            string temp_float = "temp" + std::to_string(tmp_cnt++);
            buffer.push_back(new Instruction(Operand(root->v, Type::Int),
                                             Operand(),
                                             Operand(temp_result, Type::Float),
                                             Operator::cvt_i2f));
            buffer.push_back(new Instruction(Operand(mulexp_i->v, Type::FloatLiteral),
                                             Operand(),
                                             Operand(temp_float, Type::Float),
                                             Operator::fdef));
            buffer.push_back(new Instruction(Operand(temp_result, Type::Float),
                                             Operand(temp_float, Type::Float),
                                             Operand(temp_result, Type::Float),
                                             op == "+" ? Operator::fadd : Operator::fsub));
            root->v = temp_result;
            root->t = Type::Float;
        }
        else if (mulexp_i->t == Type::FloatLiteral && root->t == Type::Float)
        {
            string temp_float = "temp" + std::to_string(tmp_cnt++);
            buffer.push_back(new Instruction(Operand(mulexp_i->v, Type::FloatLiteral),
                                             Operand(),
                                             Operand(temp_float, Type::Float),
                                             Operator::fdef));
            buffer.push_back(new Instruction(Operand(root->v, Type::Float),
                                             Operand(temp_float, Type::Float),
                                             Operand(temp_result, Type::Float),
                                             op == "+" ? Operator::fadd : Operator::fsub));
            root->v = temp_result;
        }
        else if (mulexp_i->t == Type::IntLiteral && root->t == Type::IntLiteral)
        {
            if (op == "+")
                root->v = std::to_string(std::stoi(root->v) + std::stoi(mulexp_i->v));
            else if (op == "-")
                root->v = std::to_string(std::stoi(root->v) - std::stoi(mulexp_i->v));
        }
        else if (mulexp_i->t == Type::IntLiteral && root->t == Type::FloatLiteral)
        {
            if (op == "+")
                root->v = std::to_string(std::stoi(root->v) + std::stof(mulexp_i->v));
            else if (op == "-")
                root->v = std::to_string(std::stoi(root->v) - std::stof(mulexp_i->v));
            root->t = Type::FloatLiteral;
        }
        else if (mulexp_i->t == Type::IntLiteral && root->t == Type::Int)
        {
            string temp_int = "temp" + std::to_string(tmp_cnt++);
            buffer.push_back(new Instruction(Operand(mulexp_i->v, Type::IntLiteral),
                                             Operand(),
                                             Operand(temp_int, Type::Int),
                                             Operator::def));
            buffer.push_back(new Instruction(Operand(root->v, Type::Int),
                                             Operand(temp_int, Type::Int),
                                             Operand(temp_result, Type::Int),
                                             op == "+" ? Operator::add : Operator::sub));
            root->v = temp_result;
        }
        else if (mulexp_i->t == Type::IntLiteral && root->t == Type::Float)
        {
            string temp_float = "temp" + std::to_string(tmp_cnt++);
            buffer.push_back(new Instruction(Operand(std::to_string(float(std::stof(mulexp_i->v))), Type::FloatLiteral),
                                             Operand(),
                                             Operand(temp_float, Type::Float),
                                             Operator::fdef));
            buffer.push_back(new Instruction(Operand(root->v, Type::Float),
                                             Operand(temp_float, Type::Float),
                                             Operand(temp_result, Type::Float),
                                             op == "+" ? Operator::fadd : Operator::fsub));
            root->v = temp_result;
       }
        else if (mulexp_i->t == Type::Int && root->t == Type::FloatLiteral)
        {
            string temp_float = "temp" + std::to_string(tmp_cnt++);
            buffer.push_back(new Instruction(Operand(root->v, Type::FloatLiteral),
                                             Operand(),
                                             Operand(temp_result, Type::Float),
                                             Operator::fdef));
            buffer.push_back(new Instruction(Operand(mulexp_i->v, Type::Int),
                                             Operand(),
                                             Operand(temp_float, Type::Float),
                                             Operator::cvt_i2f));
            buffer.push_back(new Instruction(Operand(temp_result, Type::Float),
                                             Operand(temp_float, Type::Float),
                                             Operand(temp_result, Type::Float),
                                             op == "+" ? Operator::fadd : Operator::fsub));
            root->v = temp_result;
            root->t = Type::Float;
        }
        else if (mulexp_i->t == Type::Int && root->t == Type::IntLiteral)
        {
            buffer.push_back(new Instruction(Operand(root->v, Type::IntLiteral),
                                             Operand(),
                                             Operand(temp_result, Type::Int),
                                             Operator::def));
            buffer.push_back(new Instruction(Operand(temp_result, Type::Int),
                                             Operand(mulexp_i->v, Type::Int),
                                             Operand(temp_result, Type::Int),
                                             op == "+" ? Operator::add : Operator::sub));
            root->v = temp_result;
            root->t = Type::Int;
        }
        else if (mulexp_i->t == Type::Int && root->t == Type::Int)
        {
            buffer.push_back(new Instruction(Operand(root->v, Type::Int),
                                             Operand(mulexp_i->v, Type::Int),
                                             Operand(temp_result, Type::Int),
                                             op == "+" ? Operator::add : Operator::sub));
            root->v = temp_result;
        }
        else if (mulexp_i->t == Type::Int && root->t == Type::Float)
        {
            string temp_Float = "temp" + std::to_string(tmp_cnt++);
            buffer.push_back(new Instruction(Operand(mulexp_i->v, Type::Int),
                                             Operand(),
                                             Operand(temp_Float, Type::Float),
                                             Operator::cvt_i2f));
            buffer.push_back(new Instruction(Operand(root->v, Type::Float),
                                             Operand(temp_Float, Type::Float),
                                             Operand(temp_result, Type::Float),
                                             op == "+" ? Operator::fadd : Operator::fsub));
            root->v = temp_result;
        }
    }
    // std::cout << "analysisaddExp end" << std::endl;
}

// MulExp -> UnaryExp { ('*' | '/' | '%') UnaryExp }
void frontend::Analyzer::analysisMulExp(MulExp *root, vector<ir::Instruction *> &buffer)
{
    // std::cout << "MulExp" << std::endl;
    ANALYSIS(unaryexp_0, UnaryExp, 0);
    COPY_EXP_NODE(unaryexp_0, root);
    for (int i = 1; i < root->children.size(); i = i + 2)
    {
        GET_CHILD_PTR(term, Term, i);
        ANALYSIS(unaryexp_i, UnaryExp, i + 1);
        string op = term->token.value;
        string temp_result = "temp" + std::to_string(tmp_cnt++);
        if (unaryexp_i->t == Type::Float && root->t == Type::IntLiteral)
        {
            buffer.push_back(new Instruction(Operand(std::to_string(float(std::stoi(root->v))), Type::FloatLiteral),
                                             Operand(),
                                             Operand(temp_result, Type::Float),
                                             Operator::fdef));
            buffer.push_back(new Instruction(Operand(temp_result, Type::Float),
                                             Operand(unaryexp_i->v, Type::Float),
                                             Operand(temp_result, Type::Float),
                                             op == "*" ? Operator::fmul : Operator::fdiv));
            root->v = temp_result;
            root->t = Type::Float;
        }
        else if (unaryexp_i->t == Type::Float && root->t == Type::FloatLiteral)
        {
            buffer.push_back(new Instruction(Operand(root->v, Type::FloatLiteral),
                                             Operand(),
                                             Operand(temp_result, Type::Float),
                                             Operator::fdef));
            buffer.push_back(new Instruction(Operand(root->v, Type::Float),
                                             Operand(unaryexp_i->v, Type::Float),
                                             Operand(temp_result, Type::Float),
                                             op == "*" ? Operator::fmul : Operator::fdiv));
            root->v = temp_result;
            root->t = Type::Float;
        }
        else if (unaryexp_i->t == Type::Float && root->t == Type::Int)
        {
            buffer.push_back(new Instruction(Operand(root->v, Type::Int),
                                             Operand(),
                                             Operand(temp_result, Type::Float),
                                             Operator::cvt_i2f));
            buffer.push_back(new Instruction(Operand(temp_result, Type::Float),
                                             Operand(unaryexp_i->v, Type::Float),
                                             Operand(temp_result, Type::Float),
                                             op == "*" ? Operator::fmul : Operator::fdiv));
            root->v = temp_result;
            root->t = Type::Float;
        }
        else if (unaryexp_i->t == Type::Float && root->t == Type::Float)
        {
            buffer.push_back(new Instruction(Operand(root->v, Type::Float),
                                             Operand(unaryexp_i->v, Type::Float),
                                             Operand(temp_result, Type::Float),
                                             op == "*" ? Operator::fmul : Operator::fdiv));
            root->v = temp_result;
        }
        else if (unaryexp_i->t == Type::FloatLiteral && root->t == Type::IntLiteral)
        {
            if (op == "*")
                root->v = std::to_string(std::stoi(root->v) * std::stof(unaryexp_i->v));
            else if (op == "/")
                root->v = std::to_string(std::stoi(root->v) / std::stof(unaryexp_i->v));
            root->t = Type::FloatLiteral;
        }
        else if (unaryexp_i->t == Type::FloatLiteral && root->t == Type::FloatLiteral)
        {
            if (op == "*")
                root->v = std::to_string(std::stof(root->v) * std::stof(unaryexp_i->v));
            else if (op == "/")
                root->v = std::to_string(std::stof(root->v) / std::stof(unaryexp_i->v));
        }
        else if (unaryexp_i->t == Type::FloatLiteral && root->t == Type::Int)
        {
            string temp_float = "temp" + std::to_string(tmp_cnt++);
            buffer.push_back(new Instruction(Operand(root->v, Type::Int),
                                             Operand(),
                                             Operand(temp_result, Type::Float),
                                             Operator::cvt_i2f));
            buffer.push_back(new Instruction(Operand(unaryexp_i->v, Type::FloatLiteral),
                                             Operand(),
                                             Operand(temp_float, Type::Float),
                                             Operator::fdef));
            buffer.push_back(new Instruction(Operand(temp_result, Type::Float),
                                             Operand(temp_float, Type::Float),
                                             Operand(temp_result, Type::Float),
                                             op == "*" ? Operator::fmul : Operator::fdiv));
            root->v = temp_result;
            root->t = Type::Float;
        }
        else if (unaryexp_i->t == Type::FloatLiteral && root->t == Type::Float)
        {
            string temp_float = "temp" + std::to_string(tmp_cnt++);
            buffer.push_back(new Instruction(Operand(unaryexp_i->v, Type::FloatLiteral),
                                             Operand(),
                                             Operand(temp_float, Type::Float),
                                             Operator::fdef));
            buffer.push_back(new Instruction(Operand(root->v, Type::Float),
                                             Operand(temp_float, Type::Float),
                                             Operand(temp_result, Type::Float),
                                             op == "*" ? Operator::fmul : Operator::fdiv));
            root->v = temp_result;
        }
        else if (unaryexp_i->t == Type::IntLiteral && root->t == Type::IntLiteral)
        {
            if (op == "*")
                root->v = std::to_string(std::stoi(root->v) * std::stoi(unaryexp_i->v));
            else if (op == "/")
                root->v = std::to_string(std::stoi(root->v) / std::stoi(unaryexp_i->v));
            else if (op == "%")
                root->v = std::to_string(std::stoi(root->v) % std::stoi(unaryexp_i->v));
        }
        else if (unaryexp_i->t == Type::IntLiteral && root->t == Type::FloatLiteral)
        {
            if (op == "*")
                root->v = std::to_string(std::stoi(root->v) * std::stof(unaryexp_i->v));
            else if (op == "/")
                root->v = std::to_string(std::stoi(root->v) / std::stof(unaryexp_i->v));
            root->t = Type::FloatLiteral;
        }
        else if (unaryexp_i->t == Type::IntLiteral && root->t == Type::Int)
        {
            string temp_int = "temp" + std::to_string(tmp_cnt++);
            buffer.push_back(new Instruction(Operand(unaryexp_i->v, Type::IntLiteral),
                                             Operand(),
                                             Operand(temp_int, Type::Int),
                                             Operator::def));
            buffer.push_back(new Instruction(Operand(root->v, Type::Int),
                                             Operand(temp_int, Type::Int),
                                             Operand(temp_result, Type::Int),
                                             op == "*" ? Operator::mul : op == "/" ? Operator::div
                                                                                   : Operator::mod));
            root->v = temp_result;
        }
        else if (unaryexp_i->t == Type::IntLiteral && root->t == Type::Float)
        {
            string temp_float = "temp" + std::to_string(tmp_cnt++);
            buffer.push_back(new Instruction(Operand(std::to_string(float(std::stof(unaryexp_i->v))), Type::FloatLiteral),
                                             Operand(),
                                             Operand(temp_float, Type::Float),
                                             Operator::fdef));
            buffer.push_back(new Instruction(Operand(root->v, Type::Float),
                                             Operand(temp_float, Type::Float),
                                             Operand(temp_result, Type::Float),
                                             op == "*" ? Operator::fmul : Operator::fdiv));
            root->v = temp_result;
        }
        else if (unaryexp_i->t == Type::Int && root->t == Type::FloatLiteral)
        {
            string temp_float = "temp" + std::to_string(tmp_cnt++);
            buffer.push_back(new Instruction(Operand(root->v, Type::FloatLiteral),
                                             Operand(),
                                             Operand(temp_result, Type::Float),
                                             Operator::fdef));
            buffer.push_back(new Instruction(Operand(unaryexp_i->v, Type::Int),
                                             Operand(),
                                             Operand(temp_float, Type::Float),
                                             Operator::cvt_i2f));
            buffer.push_back(new Instruction(Operand(temp_result, Type::Float),
                                             Operand(temp_float, Type::Float),
                                             Operand(temp_result, Type::Float),
                                             op == "*" ? Operator::fmul : Operator::fdiv));
            root->v = temp_result;
            root->t = Type::Float;
        }
        else if (unaryexp_i->t == Type::Int && root->t == Type::IntLiteral)
        {
            buffer.push_back(new Instruction(Operand(root->v, Type::IntLiteral),
                                             Operand(),
                                             Operand(temp_result, Type::Int),
                                             Operator::def));
            buffer.push_back(new Instruction(Operand(temp_result, Type::Int),
                                             Operand(unaryexp_i->v, Type::Int),
                                             Operand(temp_result, Type::Int),
                                             op == "*" ? Operator::mul : op == "/" ? Operator::div
                                                                                   : Operator::mod));
            root->v = temp_result;
            root->t = Type::Int;
        }
        else if (unaryexp_i->t == Type::Int && root->t == Type::Int)
        {
            buffer.push_back(new Instruction(Operand(root->v, Type::Int),
                                             Operand(unaryexp_i->v, Type::Int),
                                             Operand(temp_result, Type::Int),
                                             op == "*" ? Operator::mul : op == "/" ? Operator::div
                                                                                   : Operator::mod));
            root->v = temp_result;
        }
        else if (unaryexp_i->t == Type::Int && root->t == Type::Float)
        {
            string temp_Float = "temp" + std::to_string(tmp_cnt++);
            buffer.push_back(new Instruction(Operand(unaryexp_i->v, Type::Int),
                                             Operand(),
                                             Operand(temp_Float, Type::Float),
                                             Operator::cvt_i2f));
            buffer.push_back(new Instruction(Operand(root->v, Type::Float),
                                             Operand(temp_Float, Type::Float),
                                             Operand(temp_result, Type::Float),
                                             op == "*" ? Operator::fmul : Operator::fdiv));
            root->v = temp_result;
        }
    }
    // std::cout << "mulexp end" << std::endl;
}

// UnaryExp -> PrimaryExp | Ident '(' [FuncRParams] ')' | UnaryOp UnaryExp
void frontend::Analyzer::analysisUnaryExp(UnaryExp *root, vector<ir::Instruction *> &buffer)
{
    // std::cout << "unaryexp" << std::endl;
    if (root->children[0]->type == NodeType::PRIMARYEXP)
    {
        ANALYSIS(primaryexp, PrimaryExp, 0);
        COPY_EXP_NODE(primaryexp, root);
    }
    else if (root->children[0]->type == NodeType::UNARYOP)
    {
        ANALYSIS(unaryexp, UnaryExp, 1);
        UnaryOp *unaryop = dynamic_cast<UnaryOp *>(root->children[0]);
        Term *term = dynamic_cast<Term *>(unaryop->children[0]);
        //+a
        if (term->token.type == TokenType::PLUS)
        {
            COPY_EXP_NODE(unaryexp, root);
        }
        //-a
        else if (term->token.type == TokenType::MINU)
        {
            if (unaryexp->t == Type::IntLiteral || unaryexp->t == Type::FloatLiteral)
            {
                root->v = "-" + unaryexp->v;
                root->t = unaryexp->t;
            }
            else if (unaryexp->t == Type::Int)
            {
                string temp_0 = "temp" + std::to_string(tmp_cnt++);
                buffer.push_back(new Instruction(Operand("0", Type::IntLiteral),
                                                 Operand(),
                                                 Operand(temp_0, Type::Int),
                                                 Operator::def));
                string temp_neg_result = "temp" + std::to_string(tmp_cnt++);
                buffer.push_back(new Instruction(Operand(temp_0, Type::Int),
                                                 Operand(unaryexp->v, Type::Int),
                                                 Operand(temp_neg_result, Type::Int),
                                                 Operator::sub));
                root->v = temp_neg_result;
                root->t = Type::Int;
            }
            else if (unaryexp->t == Type::Float)
            {
                string temp_0 = "temp" + std::to_string(tmp_cnt++);
                buffer.push_back(new Instruction(Operand("0.0", Type::FloatLiteral),
                                                 Operand(),
                                                 Operand(temp_0, Type::Float),
                                                 Operator::fdef));
                string temp_neg_result = "temp" + std::to_string(tmp_cnt++);
                buffer.push_back(new Instruction(Operand(temp_0, Type::Float),
                                                 Operand(unaryexp->v, Type::Float),
                                                 Operand(temp_neg_result, Type::Float),
                                                 Operator::fsub));
                root->v = temp_neg_result;
                root->t = Type::Float;
            }
        }
        //! a
        else if (term->token.type == TokenType::NOT)
        {
            if (unaryexp->t == Type::IntLiteral)
            {
                root->v = unaryexp->v = std::to_string(!std::stoi(unaryexp->v));
                root->t = Type::IntLiteral;
            }
            else if (unaryexp->t == Type::FloatLiteral)
            {
                root->v = unaryexp->v = std::to_string(!std::stof(unaryexp->v));
                root->t = Type::FloatLiteral;
            }
            else if (unaryexp->t == Type::Int)
            {
                string temp_not_result = "temp" + std::to_string(tmp_cnt++);
                buffer.push_back(new Instruction(Operand(unaryexp->v, Type::Int),
                                                 Operand(),
                                                 Operand(temp_not_result, Type::Int),
                                                 Operator::_not));
                root->v = temp_not_result;
                root->t = Type::Int;
            }
            else if (unaryexp->t == Type::Float)
            {
                string temp_not_result = "temp" + std::to_string(tmp_cnt++);
                buffer.push_back(new Instruction(Operand(unaryexp->v, Type::Float),
                                                 Operand(),
                                                 Operand(temp_not_result, Type::Float),
                                                 Operator::_not));
                root->v = temp_not_result;
                root->t = Type::Float;
            }
        }
    }
    else
    {
        // std::cout << "call_func begin" << std::endl;
        GET_CHILD_PTR(ident, Term, 0);
        string func_name = ident->token.value;
        Function *call_func = symbol_table.functions[func_name];
        string temp_func_result = "temp" + std::to_string(tmp_cnt++);
        vector<ir::Operand> func_rparams;
        if (root->children.size() == 4)
        {
            GET_CHILD_PTR(funcrparams, FuncRParams, 2);
            analysisFuncRParams(funcrparams, func_rparams, buffer);
        }
        // std::cout << "call_func->name: " << call_func->name << std::endl;
        buffer.push_back(new ir::CallInst(Operand(call_func->name, call_func->returnType),
                                          func_rparams,
                                          Operand(temp_func_result, call_func->returnType)));
        // std::cout << "call_func end" << std::endl;
        root->v = temp_func_result;
        root->t = call_func->returnType;
    }
}

// UnaryOp -> '+' | '-' | '!'
void frontend::Analyzer::analysisUnaryOp(UnaryOp *root, vector<ir::Instruction *> &buffer)
{
    return;
}

// PrimaryExp -> '(' Exp ')' | LVal | Number
void frontend::Analyzer::analysisPrimaryExp(PrimaryExp *root, vector<ir::Instruction *> &buffer)
{
    // std::cout << "analysisPrimaryExp" << std::endl;
    if (root->children[0]->type == NodeType::LVAL)
    {
        ANALYSIS(lval, LVal, 0);
        COPY_EXP_NODE(lval, root);
    }
    else if (root->children[0]->type == NodeType::NUMBER)
    {
        ANALYSIS(number, Number, 0);
        COPY_EXP_NODE(number, root);
    }
    else
    {
        ANALYSIS(exp, Exp, 1);
        COPY_EXP_NODE(exp, root);
    }
}

// LVal -> Ident {'[' Exp ']'}
void frontend::Analyzer::analysisLVal(LVal *root, vector<ir::Instruction *> &buffer)
{
    // std::cout << "analysisLVal" << std::endl;
    GET_CHILD_PTR(term, Term, 0);
    std::string var_name = term->token.value;
    STE oprand_ste = symbol_table.get_ste(var_name);
    string var_name_after_rename = symbol_table.get_scoped_name(var_name);
    // a
    if (root->children.size() == 1)
    {
        root->v = (oprand_ste.operand.type == Type::IntLiteral || oprand_ste.operand.type == Type::FloatLiteral) ? oprand_ste.literal_value
                                                                                                                 : var_name_after_rename;
        root->t = oprand_ste.operand.type;
    }
    else if (root->children.size() == 4)
    {
        ANALYSIS(exp, Exp, 2);
        string temp_var_name = "temp" + std::to_string(tmp_cnt++);
        // a[1]
        if (oprand_ste.dimension.size() == 1)
        {
            Type temp_type = oprand_ste.operand.type == Type::IntPtr ? Type::Int : Type::Float;
            buffer.push_back(new Instruction(Operand(var_name_after_rename, oprand_ste.operand.type),
                                             Operand(exp->v, exp->t),
                                             Operand(temp_var_name, temp_type),
                                             Operator::load));
            root->v = temp_var_name;
            root->t = temp_type;
        }
        // a[][3]
        else
        {
            Type temp_type = oprand_ste.operand.type;
            // a[1]
            if (exp->t == Type::IntLiteral)
            {
                string offset = std::to_string(std::stoi(exp->v) * oprand_ste.dimension[1]);
                buffer.push_back(new Instruction(Operand(var_name_after_rename, oprand_ste.operand.type),
                                                 Operand(offset, Type::IntLiteral),
                                                 Operand(temp_var_name, temp_type),
                                                 Operator::getptr));
            }
            // a[1+2]
            else
            {
                string temp_offset = "temp" + std::to_string(tmp_cnt++);
                string temp_mul_name = "temp" + std::to_string(tmp_cnt++);
                buffer.push_back(new Instruction(Operand(std::to_string(oprand_ste.dimension[1]), Type::IntLiteral),
                                                 Operand(),
                                                 Operand(temp_mul_name, Type::Int),
                                                 Operator::def));
                buffer.push_back(new Instruction(Operand(exp->v, exp->t),
                                                 Operand(temp_var_name, Type::Int),
                                                 Operand(temp_offset, Type::Int),
                                                 Operator::mul));
                buffer.push_back(new Instruction(Operand(var_name_after_rename, oprand_ste.operand.type),
                                                 Operand(temp_offset, Type::Int),
                                                 Operand(temp_var_name, temp_type),
                                                 Operator::getptr));
            }
            root->v = temp_var_name;
            root->t = temp_type;
        }
    }
    // a[1][2]
    else if (root->children.size() == 7)
    {
        ANALYSIS(exp_0, Exp, 2);
        ANALYSIS(exp_1, Exp, 5);
        string temp_dim_0 = "temp" + std::to_string(tmp_cnt++);
        string temp_dim_1 = "temp" + std::to_string(tmp_cnt++);
        buffer.push_back(new Instruction(Operand(exp_0->v, exp_0->t),
                                         Operand(),
                                         Operand(temp_dim_0, Type::Int),
                                         Operator::def));
        buffer.push_back(new Instruction(Operand(exp_1->v, exp_1->t),
                                         Operand(),
                                         Operand(temp_dim_1, Type::Int),
                                         Operator::def));
        string temp_col = "temp" + std::to_string(tmp_cnt++);
        buffer.push_back(new Instruction(Operand(std::to_string(oprand_ste.dimension[1]), Type::IntLiteral),
                                         Operand(),
                                         Operand(temp_col, Type::Int),
                                         Operator::def));
        string temp_line_offset = "temp" + std::to_string(tmp_cnt++);
        buffer.push_back(new Instruction(Operand(temp_dim_0, Type::Int),
                                         Operand(temp_col, Type::Int),
                                         Operand(temp_line_offset, Type::Int),
                                         Operator::mul));
        string temp_offset = "temp" + std::to_string(tmp_cnt++);
        buffer.push_back(new Instruction(Operand(temp_line_offset, Type::Int),
                                         Operand(temp_dim_1, Type::Int),
                                         Operand(temp_offset, Type::Int),
                                         Operator::add));
        string temp_var_name = "temp" + std::to_string(tmp_cnt++);
        Type temp_type = oprand_ste.operand.type == Type::IntPtr ? Type::Int : Type::Float;
        buffer.push_back(new Instruction(Operand(var_name_after_rename, oprand_ste.operand.type),
                                         Operand(temp_offset, Type::Int),
                                         Operand(temp_var_name, temp_type),
                                         Operator::load));
        root->v = temp_var_name;
        root->t = temp_type;
    }
}

// Number -> IntConst | floatConst
void frontend::Analyzer::analysisNumber(Number *root, vector<ir::Instruction *> &buffer)
{
    // std::cout << "analysisNumber" << std::endl;
    GET_CHILD_PTR(term, Term, 0);
    if (term->token.type == TokenType::INTLTR)
    {
        root->t = Type::IntLiteral;
        std::string str = dynamic_cast<Term *>(root->children[0])->token.value;
        if (str[0] == '0' && (str[1] == 'x' || str[1] == 'X'))
            root->v = std::to_string(std::stoi(str, nullptr, 16));
        else if (str[0] == '0' && (str[1] == 'b' || str[1] == 'B'))
            root->v = std::to_string(std::stoi(str, nullptr, 2));
        else if (str[0] == '0')
            root->v = std::to_string(std::stoi(str, nullptr, 8));
        else
            root->v = str;
    }
    else if (term->token.type == TokenType::FLOATLTR)
    {
        root->t = Type::FloatLiteral;
        root->v = term->token.value;
    }
    else
    {
        assert(0 && "analysisNumber error");
    }
}

// FuncDef -> FuncType Ident '(' [FuncFParams] ')' Block
void frontend::Analyzer::analysisFuncDef(FuncDef *root, ir::Function &func)
{
    // std::cout << "analysisFuncDef begin" << std::endl;
    FuncType *func_type = dynamic_cast<FuncType *>(root->children[0]);
    GET_CHILD_PTR(ident, Term, 1);
    func.returnType = dynamic_cast<Term *>(func_type->children[0])->token.type == TokenType::VOIDTK ? Type::null : (dynamic_cast<Term *>(func_type->children[0])->token.type == TokenType::INTTK ? Type::Int : Type::Float);
    func.name = ident->token.value;
    // std::cout << "func name: " << func.name << std::endl;
    symbol_table.add_scope();
    if (root->children.size() == 6)
    {
        GET_CHILD_PTR(func_fparams, FuncFParams, 3);
        analysisFuncFParams(func_fparams, func.ParameterList, func.InstVec);
    }
    if (func.name == "main")
    {
        // std::cout << "main def" << std::endl;
        string temp_func_name = "temp" + std::to_string(tmp_cnt++);
        func.addInst(new ir::CallInst(Operand("global", Type::null),
                                      Operand(temp_func_name, Type::null)));
    }
    symbol_table.functions[func.name] = &func;
    GET_CHILD_PTR(block, Block, root->children.size() - 1);
    analysisBlock(block, func.InstVec);
    symbol_table.exit_scope();
    // if (func.name == "main")
    //     func.addInst(new Instruction(Operand(),
    //                                  Operand(),
    //                                  Operand(),
    //                                  Operator::_return));
    // std::cout << "analysisFuncDef end" << std::endl;
}

// FuncType -> 'void' | 'int' | 'float'
void frontend::Analyzer::analysisFuncType(FuncType *root)
{
    return;
}

// FuncFParam -> BType Ident ['[' ']' { '[' Exp ']' }]
void frontend::Analyzer::analysisFuncFParam(FuncFParam *root, Operand &oprand_ste, vector<ir::Instruction *> &buffer)
{
    GET_CHILD_PTR(btype, BType, 0);
    Term *bt = dynamic_cast<Term *>(btype->children[0]);
    GET_CHILD_PTR(ident, Term, 1);
    Type type = bt->token.type == TokenType::INTTK ? Type::Int : Type::Float;
    vector<int> dimension(1, -1);
    if (root->children.size() > 2)
    {
        type = type == Type::Int ? Type::IntPtr : Type::FloatPtr;
        if (root->children.size() == 7)
        {
            type = type == Type::Int ? Type::IntPtr : Type::FloatPtr;
            ANALYSIS(exp, Exp, 5);
            dimension.push_back(std::stoi(exp->v));
        }
    }
    string var_name = ident->token.value;
    Operand operand = Operand(var_name, type);
    oprand_ste = operand;
    symbol_table.scope_stack.back().table[var_name] = {operand, dimension};
    string var_after_rename = symbol_table.get_scoped_name(var_name);
    oprand_ste.name = var_after_rename;
}

// FuncFParams -> FuncFParam { ',' FuncFParam }
void frontend::Analyzer::analysisFuncFParams(FuncFParams *root, vector<Operand> &operand_list, vector<ir::Instruction *> &buffer)
{
    for (int i = 0; i < root->children.size(); i = i + 2)
    {
        Operand op_funcfparm;
        analysisFuncFParam(dynamic_cast<FuncFParam *>(root->children[i]), op_funcfparm, buffer);
        operand_list.push_back(op_funcfparm);
    }
}

// FuncRParams -> Exp { ',' Exp }
void frontend::Analyzer::analysisFuncRParams(FuncRParams *root, vector<Operand> &operand_list, vector<ir::Instruction *> &buffer)
{
    for (int i = 0; i < root->children.size(); i = i + 2)
    {
        ANALYSIS(exp, Exp, i);
        operand_list.push_back(Operand(exp->v, exp->t));
    }
}

// Block -> '{' { BlockItem } '}'
void frontend::Analyzer::analysisBlock(Block *root, vector<ir::Instruction *> &buffer)
{
    // printf("analysisBlock\n/");
    if (root->parent->type != NodeType::FUNCDEF)
        symbol_table.add_scope();
    for (int i = 1; i < root->children.size() - 1; i++)
    {
        ANALYSIS(block_item, BlockItem, i);
    }
    if (root->parent->type != NodeType::FUNCDEF)
        symbol_table.exit_scope();
}

// BlockItem -> Decl | Stmt
void frontend::Analyzer::analysisBlockItem(BlockItem *root, vector<ir::Instruction *> &buffer)
{
    // printf("analysisBlockItem\n");
    if (root->children[0]->type == NodeType::DECL)
    {
        // std::cout << "blockItem->analysisDecl" << std::endl;
        ANALYSIS(decl, Decl, 0);
    }
    else
    {
        // std::cout << "blockItem->analysisStmt" << std::endl;
        ANALYSIS(stmt, Stmt, 0);
    }
}

// Stmt -> LVal '=' Exp ';' | Block | 'if' '(' Cond ')' Stmt [ 'else' Stmt ] | 'while' '(' Cond ')' Stmt | 'break' ';' | 'continue' ';' | 'return' [Exp] ';' | [Exp] ';'
void frontend::Analyzer::analysisStmt(Stmt *root, vector<ir::Instruction *> &buffer)
{
    // printf("analysisStmt\n");
    if (LVal *lval = dynamic_cast<LVal *>(root->children[0]))
    {
        // std::cout << "analysisStmt LVal begin" << std::endl;
        // ANALYSIS(lval, LVal, 0);
        ANALYSIS(exp, Exp, 2);
        string var_name = dynamic_cast<Term *>(dynamic_cast<LVal *>(root->children[0])->children[0])->token.value;
        STE ident_ste = symbol_table.get_ste(var_name);
        string ident_after_rename = symbol_table.get_scoped_name(var_name);
        // a=...;
        if (root->children[0]->children.size() == 1)
        {
            if (ident_ste.operand.type == Type::Int)
            {
                if (exp->t == Type::IntLiteral)
                    buffer.push_back(new Instruction(Operand(exp->v, Type::IntLiteral),
                                                     Operand(),
                                                     Operand(ident_after_rename, Type::Int),
                                                     Operator::mov));
                else if (exp->t == Type::FloatLiteral)
                    buffer.push_back(new Instruction(Operand(std::to_string(int(std::stof(exp->v))), Type::IntLiteral),
                                                     Operand(),
                                                     Operand(ident_after_rename, Type::Int),
                                                     Operator::mov));
                else if (exp->t == Type::Int)
                    buffer.push_back(new Instruction(Operand(exp->v, Type::Int),
                                                     Operand(),
                                                     Operand(ident_after_rename, Type::Int),
                                                     Operator::mov));
                else if (exp->t == Type::Float)
                    buffer.push_back(new Instruction(Operand(exp->v, Type::Int),
                                                     Operand(),
                                                     Operand(ident_after_rename, Type::Int),
                                                     Operator::cvt_f2i));
            }
            else if (ident_ste.operand.type == Type::Float)
            {
                if (exp->t == Type::IntLiteral)
                    buffer.push_back(new Instruction(Operand(std::to_string(float(std::stoi(exp->v))), Type::FloatLiteral),
                                                     Operand(),
                                                     Operand(ident_after_rename, Type::Float),
                                                     Operator::fmov));
                else if (exp->t == Type::FloatLiteral)
                    buffer.push_back(new Instruction(Operand(exp->v, Type::FloatLiteral),
                                                     Operand(),
                                                     Operand(ident_after_rename, Type::Float),
                                                     Operator::fmov));
                else if (exp->t == Type::Int)
                    buffer.push_back(new Instruction(Operand(exp->v, Type::Int),
                                                     Operand(),
                                                     Operand(ident_after_rename, Type::Float),
                                                     Operator::cvt_f2i));
                else if (exp->t == Type::Float)
                    buffer.push_back(new Instruction(Operand(exp->v, Type::Float),
                                                     Operand(),
                                                     Operand(ident_after_rename, Type::Float),
                                                     Operator::fmov));
            }
        }
        // a[...]=...;
        else if (root->children[0]->children.size() == 4)
        {
            Exp *index_exp = dynamic_cast<Exp *>(root->children[0]->children[2]);
            analysisExp(index_exp, buffer);
            if (ident_ste.operand.type == Type::IntPtr)
            {
                if (exp->t == Type::IntLiteral || exp->t == Type::Int)
                    buffer.push_back(new Instruction(Operand(ident_after_rename, Type::IntPtr),
                                                     Operand(index_exp->v, index_exp->t),
                                                     Operand(exp->v, exp->t),
                                                     Operator::store));
                else if (exp->t == Type::FloatLiteral)
                    buffer.push_back(new Instruction(Operand(ident_after_rename, Type::IntPtr),
                                                     Operand(index_exp->v, index_exp->t),
                                                     Operand(std::to_string(int(std::stof(exp->v))), Type::IntLiteral),
                                                     Operator::store));
                else if (exp->t == Type::Float)
                {
                    string temp_int = "temp" + std::to_string(tmp_cnt++);
                    buffer.push_back(new Instruction(Operand(exp->v, Type::Float),
                                                     Operand(),
                                                     Operand(temp_int, Type::Int),
                                                     Operator::cvt_f2i));
                    buffer.push_back(new Instruction(Operand(ident_after_rename, Type::IntPtr),
                                                     Operand(index_exp->v, index_exp->t),
                                                     Operand(temp_int, Type::Int),
                                                     Operator::store));
                }
            }
            else if (ident_ste.operand.type == Type::FloatPtr)
            {
                if (exp->t == Type::IntLiteral)
                    buffer.push_back(new Instruction(Operand(ident_after_rename, Type::FloatPtr),
                                                     Operand(index_exp->v, index_exp->t),
                                                     Operand(std::to_string(float(std::stoi(exp->v))), Type::FloatLiteral),
                                                     Operator::store));
                else if (exp->t == Type::Int)
                {
                    string temp_float = "temp" + std::to_string(tmp_cnt++);
                    buffer.push_back(new Instruction(Operand(exp->v, Type::Int),
                                                     Operand(),
                                                     Operand(temp_float, Type::Float),
                                                     Operator::cvt_i2f));
                    buffer.push_back(new Instruction(Operand(ident_after_rename, Type::FloatPtr),
                                                     Operand(index_exp->v, index_exp->t),
                                                     Operand(temp_float, Type::Float),
                                                     Operator::store));
                }
                else if (exp->t == Type::FloatLiteral || exp->t == Type::Float)
                    buffer.push_back(new Instruction(Operand(ident_after_rename, Type::FloatPtr),
                                                     Operand(index_exp->v, index_exp->t),
                                                     Operand(exp->v, exp->t),
                                                     Operator::store));
            }
        }
        // a[...][...]=...;
        else if (root->children[0]->children.size() == 7)
        {
            Exp *index_exp_1 = dynamic_cast<Exp *>(root->children[0]->children[2]);
            Exp *index_exp_2 = dynamic_cast<Exp *>(root->children[0]->children[5]);
            analysisExp(index_exp_1, buffer);
            analysisExp(index_exp_2, buffer);
            string temp_dim_0 = "temp" + std::to_string(tmp_cnt++);
            string temp_dim_1 = "temp" + std::to_string(tmp_cnt++);
            buffer.push_back(new Instruction(Operand(index_exp_1->v, index_exp_1->t),
                                             Operand(),
                                             Operand(temp_dim_0, Type::Int),
                                             Operator::def));
            buffer.push_back(new Instruction(Operand(index_exp_2->v, index_exp_2->t),
                                             Operand(),
                                             Operand(temp_dim_1, Type::Int),
                                             Operator::def));
            string temp_col = "temp" + std::to_string(tmp_cnt++);
            buffer.push_back(new Instruction(Operand(std::to_string(ident_ste.dimension[1]), Type::IntLiteral),
                                             Operand(),
                                             Operand(temp_col, Type::Int),
                                             Operator::def));
            string temp_line_offset = "temp" + std::to_string(tmp_cnt++);
            buffer.push_back(new Instruction(Operand(temp_dim_0, Type::Int),
                                             Operand(temp_col, Type::Int),
                                             Operand(temp_line_offset, Type::Int),
                                             Operator::mul));
            string temp_offset = "temp" + std::to_string(tmp_cnt++);
            buffer.push_back(new Instruction(Operand(temp_line_offset, Type::Int),
                                             Operand(temp_dim_1, Type::Int),
                                             Operand(temp_offset, Type::Int),
                                             Operator::add));
            if (ident_ste.operand.type == Type::IntPtr)
            {
                if (exp->t == Type::IntLiteral || exp->t == Type::Int)
                    buffer.push_back(new Instruction(Operand(ident_after_rename, Type::IntPtr),
                                                     Operand(temp_offset, Type::Int),
                                                     Operand(exp->v, exp->t),
                                                     Operator::store));
                else if (exp->t == Type::FloatLiteral)
                    buffer.push_back(new Instruction(Operand(ident_after_rename, Type::IntPtr),
                                                     Operand(temp_offset, Type::Int),
                                                     Operand(std::to_string(int(std::stof(exp->v))), Type::IntLiteral),
                                                     Operator::store));
                else if (exp->t == Type::Float)
                {
                    string temp_int = "temp" + std::to_string(tmp_cnt++);
                    buffer.push_back(new Instruction(Operand(exp->v, Type::Float),
                                                     Operand(),
                                                     Operand(temp_int, Type::Int),
                                                     Operator::cvt_f2i));
                    buffer.push_back(new Instruction(Operand(ident_after_rename, Type::IntPtr),
                                                     Operand(temp_offset, Type::Int),
                                                     Operand(temp_int, Type::Int),
                                                     Operator::store));
                }
            }
            else if (ident_ste.operand.type == Type::FloatPtr)
            {
                if (exp->t == Type::IntLiteral)
                    buffer.push_back(new Instruction(Operand(ident_after_rename, Type::FloatPtr),
                                                     Operand(temp_offset, Type::Int),
                                                     Operand(std::to_string(float(std::stoi(exp->v))), Type::FloatLiteral),
                                                     Operator::store));
                else if (exp->t == Type::Int)
                {
                    string temp_float = "temp" + std::to_string(tmp_cnt++);
                    buffer.push_back(new Instruction(Operand(exp->v, Type::Int),
                                                     Operand(),
                                                     Operand(temp_float, Type::Float),
                                                     Operator::cvt_i2f));
                    buffer.push_back(new Instruction(Operand(ident_after_rename, Type::FloatPtr),
                                                     Operand(temp_offset, Type::Int),
                                                     Operand(temp_float, Type::Float),
                                                     Operator::store));
                }
                else if (exp->t == Type::FloatLiteral || exp->t == Type::Float)
                    buffer.push_back(new Instruction(Operand(ident_after_rename, Type::FloatPtr),
                                                     Operand(temp_offset, Type::Int),
                                                     Operand(exp->v, exp->t),
                                                     Operator::store));
            }
        }
        // std::cout << "analysis assign end" << std::endl;
        return;
    }
    if (Block *block_ = dynamic_cast<Block *>(root->children[0]))
    {
        // std::cout << "analysis block" << std::endl;
        ANALYSIS(block, Block, 0);
        return;
    }
    Term *term = dynamic_cast<Term *>(root->children[0]);
    if (term == nullptr)
    {
        // std::cout << "term is null" << std::endl;
        ANALYSIS(exp, Exp, 0);
        return;
    }
    if (term->token.type == TokenType::IFTK)
    {
        // std::cout << "analysis if" << std::endl;
        ANALYSIS(cond, Cond, 2);
        buffer.push_back(new Instruction(Operand(cond->v, cond->t),
                                         Operand(),
                                         Operand("2", Type::IntLiteral),
                                         Operator::_goto));
        int if_pc_index = buffer.size();
        Instruction *goto_if_inst = new Instruction(Operand(),
                                                    Operand(),
                                                    Operand(),
                                                    Operator::_goto);
        buffer.push_back(goto_if_inst);

        symbol_table.add_scope();
        ANALYSIS(stmt_0, Stmt, 4);
        symbol_table.exit_scope();

        // int if_false_goto_index = buffer.size() - if_pc_index;

        if (root->children.size() == 5)
        {
            int end_if_index = buffer.size();
            goto_if_inst->des = Operand(std::to_string(end_if_index - if_pc_index), Type::IntLiteral);
            buffer.push_back(new Instruction(Operand(),
                                             Operand(),
                                             Operand(),
                                             Operator::__unuse__));
        }
        else if (root->children.size() == 7)
        {
            Instruction *goto_if_true_inst = new Instruction(Operand(),
                                                             Operand(),
                                                             Operand(),
                                                             Operator::_goto);
            buffer.push_back(goto_if_true_inst);

            int before_else_goto_pc_index = buffer.size();

            symbol_table.add_scope();
            ANALYSIS(stmt_1, Stmt, 6);
            symbol_table.exit_scope();

            int end_else_index = buffer.size();
            // std::cout << "buffer.size()-before_else_pc_index+1:" << buffer.size() - before_else_pc_index + 1 << std::endl;
            goto_if_true_inst->des = Operand(std::to_string(end_else_index - before_else_goto_pc_index + 1), Type::IntLiteral);
            // std::cout << goto_if_true_inst->des.name << std::endl;
            // int if_true_else_goto_index = buffer.size() - if_pc_index - if_false_goto_index - 1;
            // buffer.push_back(new Instruction(Operand(),
            //                                  Operand(),
            //                                  Operand(std::to_string(if_true_else_goto_index), Type::IntLiteral),
            //                                  Operator::_goto));
            goto_if_inst->des = Operand(std::to_string(before_else_goto_pc_index - if_pc_index), Type::IntLiteral);
            buffer.push_back(new Instruction(Operand(),
                                             Operand(),
                                             Operand(),
                                             Operator::__unuse__));
        }
        return;
    }
    if (term->token.type == TokenType::WHILETK)
    {
        // std::cout << "analysis while" << std::endl;
        ANALYSIS(cond, Cond, 2);
        buffer.push_back(new Instruction(Operand(cond->v, cond->t),
                                         Operand(),
                                         Operand("2", Type::IntLiteral),
                                         Operator::_goto));
        int while_pc_index = buffer.size();
        Instruction *goto_while_inst = new Instruction(Operand(),
                                                       Operand(),
                                                       Operand(),
                                                       Operator::_goto);
        buffer.push_back(goto_while_inst);

        symbol_table.add_scope();
        ANALYSIS(stmt_0, Stmt, 4);
        symbol_table.exit_scope();

        int while_false_goto_index = buffer.size() - while_pc_index + 1;
        goto_while_inst->des = Operand(std::to_string(while_false_goto_index), Type::IntLiteral);
        int while_true_goto_index = -(buffer.size() - while_pc_index + 2);
        buffer.push_back(new Instruction(Operand(),
                                         Operand(),
                                         Operand(std::to_string(while_true_goto_index), Type::IntLiteral),
                                         Operator::_goto));
        buffer.push_back(new Instruction(Operand(),
                                         Operand(),
                                         Operand(),
                                         Operator::__unuse__));
        for (int i = while_pc_index; i < while_pc_index - 1 + while_false_goto_index; i++)
        {
            if (buffer[i]->op1.name == "break")
            {
                buffer[i]->op1 = Operand();
                buffer[i]->des = Operand(std::to_string(while_pc_index + while_false_goto_index - i), Type::IntLiteral);
                buffer[i]->op = Operator::_goto;
            }
            else if (buffer[i]->op1.name == "continue")
            {
                buffer[i]->op1 = Operand();
                buffer[i]->des = Operand(std::to_string(-(i - while_pc_index + 1)), Type::IntLiteral);
                buffer[i]->op = Operator::_goto;
            }
        }
        return;
    }
    if (term->token.type == TokenType::BREAKTK)
    {
        buffer.push_back(new Instruction(Operand("break", Type::null),
                                         Operand(),
                                         Operand(),
                                         Operator::__unuse__));
    }
    if (term->token.type == TokenType::CONTINUETK)
    {
        // std::cout << "analysis continue" << std::endl;
        buffer.push_back(new Instruction(Operand("continue", Type::null),
                                         Operand(),
                                         Operand(),
                                         Operator::__unuse__));
    }
    if (term->token.type == TokenType::RETURNTK)
    {
        // std::cout << "return begin" << std::endl;
        // std::cout << root->children.size() << std::endl;
        if (root->children.size() == 2)
            buffer.push_back(new Instruction(Operand(),
                                             Operand(),
                                             Operand(),
                                             Operator::_return));
        else if (root->children.size() == 3)
        {
            ANALYSIS(exp, Exp, 1);
            if (cur_func->returnType == Type::Int)
            {
                if (exp->t == Type::Int || exp->t == Type::IntLiteral)
                    buffer.push_back(new Instruction(Operand(exp->v, exp->t),
                                                     Operand(),
                                                     Operand(),
                                                     Operator::_return));
                else if (exp->t == Type::FloatLiteral)
                    buffer.push_back(new Instruction(Operand(std::to_string(int(std::stof(exp->v))), Type::IntLiteral),
                                                     Operand(),
                                                     Operand(),
                                                     Operator::_return));
                else if (exp->t == Type::Float)
                {
                    string temp_int = "temp" + std::to_string(tmp_cnt++);
                    buffer.push_back(new Instruction(Operand(exp->v, Type::Float),
                                                     Operand(),
                                                     Operand(temp_int, Type::Int),
                                                     Operator::cvt_f2i));
                    buffer.push_back(new Instruction(Operand(temp_int, Type::Int),
                                                     Operand(),
                                                     Operand(),
                                                     Operator::_return));
                }
            }
            else if (cur_func->returnType == Type::Float)
            {
                if (exp->t == Type::Float || exp->t == Type::FloatLiteral)
                    buffer.push_back(new Instruction(Operand(exp->v, exp->t),
                                                     Operand(),
                                                     Operand(),
                                                     Operator::_return));
                else if (exp->t == Type::IntLiteral)
                    buffer.push_back(new Instruction(Operand(std::to_string(float(std::stoi(exp->v))), Type::FloatLiteral),
                                                     Operand(),
                                                     Operand(),
                                                     Operator::_return));
                else if (exp->t == Type::Int)
                {
                    string temp_float = "temp" + std::to_string(tmp_cnt++);
                    buffer.push_back(new Instruction(Operand(exp->v, Type::Int),
                                                     Operand(),
                                                     Operand(temp_float, Type::Float),
                                                     Operator::cvt_i2f));
                    buffer.push_back(new Instruction(Operand(temp_float, Type::Float),
                                                     Operand(),
                                                     Operand(),
                                                     Operator::_return));
                }
            }
        }
        return;
        // std::cout << "return end" << std::endl;
    }
}

// Cond -> LOrExp
void frontend::Analyzer::analysisCond(Cond *root, vector<ir::Instruction *> &buffer)
{
    ANALYSIS(l_or_exp, LOrExp, 0);
    COPY_EXP_NODE(l_or_exp, root);
}

// RelExp -> AddExp { ('<' | '>' | '<=' | '>=') AddExp }
void frontend::Analyzer::analysisRelExp(RelExp *root, vector<ir::Instruction *> &buffer)
{
    // std::cout << "analysisRelExp begin" << std::endl;
    ANALYSIS(add_exp_0, AddExp, 0);
    // std::cout << "analysisRelExp for0" << std::endl;
    COPY_EXP_NODE(add_exp_0, root);
    // std::cout << "Im here" << std::endl;
    for (int i = 1; i < root->children.size(); i = i + 2)
    {
        // std::cout << "analysisRelExp for1" << std::endl;
        GET_CHILD_PTR(rel_op, Term, i);
        // std::cout << "analysisRelExp for2" << std::endl;
        string comp_op = rel_op->token.value;
        ANALYSIS(add_exp_i, AddExp, i + 1);
        // std::cout << "analysisRelExp for3" << std::endl;
        string temp_1 = "temp" + std::to_string(tmp_cnt++);
        string temp_2 = "temp" + std::to_string(tmp_cnt++);
        Type comp_type;
        if (root->t == Type::IntLiteral)
        {
            if (add_exp_i->t == Type::IntLiteral)
            {
                buffer.push_back(new Instruction(Operand(root->v, Type::IntLiteral),
                                                 Operand(),
                                                 Operand(temp_1, Type::Int),
                                                 Operator::def));
                buffer.push_back(new Instruction(Operand(add_exp_i->v, Type::IntLiteral),
                                                 Operand(),
                                                 Operand(temp_2, Type::Int),
                                                 Operator::def));
                comp_type = Type::Int;
            }
            else if (add_exp_i->t == Type::FloatLiteral)
            {
                buffer.push_back(new Instruction(Operand(std::to_string(float(std::stoi(add_exp_i->v))), Type::FloatLiteral),
                                                 Operand(),
                                                 Operand(temp_1, Type::Float),
                                                 Operator::fdef));
                buffer.push_back(new Instruction(Operand(add_exp_i->v, Type::FloatLiteral),
                                                 Operand(),
                                                 Operand(temp_2, Type::Float),
                                                 Operator::fdef));
                comp_type = Type::Float;
            }
            else if (add_exp_i->t == Type::Int)
            {
                buffer.push_back(new Instruction(Operand(root->v, Type::IntLiteral),
                                                 Operand(),
                                                 Operand(temp_1, Type::Int),
                                                 Operator::def));
                temp_2 = add_exp_i->v;
                comp_type = Type::Int;
            }
            else if (add_exp_i->t == Type::Float)
            {
                buffer.push_back(new Instruction(Operand(std::to_string(float(std::stoi(add_exp_i->v))), Type::FloatLiteral),
                                                 Operand(),
                                                 Operand(temp_1, Type::Float),
                                                 Operator::fdef));
                temp_2 = add_exp_i->v;
                comp_type = Type::Float;
            }
        }
        else if (root->t == Type::FloatLiteral)
        {
            comp_type = Type::Float;
            if (add_exp_i->t == Type::IntLiteral)
            {
                buffer.push_back(new Instruction(Operand(root->v, Type::FloatLiteral),
                                                 Operand(),
                                                 Operand(temp_1, Type::Float),
                                                 Operator::fdef));
                buffer.push_back(new Instruction(Operand(std::to_string(float(std::stoi(add_exp_i->v))), Type::FloatLiteral),
                                                 Operand(),
                                                 Operand(temp_2, Type::Float),
                                                 Operator::fdef));
            }
            else if (add_exp_i->t == Type::FloatLiteral)
            {
                buffer.push_back(new Instruction(Operand(root->v, Type::FloatLiteral),
                                                 Operand(),
                                                 Operand(temp_1, Type::Float),
                                                 Operator::fdef));
                buffer.push_back(new Instruction(Operand(add_exp_i->v, Type::FloatLiteral),
                                                 Operand(),
                                                 Operand(temp_2, Type::Float),
                                                 Operator::fdef));
            }
            else if (add_exp_i->t == Type::Int)
            {
                buffer.push_back(new Instruction(Operand(std::to_string(float(std::stoi(root->v))), Type::FloatLiteral),
                                                 Operand(),
                                                 Operand(temp_1, Type::Float),
                                                 Operator::fdef));
                buffer.push_back(new Instruction(Operand(add_exp_i->v, Type::Int),
                                                 Operand(),
                                                 Operand(temp_2, Type::Float),
                                                 Operator::cvt_i2f));
            }
            else if (add_exp_i->t == Type::Float)
            {
                buffer.push_back(new Instruction(Operand(root->v, Type::FloatLiteral),
                                                 Operand(),
                                                 Operand(temp_1, Type::Float),
                                                 Operator::fdef));
                temp_2 = add_exp_i->v;
            }
        }
        else if (root->t == Type::Int)
        {
            if (add_exp_i->t == Type::IntLiteral)
            {
                temp_1 = root->v;
                buffer.push_back(new Instruction(Operand(add_exp_i->v, Type::IntLiteral),
                                                 Operand(),
                                                 Operand(temp_2, Type::Int),
                                                 Operator::def));
                comp_type = Type::Int;
            }
            else if (add_exp_i->t == Type::FloatLiteral)
            {
                buffer.push_back(new Instruction(Operand(root->v, Type::Int),
                                                 Operand(),
                                                 Operand(temp_1, Type::Float),
                                                 Operator::cvt_i2f));
                buffer.push_back(new Instruction(Operand(add_exp_i->v, Type::FloatLiteral),
                                                 Operand(),
                                                 Operand(temp_2, Type::Float),
                                                 Operator::fdef));
                comp_type = Type::Float;
            }
            else if (add_exp_i->t == Type::Int)
            {
                temp_1 = root->v;
                buffer.push_back(new Instruction(Operand(add_exp_i->v, Type::Int),
                                                 Operand(),
                                                 Operand(temp_2, Type::Int),
                                                 Operator::def));
                comp_type = Type::Int;
            }
            else if (add_exp_i->t == Type::Float)
            {
                buffer.push_back(new Instruction(Operand(root->v, Type::Int),
                                                 Operand(),
                                                 Operand(temp_1, Type::Float),
                                                 Operator::cvt_i2f));
                temp_2 = add_exp_i->v;
                comp_type = Type::Float;
            }
        }
        else if (root->t == Type::Float)
        {
            comp_type = Type::Float;
            if (add_exp_i->t == Type::IntLiteral)
            {
                temp_1 = root->v;
                buffer.push_back(new Instruction(Operand(std::to_string(float(std::stoi(add_exp_i->v))), Type::FloatLiteral),
                                                 Operand(),
                                                 Operand(temp_2, Type::Float),
                                                 Operator::fdef));
            }
            else if (add_exp_i->t == Type::FloatLiteral)
            {
                temp_1 = root->v;
                buffer.push_back(new Instruction(Operand(add_exp_i->v, Type::FloatLiteral),
                                                 Operand(),
                                                 Operand(temp_2, Type::Float),
                                                 Operator::fdef));
            }
            else if (add_exp_i->t == Type::Int)
            {
                temp_1 = root->v;
                buffer.push_back(new Instruction(Operand(add_exp_i->v, Type::Int),
                                                 Operand(),
                                                 Operand(temp_2, Type::Float),
                                                 Operator::cvt_i2f));
            }
            else if (add_exp_i->t == Type::Float)
            {
                temp_1 = root->v;
                temp_2 = add_exp_i->v;
            }
        }
        string temp_3 = "temp" + std::to_string(tmp_cnt++);
        if (comp_type == Type::Int)
            buffer.push_back(new Instruction(Operand(temp_1, Type::Int),
                                             Operand(temp_2, Type::Int),
                                             Operand(temp_3, Type::Int),
                                             comp_op == "<" ? Operator::lss : comp_op == ">" ? Operator::gtr
                                                                          : comp_op == "<="  ? Operator::leq
                                                                                             : Operator::geq));
        else if (comp_type == Type::Float)
            buffer.push_back(new Instruction(Operand(temp_1, Type::Float),
                                             Operand(temp_2, Type::Float),
                                             Operand(temp_3, Type::Float),
                                             comp_op == "<" ? Operator::flss : comp_op == ">" ? Operator::fgtr
                                                                           : comp_op == "<="  ? Operator::fleq
                                                                                              : Operator::fgeq));
        root->v = temp_3;
        root->t = comp_type;
    }
    // std::cout << "analysisRelExp end" << std::endl;
}

// EqExp -> RelExp { ('==' | '!=') RelExp }
void frontend::Analyzer::analysisEqExp(EqExp *root, vector<ir::Instruction *> &buffer)
{
    ANALYSIS(rel_exp_0, RelExp, 0);
    COPY_EXP_NODE(rel_exp_0, root);
    for (int i = 1; i < root->children.size(); i = i + 2)
    {

        // std::cout << "EqExp1" << std::endl;
        GET_CHILD_PTR(eq_op, Term, i);
        string comp_op = eq_op->token.value;
        // std::cout << "EqExp2" << std::endl;
        ANALYSIS(rel_exp_i, RelExp, i + 1);
        string temp_1 = "temp" + std::to_string(tmp_cnt++);
        string temp_2 = "temp" + std::to_string(tmp_cnt++);
        // std::cout << "rel_exp_i->v" << rel_exp_i->v << " rel_exp_i->t" << toString(rel_exp_i->t) << std::endl;
        Type comp_type;
        if (root->t == Type::IntLiteral)
        {
            if (rel_exp_i->t == Type::IntLiteral)
            {
                buffer.push_back(new Instruction(Operand(root->v, Type::IntLiteral),
                                                 Operand(),
                                                 Operand(temp_1, Type::Int),
                                                 Operator::def));
                buffer.push_back(new Instruction(Operand(rel_exp_i->v, Type::IntLiteral),
                                                 Operand(),
                                                 Operand(temp_2, Type::Int),
                                                 Operator::def));
                comp_type = Type::Int;
            }
            else if (rel_exp_i->t == Type::FloatLiteral)
            {
                buffer.push_back(new Instruction(Operand(std::to_string(float(std::stoi(rel_exp_i->v))), Type::FloatLiteral),
                                                 Operand(),
                                                 Operand(temp_1, Type::Float),
                                                 Operator::fdef));
                buffer.push_back(new Instruction(Operand(rel_exp_i->v, Type::FloatLiteral),
                                                 Operand(),
                                                 Operand(temp_2, Type::Float),
                                                 Operator::fdef));
                comp_type = Type::Float;
            }
            else if (rel_exp_i->t == Type::Int)
            {
                buffer.push_back(new Instruction(Operand(root->v, Type::IntLiteral),
                                                 Operand(),
                                                 Operand(temp_1, Type::Int),
                                                 Operator::def));
                temp_2 = rel_exp_i->v;
                comp_type = Type::Int;
            }
            else if (rel_exp_i->t == Type::Float)
            {
                buffer.push_back(new Instruction(Operand(std::to_string(float(std::stoi(rel_exp_i->v))), Type::FloatLiteral),
                                                 Operand(),
                                                 Operand(temp_1, Type::Float),
                                                 Operator::fdef));
                temp_2 = rel_exp_i->v;
                comp_type = Type::Float;
            }
        }
        else if (root->t == Type::FloatLiteral)
        {
            comp_type = Type::Float;
            if (rel_exp_i->t == Type::IntLiteral)
            {
                buffer.push_back(new Instruction(Operand(root->v, Type::FloatLiteral),
                                                 Operand(),
                                                 Operand(temp_1, Type::Float),
                                                 Operator::fdef));
                buffer.push_back(new Instruction(Operand(std::to_string(float(std::stoi(rel_exp_i->v))), Type::FloatLiteral),
                                                 Operand(),
                                                 Operand(temp_2, Type::Float),
                                                 Operator::fdef));
            }
            else if (rel_exp_i->t == Type::FloatLiteral)
            {
                buffer.push_back(new Instruction(Operand(root->v, Type::FloatLiteral),
                                                 Operand(),
                                                 Operand(temp_1, Type::Float),
                                                 Operator::fdef));
                buffer.push_back(new Instruction(Operand(rel_exp_i->v, Type::FloatLiteral),
                                                 Operand(),
                                                 Operand(temp_2, Type::Float),
                                                 Operator::fdef));
            }
            else if (rel_exp_i->t == Type::Int)
            {
                buffer.push_back(new Instruction(Operand(std::to_string(float(std::stoi(root->v))), Type::FloatLiteral),
                                                 Operand(),
                                                 Operand(temp_1, Type::Float),
                                                 Operator::fdef));
                buffer.push_back(new Instruction(Operand(rel_exp_i->v, Type::Int),
                                                 Operand(),
                                                 Operand(temp_2, Type::Float),
                                                 Operator::cvt_i2f));
            }
            else if (rel_exp_i->t == Type::Float)
            {
                buffer.push_back(new Instruction(Operand(root->v, Type::FloatLiteral),
                                                 Operand(),
                                                 Operand(temp_1, Type::Float),
                                                 Operator::fdef));
                temp_2 = rel_exp_i->v;
            }
        }
        else if (root->t == Type::Int)
        {
            if (rel_exp_i->t == Type::IntLiteral)
            {
                temp_1 = root->v;
                buffer.push_back(new Instruction(Operand(rel_exp_i->v, Type::IntLiteral),
                                                 Operand(),
                                                 Operand(temp_2, Type::Int),
                                                 Operator::def));
                comp_type = Type::Int;
            }
            else if (rel_exp_i->t == Type::FloatLiteral)
            {
                buffer.push_back(new Instruction(Operand(root->v, Type::Int),
                                                 Operand(),
                                                 Operand(temp_1, Type::Float),
                                                 Operator::cvt_i2f));
                buffer.push_back(new Instruction(Operand(rel_exp_i->v, Type::FloatLiteral),
                                                 Operand(),
                                                 Operand(temp_2, Type::Float),
                                                 Operator::fdef));
                comp_type = Type::Float;
            }
            else if (rel_exp_i->t == Type::Int)
            {
                temp_1 = root->v;
                buffer.push_back(new Instruction(Operand(rel_exp_i->v, Type::Int),
                                                 Operand(),
                                                 Operand(temp_2, Type::Int),
                                                 Operator::def));
                comp_type = Type::Int;
            }
            else if (rel_exp_i->t == Type::Float)
            {
                buffer.push_back(new Instruction(Operand(root->v, Type::Int),
                                                 Operand(),
                                                 Operand(temp_1, Type::Float),
                                                 Operator::cvt_i2f));
                temp_2 = rel_exp_i->v;
                comp_type = Type::Float;
            }
        }
        else if (root->t == Type::Float)
        {
            comp_type = Type::Float;
            if (rel_exp_i->t == Type::IntLiteral)
            {
                temp_1 = root->v;
                buffer.push_back(new Instruction(Operand(std::to_string(float(std::stoi(rel_exp_i->v))), Type::FloatLiteral),
                                                 Operand(),
                                                 Operand(temp_2, Type::Float),
                                                 Operator::fdef));
            }
            else if (rel_exp_i->t == Type::FloatLiteral)
            {
                temp_1 = root->v;
                buffer.push_back(new Instruction(Operand(rel_exp_i->v, Type::FloatLiteral),
                                                 Operand(),
                                                 Operand(temp_2, Type::Float),
                                                 Operator::fdef));
            }
            else if (rel_exp_i->t == Type::Int)
            {
                temp_1 = root->v;
                buffer.push_back(new Instruction(Operand(rel_exp_i->v, Type::Int),
                                                 Operand(),
                                                 Operand(temp_2, Type::Float),
                                                 Operator::cvt_i2f));
            }
            else if (rel_exp_i->t == Type::Float)
            {
                temp_1 = root->v;
                temp_2 = rel_exp_i->v;
            }
        }
        string temp_3 = "temp" + std::to_string(tmp_cnt++);
        if (comp_type == Type::Int)
            buffer.push_back(new Instruction(Operand(temp_1, Type::Int),
                                             Operand(temp_2, Type::Int),
                                             Operand(temp_3, Type::Int),
                                             comp_op == "==" ? Operator::eq : Operator::neq));
        else if (comp_type == Type::Float)
            buffer.push_back(new Instruction(Operand(temp_1, Type::Float),
                                             Operand(temp_2, Type::Float),
                                             Operand(temp_3, Type::Float),
                                             comp_op == "==" ? Operator::feq : Operator::fneq));
        root->v = temp_3;
        root->t = comp_type;
    }
    // std::cout << "EqExp3    " << std::endl;
}

// LAndExp -> EqExp [ '&&' LAndExp ]
void frontend::Analyzer::analysisLAndExp(LAndExp *root, vector<ir::Instruction *> &buffer)
{
    // std::cout << "LAndExp begin  " << std::endl;
    ANALYSIS(eqexp, EqExp, 0);
    COPY_EXP_NODE(eqexp, root);
    if (root->children.size() > 1)
    {
        string temp_bool1 = "temp" + std::to_string(tmp_cnt++);
        string temp_result = "temp" + std::to_string(tmp_cnt++);
        buffer.push_back(new Instruction(Operand(root->v, Type::Int),
                                         Operand("0", Type::IntLiteral),
                                         Operand(temp_bool1, Type::Int),
                                         Operator::neq));
        buffer.push_back(new Instruction(Operand("0", Type::IntLiteral),
                                         Operand(),
                                         Operand(temp_result, Type::Int),
                                         Operator::def));
        buffer.push_back(new Instruction(Operand(temp_bool1, Type::Int),
                                         Operand(),
                                         Operand("2", Type::IntLiteral),
                                         Operator::_goto));
        Instruction *label_1 = new Instruction(Operand(),
                                               Operand(),
                                               Operand(),
                                               Operator::_goto);
        buffer.push_back(label_1);
        int index1 = buffer.size();
        ANALYSIS(landexp, LAndExp, 2);
        buffer.push_back(new Instruction(Operand(root->v, Type::Int),
                                         Operand(landexp->v, Type::Int),
                                         Operand(temp_result, Type::Int),
                                         Operator::_and));
        int index2 = buffer.size();
        label_1->des = Operand(std::to_string(index2 - index1 + 1), Type::IntLiteral);
        root->v = temp_result;
    }
}

// LOrExp -> LAndExp [ '||' LOrExp ]
void frontend::Analyzer::analysisLOrExp(LOrExp *root, vector<ir::Instruction *> &buffer)
{
    // std::cout << "LoRExp begin  " << std::endl;
    ANALYSIS(landexp, LAndExp, 0);
    COPY_EXP_NODE(landexp, root);
    if (root->children.size() > 1)
    {
        string temp_bool1 = "temp" + std::to_string(tmp_cnt++);
        string temp_result = "temp" + std::to_string(tmp_cnt++);
        buffer.push_back(new Instruction(Operand(root->v, Type::Int),
                                         Operand("0", Type::IntLiteral),
                                         Operand(temp_bool1, Type::Int),
                                         Operator::eq));
        buffer.push_back(new Instruction(Operand("1", Type::IntLiteral),
                                         Operand(),
                                         Operand(temp_result, Type::Int),
                                         Operator::def));
        buffer.push_back(new Instruction(Operand(temp_bool1, Type::Int),
                                         Operand(),
                                         Operand("2", Type::IntLiteral),
                                         Operator::_goto));
        Instruction *label_1 = new Instruction(Operand(),
                                               Operand(),
                                               Operand(),
                                               Operator::_goto);
        buffer.push_back(label_1);
        int index1 = buffer.size();
        ANALYSIS(lorexp, LOrExp, 2);
        buffer.push_back(new Instruction(Operand(root->v, Type::Int),
                                         Operand(lorexp->v, Type::Int),
                                         Operand(temp_result, Type::Int),
                                         Operator::_or));
        int index2 = buffer.size();
        label_1->des = Operand(std::to_string(index2 - index1 + 1), Type::IntLiteral);
        root->v = temp_result;
    }
    // std::cout << "LoRExp end   " << std::endl;
}
=======
#include "front/semantic.h"
#include <iostream>
#include <cassert>

using ir::Function;
using ir::Instruction;
using ir::Operand;
using ir::Operator;

#define TODO assert(0 && "TODO");

#define GET_CHILD_PTR(node, type, index)                     \
    auto node = dynamic_cast<type *>(root->children[index]); \
    assert(node);
#define ANALYSIS(node, type, index)                          \
    auto node = dynamic_cast<type *>(root->children[index]); \
    assert(node);                                            \
    analysis##type(node, buffer);
#define COPY_EXP_NODE(from, to)              \
    to->is_computable = from->is_computable; \
    to->v = from->v;                         \
    to->t = from->t;

map<std::string, ir::Function *> *frontend::get_lib_funcs()
{
    static map<std::string, ir::Function *> lib_funcs = {
        {"getint", new Function("getint", Type::Int)},
        {"getch", new Function("getch", Type::Int)},
        {"getfloat", new Function("getfloat", Type::Float)},
        {"getarray", new Function("getarray", {Operand("arr", Type::IntPtr)}, Type::Int)},
        {"getfarray", new Function("getfarray", {Operand("arr", Type::FloatPtr)}, Type::Int)},
        {"putint", new Function("putint", {Operand("i", Type::Int)}, Type::null)},
        {"putch", new Function("putch", {Operand("i", Type::Int)}, Type::null)},
        {"putfloat", new Function("putfloat", {Operand("f", Type::Float)}, Type::null)},
        {"putarray", new Function("putarray", {Operand("n", Type::Int), Operand("arr", Type::IntPtr)}, Type::null)},
        {"putfarray", new Function("putfarray", {Operand("n", Type::Int), Operand("arr", Type::FloatPtr)}, Type::null)},
    };
    return &lib_funcs;
}

void frontend::SymbolTable::add_scope()
{
    ScopeInfo scope_info;
    scope_info.cnt = 0;
    scope_info.name = "block " + std::to_string(scope_stack.size());
    scope_stack.push_back(scope_info);
}
void frontend::SymbolTable::exit_scope()
{
    scope_stack.pop_back();
}

string frontend::SymbolTable::get_scoped_name(string id) const
{
    for (auto it = scope_stack.rbegin(); it != scope_stack.rend(); it++)
    {
        auto scope = *it;
        if (scope.table.find(id) != scope.table.end())
        {
            return scope.name + "_" + id;
        }
    }
}

Operand frontend::SymbolTable::get_operand(string id) const
{
    for (int i = scope_stack.size() - 1; i >= 0; i--)
    {
        auto find_result = scope_stack[i].table.find(id);
        if (find_result != scope_stack[i].table.end())
            return find_result->second.operand;
    }
}

frontend::STE frontend::SymbolTable::get_ste(string id) const
{
    for (int i = scope_stack.size() - 1; i >= 0; i--)
    {
        auto find_result = scope_stack[i].table.find(id);
        if (find_result != scope_stack[i].table.end())
            return find_result->second;
    }
}

frontend::Analyzer::Analyzer() : tmp_cnt(0), symbol_table()
{
    // add lib functions
    auto lib_funcs = get_lib_funcs();
    for (auto &func : *lib_funcs)
    {
        symbol_table.functions[func.first] = func.second;
    }
}

ir::Program frontend::Analyzer::get_ir_program(CompUnit *root)
{
    symbol_table.add_scope();
    ir::Function *global_fun = new ir::Function("global", Type::null);
    symbol_table.functions["global"] = global_fun;
    ir::Program program;
    analysisCompUnit(root, program);
    global_fun->addInst(new Instruction(ir::Operand(),
                                        ir::Operand(),
                                        ir::Operand(), ir::Operator::_return));
    program.addFunction(*global_fun);
    for (auto it = symbol_table.scope_stack[0].table.begin(); it != symbol_table.scope_stack[0].table.end(); it++)
    {
        if (it->second.dimension.size() != 0)
        {
            int arr_len = 1;
            for (int i = 0; i < it->second.dimension.size(); i++)
                arr_len *= it->second.dimension[i];
            program.globalVal.push_back({{symbol_table.get_scoped_name(it->second.operand.name), it->second.operand.type}, arr_len});
        }
        else
        {
            if (it->second.operand.type == Type::FloatLiteral)
                program.globalVal.push_back({{symbol_table.get_scoped_name(it->second.operand.name), Type::Float}});
            else if (it->second.operand.type == Type::IntLiteral)
                program.globalVal.push_back({{symbol_table.get_scoped_name(it->second.operand.name), Type::Int}});
            else
                program.globalVal.push_back({{symbol_table.get_scoped_name(it->second.operand.name), it->second.operand.type}});
        }
    }
    for (Function &func : program.functions)
        if (func.returnType == Type::null)
        {
            func.addInst(new Instruction(ir::Operand(),
                                         ir::Operand(),
                                         ir::Operand(),
                                         ir::Operator::_return));
        }
    return program;
}

// CompUnit -> (Decl | FuncDef) [CompUnit]
void frontend::Analyzer::analysisCompUnit(CompUnit *root, ir::Program &program)
{
    if (Decl *decl = dynamic_cast<Decl *>(root->children[0]))
    {
        vector<ir::Instruction *> buffer;
        analysisDecl(decl, buffer);
        for (Instruction *inst : buffer)
            symbol_table.functions["global"]->addInst(inst);
    }
    else if (FuncDef *funcdef = dynamic_cast<FuncDef *>(root->children[0]))
    {
        ir::Function *func = new Function();
        cur_func = func;
        analysisFuncDef(funcdef, *func);
        program.addFunction(*func);
    }
    if (root->children.size() > 1)
        analysisCompUnit(dynamic_cast<CompUnit *>(root->children[1]), program);
}

// BType -> 'int' | 'float'
void frontend::Analyzer::analysisBType(BType *root)
{
    Term *bt = dynamic_cast<Term *>(root->children[0]);
    root->t = bt->token.type == TokenType::INTTK ? Type::Int : Type::Float;
}
// Decl -> ConstDecl | VarDecl
void frontend::Analyzer::analysisDecl(Decl *root, vector<ir::Instruction *> &buffer)
{
    if (root->children[0]->type == NodeType::CONSTDECL)
    {
        ANALYSIS(constdecl, ConstDecl, 0);
    }
    else if (root->children[0]->type == NodeType::VARDECL)
    {
        ANALYSIS(vardecl, VarDecl, 0);
    }
}

// ConstDecl -> 'const' BType ConstDef { ',' ConstDef } ';'
void frontend::Analyzer::analysisConstDecl(ConstDecl *root, vector<ir::Instruction *> &buffer)
{
    GET_CHILD_PTR(btype, BType, 1);
    Term *bt = dynamic_cast<Term *>(btype->children[0]);
    root->t = bt->token.type == TokenType::INTTK ? Type::Int : Type::Float;
    ANALYSIS(constdef, ConstDef, 2);
    for (int i = 3; i < root->children.size(); i = i + 2)
    {
        GET_CHILD_PTR(term, Term, i);
        if (term->token.type == TokenType::SEMICN)
            break;
        ANALYSIS(constdef, ConstDef, i + 1);
    }
}

// ConstDef -> Ident { '[' ConstExp ']' } '=' ConstInitVal
void frontend::Analyzer::analysisConstDef(ConstDef *root, vector<ir::Instruction *> &buffer)
{
    ConstDecl *parent = dynamic_cast<ConstDecl *>(root->parent);
    Type ident_type = parent->t == Type::Int ? Type::IntLiteral : Type::FloatLiteral;
    GET_CHILD_PTR(ident, Term, 0);
    string Ident_name = ident->token.value;
    STE ste;
    Operand ident_operand = Operand(Ident_name, ident_type);
    if (root->children.size() > 1)
    {
        GET_CHILD_PTR(term_0, Term, 1);
        if (term_0->token.type == TokenType::LBRACK)
        {
            if (ident_operand.type == Type::IntLiteral)
                ident_operand.type = Type::IntPtr;
            else if (ident_operand.type == Type::FloatLiteral)
                ident_operand.type = Type::FloatPtr;
            ANALYSIS(constexp_1, ConstExp, 2);
            ste.dimension.push_back(std::stoi(constexp_1->v));
            if (root->children.size() > 4)
            {
                GET_CHILD_PTR(term_1, Term, 4);
                if (term_1->token.type == TokenType::LBRACK)
                {
                    ANALYSIS(constexp_2, ConstExp, 5);
                    ste.dimension.push_back(std::stoi(constexp_2->v));
                }
            }
        }
    }
    ste.operand = ident_operand;
    symbol_table.scope_stack.back().table[Ident_name] = ste;
    string ident_after_rename = symbol_table.get_scoped_name(Ident_name);

    GET_CHILD_PTR(term_0, Term, 1);
    // const int/float a = ...;
    if (term_0->token.type == TokenType::ASSIGN)
    {
        ANALYSIS(constinitval, ConstInitVal, 2);
        string initval_value = constinitval->v;
        if (ident_type == Type::IntLiteral && constinitval->t == Type::FloatLiteral)
            initval_value = std::to_string((int)std::stof(initval_value));
        else if (ident_type == Type::FloatLiteral && constinitval->t == Type::IntLiteral)
            initval_value = std::to_string((float)std::stoi(initval_value));
        symbol_table.scope_stack.back().table[Ident_name].literal_value = initval_value;
    }
    else if (term_0->token.type == TokenType::LBRACK)
    {
        int ident_array_dim = ste.dimension.size();
        int allco_size;
        allco_size = ident_array_dim == 1 ? ste.dimension[0] : ste.dimension[0] * ste.dimension[1];
        if (symbol_table.scope_stack.size() > 1)
            buffer.push_back(new Instruction(Operand(std::to_string(allco_size), Type::IntLiteral),
                                             Operand(),
                                             Operand(ident_after_rename, ident_operand.type),
                                             Operator::alloc));

        // int/float a[3]={1,2,3};
        // int/float a[2][3]={1,2,3,4,5,6};

        ANALYSIS(constinitval, ConstInitVal, root->children.size() - 1);
        string const_list = constinitval->v;
        Type const_type = constinitval->t;
        string temp_const = "";
        int j = 0;
        for (int i = 0; i < constinitval->v.size(); i++)
        {
            if (constinitval->v[i] == ',')
            {
                buffer.push_back(new Instruction(Operand(symbol_table.get_scoped_name(Ident_name), ident_operand.type),
                                                 Operand(std::to_string(j), Type::IntLiteral),
                                                 Operand(temp_const, ident_type),
                                                 Operator::store));
                j++;
                temp_const = "";
            }
            else
                temp_const += const_list[i];
        }
    }
}

// ConstInitVal -> ConstExp | '{' [ ConstInitVal { ',' ConstInitVal } ] '}'
void frontend::Analyzer::analysisConstInitVal(ConstInitVal *root, vector<ir::Instruction *> &buffer)
{
    if (root->children.size() == 1)
    {
        ANALYSIS(constexp, ConstExp, 0);
        root->v = constexp->v;
        root->t = constexp->t;
    }
    else
    {
        ConstDef *const_def = dynamic_cast<ConstDef *>(root->parent);
        ConstDecl *const_decl = dynamic_cast<ConstDecl *>(const_def->parent);
        string const_list = "";
        for (int i = 1; i < root->children.size() - 1; i += 2)
        {
            ANALYSIS(constinitval_i, ConstInitVal, i);
            string constinitval_v = constinitval_i->v;
            const_list = const_list + constinitval_v + ",";
        }
        root->v = const_list;
        root->t = const_decl->t;
    }
}

// VarDecl -> BType VarDef { ',' VarDef } ';'
void frontend::Analyzer::analysisVarDecl(VarDecl *root, vector<ir::Instruction *> &buffer)
{
    GET_CHILD_PTR(btype, BType, 0);
    Term *bt = dynamic_cast<Term *>(btype->children[0]);
    root->t = bt->token.type == TokenType::INTTK ? Type::Int : Type::Float;
    ANALYSIS(vardef, VarDef, 1);
    for (int i = 2; i < root->children.size(); i += 2)
    {
        GET_CHILD_PTR(term, Term, i);
        if (term->token.type == TokenType::SEMICN)
            break;
        ANALYSIS(vardef, VarDef, i + 1);
    }
}

// VarDef -> Ident { '[' ConstExp ']' } [ '=' InitVal ]
void frontend::Analyzer::analysisVarDef(VarDef *root, vector<ir::Instruction *> &buffer)
{
    VarDecl *parent = dynamic_cast<VarDecl *>(root->parent);
    Type ident_type = parent->t;
    GET_CHILD_PTR(ident, Term, 0);
    string Ident_name = ident->token.value;
    STE ste;
    Operand ident_operand = Operand(Ident_name, ident_type);
    if (root->children.size() > 1)
    {
        GET_CHILD_PTR(term_0, Term, 1);
        if (term_0->token.type == TokenType::LBRACK)
        {
            if (ident_operand.type == Type::Int)
                ident_operand.type = Type::IntPtr;
            else if (ident_operand.type == Type::Float)
                ident_operand.type = Type::FloatPtr;
            ANALYSIS(constexp_1, ConstExp, 2);
            ste.dimension.push_back(std::stoi(constexp_1->v));
            if (root->children.size() > 4)
            {
                GET_CHILD_PTR(term_1, Term, 4);
                if (term_1->token.type == TokenType::LBRACK)
                {
                    ANALYSIS(constexp_2, ConstExp, 5);
                    ste.dimension.push_back(std::stoi(constexp_2->v));
                }
            }
        }
    }
    ste.operand = ident_operand;
    symbol_table.scope_stack.back().table[Ident_name] = ste;
    string ident_after_rename = symbol_table.get_scoped_name(Ident_name);

    // int/float a;
    if (root->children.size() == 1)
    {
        if (ident_operand.type == Type::Int)
            buffer.push_back(new Instruction(Operand("0", Type::IntLiteral),
                                             Operand(),
                                             Operand(ident_after_rename, Type::Int),
                                             Operator::def));
        else if (ident_operand.type == Type::Float)
            buffer.push_back(new Instruction(Operand("0.0", Type::FloatLiteral),
                                             Operand(),
                                             Operand(ident_after_rename, Type::Float),
                                             Operator::fdef));
    }
    else
    {
        GET_CHILD_PTR(term_0, Term, 1);
        // int/float a = ...;
        if (term_0->token.type == TokenType::ASSIGN)
        {
            ANALYSIS(initval, InitVal, 2);
            string initval_value = initval->v;
            if (ident_type == Type::Int)
            {
                if (initval->t == Type::IntLiteral)
                    buffer.push_back(new Instruction(Operand(initval_value, Type::IntLiteral),
                                                     Operand(),
                                                     Operand(ident_after_rename, Type::Int),
                                                     Operator::def));
                else if (initval->t == Type::Int)
                    buffer.push_back(new Instruction(Operand(initval_value, Type::Int),
                                                     Operand(),
                                                     Operand(ident_after_rename, Type::Int),
                                                     Operator::def));
                else if (initval->t == Type::FloatLiteral)
                {
                    string temp_float = "temp" + std::to_string(tmp_cnt++);
                    buffer.push_back(new Instruction(Operand(initval_value, Type::FloatLiteral),
                                                     Operand(),
                                                     Operand(temp_float, Type::Float),
                                                     Operator::fdef));
                    buffer.push_back(new Instruction(Operand(temp_float, Type::Float),
                                                     Operand(),
                                                     Operand(ident_after_rename, Type::Int),
                                                     Operator::cvt_f2i));
                }
                else if (initval->t == Type::Float)
                    buffer.push_back(new Instruction(Operand(initval_value, Type::Float),
                                                     Operand(),
                                                     Operand(ident_after_rename, Type::Int),
                                                     Operator::cvt_f2i));
            }
            else if (ident_type == Type::Float)
            {
                if (initval->t == Type::FloatLiteral)
                    buffer.push_back(new Instruction(Operand(initval_value, Type::FloatLiteral),
                                                     Operand(),
                                                     Operand(ident_after_rename, Type::Float),
                                                     Operator::fdef));
                else if (initval->t == Type::Float)
                    buffer.push_back(new Instruction(Operand(initval_value, Type::Float),
                                                     Operand(),
                                                     Operand(ident_after_rename, Type::Float),
                                                     Operator::fdef));
                else if (initval->t == Type::IntLiteral)
                {
                    string temp_int = "temp" + std::to_string(tmp_cnt++);
                    buffer.push_back(new Instruction(Operand(initval_value, Type::IntLiteral),
                                                     Operand(),
                                                     Operand(temp_int, Type::Int),
                                                     Operator::def));
                    buffer.push_back(new Instruction(Operand(temp_int, Type::IntLiteral),
                                                     Operand(),
                                                     Operand(ident_after_rename, Type::Float),
                                                     Operator::cvt_i2f));
                }
                else if (initval->t == Type::Int)
                    buffer.push_back(new Instruction(Operand(initval_value, Type::Int),
                                                     Operand(),
                                                     Operand(ident_after_rename, Type::Float),
                                                     Operator::cvt_i2f));
            }
        }
        else if (term_0->token.type == TokenType::LBRACK)
        {
            int ident_array_dim = ste.dimension.size();
            int allco_size;
            allco_size = ident_array_dim == 1 ? ste.dimension[0] : ste.dimension[0] * ste.dimension[1];
            if (symbol_table.scope_stack.size() > 1)
                buffer.push_back(new Instruction(Operand(std::to_string(allco_size), Type::IntLiteral),
                                                 Operand(),
                                                 Operand(ident_after_rename, ident_operand.type),
                                                 Operator::alloc));
            // int/float a[10];
            // 申请空间后什么都不做

            if (root->children.size() == 6 || root->children.size() == 9)
            {
                ANALYSIS(initval, InitVal, root->children.size() - 1);
                string temp_var_list = initval->v;
                // //int/float a[10]={};
                // inr/float a[10][10]={};
                if (temp_var_list == "")
                {
                    for (int i = 0; i < allco_size; i++)
                        buffer.push_back(new Instruction(Operand(ident_after_rename, ident_operand.type),
                                                         Operand(std::to_string(i), Type::IntLiteral),
                                                         Operand(ident_type == Type::Int ? "0" : "0.0", ident_type == Type::Int ? Type::IntLiteral : Type::FloatLiteral),
                                                         Operator::store));
                }
                // int/float a[3]={1,2,3};
                // int/float a[2][3]={1,2,3,4,5,6};
                else
                {
                    Type var_type = initval->t;
                    string temp_var_name = "";
                    int j = 0;
                    for (int i = 0; i < initval->v.size(); i++)
                    {
                        if (initval->v[i] == ',')
                        {
                            buffer.push_back(new Instruction(Operand(ident_after_rename, ident_operand.type),
                                                             Operand(std::to_string(j), Type::IntLiteral),
                                                             Operand(temp_var_name, ident_type),
                                                             Operator::store));
                            j++;
                            temp_var_name = "";
                        }
                        else
                            temp_var_name += temp_var_list[i];
                    }
                    if (j < allco_size - 1)
                    {
                        for (int i = j; i < allco_size; i++)
                            buffer.push_back(new Instruction(Operand(ident_after_rename, ident_operand.type),
                                                             Operand(std::to_string(i), Type::IntLiteral),
                                                             Operand(ident_type == Type::Int ? "0" : "0.0", ident_type == Type::Int ? Type::IntLiteral : Type::FloatLiteral),
                                                             Operator::store));
                    }
                }
            }
        }
    }
    // std::cout << "vardef end" << std::endl;
}

// ConstExp -> AddExp
void frontend::Analyzer::analysisConstExp(ConstExp *root, vector<ir::Instruction *> &buffer)
{
    ANALYSIS(addexp, AddExp, 0);
    COPY_EXP_NODE(addexp, root);
}

// InitVal -> Exp | '{' [ InitVal { ',' InitVal } ] '}'
void frontend::Analyzer::analysisInitVal(InitVal *root, vector<ir::Instruction *> &buffer)
{
    if (root->children.size() == 1)
    {
        ANALYSIS(exp, Exp, 0);
        COPY_EXP_NODE(exp, root);
    }
    else if (root->children.size() == 2)
        root->v = "";
    else
    {
        VarDef *var_def = dynamic_cast<VarDef *>(root->parent);
        VarDecl *var_decl = dynamic_cast<VarDecl *>(var_def->parent);
        string var_name_list = "";
        for (int i = 1; i < root->children.size() - 1; i += 2)
        {
            ANALYSIS(initval_i, InitVal, i);
            string initval_v = initval_i->v;
            Type initval_t = initval_i->t;
            if (var_decl->t == Type::Int)
            {
                if (initval_t == Type::Int)
                    var_name_list = var_name_list + initval_i->v + ",";
                else if (initval_t == Type::Float)
                {
                    string temp_int = "temp" + std::to_string(tmp_cnt++);
                    buffer.push_back(new Instruction(Operand(initval_v, Type::Float),
                                                     Operand(),
                                                     Operand(temp_int, Type::Int),
                                                     Operator::cvt_f2i));
                    var_name_list = var_name_list + temp_int + ",";
                }
                else if (initval_t == Type::IntLiteral)
                {
                    string temp_int = "temp" + std::to_string(tmp_cnt++);
                    buffer.push_back(new Instruction(Operand(initval_v, Type::IntLiteral),
                                                     Operand(),
                                                     Operand(temp_int, Type::Int),
                                                     Operator::def));
                    var_name_list = var_name_list + temp_int + ",";
                }
                else if (initval_t == Type::FloatLiteral)
                {
                    string temp_int = "temp" + std::to_string(tmp_cnt++);
                    buffer.push_back(new Instruction(Operand(std::to_string(int(std::stof(initval_v))), Type::IntLiteral),
                                                     Operand(),
                                                     Operand(temp_int, Type::Int),
                                                     Operator::def));
                    var_name_list = var_name_list + temp_int + ",";
                }
            }
            else if (var_decl->t == Type::Float)
            {
                if (initval_t == Type::Float)
                    var_name_list = var_name_list + initval_i->v + ",";
                else if (initval_t == Type::Int)
                {
                    string temp_float = "temp" + std::to_string(tmp_cnt++);
                    buffer.push_back(new Instruction(Operand(initval_v, Type::Int),
                                                     Operand(),
                                                     Operand(temp_float, Type::Float),
                                                     Operator::cvt_i2f));
                    var_name_list = var_name_list + temp_float + ",";
                }
                else if (initval_t == Type::IntLiteral)
                {
                    string temp_float = "temp" + std::to_string(tmp_cnt++);
                    buffer.push_back(new Instruction(Operand(std::to_string(float(std::stof(initval_v))), Type::FloatLiteral),
                                                     Operand(),
                                                     Operand(temp_float, Type::Float),
                                                     Operator::fdef));
                    var_name_list = var_name_list + temp_float + ",";
                }
                else if (initval_t == Type::FloatLiteral)
                {
                    string temp_float = "temp" + std::to_string(tmp_cnt++);
                    buffer.push_back(new Instruction(Operand(initval_v, Type::FloatLiteral),
                                                     Operand(),
                                                     Operand(temp_float, Type::Float),
                                                     Operator::fdef));
                    var_name_list = var_name_list + temp_float + ",";
                }
            }
        }
        root->v = var_name_list;
        root->t = var_decl->t;
    }
}

// Exp -> AddExp
void frontend::Analyzer::analysisExp(Exp *root, vector<ir::Instruction *> &buffer)
{
    ANALYSIS(addexp, AddExp, 0);
    COPY_EXP_NODE(addexp, root);
}

// AddExp -> MulExp { ('+' | '-') MulExp }
void frontend::Analyzer::analysisAddExp(AddExp *root, vector<ir::Instruction *> &buffer)
{
    ANALYSIS(mulexp_0, MulExp, 0);
    COPY_EXP_NODE(mulexp_0, root);
    for (int i = 1; i < root->children.size(); i = i + 2)
    {
        GET_CHILD_PTR(term, Term, i);
        ANALYSIS(mulexp_i, MulExp, i + 1);
        string op = term->token.value;
        string temp_result = "temp" + std::to_string(tmp_cnt++);
        if (mulexp_i->t == Type::Float && root->t == Type::IntLiteral)
        {
            buffer.push_back(new Instruction(Operand(std::to_string(float(std::stoi(root->v))), Type::FloatLiteral),
                                             Operand(),
                                             Operand(temp_result, Type::Float),
                                             Operator::fdef));
            buffer.push_back(new Instruction(Operand(temp_result, Type::Float),
                                             Operand(mulexp_i->v, Type::Float),
                                             Operand(temp_result, Type::Float),
                                             op == "+" ? Operator::fadd : Operator::fsub));
            root->v = temp_result;
            root->t = Type::Float;
        }
        else if (mulexp_i->t == Type::Float && root->t == Type::FloatLiteral)
        {
            buffer.push_back(new Instruction(Operand(root->v, Type::FloatLiteral),
                                             Operand(),
                                             Operand(temp_result, Type::Float),
                                             Operator::fdef));
            buffer.push_back(new Instruction(Operand(temp_result, Type::Float),
                                             Operand(mulexp_i->v, Type::Float),
                                             Operand(temp_result, Type::Float),
                                             op == "+" ? Operator::fadd : Operator::fsub));
            root->v = temp_result;
            root->t = Type::Float;
        }
        else if (mulexp_i->t == Type::Float && root->t == Type::Int)
        {
            buffer.push_back(new Instruction(Operand(root->v, Type::Int),
                                             Operand(),
                                             Operand(temp_result, Type::Float),
                                             Operator::cvt_i2f));
            buffer.push_back(new Instruction(Operand(temp_result, Type::Float),
                                             Operand(mulexp_i->v, Type::Float),
                                             Operand(temp_result, Type::Float),
                                             op == "+" ? Operator::fadd : Operator::fsub));
            root->v = temp_result;
            root->t = Type::Float;
        }
        else if (mulexp_i->t == Type::Float && root->t == Type::Float)
        {
            buffer.push_back(new Instruction(Operand(root->v, Type::Float),
                                             Operand(mulexp_i->v, Type::Float),
                                             Operand(temp_result, Type::Float),
                                             op == "+" ? Operator::fadd : Operator::fsub));
            root->v = temp_result;
        }
        else if (mulexp_i->t == Type::FloatLiteral && root->t == Type::IntLiteral)
        {
            if (op == "+")
                root->v = std::to_string(std::stoi(root->v) + std::stof(mulexp_i->v));
            else if (op == "-")
                root->v = std::to_string(std::stoi(root->v) - std::stof(mulexp_i->v));
            root->t = Type::FloatLiteral;
        }
        else if (mulexp_i->t == Type::FloatLiteral && root->t == Type::FloatLiteral)
        {
            if (op == "+")
                root->v = std::to_string(std::stof(root->v) + std::stof(mulexp_i->v));
            else if (op == "-")
                root->v = std::to_string(std::stof(root->v) - std::stof(mulexp_i->v));
        }
        else if (mulexp_i->t == Type::FloatLiteral && root->t == Type::Int)
        {
            string temp_float = "temp" + std::to_string(tmp_cnt++);
            buffer.push_back(new Instruction(Operand(root->v, Type::Int),
                                             Operand(),
                                             Operand(temp_result, Type::Float),
                                             Operator::cvt_i2f));
            buffer.push_back(new Instruction(Operand(mulexp_i->v, Type::FloatLiteral),
                                             Operand(),
                                             Operand(temp_float, Type::Float),
                                             Operator::fdef));
            buffer.push_back(new Instruction(Operand(temp_result, Type::Float),
                                             Operand(temp_float, Type::Float),
                                             Operand(temp_result, Type::Float),
                                             op == "+" ? Operator::fadd : Operator::fsub));
            root->v = temp_result;
            root->t = Type::Float;
        }
        else if (mulexp_i->t == Type::FloatLiteral && root->t == Type::Float)
        {
            string temp_float = "temp" + std::to_string(tmp_cnt++);
            buffer.push_back(new Instruction(Operand(mulexp_i->v, Type::FloatLiteral),
                                             Operand(),
                                             Operand(temp_float, Type::Float),
                                             Operator::fdef));
            buffer.push_back(new Instruction(Operand(root->v, Type::Float),
                                             Operand(temp_float, Type::Float),
                                             Operand(temp_result, Type::Float),
                                             op == "+" ? Operator::fadd : Operator::fsub));
            root->v = temp_result;
        }
        else if (mulexp_i->t == Type::IntLiteral && root->t == Type::IntLiteral)
        {
            if (op == "+")
                root->v = std::to_string(std::stoi(root->v) + std::stoi(mulexp_i->v));
            else if (op == "-")
                root->v = std::to_string(std::stoi(root->v) - std::stoi(mulexp_i->v));
        }
        else if (mulexp_i->t == Type::IntLiteral && root->t == Type::FloatLiteral)
        {
            if (op == "+")
                root->v = std::to_string(std::stof(root->v) + std::stoi(mulexp_i->v));
            else if (op == "-")
                root->v = std::to_string(std::stof(root->v) - std::stoi(mulexp_i->v));
            root->t = Type::FloatLiteral;
        }
        else if (mulexp_i->t == Type::IntLiteral && root->t == Type::Int)
        {
            string temp_int = "temp" + std::to_string(tmp_cnt++);
            buffer.push_back(new Instruction(Operand(mulexp_i->v, Type::IntLiteral),
                                             Operand(),
                                             Operand(temp_int, Type::Int),
                                             Operator::def));
            buffer.push_back(new Instruction(Operand(root->v, Type::Int),
                                             Operand(temp_int, Type::Int),
                                             Operand(temp_result, Type::Int),
                                             op == "+" ? Operator::add : Operator::sub));
            root->v = temp_result;
        }
        else if (mulexp_i->t == Type::IntLiteral && root->t == Type::Float)
        {
            string temp_float = "temp" + std::to_string(tmp_cnt++);
            buffer.push_back(new Instruction(Operand(std::to_string(float(std::stof(mulexp_i->v))), Type::FloatLiteral),
                                             Operand(),
                                             Operand(temp_float, Type::Float),
                                             Operator::fdef));
            buffer.push_back(new Instruction(Operand(root->v, Type::Float),
                                             Operand(temp_float, Type::Float),
                                             Operand(temp_result, Type::Float),
                                             op == "+" ? Operator::fadd : Operator::fsub));
            root->v = temp_result;
        }
        else if (mulexp_i->t == Type::Int && root->t == Type::FloatLiteral)
        {
            string temp_float = "temp" + std::to_string(tmp_cnt++);
            buffer.push_back(new Instruction(Operand(root->v, Type::FloatLiteral),
                                             Operand(),
                                             Operand(temp_result, Type::Float),
                                             Operator::fdef));
            buffer.push_back(new Instruction(Operand(mulexp_i->v, Type::Int),
                                             Operand(),
                                             Operand(temp_float, Type::Float),
                                             Operator::cvt_i2f));
            buffer.push_back(new Instruction(Operand(temp_result, Type::Float),
                                             Operand(temp_float, Type::Float),
                                             Operand(temp_result, Type::Float),
                                             op == "+" ? Operator::fadd : Operator::fsub));
            root->v = temp_result;
            root->t = Type::Float;
        }
        else if (mulexp_i->t == Type::Int && root->t == Type::IntLiteral)
        {
            buffer.push_back(new Instruction(Operand(root->v, Type::IntLiteral),
                                             Operand(),
                                             Operand(temp_result, Type::Int),
                                             Operator::def));
            buffer.push_back(new Instruction(Operand(temp_result, Type::Int),
                                             Operand(mulexp_i->v, Type::Int),
                                             Operand(temp_result, Type::Int),
                                             op == "+" ? Operator::add : Operator::sub));
            root->v = temp_result;
            root->t = Type::Int;
        }
        else if (mulexp_i->t == Type::Int && root->t == Type::Int)
        {
            buffer.push_back(new Instruction(Operand(root->v, Type::Int),
                                             Operand(mulexp_i->v, Type::Int),
                                             Operand(temp_result, Type::Int),
                                             op == "+" ? Operator::add : Operator::sub));
            root->v = temp_result;
        }
        else if (mulexp_i->t == Type::Int && root->t == Type::Float)
        {
            string temp_Float = "temp" + std::to_string(tmp_cnt++);
            buffer.push_back(new Instruction(Operand(mulexp_i->v, Type::Int),
                                             Operand(),
                                             Operand(temp_Float, Type::Float),
                                             Operator::cvt_i2f));
            buffer.push_back(new Instruction(Operand(root->v, Type::Float),
                                             Operand(temp_Float, Type::Float),
                                             Operand(temp_result, Type::Float),
                                             op == "+" ? Operator::fadd : Operator::fsub));
            root->v = temp_result;
        }
    }
    // std::cout << "analysisaddExp end" << std::endl;
}

// MulExp -> UnaryExp { ('*' | '/' | '%') UnaryExp }
void frontend::Analyzer::analysisMulExp(MulExp *root, vector<ir::Instruction *> &buffer)
{
    ANALYSIS(unaryexp_0, UnaryExp, 0);
    COPY_EXP_NODE(unaryexp_0, root);
    for (int i = 1; i < root->children.size(); i = i + 2)
    {
        GET_CHILD_PTR(term, Term, i);
        ANALYSIS(unaryexp_i, UnaryExp, i + 1);
        string op = term->token.value;
        string temp_result = "temp" + std::to_string(tmp_cnt++);
        if (unaryexp_i->t == Type::Float && root->t == Type::IntLiteral)
        {
            buffer.push_back(new Instruction(Operand(std::to_string(float(std::stoi(root->v))), Type::FloatLiteral),
                                             Operand(),
                                             Operand(temp_result, Type::Float),
                                             Operator::fdef));
            buffer.push_back(new Instruction(Operand(temp_result, Type::Float),
                                             Operand(unaryexp_i->v, Type::Float),
                                             Operand(temp_result, Type::Float),
                                             op == "*" ? Operator::fmul : Operator::fdiv));
            root->v = temp_result;
            root->t = Type::Float;
        }
        else if (unaryexp_i->t == Type::Float && root->t == Type::FloatLiteral)
        {
            buffer.push_back(new Instruction(Operand(root->v, Type::FloatLiteral),
                                             Operand(),
                                             Operand(temp_result, Type::Float),
                                             Operator::fdef));
            buffer.push_back(new Instruction(Operand(temp_result, Type::Float),
                                             Operand(unaryexp_i->v, Type::Float),
                                             Operand(temp_result, Type::Float),
                                             op == "*" ? Operator::fmul : Operator::fdiv));
            root->v = temp_result;
            root->t = Type::Float;
        }
        else if (unaryexp_i->t == Type::Float && root->t == Type::Int)
        {
            buffer.push_back(new Instruction(Operand(root->v, Type::Int),
                                             Operand(),
                                             Operand(temp_result, Type::Float),
                                             Operator::cvt_i2f));
            buffer.push_back(new Instruction(Operand(temp_result, Type::Float),
                                             Operand(unaryexp_i->v, Type::Float),
                                             Operand(temp_result, Type::Float),
                                             op == "*" ? Operator::fmul : Operator::fdiv));
            root->v = temp_result;
            root->t = Type::Float;
        }
        else if (unaryexp_i->t == Type::Float && root->t == Type::Float)
        {
            buffer.push_back(new Instruction(Operand(root->v, Type::Float),
                                             Operand(unaryexp_i->v, Type::Float),
                                             Operand(temp_result, Type::Float),
                                             op == "*" ? Operator::fmul : Operator::fdiv));
            root->v = temp_result;
        }
        else if (unaryexp_i->t == Type::FloatLiteral && root->t == Type::IntLiteral)
        {
            if (op == "*")
                root->v = std::to_string(std::stoi(root->v) * std::stof(unaryexp_i->v));
            else if (op == "/")
                root->v = std::to_string(std::stoi(root->v) / std::stof(unaryexp_i->v));
            root->t = Type::FloatLiteral;
        }
        else if (unaryexp_i->t == Type::FloatLiteral && root->t == Type::FloatLiteral)
        {
            if (op == "*")
                root->v = std::to_string(std::stof(root->v) * std::stof(unaryexp_i->v));
            else if (op == "/")
                root->v = std::to_string(std::stof(root->v) / std::stof(unaryexp_i->v));
        }
        else if (unaryexp_i->t == Type::FloatLiteral && root->t == Type::Int)
        {
            string temp_float = "temp" + std::to_string(tmp_cnt++);
            buffer.push_back(new Instruction(Operand(root->v, Type::Int),
                                             Operand(),
                                             Operand(temp_result, Type::Float),
                                             Operator::cvt_i2f));
            buffer.push_back(new Instruction(Operand(unaryexp_i->v, Type::FloatLiteral),
                                             Operand(),
                                             Operand(temp_float, Type::Float),
                                             Operator::fdef));
            buffer.push_back(new Instruction(Operand(temp_result, Type::Float),
                                             Operand(temp_float, Type::Float),
                                             Operand(temp_result, Type::Float),
                                             op == "*" ? Operator::fmul : Operator::fdiv));
            root->v = temp_result;
            root->t = Type::Float;
        }
        else if (unaryexp_i->t == Type::FloatLiteral && root->t == Type::Float)
        {
            string temp_float = "temp" + std::to_string(tmp_cnt++);
            buffer.push_back(new Instruction(Operand(unaryexp_i->v, Type::FloatLiteral),
                                             Operand(),
                                             Operand(temp_float, Type::Float),
                                             Operator::fdef));
            buffer.push_back(new Instruction(Operand(root->v, Type::Float),
                                             Operand(temp_float, Type::Float),
                                             Operand(temp_result, Type::Float),
                                             op == "*" ? Operator::fmul : Operator::fdiv));
            root->v = temp_result;
        }
        else if (unaryexp_i->t == Type::IntLiteral && root->t == Type::IntLiteral)
        {
            if (op == "*")
                root->v = std::to_string(std::stoi(root->v) * std::stoi(unaryexp_i->v));
            else if (op == "/")
                root->v = std::to_string(std::stoi(root->v) / std::stoi(unaryexp_i->v));
            else if (op == "%")
                root->v = std::to_string(std::stoi(root->v) % std::stoi(unaryexp_i->v));
        }
        else if (unaryexp_i->t == Type::IntLiteral && root->t == Type::FloatLiteral)
        {
            if (op == "*")
                root->v = std::to_string(std::stof(root->v) * std::stoi(unaryexp_i->v));
            else if (op == "/")
                root->v = std::to_string(std::stof(root->v) / std::stoi(unaryexp_i->v));
            root->t = Type::FloatLiteral;
        }
        else if (unaryexp_i->t == Type::IntLiteral && root->t == Type::Int)
        {
            string temp_int = "temp" + std::to_string(tmp_cnt++);
            buffer.push_back(new Instruction(Operand(unaryexp_i->v, Type::IntLiteral),
                                             Operand(),
                                             Operand(temp_int, Type::Int),
                                             Operator::def));
            buffer.push_back(new Instruction(Operand(root->v, Type::Int),
                                             Operand(temp_int, Type::Int),
                                             Operand(temp_result, Type::Int),
                                             op == "*" ? Operator::mul : op == "/" ? Operator::div
                                                                                   : Operator::mod));
            root->v = temp_result;
        }
        else if (unaryexp_i->t == Type::IntLiteral && root->t == Type::Float)
        {
            string temp_float = "temp" + std::to_string(tmp_cnt++);
            buffer.push_back(new Instruction(Operand(std::to_string(float(std::stof(unaryexp_i->v))), Type::FloatLiteral),
                                             Operand(),
                                             Operand(temp_float, Type::Float),
                                             Operator::fdef));
            buffer.push_back(new Instruction(Operand(root->v, Type::Float),
                                             Operand(temp_float, Type::Float),
                                             Operand(temp_result, Type::Float),
                                             op == "*" ? Operator::fmul : Operator::fdiv));
            root->v = temp_result;
        }
        else if (unaryexp_i->t == Type::Int && root->t == Type::FloatLiteral)
        {
            string temp_float = "temp" + std::to_string(tmp_cnt++);
            buffer.push_back(new Instruction(Operand(root->v, Type::FloatLiteral),
                                             Operand(),
                                             Operand(temp_result, Type::Float),
                                             Operator::fdef));
            buffer.push_back(new Instruction(Operand(unaryexp_i->v, Type::Int),
                                             Operand(),
                                             Operand(temp_float, Type::Float),
                                             Operator::cvt_i2f));
            buffer.push_back(new Instruction(Operand(temp_result, Type::Float),
                                             Operand(temp_float, Type::Float),
                                             Operand(temp_result, Type::Float),
                                             op == "*" ? Operator::fmul : Operator::fdiv));
            root->v = temp_result;
            root->t = Type::Float;
        }
        else if (unaryexp_i->t == Type::Int && root->t == Type::IntLiteral)
        {
            buffer.push_back(new Instruction(Operand(root->v, Type::IntLiteral),
                                             Operand(),
                                             Operand(temp_result, Type::Int),
                                             Operator::def));
            buffer.push_back(new Instruction(Operand(temp_result, Type::Int),
                                             Operand(unaryexp_i->v, Type::Int),
                                             Operand(temp_result, Type::Int),
                                             op == "*" ? Operator::mul : op == "/" ? Operator::div
                                                                                   : Operator::mod));
            root->v = temp_result;
            root->t = Type::Int;
        }
        else if (unaryexp_i->t == Type::Int && root->t == Type::Int)
        {
            buffer.push_back(new Instruction(Operand(root->v, Type::Int),
                                             Operand(unaryexp_i->v, Type::Int),
                                             Operand(temp_result, Type::Int),
                                             op == "*" ? Operator::mul : op == "/" ? Operator::div
                                                                                   : Operator::mod));
            root->v = temp_result;
        }
        else if (unaryexp_i->t == Type::Int && root->t == Type::Float)
        {
            string temp_Float = "temp" + std::to_string(tmp_cnt++);
            buffer.push_back(new Instruction(Operand(unaryexp_i->v, Type::Int),
                                             Operand(),
                                             Operand(temp_Float, Type::Float),
                                             Operator::cvt_i2f));
            buffer.push_back(new Instruction(Operand(root->v, Type::Float),
                                             Operand(temp_Float, Type::Float),
                                             Operand(temp_result, Type::Float),
                                             op == "*" ? Operator::fmul : Operator::fdiv));
            root->v = temp_result;
        }
    }
    // std::cout << "mulexp end" << std::endl;
}

// UnaryExp -> PrimaryExp | Ident '(' [FuncRParams] ')' | UnaryOp UnaryExp
void frontend::Analyzer::analysisUnaryExp(UnaryExp *root, vector<ir::Instruction *> &buffer)
{
    if (root->children[0]->type == NodeType::PRIMARYEXP)
    {
        ANALYSIS(primaryexp, PrimaryExp, 0);
        COPY_EXP_NODE(primaryexp, root);
    }
    else if (root->children[0]->type == NodeType::UNARYOP)
    {
        ANALYSIS(unaryexp, UnaryExp, 1);
        UnaryOp *unaryop = dynamic_cast<UnaryOp *>(root->children[0]);
        Term *term = dynamic_cast<Term *>(unaryop->children[0]);
        //+a
        if (term->token.type == TokenType::PLUS)
        {
            COPY_EXP_NODE(unaryexp, root);
        }
        //-a
        else if (term->token.type == TokenType::MINU)
        {
            if (unaryexp->t == Type::IntLiteral)
            {
                root->v = std::to_string(-std::stoi(unaryexp->v));
                root->t = unaryexp->t;
            }
            else if (unaryexp->t == Type::FloatLiteral)
            {
                root->v = std::to_string(-std::stof(unaryexp->v));
                root->t = unaryexp->t;
            }
            else if (unaryexp->t == Type::Int)
            {
                string temp_0 = "temp" + std::to_string(tmp_cnt++);
                buffer.push_back(new Instruction(Operand("0", Type::IntLiteral),
                                                 Operand(),
                                                 Operand(temp_0, Type::Int),
                                                 Operator::def));
                string temp_neg_result = "temp" + std::to_string(tmp_cnt++);
                buffer.push_back(new Instruction(Operand(temp_0, Type::Int),
                                                 Operand(unaryexp->v, Type::Int),
                                                 Operand(temp_neg_result, Type::Int),
                                                 Operator::sub));
                root->v = temp_neg_result;
                root->t = Type::Int;
            }
            else if (unaryexp->t == Type::Float)
            {
                string temp_0 = "temp" + std::to_string(tmp_cnt++);
                buffer.push_back(new Instruction(Operand("0.0", Type::FloatLiteral),
                                                 Operand(),
                                                 Operand(temp_0, Type::Float),
                                                 Operator::fdef));
                string temp_neg_result = "temp" + std::to_string(tmp_cnt++);
                buffer.push_back(new Instruction(Operand(temp_0, Type::Float),
                                                 Operand(unaryexp->v, Type::Float),
                                                 Operand(temp_neg_result, Type::Float),
                                                 Operator::fsub));
                root->v = temp_neg_result;
                root->t = Type::Float;
            }
        }
        //! a
        else if (term->token.type == TokenType::NOT)
        {
            if (unaryexp->t == Type::IntLiteral)
            {
                root->v = unaryexp->v = std::to_string(!std::stoi(unaryexp->v));
                root->t = Type::IntLiteral;
            }
            else if (unaryexp->t == Type::FloatLiteral)
            {
                root->v = unaryexp->v = std::to_string(!std::stof(unaryexp->v));
                root->t = Type::FloatLiteral;
            }
            else if (unaryexp->t == Type::Int)
            {
                string temp_not_result = "temp" + std::to_string(tmp_cnt++);
                buffer.push_back(new Instruction(Operand(unaryexp->v, Type::Int),
                                                 Operand(),
                                                 Operand(temp_not_result, Type::Int),
                                                 Operator::_not));
                root->v = temp_not_result;
                root->t = Type::Int;
            }
            else if (unaryexp->t == Type::Float)
            {
                string temp_not_result = "temp" + std::to_string(tmp_cnt++);
                buffer.push_back(new Instruction(Operand(unaryexp->v, Type::Float),
                                                 Operand(),
                                                 Operand(temp_not_result, Type::Float),
                                                 Operator::_not));
                root->v = temp_not_result;
                root->t = Type::Float;
            }
        }
    }
    else
    {
        GET_CHILD_PTR(ident, Term, 0);
        string func_name = ident->token.value;
        Function *call_func = symbol_table.functions[func_name];
        string temp_func_result = "temp" + std::to_string(tmp_cnt++);
        vector<ir::Operand> func_rparams;
        vector<Operand> func_fparams = call_func->ParameterList;
        if (root->children.size() == 4)
        {
            GET_CHILD_PTR(funcrparams, FuncRParams, 2);
            analysisFuncRParams(funcrparams, func_rparams, buffer);
            for (int i = 0; i < func_fparams.size(); i++)
            {
                if (func_fparams[i].type == Type::Int && func_rparams[i].type != Type::Int)
                {
                    string temp_int = "temp" + std::to_string(tmp_cnt++);
                    if (func_rparams[i].type == Type::IntLiteral)
                        buffer.push_back(new ir::Instruction(Operand(func_rparams[i].name, Type::IntLiteral),
                                                             Operand(),
                                                             Operand(temp_int, Type::Int),
                                                             Operator::def));
                    else if (func_rparams[i].type == Type::FloatLiteral)
                        buffer.push_back(new ir::Instruction(Operand(std::to_string(int(std::stof(func_rparams[i].name))), Type::IntLiteral),
                                                             Operand(),
                                                             Operand(temp_int, Type::Int),
                                                             Operator::def));
                    else if (func_rparams[i].type == Type::Float)
                        buffer.push_back(new ir::Instruction(Operand(func_rparams[i].name, Type::Float),
                                                             Operand(),
                                                             Operand(temp_int, Type::Int),
                                                             Operator::cvt_f2i));
                    func_rparams[i].name = temp_int;
                    func_rparams[i].type = Type::Int;
                }
                else if (func_fparams[i].type == Type::Float && func_rparams[i].type != Type::Float)
                {
                    string temp_float = "temp" + std::to_string(tmp_cnt++);
                    if (func_rparams[i].type == Type::IntLiteral)
                        buffer.push_back(new ir::Instruction(Operand(std::to_string(float(std::stoi(func_rparams[i].name))), Type::FloatLiteral),
                                                             Operand(),
                                                             Operand(temp_float, Type::Float),
                                                             Operator::fdef));
                    else if (func_rparams[i].type == Type::FloatLiteral)
                        buffer.push_back(new ir::Instruction(Operand(func_rparams[i].name, Type::FloatLiteral),
                                                             Operand(),
                                                             Operand(temp_float, Type::Float),
                                                             Operator::fdef));
                    else if (func_rparams[i].type == Type::Int)
                        buffer.push_back(new ir::Instruction(Operand(func_rparams[i].name, Type::Int),
                                                             Operand(),
                                                             Operand(temp_float, Type::Float),
                                                             Operator::cvt_i2f));
                    func_rparams[i].name = temp_float;
                    func_rparams[i].type = Type::Float;
                }
            }
        }
        buffer.push_back(new ir::CallInst(Operand(call_func->name, call_func->returnType),
                                          func_rparams,
                                          Operand(temp_func_result, call_func->returnType)));
        root->v = temp_func_result;
        root->t = call_func->returnType;
    }
}

// UnaryOp -> '+' | '-' | '!'
void frontend::Analyzer::analysisUnaryOp(UnaryOp *root, vector<ir::Instruction *> &buffer)
{
    return;
}

// PrimaryExp -> '(' Exp ')' | LVal | Number
void frontend::Analyzer::analysisPrimaryExp(PrimaryExp *root, vector<ir::Instruction *> &buffer)
{
    if (root->children[0]->type == NodeType::LVAL)
    {
        ANALYSIS(lval, LVal, 0);
        COPY_EXP_NODE(lval, root);
    }
    else if (root->children[0]->type == NodeType::NUMBER)
    {
        ANALYSIS(number, Number, 0);
        COPY_EXP_NODE(number, root);
    }
    else
    {
        ANALYSIS(exp, Exp, 1);
        COPY_EXP_NODE(exp, root);
    }
}

// LVal -> Ident {'[' Exp ']'}
void frontend::Analyzer::analysisLVal(LVal *root, vector<ir::Instruction *> &buffer)
{
    GET_CHILD_PTR(term, Term, 0);
    std::string var_name = term->token.value;
    STE oprand_ste = symbol_table.get_ste(var_name);
    string var_name_after_rename = symbol_table.get_scoped_name(var_name);
    // a
    if (root->children.size() == 1)
    {
        root->v = (oprand_ste.operand.type == Type::IntLiteral || oprand_ste.operand.type == Type::FloatLiteral) ? oprand_ste.literal_value
                                                                                                                 : var_name_after_rename;
        root->t = oprand_ste.operand.type;
    }
    else if (root->children.size() == 4)
    {
        ANALYSIS(exp, Exp, 2);
        string temp_var_name = "temp" + std::to_string(tmp_cnt++);
        // a[1]
        if (oprand_ste.dimension.size() == 1)
        {
            Type temp_type = oprand_ste.operand.type == Type::IntPtr ? Type::Int : Type::Float;
            buffer.push_back(new Instruction(Operand(var_name_after_rename, oprand_ste.operand.type),
                                             Operand(exp->v, exp->t),
                                             Operand(temp_var_name, temp_type),
                                             Operator::load));
            root->v = temp_var_name;
            root->t = temp_type;
        }
        // a[][3]
        else
        {
            Type temp_type = oprand_ste.operand.type;
            // a[1]
            if (exp->t == Type::IntLiteral)
            {
                string offset = std::to_string(std::stoi(exp->v) * oprand_ste.dimension[1]);
                buffer.push_back(new Instruction(Operand(var_name_after_rename, oprand_ste.operand.type),
                                                 Operand(offset, Type::IntLiteral),
                                                 Operand(temp_var_name, temp_type),
                                                 Operator::getptr));
            }
            // a[2]
            else
            {
                string temp_offset = "temp" + std::to_string(tmp_cnt++);
                string temp_mul_name = "temp" + std::to_string(tmp_cnt++);
                buffer.push_back(new Instruction(Operand(std::to_string(oprand_ste.dimension[1]), Type::IntLiteral),
                                                 Operand(),
                                                 Operand(temp_mul_name, Type::Int),
                                                 Operator::def));
                buffer.push_back(new Instruction(Operand(exp->v, exp->t),
                                                 Operand(temp_var_name, Type::Int),
                                                 Operand(temp_offset, Type::Int),
                                                 Operator::mul));
                buffer.push_back(new Instruction(Operand(var_name_after_rename, oprand_ste.operand.type),
                                                 Operand(temp_offset, Type::Int),
                                                 Operand(temp_var_name, temp_type),
                                                 Operator::getptr));
            }
            root->v = temp_var_name;
            root->t = temp_type;
        }
    }
    // a[1][2]
    else if (root->children.size() == 7)
    {
        ANALYSIS(exp_0, Exp, 2);
        ANALYSIS(exp_1, Exp, 5);
        string temp_dim_0 = "temp" + std::to_string(tmp_cnt++);
        string temp_dim_1 = "temp" + std::to_string(tmp_cnt++);
        buffer.push_back(new Instruction(Operand(exp_0->v, exp_0->t),
                                         Operand(),
                                         Operand(temp_dim_0, Type::Int),
                                         Operator::def));
        buffer.push_back(new Instruction(Operand(exp_1->v, exp_1->t),
                                         Operand(),
                                         Operand(temp_dim_1, Type::Int),
                                         Operator::def));
        string temp_col = "temp" + std::to_string(tmp_cnt++);
        buffer.push_back(new Instruction(Operand(std::to_string(oprand_ste.dimension[1]), Type::IntLiteral),
                                         Operand(),
                                         Operand(temp_col, Type::Int),
                                         Operator::def));
        string temp_line_offset = "temp" + std::to_string(tmp_cnt++);
        buffer.push_back(new Instruction(Operand(temp_dim_0, Type::Int),
                                         Operand(temp_col, Type::Int),
                                         Operand(temp_line_offset, Type::Int),
                                         Operator::mul));
        string temp_offset = "temp" + std::to_string(tmp_cnt++);
        buffer.push_back(new Instruction(Operand(temp_line_offset, Type::Int),
                                         Operand(temp_dim_1, Type::Int),
                                         Operand(temp_offset, Type::Int),
                                         Operator::add));
        string temp_var_name = "temp" + std::to_string(tmp_cnt++);
        Type temp_type = oprand_ste.operand.type == Type::IntPtr ? Type::Int : Type::Float;
        buffer.push_back(new Instruction(Operand(var_name_after_rename, oprand_ste.operand.type),
                                         Operand(temp_offset, Type::Int),
                                         Operand(temp_var_name, temp_type),
                                         Operator::load));
        root->v = temp_var_name;
        root->t = temp_type;
    }
}

// Number -> IntConst | floatConst
void frontend::Analyzer::analysisNumber(Number *root, vector<ir::Instruction *> &buffer)
{
    GET_CHILD_PTR(term, Term, 0);
    if (term->token.type == TokenType::INTLTR)
    {
        root->t = Type::IntLiteral;
        std::string str = dynamic_cast<Term *>(root->children[0])->token.value;
        if (str[0] == '0' && (str[1] == 'x' || str[1] == 'X'))
            root->v = std::to_string(std::stoi(str, nullptr, 16));
        else if (str[0] == '0' && (str[1] == 'b' || str[1] == 'B'))
            root->v = std::to_string(std::stoi(str, nullptr, 2));
        else if (str[0] == '0')
            root->v = std::to_string(std::stoi(str, nullptr, 8));
        else
            root->v = str;
    }
    else if (term->token.type == TokenType::FLOATLTR)
    {
        root->t = Type::FloatLiteral;
        root->v = term->token.value;
    }
    else
    {
        assert(0 && "analysisNumber error");
    }
}

// FuncDef -> FuncType Ident '(' [FuncFParams] ')' Block
void frontend::Analyzer::analysisFuncDef(FuncDef *root, ir::Function &func)
{
    FuncType *func_type = dynamic_cast<FuncType *>(root->children[0]);
    GET_CHILD_PTR(ident, Term, 1);
    func.returnType = dynamic_cast<Term *>(func_type->children[0])->token.type == TokenType::VOIDTK ? Type::null : (dynamic_cast<Term *>(func_type->children[0])->token.type == TokenType::INTTK ? Type::Int : Type::Float);
    func.name = ident->token.value;
    symbol_table.add_scope();
    if (root->children.size() == 6)
    {
        GET_CHILD_PTR(func_fparams, FuncFParams, 3);
        analysisFuncFParams(func_fparams, func.ParameterList, func.InstVec);
    }
    if (func.name == "main")
    {
        string temp_func_name = "temp" + std::to_string(tmp_cnt++);
        func.addInst(new ir::CallInst(Operand("global", Type::null),
                                      Operand(temp_func_name, Type::null)));
    }
    symbol_table.functions[func.name] = &func;
    GET_CHILD_PTR(block, Block, root->children.size() - 1);
    analysisBlock(block, func.InstVec);
    symbol_table.exit_scope();
}

// FuncType -> 'void' | 'int' | 'float'
void frontend::Analyzer::analysisFuncType(FuncType *root)
{
    return;
}

// FuncFParam -> BType Ident ['[' ']' { '[' Exp ']' }]
void frontend::Analyzer::analysisFuncFParam(FuncFParam *root, Operand &oprand_ste, vector<ir::Instruction *> &buffer)
{
    GET_CHILD_PTR(btype, BType, 0);
    Term *bt = dynamic_cast<Term *>(btype->children[0]);
    GET_CHILD_PTR(ident, Term, 1);
    Type type = bt->token.type == TokenType::INTTK ? Type::Int : Type::Float;
    vector<int> dimension(1, -1);
    if (root->children.size() > 2)
    {
        type = type == Type::Int ? Type::IntPtr : Type::FloatPtr;
        if (root->children.size() == 7)
        {
            type = type == Type::Int ? Type::IntPtr : Type::FloatPtr;
            ANALYSIS(exp, Exp, 5);
            dimension.push_back(std::stoi(exp->v));
        }
    }
    string var_name = ident->token.value;
    Operand operand = Operand(var_name, type);
    oprand_ste = operand;
    symbol_table.scope_stack.back().table[var_name] = {operand, dimension};
    string var_after_rename = symbol_table.get_scoped_name(var_name);
    oprand_ste.name = var_after_rename;
}

// FuncFParams -> FuncFParam { ',' FuncFParam }
void frontend::Analyzer::analysisFuncFParams(FuncFParams *root, vector<Operand> &operand_list, vector<ir::Instruction *> &buffer)
{
    for (int i = 0; i < root->children.size(); i = i + 2)
    {
        Operand op_funcfparm;
        analysisFuncFParam(dynamic_cast<FuncFParam *>(root->children[i]), op_funcfparm, buffer);
        operand_list.push_back(op_funcfparm);
    }
}

// FuncRParams -> Exp { ',' Exp }
void frontend::Analyzer::analysisFuncRParams(FuncRParams *root, vector<Operand> &operand_list, vector<ir::Instruction *> &buffer)
{
    for (int i = 0; i < root->children.size(); i = i + 2)
    {
        ANALYSIS(exp, Exp, i);
        operand_list.push_back(Operand(exp->v, exp->t));
    }
}

// Block -> '{' { BlockItem } '}'
void frontend::Analyzer::analysisBlock(Block *root, vector<ir::Instruction *> &buffer)
{
    // printf("analysisBlock\n/");
    if (root->parent->type != NodeType::FUNCDEF)
        symbol_table.add_scope();
    for (int i = 1; i < root->children.size() - 1; i++)
    {
        ANALYSIS(block_item, BlockItem, i);
    }
    if (root->parent->type != NodeType::FUNCDEF)
        symbol_table.exit_scope();
}

// BlockItem -> Decl | Stmt
void frontend::Analyzer::analysisBlockItem(BlockItem *root, vector<ir::Instruction *> &buffer)
{
    if (root->children[0]->type == NodeType::DECL)
    {
        ANALYSIS(decl, Decl, 0);
    }
    else
    {
        ANALYSIS(stmt, Stmt, 0);
    }
}

// Stmt -> LVal '=' Exp ';' | Block | 'if' '(' Cond ')' Stmt [ 'else' Stmt ] | 'while' '(' Cond ')' Stmt | 'break' ';' | 'continue' ';' | 'return' [Exp] ';' | [Exp] ';'
void frontend::Analyzer::analysisStmt(Stmt *root, vector<ir::Instruction *> &buffer)
{
    if (LVal *lval = dynamic_cast<LVal *>(root->children[0]))
    {
        ANALYSIS(exp, Exp, 2);
        string var_name = dynamic_cast<Term *>(dynamic_cast<LVal *>(root->children[0])->children[0])->token.value;
        STE ident_ste = symbol_table.get_ste(var_name);
        string ident_after_rename = symbol_table.get_scoped_name(var_name);
        // a=...;
        if (root->children[0]->children.size() == 1)
        {
            if (ident_ste.operand.type == Type::Int)
            {
                if (exp->t == Type::IntLiteral)
                    buffer.push_back(new Instruction(Operand(exp->v, Type::IntLiteral),
                                                     Operand(),
                                                     Operand(ident_after_rename, Type::Int),
                                                     Operator::mov));
                else if (exp->t == Type::FloatLiteral)
                    buffer.push_back(new Instruction(Operand(std::to_string(int(std::stof(exp->v))), Type::IntLiteral),
                                                     Operand(),
                                                     Operand(ident_after_rename, Type::Int),
                                                     Operator::mov));
                else if (exp->t == Type::Int)
                    buffer.push_back(new Instruction(Operand(exp->v, Type::Int),
                                                     Operand(),
                                                     Operand(ident_after_rename, Type::Int),
                                                     Operator::mov));
                else if (exp->t == Type::Float)
                    buffer.push_back(new Instruction(Operand(exp->v, Type::Int),
                                                     Operand(),
                                                     Operand(ident_after_rename, Type::Int),
                                                     Operator::cvt_f2i));
            }
            else if (ident_ste.operand.type == Type::Float)
            {
                if (exp->t == Type::IntLiteral)
                    buffer.push_back(new Instruction(Operand(std::to_string(float(std::stoi(exp->v))), Type::FloatLiteral),
                                                     Operand(),
                                                     Operand(ident_after_rename, Type::Float),
                                                     Operator::fmov));
                else if (exp->t == Type::FloatLiteral)
                    buffer.push_back(new Instruction(Operand(exp->v, Type::FloatLiteral),
                                                     Operand(),
                                                     Operand(ident_after_rename, Type::Float),
                                                     Operator::fmov));
                else if (exp->t == Type::Int)
                    buffer.push_back(new Instruction(Operand(exp->v, Type::Int),
                                                     Operand(),
                                                     Operand(ident_after_rename, Type::Float),
                                                     Operator::cvt_f2i));
                else if (exp->t == Type::Float)
                    buffer.push_back(new Instruction(Operand(exp->v, Type::Float),
                                                     Operand(),
                                                     Operand(ident_after_rename, Type::Float),
                                                     Operator::fmov));
            }
        }
        // a[...]=...;
        else if (root->children[0]->children.size() == 4)
        {
            Exp *index_exp = dynamic_cast<Exp *>(root->children[0]->children[2]);
            analysisExp(index_exp, buffer);
            if (ident_ste.operand.type == Type::IntPtr)
            {
                if (exp->t == Type::IntLiteral || exp->t == Type::Int)
                    buffer.push_back(new Instruction(Operand(ident_after_rename, Type::IntPtr),
                                                     Operand(index_exp->v, index_exp->t),
                                                     Operand(exp->v, exp->t),
                                                     Operator::store));
                else if (exp->t == Type::FloatLiteral)
                    buffer.push_back(new Instruction(Operand(ident_after_rename, Type::IntPtr),
                                                     Operand(index_exp->v, index_exp->t),
                                                     Operand(std::to_string(int(std::stof(exp->v))), Type::IntLiteral),
                                                     Operator::store));
                else if (exp->t == Type::Float)
                {
                    string temp_int = "temp" + std::to_string(tmp_cnt++);
                    buffer.push_back(new Instruction(Operand(exp->v, Type::Float),
                                                     Operand(),
                                                     Operand(temp_int, Type::Int),
                                                     Operator::cvt_f2i));
                    buffer.push_back(new Instruction(Operand(ident_after_rename, Type::IntPtr),
                                                     Operand(index_exp->v, index_exp->t),
                                                     Operand(temp_int, Type::Int),
                                                     Operator::store));
                }
            }
            else if (ident_ste.operand.type == Type::FloatPtr)
            {
                if (exp->t == Type::IntLiteral)
                    buffer.push_back(new Instruction(Operand(ident_after_rename, Type::FloatPtr),
                                                     Operand(index_exp->v, index_exp->t),
                                                     Operand(std::to_string(float(std::stoi(exp->v))), Type::FloatLiteral),
                                                     Operator::store));
                else if (exp->t == Type::Int)
                {
                    string temp_float = "temp" + std::to_string(tmp_cnt++);
                    buffer.push_back(new Instruction(Operand(exp->v, Type::Int),
                                                     Operand(),
                                                     Operand(temp_float, Type::Float),
                                                     Operator::cvt_i2f));
                    buffer.push_back(new Instruction(Operand(ident_after_rename, Type::FloatPtr),
                                                     Operand(index_exp->v, index_exp->t),
                                                     Operand(temp_float, Type::Float),
                                                     Operator::store));
                }
                else if (exp->t == Type::FloatLiteral || exp->t == Type::Float)
                    buffer.push_back(new Instruction(Operand(ident_after_rename, Type::FloatPtr),
                                                     Operand(index_exp->v, index_exp->t),
                                                     Operand(exp->v, exp->t),
                                                     Operator::store));
            }
        }
        // a[...][...]=...;
        else if (root->children[0]->children.size() == 7)
        {
            Exp *index_exp_1 = dynamic_cast<Exp *>(root->children[0]->children[2]);
            Exp *index_exp_2 = dynamic_cast<Exp *>(root->children[0]->children[5]);
            analysisExp(index_exp_1, buffer);
            analysisExp(index_exp_2, buffer);
            string temp_dim_0 = "temp" + std::to_string(tmp_cnt++);
            string temp_dim_1 = "temp" + std::to_string(tmp_cnt++);
            buffer.push_back(new Instruction(Operand(index_exp_1->v, index_exp_1->t),
                                             Operand(),
                                             Operand(temp_dim_0, Type::Int),
                                             Operator::def));
            buffer.push_back(new Instruction(Operand(index_exp_2->v, index_exp_2->t),
                                             Operand(),
                                             Operand(temp_dim_1, Type::Int),
                                             Operator::def));
            string temp_col = "temp" + std::to_string(tmp_cnt++);
            buffer.push_back(new Instruction(Operand(std::to_string(ident_ste.dimension[1]), Type::IntLiteral),
                                             Operand(),
                                             Operand(temp_col, Type::Int),
                                             Operator::def));
            string temp_line_offset = "temp" + std::to_string(tmp_cnt++);
            buffer.push_back(new Instruction(Operand(temp_dim_0, Type::Int),
                                             Operand(temp_col, Type::Int),
                                             Operand(temp_line_offset, Type::Int),
                                             Operator::mul));
            string temp_offset = "temp" + std::to_string(tmp_cnt++);
            buffer.push_back(new Instruction(Operand(temp_line_offset, Type::Int),
                                             Operand(temp_dim_1, Type::Int),
                                             Operand(temp_offset, Type::Int),
                                             Operator::add));
            if (ident_ste.operand.type == Type::IntPtr)
            {
                if (exp->t == Type::IntLiteral || exp->t == Type::Int)
                    buffer.push_back(new Instruction(Operand(ident_after_rename, Type::IntPtr),
                                                     Operand(temp_offset, Type::Int),
                                                     Operand(exp->v, exp->t),
                                                     Operator::store));
                else if (exp->t == Type::FloatLiteral)
                    buffer.push_back(new Instruction(Operand(ident_after_rename, Type::IntPtr),
                                                     Operand(temp_offset, Type::Int),
                                                     Operand(std::to_string(int(std::stof(exp->v))), Type::IntLiteral),
                                                     Operator::store));
                else if (exp->t == Type::Float)
                {
                    string temp_int = "temp" + std::to_string(tmp_cnt++);
                    buffer.push_back(new Instruction(Operand(exp->v, Type::Float),
                                                     Operand(),
                                                     Operand(temp_int, Type::Int),
                                                     Operator::cvt_f2i));
                    buffer.push_back(new Instruction(Operand(ident_after_rename, Type::IntPtr),
                                                     Operand(temp_offset, Type::Int),
                                                     Operand(temp_int, Type::Int),
                                                     Operator::store));
                }
            }
            else if (ident_ste.operand.type == Type::FloatPtr)
            {
                if (exp->t == Type::IntLiteral)
                    buffer.push_back(new Instruction(Operand(ident_after_rename, Type::FloatPtr),
                                                     Operand(temp_offset, Type::Int),
                                                     Operand(std::to_string(float(std::stoi(exp->v))), Type::FloatLiteral),
                                                     Operator::store));
                else if (exp->t == Type::Int)
                {
                    string temp_float = "temp" + std::to_string(tmp_cnt++);
                    buffer.push_back(new Instruction(Operand(exp->v, Type::Int),
                                                     Operand(),
                                                     Operand(temp_float, Type::Float),
                                                     Operator::cvt_i2f));
                    buffer.push_back(new Instruction(Operand(ident_after_rename, Type::FloatPtr),
                                                     Operand(temp_offset, Type::Int),
                                                     Operand(temp_float, Type::Float),
                                                     Operator::store));
                }
                else if (exp->t == Type::FloatLiteral || exp->t == Type::Float)
                    buffer.push_back(new Instruction(Operand(ident_after_rename, Type::FloatPtr),
                                                     Operand(temp_offset, Type::Int),
                                                     Operand(exp->v, exp->t),
                                                     Operator::store));
            }
        }
        return;
    }
    if (Block *block_ = dynamic_cast<Block *>(root->children[0]))
    {
        ANALYSIS(block, Block, 0);
        return;
    }
    Term *term = dynamic_cast<Term *>(root->children[0]);
    if (term == nullptr)
    {
        ANALYSIS(exp, Exp, 0);
        return;
    }
    if (term->token.type == TokenType::IFTK)
    {
        ANALYSIS(cond, Cond, 2);
        buffer.push_back(new Instruction(Operand(cond->v, cond->t),
                                         Operand(),
                                         Operand("2", Type::IntLiteral),
                                         Operator::_goto));
        int if_pc_index = buffer.size();
        Instruction *goto_if_inst = new Instruction(Operand(),
                                                    Operand(),
                                                    Operand(),
                                                    Operator::_goto);
        buffer.push_back(goto_if_inst);

        symbol_table.add_scope();
        ANALYSIS(stmt_0, Stmt, 4);
        symbol_table.exit_scope();

        if (root->children.size() == 5)
        {
            int end_if_index = buffer.size();
            goto_if_inst->des = Operand(std::to_string(end_if_index - if_pc_index), Type::IntLiteral);
            buffer.push_back(new Instruction(Operand(),
                                             Operand(),
                                             Operand(),
                                             Operator::__unuse__));
        }
        else if (root->children.size() == 7)
        {
            Instruction *goto_if_true_inst = new Instruction(Operand(),
                                                             Operand(),
                                                             Operand(),
                                                             Operator::_goto);
            buffer.push_back(goto_if_true_inst);

            int before_else_goto_pc_index = buffer.size();

            symbol_table.add_scope();
            ANALYSIS(stmt_1, Stmt, 6);
            symbol_table.exit_scope();

            int end_else_index = buffer.size();
            goto_if_true_inst->des = Operand(std::to_string(end_else_index - before_else_goto_pc_index + 1), Type::IntLiteral);
            goto_if_inst->des = Operand(std::to_string(before_else_goto_pc_index - if_pc_index), Type::IntLiteral);
            buffer.push_back(new Instruction(Operand(),
                                             Operand(),
                                             Operand(),
                                             Operator::__unuse__));
        }
        return;
    }
    if (term->token.type == TokenType::WHILETK)
    {
        int before_while_pc_index = buffer.size();
        ANALYSIS(cond, Cond, 2);
        buffer.push_back(new Instruction(Operand(cond->v, cond->t),
                                         Operand(),
                                         Operand("2", Type::IntLiteral),
                                         Operator::_goto));
        int while_pc_index = buffer.size();
        Instruction *goto_while_inst = new Instruction(Operand(),
                                                       Operand(),
                                                       Operand(),
                                                       Operator::_goto);
        buffer.push_back(goto_while_inst);

        symbol_table.add_scope();
        ANALYSIS(stmt_0, Stmt, 4);
        symbol_table.exit_scope();

        int while_false_goto_index = buffer.size() - while_pc_index + 1;
        goto_while_inst->des = Operand(std::to_string(while_false_goto_index), Type::IntLiteral);
        int while_true_goto_index = -(buffer.size() - before_while_pc_index);
        buffer.push_back(new Instruction(Operand(),
                                         Operand(),
                                         Operand(std::to_string(while_true_goto_index), Type::IntLiteral),
                                         Operator::_goto));
        buffer.push_back(new Instruction(Operand(),
                                         Operand(),
                                         Operand(),
                                         Operator::__unuse__));
        for (int i = while_pc_index; i < while_pc_index - 1 + while_false_goto_index; i++)
        {
            if (buffer[i]->op1.name == "break")
            {
                buffer[i]->op1 = Operand();
                buffer[i]->des = Operand(std::to_string(while_pc_index + while_false_goto_index - i), Type::IntLiteral);
                buffer[i]->op = Operator::_goto;
            }
            else if (buffer[i]->op1.name == "continue")
            {
                buffer[i]->op1 = Operand();
                buffer[i]->des = Operand(std::to_string(-(i - before_while_pc_index)), Type::IntLiteral);
                buffer[i]->op = Operator::_goto;
            }
        }
        return;
    }
    if (term->token.type == TokenType::BREAKTK)
        buffer.push_back(new Instruction(Operand("break", Type::null),
                                         Operand(),
                                         Operand(),
                                         Operator::__unuse__));
    if (term->token.type == TokenType::CONTINUETK)
        buffer.push_back(new Instruction(Operand("continue", Type::null),
                                         Operand(),
                                         Operand(),
                                         Operator::__unuse__));
    if (term->token.type == TokenType::RETURNTK)
    {
        if (root->children.size() == 2)
            buffer.push_back(new Instruction(Operand(),
                                             Operand(),
                                             Operand(),
                                             Operator::_return));
        else if (root->children.size() == 3)
        {
            ANALYSIS(exp, Exp, 1);
            if (cur_func->returnType == Type::Int)
            {
                if (exp->t == Type::Int || exp->t == Type::IntLiteral)
                    buffer.push_back(new Instruction(Operand(exp->v, exp->t),
                                                     Operand(),
                                                     Operand(),
                                                     Operator::_return));
                else if (exp->t == Type::FloatLiteral)
                    buffer.push_back(new Instruction(Operand(std::to_string(int(std::stof(exp->v))), Type::IntLiteral),
                                                     Operand(),
                                                     Operand(),
                                                     Operator::_return));
                else if (exp->t == Type::Float)
                {
                    string temp_int = "temp" + std::to_string(tmp_cnt++);
                    buffer.push_back(new Instruction(Operand(exp->v, Type::Float),
                                                     Operand(),
                                                     Operand(temp_int, Type::Int),
                                                     Operator::cvt_f2i));
                    buffer.push_back(new Instruction(Operand(temp_int, Type::Int),
                                                     Operand(),
                                                     Operand(),
                                                     Operator::_return));
                }
            }
            else if (cur_func->returnType == Type::Float)
            {
                if (exp->t == Type::Float || exp->t == Type::FloatLiteral)
                    buffer.push_back(new Instruction(Operand(exp->v, exp->t),
                                                     Operand(),
                                                     Operand(),
                                                     Operator::_return));
                else if (exp->t == Type::IntLiteral)
                    buffer.push_back(new Instruction(Operand(std::to_string(float(std::stoi(exp->v))), Type::FloatLiteral),
                                                     Operand(),
                                                     Operand(),
                                                     Operator::_return));
                else if (exp->t == Type::Int)
                {
                    string temp_float = "temp" + std::to_string(tmp_cnt++);
                    buffer.push_back(new Instruction(Operand(exp->v, Type::Int),
                                                     Operand(),
                                                     Operand(temp_float, Type::Float),
                                                     Operator::cvt_i2f));
                    buffer.push_back(new Instruction(Operand(temp_float, Type::Float),
                                                     Operand(),
                                                     Operand(),
                                                     Operator::_return));
                }
            }
        }
        return;
    }
}

// Cond -> LOrExp
void frontend::Analyzer::analysisCond(Cond *root, vector<ir::Instruction *> &buffer)
{
    ANALYSIS(l_or_exp, LOrExp, 0);
    COPY_EXP_NODE(l_or_exp, root);
}

// RelExp -> AddExp { ('<' | '>' | '<=' | '>=') AddExp }
void frontend::Analyzer::analysisRelExp(RelExp *root, vector<ir::Instruction *> &buffer)
{
    ANALYSIS(add_exp_0, AddExp, 0);
    COPY_EXP_NODE(add_exp_0, root);
    for (int i = 1; i < root->children.size(); i = i + 2)
    {
        GET_CHILD_PTR(rel_op, Term, i);
        string comp_op = rel_op->token.value;
        ANALYSIS(add_exp_i, AddExp, i + 1);
        string temp_1 = "temp" + std::to_string(tmp_cnt++);
        string temp_2 = "temp" + std::to_string(tmp_cnt++);
        Type comp_type;
        if (root->t == Type::IntLiteral)
        {
            if (add_exp_i->t == Type::IntLiteral)
            {
                buffer.push_back(new Instruction(Operand(root->v, Type::IntLiteral),
                                                 Operand(),
                                                 Operand(temp_1, Type::Int),
                                                 Operator::def));
                buffer.push_back(new Instruction(Operand(add_exp_i->v, Type::IntLiteral),
                                                 Operand(),
                                                 Operand(temp_2, Type::Int),
                                                 Operator::def));
                comp_type = Type::Int;
            }
            else if (add_exp_i->t == Type::FloatLiteral)
            {
                buffer.push_back(new Instruction(Operand(std::to_string(float(std::stoi(add_exp_i->v))), Type::FloatLiteral),
                                                 Operand(),
                                                 Operand(temp_1, Type::Float),
                                                 Operator::fdef));
                buffer.push_back(new Instruction(Operand(add_exp_i->v, Type::FloatLiteral),
                                                 Operand(),
                                                 Operand(temp_2, Type::Float),
                                                 Operator::fdef));
                comp_type = Type::Float;
            }
            else if (add_exp_i->t == Type::Int)
            {
                buffer.push_back(new Instruction(Operand(root->v, Type::IntLiteral),
                                                 Operand(),
                                                 Operand(temp_1, Type::Int),
                                                 Operator::def));
                temp_2 = add_exp_i->v;
                comp_type = Type::Int;
            }
            else if (add_exp_i->t == Type::Float)
            {
                buffer.push_back(new Instruction(Operand(std::to_string(float(std::stoi(add_exp_i->v))), Type::FloatLiteral),
                                                 Operand(),
                                                 Operand(temp_1, Type::Float),
                                                 Operator::fdef));
                temp_2 = add_exp_i->v;
                comp_type = Type::Float;
            }
        }
        else if (root->t == Type::FloatLiteral)
        {
            comp_type = Type::Float;
            if (add_exp_i->t == Type::IntLiteral)
            {
                buffer.push_back(new Instruction(Operand(root->v, Type::FloatLiteral),
                                                 Operand(),
                                                 Operand(temp_1, Type::Float),
                                                 Operator::fdef));
                buffer.push_back(new Instruction(Operand(std::to_string(float(std::stoi(add_exp_i->v))), Type::FloatLiteral),
                                                 Operand(),
                                                 Operand(temp_2, Type::Float),
                                                 Operator::fdef));
            }
            else if (add_exp_i->t == Type::FloatLiteral)
            {
                buffer.push_back(new Instruction(Operand(root->v, Type::FloatLiteral),
                                                 Operand(),
                                                 Operand(temp_1, Type::Float),
                                                 Operator::fdef));
                buffer.push_back(new Instruction(Operand(add_exp_i->v, Type::FloatLiteral),
                                                 Operand(),
                                                 Operand(temp_2, Type::Float),
                                                 Operator::fdef));
            }
            else if (add_exp_i->t == Type::Int)
            {
                buffer.push_back(new Instruction(Operand(std::to_string(float(std::stoi(root->v))), Type::FloatLiteral),
                                                 Operand(),
                                                 Operand(temp_1, Type::Float),
                                                 Operator::fdef));
                buffer.push_back(new Instruction(Operand(add_exp_i->v, Type::Int),
                                                 Operand(),
                                                 Operand(temp_2, Type::Float),
                                                 Operator::cvt_i2f));
            }
            else if (add_exp_i->t == Type::Float)
            {
                buffer.push_back(new Instruction(Operand(root->v, Type::FloatLiteral),
                                                 Operand(),
                                                 Operand(temp_1, Type::Float),
                                                 Operator::fdef));
                temp_2 = add_exp_i->v;
            }
        }
        else if (root->t == Type::Int)
        {
            if (add_exp_i->t == Type::IntLiteral)
            {
                temp_1 = root->v;
                buffer.push_back(new Instruction(Operand(add_exp_i->v, Type::IntLiteral),
                                                 Operand(),
                                                 Operand(temp_2, Type::Int),
                                                 Operator::def));
                comp_type = Type::Int;
            }
            else if (add_exp_i->t == Type::FloatLiteral)
            {
                buffer.push_back(new Instruction(Operand(root->v, Type::Int),
                                                 Operand(),
                                                 Operand(temp_1, Type::Float),
                                                 Operator::cvt_i2f));
                buffer.push_back(new Instruction(Operand(add_exp_i->v, Type::FloatLiteral),
                                                 Operand(),
                                                 Operand(temp_2, Type::Float),
                                                 Operator::fdef));
                comp_type = Type::Float;
            }
            else if (add_exp_i->t == Type::Int)
            {
                temp_1 = root->v;
                buffer.push_back(new Instruction(Operand(add_exp_i->v, Type::Int),
                                                 Operand(),
                                                 Operand(temp_2, Type::Int),
                                                 Operator::def));
                comp_type = Type::Int;
            }
            else if (add_exp_i->t == Type::Float)
            {
                buffer.push_back(new Instruction(Operand(root->v, Type::Int),
                                                 Operand(),
                                                 Operand(temp_1, Type::Float),
                                                 Operator::cvt_i2f));
                temp_2 = add_exp_i->v;
                comp_type = Type::Float;
            }
        }
        else if (root->t == Type::Float)
        {
            comp_type = Type::Float;
            if (add_exp_i->t == Type::IntLiteral)
            {
                temp_1 = root->v;
                buffer.push_back(new Instruction(Operand(std::to_string(float(std::stoi(add_exp_i->v))), Type::FloatLiteral),
                                                 Operand(),
                                                 Operand(temp_2, Type::Float),
                                                 Operator::fdef));
            }
            else if (add_exp_i->t == Type::FloatLiteral)
            {
                temp_1 = root->v;
                buffer.push_back(new Instruction(Operand(add_exp_i->v, Type::FloatLiteral),
                                                 Operand(),
                                                 Operand(temp_2, Type::Float),
                                                 Operator::fdef));
            }
            else if (add_exp_i->t == Type::Int)
            {
                temp_1 = root->v;
                buffer.push_back(new Instruction(Operand(add_exp_i->v, Type::Int),
                                                 Operand(),
                                                 Operand(temp_2, Type::Float),
                                                 Operator::cvt_i2f));
            }
            else if (add_exp_i->t == Type::Float)
            {
                temp_1 = root->v;
                temp_2 = add_exp_i->v;
            }
        }
        string temp_3 = "temp" + std::to_string(tmp_cnt++);
        if (comp_type == Type::Int)
            buffer.push_back(new Instruction(Operand(temp_1, Type::Int),
                                             Operand(temp_2, Type::Int),
                                             Operand(temp_3, Type::Int),
                                             comp_op == "<" ? Operator::lss : comp_op == ">" ? Operator::gtr
                                                                          : comp_op == "<="  ? Operator::leq
                                                                                             : Operator::geq));
        else if (comp_type == Type::Float)
            buffer.push_back(new Instruction(Operand(temp_1, Type::Float),
                                             Operand(temp_2, Type::Float),
                                             Operand(temp_3, Type::Float),
                                             comp_op == "<" ? Operator::flss : comp_op == ">" ? Operator::fgtr
                                                                           : comp_op == "<="  ? Operator::fleq
                                                                                              : Operator::fgeq));
        root->v = temp_3;
        root->t = comp_type;
    }
}

// EqExp -> RelExp { ('==' | '!=') RelExp }
void frontend::Analyzer::analysisEqExp(EqExp *root, vector<ir::Instruction *> &buffer)
{
    ANALYSIS(rel_exp_0, RelExp, 0);
    COPY_EXP_NODE(rel_exp_0, root);
    for (int i = 1; i < root->children.size(); i = i + 2)
    {
        GET_CHILD_PTR(eq_op, Term, i);
        string comp_op = eq_op->token.value;
        ANALYSIS(rel_exp_i, RelExp, i + 1);
        string temp_1 = "temp" + std::to_string(tmp_cnt++);
        string temp_2 = "temp" + std::to_string(tmp_cnt++);
        Type comp_type;
        if (root->t == Type::IntLiteral)
        {
            if (rel_exp_i->t == Type::IntLiteral)
            {
                buffer.push_back(new Instruction(Operand(root->v, Type::IntLiteral),
                                                 Operand(),
                                                 Operand(temp_1, Type::Int),
                                                 Operator::def));
                buffer.push_back(new Instruction(Operand(rel_exp_i->v, Type::IntLiteral),
                                                 Operand(),
                                                 Operand(temp_2, Type::Int),
                                                 Operator::def));
                comp_type = Type::Int;
            }
            else if (rel_exp_i->t == Type::FloatLiteral)
            {
                buffer.push_back(new Instruction(Operand(std::to_string(float(std::stoi(rel_exp_i->v))), Type::FloatLiteral),
                                                 Operand(),
                                                 Operand(temp_1, Type::Float),
                                                 Operator::fdef));
                buffer.push_back(new Instruction(Operand(rel_exp_i->v, Type::FloatLiteral),
                                                 Operand(),
                                                 Operand(temp_2, Type::Float),
                                                 Operator::fdef));
                comp_type = Type::Float;
            }
            else if (rel_exp_i->t == Type::Int)
            {
                buffer.push_back(new Instruction(Operand(root->v, Type::IntLiteral),
                                                 Operand(),
                                                 Operand(temp_1, Type::Int),
                                                 Operator::def));
                temp_2 = rel_exp_i->v;
                comp_type = Type::Int;
            }
            else if (rel_exp_i->t == Type::Float)
            {
                buffer.push_back(new Instruction(Operand(std::to_string(float(std::stoi(rel_exp_i->v))), Type::FloatLiteral),
                                                 Operand(),
                                                 Operand(temp_1, Type::Float),
                                                 Operator::fdef));
                temp_2 = rel_exp_i->v;
                comp_type = Type::Float;
            }
        }
        else if (root->t == Type::FloatLiteral)
        {
            comp_type = Type::Float;
            if (rel_exp_i->t == Type::IntLiteral)
            {
                buffer.push_back(new Instruction(Operand(root->v, Type::FloatLiteral),
                                                 Operand(),
                                                 Operand(temp_1, Type::Float),
                                                 Operator::fdef));
                buffer.push_back(new Instruction(Operand(std::to_string(float(std::stoi(rel_exp_i->v))), Type::FloatLiteral),
                                                 Operand(),
                                                 Operand(temp_2, Type::Float),
                                                 Operator::fdef));
            }
            else if (rel_exp_i->t == Type::FloatLiteral)
            {
                buffer.push_back(new Instruction(Operand(root->v, Type::FloatLiteral),
                                                 Operand(),
                                                 Operand(temp_1, Type::Float),
                                                 Operator::fdef));
                buffer.push_back(new Instruction(Operand(rel_exp_i->v, Type::FloatLiteral),
                                                 Operand(),
                                                 Operand(temp_2, Type::Float),
                                                 Operator::fdef));
            }
            else if (rel_exp_i->t == Type::Int)
            {
                buffer.push_back(new Instruction(Operand(std::to_string(float(std::stoi(root->v))), Type::FloatLiteral),
                                                 Operand(),
                                                 Operand(temp_1, Type::Float),
                                                 Operator::fdef));
                buffer.push_back(new Instruction(Operand(rel_exp_i->v, Type::Int),
                                                 Operand(),
                                                 Operand(temp_2, Type::Float),
                                                 Operator::cvt_i2f));
            }
            else if (rel_exp_i->t == Type::Float)
            {
                buffer.push_back(new Instruction(Operand(root->v, Type::FloatLiteral),
                                                 Operand(),
                                                 Operand(temp_1, Type::Float),
                                                 Operator::fdef));
                temp_2 = rel_exp_i->v;
            }
        }
        else if (root->t == Type::Int)
        {
            if (rel_exp_i->t == Type::IntLiteral)
            {
                temp_1 = root->v;
                buffer.push_back(new Instruction(Operand(rel_exp_i->v, Type::IntLiteral),
                                                 Operand(),
                                                 Operand(temp_2, Type::Int),
                                                 Operator::def));
                comp_type = Type::Int;
            }
            else if (rel_exp_i->t == Type::FloatLiteral)
            {
                buffer.push_back(new Instruction(Operand(root->v, Type::Int),
                                                 Operand(),
                                                 Operand(temp_1, Type::Float),
                                                 Operator::cvt_i2f));
                buffer.push_back(new Instruction(Operand(rel_exp_i->v, Type::FloatLiteral),
                                                 Operand(),
                                                 Operand(temp_2, Type::Float),
                                                 Operator::fdef));
                comp_type = Type::Float;
            }
            else if (rel_exp_i->t == Type::Int)
            {
                temp_1 = root->v;
                buffer.push_back(new Instruction(Operand(rel_exp_i->v, Type::Int),
                                                 Operand(),
                                                 Operand(temp_2, Type::Int),
                                                 Operator::def));
                comp_type = Type::Int;
            }
            else if (rel_exp_i->t == Type::Float)
            {
                buffer.push_back(new Instruction(Operand(root->v, Type::Int),
                                                 Operand(),
                                                 Operand(temp_1, Type::Float),
                                                 Operator::cvt_i2f));
                temp_2 = rel_exp_i->v;
                comp_type = Type::Float;
            }
        }
        else if (root->t == Type::Float)
        {
            comp_type = Type::Float;
            if (rel_exp_i->t == Type::IntLiteral)
            {
                temp_1 = root->v;
                buffer.push_back(new Instruction(Operand(std::to_string(float(std::stoi(rel_exp_i->v))), Type::FloatLiteral),
                                                 Operand(),
                                                 Operand(temp_2, Type::Float),
                                                 Operator::fdef));
            }
            else if (rel_exp_i->t == Type::FloatLiteral)
            {
                temp_1 = root->v;
                buffer.push_back(new Instruction(Operand(rel_exp_i->v, Type::FloatLiteral),
                                                 Operand(),
                                                 Operand(temp_2, Type::Float),
                                                 Operator::fdef));
            }
            else if (rel_exp_i->t == Type::Int)
            {
                temp_1 = root->v;
                buffer.push_back(new Instruction(Operand(rel_exp_i->v, Type::Int),
                                                 Operand(),
                                                 Operand(temp_2, Type::Float),
                                                 Operator::cvt_i2f));
            }
            else if (rel_exp_i->t == Type::Float)
            {
                temp_1 = root->v;
                temp_2 = rel_exp_i->v;
            }
        }
        string temp_3 = "temp" + std::to_string(tmp_cnt++);
        if (comp_type == Type::Int)
            buffer.push_back(new Instruction(Operand(temp_1, Type::Int),
                                             Operand(temp_2, Type::Int),
                                             Operand(temp_3, Type::Int),
                                             comp_op == "==" ? Operator::eq : Operator::neq));
        else if (comp_type == Type::Float)
            buffer.push_back(new Instruction(Operand(temp_1, Type::Float),
                                             Operand(temp_2, Type::Float),
                                             Operand(temp_3, Type::Float),
                                             comp_op == "==" ? Operator::feq : Operator::fneq));
        root->v = temp_3;
        root->t = comp_type;
    }
    // std::cout << "EqExp3    " << std::endl;
}

// LAndExp -> EqExp [ '&&' LAndExp ]
void frontend::Analyzer::analysisLAndExp(LAndExp *root, vector<ir::Instruction *> &buffer)
{
    ANALYSIS(eqexp, EqExp, 0);
    COPY_EXP_NODE(eqexp, root);
    if (root->children.size() > 1)
    {
        string temp_bool1 = "temp" + std::to_string(tmp_cnt++);
        string temp_result = "temp" + std::to_string(tmp_cnt++);
        if (root->t == Type::Int)
            buffer.push_back(new Instruction(Operand(root->v, Type::Int),
                                             Operand("0", Type::IntLiteral),
                                             Operand(temp_bool1, Type::Int),
                                             Operator::neq));
        else if (root->t == Type::IntLiteral)
        {
            string temp_int = "temp" + std::to_string(tmp_cnt++);
            buffer.push_back(new Instruction(Operand(root->v, Type::IntLiteral),
                                             Operand(),
                                             Operand(temp_int, Type::Int),
                                             Operator::def));
            buffer.push_back(new Instruction(Operand(temp_int, Type::Int),
                                             Operand("0", Type::IntLiteral),
                                             Operand(temp_bool1, Type::Int),
                                             Operator::neq));
        }
        else if (root->t == Type::Float)
        {
            buffer.push_back(new Instruction(Operand(root->v, Type::Float),
                                             Operand("0.0", Type::FloatLiteral),
                                             Operand(temp_bool1, Type::Float),
                                             Operator::fneq));
            buffer.push_back(new Instruction(Operand(temp_bool1, Type::Float),
                                             Operand(),
                                             Operand(temp_bool1, Type::Int),
                                             Operator::cvt_f2i));
        }
        else if (root->t == Type::FloatLiteral)
        {
            string temp_float = "temp" + std::to_string(tmp_cnt++);
            buffer.push_back(new Instruction(Operand(root->v, Type::FloatLiteral),
                                             Operand(),
                                             Operand(temp_float, Type::Float),
                                             Operator::fdef));
            buffer.push_back(new Instruction(Operand(temp_float, Type::Float),
                                             Operand("0.0", Type::FloatLiteral),
                                             Operand(temp_float, Type::Float),
                                             Operator::fneq));
            buffer.push_back(new Instruction(Operand(temp_float, Type::Float),
                                             Operand(),
                                             Operand(temp_bool1, Type::Int),
                                             Operator::cvt_f2i));
        }
        buffer.push_back(new Instruction(Operand("0", Type::IntLiteral),
                                         Operand(),
                                         Operand(temp_result, Type::Int),
                                         Operator::def));
        buffer.push_back(new Instruction(Operand(temp_bool1, Type::Int),
                                         Operand(),
                                         Operand("2", Type::IntLiteral),
                                         Operator::_goto));
        Instruction *label_1 = new Instruction(Operand(),
                                               Operand(),
                                               Operand(),
                                               Operator::_goto);
        buffer.push_back(label_1);
        int index1 = buffer.size();
        ANALYSIS(landexp, LAndExp, 2);
        buffer.push_back(new Instruction(Operand(root->v, root->t),
                                         Operand(landexp->v, landexp->t),
                                         Operand(temp_result, Type::Int),
                                         Operator::_and));
        int index2 = buffer.size();
        label_1->des = Operand(std::to_string(index2 - index1 + 1), Type::IntLiteral);
        root->v = temp_result;
    }
}

// LOrExp -> LAndExp [ '||' LOrExp ]
void frontend::Analyzer::analysisLOrExp(LOrExp *root, vector<ir::Instruction *> &buffer)
{
    ANALYSIS(landexp, LAndExp, 0);
    COPY_EXP_NODE(landexp, root);
    if (root->children.size() > 1)
    {
        string temp_bool1 = "temp" + std::to_string(tmp_cnt++);
        string temp_result = "temp" + std::to_string(tmp_cnt++);
        if (root->t == Type::Int)
            buffer.push_back(new Instruction(Operand(root->v, Type::Int),
                                             Operand("0", Type::IntLiteral),
                                             Operand(temp_bool1, Type::Int),
                                             Operator::eq));
        else if (root->t == Type::IntLiteral)
        {
            buffer.push_back(new Instruction(Operand(root->v, Type::IntLiteral),
                                             Operand(),
                                             Operand(temp_bool1, Type::Int),
                                             Operator::def));
            buffer.push_back(new Instruction(Operand(temp_bool1, Type::Int),
                                             Operand("0", Type::IntLiteral),
                                             Operand(temp_bool1, Type::Int),
                                             Operator::eq));
        }
        else if (root->t == Type::Float)
        {
            buffer.push_back(new Instruction(Operand(root->v, Type::Float),
                                             Operand("0.0", Type::FloatLiteral),
                                             Operand(temp_bool1, Type::Float),
                                             Operator::feq));
            buffer.push_back(new Instruction(Operand(temp_bool1, Type::Float),
                                             Operand(),
                                             Operand(temp_bool1, Type::Int),
                                             Operator::cvt_f2i));
        }
        else if (root->t == Type::FloatLiteral)
        {
            string temp_float = "temp" + std::to_string(tmp_cnt++);
            buffer.push_back(new Instruction(Operand(root->v, Type::FloatLiteral),
                                             Operand(),
                                             Operand(temp_float, Type::Float),
                                             Operator::fdef));
            buffer.push_back(new Instruction(Operand(temp_float, Type::Float),
                                             Operand("0.0", Type::FloatLiteral),
                                             Operand(temp_float, Type::Float),
                                             Operator::feq));
            buffer.push_back(new Instruction(Operand(temp_float, Type::Float),
                                             Operand(),
                                             Operand(temp_bool1, Type::Int),
                                             Operator::cvt_f2i));
        }
        buffer.push_back(new Instruction(Operand("1", Type::IntLiteral),
                                         Operand(),
                                         Operand(temp_result, Type::Int),
                                         Operator::def));
        buffer.push_back(new Instruction(Operand(temp_bool1, Type::Int),
                                         Operand(),
                                         Operand("2", Type::IntLiteral),
                                         Operator::_goto));
        Instruction *label_1 = new Instruction(Operand(),
                                               Operand(),
                                               Operand(),
                                               Operator::_goto);
        buffer.push_back(label_1);
        int index1 = buffer.size();
        ANALYSIS(lorexp, LOrExp, 2);
        string temp_op1 = "temp" + std::to_string(tmp_cnt++);
        Type temp_op1_type;
        string temp_op2 = "temp" + std::to_string(tmp_cnt++);
        Type temp_op2_type;
        if (root->t == Type::Int)
        {
            temp_op1 = root->v;
            temp_op1_type = Type::Int;
        }
        else if (root->t == Type::IntLiteral)
        {
            temp_op1 = root->v;
            temp_op1_type = Type::IntLiteral;
        }
        else if (root->t == Type::FloatLiteral)
        {
            temp_op1 = root->v == "0.0" ? "0" : "1";
            temp_op1_type = Type::IntLiteral;
        }
        else if (root->t == Type::Float)
        {
            buffer.push_back(new Instruction(Operand(root->v, Type::Float),
                                             Operand(),
                                             Operand(temp_op1, Type::Int),
                                             Operator::cvt_f2i));
            temp_op1_type = Type::Int;
        }
        if (lorexp->t == Type::Int)
        {
            temp_op2 = lorexp->v;
            temp_op2_type = Type::Int;
        }
        else if (lorexp->t == Type::IntLiteral)
        {
            temp_op2 = lorexp->v;
            temp_op2_type = Type::IntLiteral;
        }
        else if (lorexp->t == Type::FloatLiteral)
        {
            temp_op2 = lorexp->v == "0.0" ? "0" : "1";
            temp_op2_type = Type::IntLiteral;
        }
        else if (lorexp->t == Type::Float)
        {
            buffer.push_back(new Instruction(Operand(lorexp->v, Type::Float),
                                             Operand(),
                                             Operand(temp_op2, Type::Int),
                                             Operator::cvt_f2i));
            temp_op2_type = Type::Int;
        }
        buffer.push_back(new Instruction(Operand(temp_op1, temp_op1_type),
                                         Operand(temp_op2, temp_op2_type),
                                         Operand(temp_result, Type::Int),
                                         Operator::_or));
        int index2 = buffer.size();
        label_1->des = Operand(std::to_string(index2 - index1 + 1), Type::IntLiteral);
        root->v = temp_result;
        root->t = Type::Int;
    }
}
>>>>>>> ffd4636 (58/58 拿下)
