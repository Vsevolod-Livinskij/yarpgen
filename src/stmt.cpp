/*
Copyright (c) 2015-2017, Intel Corporation

Licensed under the Apache License, Version 2.0 (the "License");
you may not use this file except in compliance with the License.
You may obtain a copy of the License at

     http://www.apache.org/licenses/LICENSE-2.0

Unless required by applicable law or agreed to in writing, software
distributed under the License is distributed on an "AS IS" BASIS,
WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
See the License for the specific language governing permissions and
limitations under the License.
*/

//////////////////////////////////////////////////////////////////////////////

#include "stmt.h"
#include "sym_table.h"
#include "util.h"

using namespace yarpgen;

uint32_t Stmt::total_stmt_count = 0;
uint32_t Stmt::func_stmt_count = 0;

// C++03 and previous versions doesn't allow to use list-initialization for vector and valarray,
// so we need to use StubExpr as init expression
static bool is_cxx03_and_special_arr_kind(std::shared_ptr<Data> data) {
    return options->is_cxx() && options->standard_id <= Options::CXX03 &&
           data->get_class_id() == Data::ARRAY &&
           (std::static_pointer_cast<ArrayType>(data->get_type())->get_kind() == ArrayType::STD_VEC ||
            std::static_pointer_cast<ArrayType>(data->get_type())->get_kind() == ArrayType::VAL_ARR);
}

DeclStmt::DeclStmt (std::shared_ptr<Data> _data, std::shared_ptr<Expr> _init, bool _is_extern) :
                  Stmt(Node::NodeID::DECL), data(_data), init(_init), is_extern(_is_extern) {
    if (init == nullptr || is_cxx03_and_special_arr_kind(data))
        return;
    if (data->get_class_id() != Data::VarClassID::VAR || init->get_value()->get_class_id() != Data::VarClassID::VAR) {
        ERROR("can init only ScalarVariable in DeclStmt");
    }
    if (is_extern) {
        ERROR("init of extern var DeclStmt");
    }
    std::shared_ptr<ScalarVariable> data_var = std::static_pointer_cast<ScalarVariable>(data);
    std::shared_ptr<TypeCastExpr> cast_type = std::make_shared<TypeCastExpr>(init, data_var->get_type());
    data_var->set_init_value(std::static_pointer_cast<ScalarVariable>(cast_type->get_value())->get_cur_value());
}

// This function randomly creates new ScalarVariable, its initializing arithmetic expression and
// adds new variable to local_sym_table of parent Context
std::shared_ptr<DeclStmt> DeclStmt::generate (std::shared_ptr<Context> ctx,
                                              std::vector<std::shared_ptr<Expr>> inp,
                                              bool count_up_total) {
    Stmt::increase_stmt_count();
    GenPolicy::add_to_complexity(Node::NodeID::DECL);

    std::shared_ptr<ScalarVariable> new_var = ScalarVariable::generate(ctx);
    std::shared_ptr<Expr> new_init = ArithExpr::generate(ctx, inp);
    if (count_up_total)
        Expr::increase_expr_count(new_init->get_complexity());
    std::shared_ptr<DeclStmt> ret =  std::make_shared<DeclStmt>(new_var, new_init);
    if (ctx->get_parent_ctx() == nullptr || ctx->get_parent_ctx()->get_local_sym_table() == nullptr) {
        ERROR("no par_ctx or local_sym_table (DeclStmt)");
    }
    ctx->get_parent_ctx()->get_local_sym_table()->add_variable(new_var);
    return ret;
}

// This function creates list-initialization for structs in arrays
static void emit_list_init_for_struct(std::ostream &stream, std::shared_ptr<Struct> struct_elem) {
    stream << "{";
    uint64_t member_count = struct_elem->get_member_count();
    for (int i = 0; i < member_count; ++i) {
        std::shared_ptr<Data> member = struct_elem->get_member(i);
        // Skip static members
        if (member->get_type()->get_is_static())
            continue;

        switch (member->get_class_id()) {
            case Data::VAR: {
                std::shared_ptr<ScalarVariable> var_member = std::static_pointer_cast<ScalarVariable>(member);
                ConstExpr init_const(var_member->get_init_value());
                init_const.emit(stream);
            }
                break;
            case Data::STRUCT: {
                std::shared_ptr<Struct> struct_member = std::static_pointer_cast<Struct>(member);
                emit_list_init_for_struct(stream, struct_member);
            }
                break;
            case Data::ARRAY:
            case Data::MAX_CLASS_ID:
                ERROR("inappropriate type of Struct member");
                break;
        }
        if (i < member_count - 1)
            stream << ", ";
    }
    stream << "} ";
}

void DeclStmt::emit (std::ostream& stream, std::string offset) {
    stream << offset;
    stream << (data->get_type()->get_is_static() && !is_extern ? "static " : "");
    stream << (is_extern ? "extern " : "");
    switch (data->get_type()->get_cv_qual()) {
        case Type::CV_Qual::VOLAT:
            stream << "volatile ";
            break;
        case Type::CV_Qual::CONST:
            stream << "const ";
            break;
        case Type::CV_Qual::CONST_VOLAT:
            stream << "const volatile ";
            break;
        case Type::CV_Qual::NTHG:
            break;
        case Type::CV_Qual::MAX_CV_QUAL:
            ERROR("bad cv_qual (DeclStmt)");
            break;
    }
    stream << data->get_type()->get_simple_name() + " " + data->get_name() + data->get_type()->get_type_suffix();
    if (data->get_type()->get_align() != 0 && is_extern) // TODO: Should we set __attribute__ to non-extern variable?
        stream << " __attribute__((aligned(" + std::to_string(data->get_type()->get_align()) + ")))";
    if (init != nullptr &&
       // C++03 and previous versions doesn't allow to use list-initialization for vector and valarray,
       // so we need to use StubExpr as init expression
       !is_cxx03_and_special_arr_kind(data)) {
        if (data->get_class_id() == Data::VarClassID::STRUCT ||
           (data->get_class_id() == Data::VarClassID::ARRAY && !is_cxx03_and_special_arr_kind(data))) {
            ERROR("emit init of struct (DeclStmt)");
        }
        if (is_extern) {
            ERROR("init of extern var (DeclStmt)");
        }
        stream << " = ";
        TypeCastExpr init_cast (init, data->get_type(), true);
        init_cast.emit(stream);
    }
    if (data->get_class_id() == Data::VarClassID::ARRAY && !is_extern) {
        //TODO: it is a stub. We should use something to represent list-initialization.
        if (!is_cxx03_and_special_arr_kind(data)) {
            stream << " = {";
            std::shared_ptr<Array> array = std::static_pointer_cast<Array>(data);
            std::shared_ptr<ArrayType> array_type = std::static_pointer_cast<ArrayType>(array->get_type());
            uint64_t array_elements_count = array->get_elements_count();
            // std::array requires additional curly brackets in list-initialization
            if (array_type->get_kind() == ArrayType::STD_ARR)
                stream << "{";

            for (int i = 0; i < array_elements_count; ++i) {
                if (array_type->get_base_type()->is_int_type()) {
                    std::shared_ptr<ScalarVariable> elem = std::static_pointer_cast<ScalarVariable>(
                            array->get_element(i));
                    ConstExpr init_const(elem->get_init_value());
                    init_const.emit(stream);
                } else if (array_type->get_base_type()->is_struct_type()) {
                    std::shared_ptr<Struct> elem = std::static_pointer_cast<Struct>(array->get_element(i));
                    emit_list_init_for_struct(stream, elem);
                } else
                    ERROR("bad base type of array");
                if (i < array_elements_count - 1)
                    stream << ", ";
            }

            // std::array requires additional curly brackets in list-initialization
            if (array_type->get_kind() == ArrayType::STD_ARR)
                stream << "}";
            stream << "}";
        }
        else {
            // Same note about C++03 and previous versions
            stream << " (";
            std::static_pointer_cast<StubExpr>(init)->emit(stream);
            stream << ")";
        }
    }
    stream << ";";
}

// One of the most important generation methods (top-level generator for everything between curve brackets).
// It acts as a top-level dispatcher for other statement generation functions.
// Also it initially fills extern symbol table.
std::shared_ptr<ScopeStmt> ScopeStmt::generate (std::shared_ptr<Context> ctx) {
    GenPolicy::add_to_complexity(Node::NodeID::SCOPE);

    std::shared_ptr<ScopeStmt> ret = std::make_shared<ScopeStmt>();

    // Before the main generation loop starts, we need to extract from all contexts every input / mixed variable and structure.
    std::vector<std::shared_ptr<Expr>> inp = extract_inp_and_mix_from_ctx(ctx);

    //TODO: add to gen_policy stmt number
    auto p = ctx->get_gen_policy();
    uint32_t scope_stmt_count = rand_val_gen->get_rand_value(p->get_min_scope_stmt_count(),
                                                             p->get_max_scope_stmt_count());

    for (uint32_t i = 0; i < scope_stmt_count; ++i) {
        if (Stmt::total_stmt_count >= p->get_max_total_stmt_count() ||
            Stmt::func_stmt_count  >= p->get_max_func_stmt_count())
            //TODO: Can we somehow eliminate compiler timeout with the help of this?
            //GenPolicy::get_test_complexity() >= p->get_max_test_complexity())
            break;

        // Randomly decide if we want to create a new CSE
        GenPolicy::ArithCSEGenID add_cse = rand_val_gen->get_rand_id(p->get_arith_cse_gen());
        if (add_cse == GenPolicy::ArithCSEGenID::Add &&
           ((p->get_cse().size() - 1 < p->get_max_cse_count()) ||
            (p->get_cse().size() == 0))) {
            std::vector<std::shared_ptr<Expr>> cse_inp = extract_inp_from_ctx(ctx);
            p->add_cse(ArithExpr::generate(ctx, cse_inp));
        }

        // Randomly pick next Stmt ID
        Node::NodeID gen_id = rand_val_gen->get_rand_id(p->get_stmt_gen_prob());
        // ExprStmt
        if (gen_id == Node::NodeID::EXPR) {
            //TODO: add to gen_policy
            // Are we going to use mixed variable or create new output variable?
            bool use_mix = rand_val_gen->get_rand_id(p->get_out_data_category_prob()) == GenPolicy::OutDataCategoryID::MIX;
            GenPolicy::OutDataTypeID out_data_type = rand_val_gen->get_rand_id(p->get_out_data_type_prob());
            std::shared_ptr<Expr> assign_lhs = nullptr;

            // This function checks if we have any suitable members / variables / arrays
            auto check_ctx_for_zero_size = [&out_data_type] (std::shared_ptr<SymbolTable> sym_table) -> bool {
                return (out_data_type == GenPolicy::OutDataTypeID::VAR_IN_ARRAY &&
                            sym_table->get_var_use_exprs_in_arrays().empty()) ||
                       (out_data_type == GenPolicy::OutDataTypeID::STRUCT &&
                            sym_table->get_members_in_structs().empty()) ||
                       (out_data_type == GenPolicy::OutDataTypeID::STRUCT_IN_ARRAY &&
                            sym_table->get_members_in_arrays().empty());
            };

            // This function randomly picks element from vector.
            // Also it optionally returns picked element's id in ret_rand_num
            auto pick_elem = [&assign_lhs] (auto vector_of_exprs, uint32_t* ret_rand_num = nullptr) {
                uint64_t rand_num = rand_val_gen->get_rand_value(0UL, vector_of_exprs.size() - 1);
                assign_lhs = vector_of_exprs.at(rand_num);
                if (ret_rand_num != nullptr)
                    *ret_rand_num = rand_num;
            };

            if (!use_mix || ctx->get_extern_mix_sym_table()->get_var_use_exprs_from_vars().empty()) {
                bool zero_size = check_ctx_for_zero_size(ctx->get_extern_out_sym_table());

                // Create new output variable or we don't have any suitable members / variables / arrays
                if (out_data_type == GenPolicy::OutDataTypeID::VAR || zero_size) {
                    std::shared_ptr<ScalarVariable> out_var = ScalarVariable::generate(ctx);
                    ctx->get_extern_out_sym_table()->add_variable (out_var);
                    assign_lhs = std::make_shared<VarUseExpr>(out_var);
                }
                // Use variable in output array
                else if (out_data_type == GenPolicy::OutDataTypeID::VAR_IN_ARRAY)
                    //TODO: we should also delete it (make it not-reusable)
                    pick_elem(ctx->get_extern_out_sym_table()->get_var_use_exprs_in_arrays());
                // Use member of output struct
                else if (out_data_type == GenPolicy::OutDataTypeID::STRUCT) {
                    uint32_t elem_num;
                    pick_elem(ctx->get_extern_out_sym_table()->get_members_in_structs(), &elem_num);
                    ctx->get_extern_out_sym_table()->del_member_in_structs(elem_num);
                }
                // Use member of struct in output array
                else if (out_data_type == GenPolicy::OutDataTypeID::STRUCT_IN_ARRAY) {
                    uint32_t elem_num;
                    pick_elem(ctx->get_extern_out_sym_table()->get_members_in_arrays(), &elem_num);
                    ctx->get_extern_out_sym_table()->del_member_in_arrays(elem_num);
                }
                else
                    ERROR("bad OutDataTypeID");

            }
            else {
                bool zero_size = check_ctx_for_zero_size(ctx->get_extern_mix_sym_table());

                // Use mixed variable or we don't have any suitable members / variables / arrays
                if (out_data_type == GenPolicy::OutDataTypeID::VAR || zero_size)
                    pick_elem(ctx->get_extern_mix_sym_table()->get_var_use_exprs_from_vars());
                // Use variable in mixed array
                else if (out_data_type == GenPolicy::OutDataTypeID::VAR_IN_ARRAY)
                    pick_elem(ctx->get_extern_mix_sym_table()->get_var_use_exprs_in_arrays());
                // Use member of mixed struct
                else if (out_data_type == GenPolicy::OutDataTypeID::STRUCT)
                    pick_elem(ctx->get_extern_mix_sym_table()->get_members_in_structs());
                // Use member of struct in mixed array
                else if (out_data_type == GenPolicy::OutDataTypeID::STRUCT_IN_ARRAY)
                    pick_elem(ctx->get_extern_mix_sym_table()->get_members_in_arrays());
                else
                    ERROR("bad OutDataTypeID");

            }
            ret->add_stmt(ExprStmt::generate(ctx, inp, assign_lhs, true));
        }
        // DeclStmt or if we want IfStmt, but have reached its depth limit
        else if (gen_id == Node::NodeID::DECL || (ctx->get_if_depth() == p->get_max_if_depth())) {
            std::shared_ptr<Context> decl_ctx = std::make_shared<Context>(*(p), ctx, Node::NodeID::DECL, true);
            std::shared_ptr<DeclStmt> tmp_decl = DeclStmt::generate(decl_ctx, inp, true);
            // Add created variable to inp
            std::shared_ptr<ScalarVariable> tmp_var = std::static_pointer_cast<ScalarVariable>(tmp_decl->get_data());
            inp.push_back(std::make_shared<VarUseExpr>(tmp_var));
            ret->add_stmt(tmp_decl);
        }
        // IfStmt
        else if (gen_id == Node::NodeID::IF) {
            ret->add_stmt(IfStmt::generate(std::make_shared<Context>(*(p), ctx, Node::NodeID::IF, true), inp, true));
        }

    }
    return ret;
}

// CSE shouldn't change during the scope to make generation process easy. In order to achieve this,
// we use only "input" variables for them and this function extracts such variables from extern symbol table.
std::vector<std::shared_ptr<Expr>> ScopeStmt::extract_inp_from_ctx(std::shared_ptr<Context> ctx) {
    std::vector<std::shared_ptr<Expr>> ret = ctx->get_extern_inp_sym_table()->get_all_var_use_exprs();

    for (auto i : ctx->get_extern_inp_sym_table()->get_const_members_in_structs())
        ret.push_back(i);

    for (auto i : ctx->get_extern_inp_sym_table()->get_const_members_in_arrays())
        ret.push_back(i);

    return ret;
}

// This function extracts local symbol tables of current Context and all it's predecessors.
std::vector<std::shared_ptr<Expr>> ScopeStmt::extract_locals_from_ctx(std::shared_ptr<Context> ctx) {
    //TODO: add struct members
    std::vector<std::shared_ptr<Expr>> ret = ctx->get_local_sym_table()->get_all_var_use_exprs();

    if (ctx->get_parent_ctx() != nullptr)
        ret = extract_locals_from_ctx(ctx->get_parent_ctx());

    return ret;
}

// This function extracts all available input and mixed variables from extern symbol table and
// local symbol tables of current Context and all it's predecessors (by calling extract_locals_from_ctx).
// TODO: we create multiple entry for variables from extern_sym_tables
std::vector<std::shared_ptr<Expr>> ScopeStmt::extract_inp_and_mix_from_ctx(std::shared_ptr<Context> ctx) {
    std::vector<std::shared_ptr<Expr>> ret = extract_inp_from_ctx(ctx);
    for (auto i : ctx->get_extern_mix_sym_table()->get_members_in_structs())
        ret.push_back(i);
    for (auto i : ctx->get_extern_mix_sym_table()->get_members_in_arrays())
        ret.push_back(i);

    for (auto i : ctx->get_extern_mix_sym_table()->get_all_var_use_exprs())
        ret.push_back(i);

    auto locals = extract_locals_from_ctx(ctx);
    ret.insert(ret.end(), locals.begin(), locals.end());

    return ret;
}

void ScopeStmt::emit (std::ostream& stream, std::string offset) {
    stream << offset + "{\n";
    for (const auto &i : scope) {
        i->emit(stream, offset + "    ");
        stream << "\n";
    }
    stream << offset + "}\n";
}

// This function randomly creates new AssignExpr and wraps it to ExprStmt.
std::shared_ptr<ExprStmt> ExprStmt::generate (std::shared_ptr<Context> ctx,
                                              std::vector<std::shared_ptr<Expr>> inp,
                                              std::shared_ptr<Expr> out,
                                              bool count_up_total) {
    Stmt::increase_stmt_count();
    GenPolicy::add_to_complexity(Node::NodeID::EXPR);

    //TODO: now it can be only assign. Do we want something more?
    std::shared_ptr<Expr> from = ArithExpr::generate(ctx, inp);
    std::shared_ptr<AssignExpr> assign_exp = std::make_shared<AssignExpr>(out, from, ctx->get_taken());
    if (count_up_total)
        Expr::increase_expr_count(assign_exp->get_complexity());
    GenPolicy::add_to_complexity(Node::NodeID::ASSIGN);
    return std::make_shared<ExprStmt>(assign_exp);
}

void ExprStmt::emit (std::ostream& stream, std::string offset) {
    stream << offset;
    expr->emit(stream);
    stream << ";";
}

bool IfStmt::count_if_taken (std::shared_ptr<Expr> cond) {
    std::shared_ptr<TypeCastExpr> cond_to_bool = std::make_shared<TypeCastExpr> (cond, IntegerType::init(Type::IntegerTypeID::BOOL), true);
    if (cond_to_bool->get_value()->get_class_id() != Data::VarClassID::VAR) {
        ERROR("bad class id (IfStmt)");
    }
    std::shared_ptr<ScalarVariable> cond_var = std::static_pointer_cast<ScalarVariable> (cond_to_bool->get_value());
    return cond_var->get_cur_value().val.bool_val;
}

IfStmt::IfStmt (std::shared_ptr<Expr> _cond, std::shared_ptr<ScopeStmt> _if_br, std::shared_ptr<ScopeStmt> _else_br) :
                Stmt(Node::NodeID::IF), cond(_cond), if_branch(_if_br), else_branch(_else_br) {
    if (cond == nullptr || if_branch == nullptr) {
        ERROR("if branchescan't be empty (IfStmt)");
    }
    taken = count_if_taken(cond);
}

// This function randomly creates new IfStmt (its condition, if branch body and and optional else branch).
std::shared_ptr<IfStmt> IfStmt::generate (std::shared_ptr<Context> ctx,
                                          std::vector<std::shared_ptr<Expr>> inp,
                                          bool count_up_total) {
    Stmt::increase_stmt_count();
    GenPolicy::add_to_complexity(Node::NodeID::IF);
    std::shared_ptr<Expr> cond = ArithExpr::generate(ctx, inp);
    if (count_up_total)
        Expr::increase_expr_count(cond->get_complexity());
    bool else_exist = rand_val_gen->get_rand_id(ctx->get_gen_policy()->get_else_prob());
    bool cond_taken = IfStmt::count_if_taken(cond);
    std::shared_ptr<ScopeStmt> then_br = ScopeStmt::generate(std::make_shared<Context>(*(ctx->get_gen_policy()), ctx, Node::NodeID::SCOPE, cond_taken));
    std::shared_ptr<ScopeStmt> else_br = nullptr;
    if (else_exist)
        else_br = ScopeStmt::generate(std::make_shared<Context>(*(ctx->get_gen_policy()), ctx, Node::NodeID::SCOPE, !cond_taken));
    return std::make_shared<IfStmt>(cond, then_br, else_br);
}

void IfStmt::emit (std::ostream& stream, std::string offset) {
    stream << offset << "if (";
    cond->emit(stream);
    stream << ")\n";
    if_branch->emit(stream, offset);
    if (else_branch != nullptr) {
        stream << offset + "else\n";
        else_branch->emit(stream, offset);
    }
}
