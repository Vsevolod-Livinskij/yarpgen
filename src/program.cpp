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

#include "program.h"
#include "util.h"

///////////////////////////////////////////////////////////////////////////////

using namespace yarpgen;

Program::Program (std::string _out_folder) {
    out_folder = _out_folder;
    uint32_t test_func_count = gen_policy.get_test_func_count();
    extern_inp_sym_table.reserve(test_func_count);
    extern_mix_sym_table.reserve(test_func_count);
    extern_out_sym_table.reserve(test_func_count);
    functions.reserve(gen_policy.get_test_func_count());
}

void Program::generate () {
    NameHandler& name_handler = NameHandler::get_instance();
    for (int i = 0; i < gen_policy.get_test_func_count(); ++i) {
        name_handler.set_test_func_prefix(i);

        extern_inp_sym_table.push_back(std::make_shared<SymbolTable>());
        extern_mix_sym_table.push_back(std::make_shared<SymbolTable>());
        extern_out_sym_table.push_back(std::make_shared<SymbolTable>());

        Context ctx(gen_policy, nullptr, Node::NodeID::MAX_STMT_ID, true);
        ctx.set_extern_inp_sym_table(extern_inp_sym_table.back());
        ctx.set_extern_mix_sym_table(extern_mix_sym_table.back());
        ctx.set_extern_out_sym_table(extern_out_sym_table.back());
        std::shared_ptr<Context> ctx_ptr = std::make_shared<Context>(ctx);
        form_extern_sym_table(ctx_ptr);
        functions.push_back(ScopeStmt::generate(ctx_ptr));

        name_handler.zero_out_counters();
        Stmt::zero_out_func_stmt_count();
        Expr::zero_out_func_expr_count();
    }
}

// This function initially fills extern symbol table with inp and mix variables. It also creates type structs definitions.
void Program::form_extern_sym_table(std::shared_ptr<Context> ctx) {
    auto p = ctx->get_gen_policy();
    // Allow const cv-qualifier in gen_policy, pass it to new Context
    std::shared_ptr<Context> const_ctx = std::make_shared<Context>(*(ctx));
    GenPolicy const_gen_policy = *(const_ctx->get_gen_policy());
    const_gen_policy.set_allow_const(true);
    const_ctx->set_gen_policy(const_gen_policy);
    // Generate random number of random input variables
    uint32_t inp_var_count = rand_val_gen->get_rand_value(p->get_min_inp_var_count(),
                                                          p->get_max_inp_var_count());
    for (uint32_t i = 0; i < inp_var_count; ++i) {
        ctx->get_extern_inp_sym_table()->add_variable(ScalarVariable::generate(const_ctx));
    }
    //TODO: add to gen_policy
    // Same for mixed variables
    uint32_t mix_var_count = rand_val_gen->get_rand_value(p->get_min_mix_var_count(),
                                                          p->get_max_mix_var_count());
    for (uint32_t i = 0; i < mix_var_count; ++i) {
        ctx->get_extern_mix_sym_table()->add_variable(ScalarVariable::generate(ctx));
    }

    uint32_t struct_type_count = rand_val_gen->get_rand_value(p->get_min_struct_type_count(),
                                                              p->get_max_struct_type_count());
    if (struct_type_count == 0)
        return;

    // Create random number of struct definition
    for (uint32_t i = 0; i < struct_type_count; ++i) {
        //TODO: Maybe we should create one container for all struct types? And should they all be equal?
        std::shared_ptr<StructType> struct_type = StructType::generate(ctx, ctx->get_extern_inp_sym_table()->
                                                                                 get_struct_types());
        ctx->get_extern_inp_sym_table()->add_struct_type(struct_type);
        ctx->get_extern_out_sym_table()->add_struct_type(struct_type);
        ctx->get_extern_mix_sym_table()->add_struct_type(struct_type);
    }

    // Create random number of input structures
    uint32_t inp_struct_count = rand_val_gen->get_rand_value(p->get_min_inp_struct_count(),
                                                             p->get_max_inp_struct_count());
    for (uint32_t i = 0; i < inp_struct_count; ++i) {
        std::shared_ptr<StructType>& struct_type = rand_val_gen->get_rand_elem(ctx->get_extern_inp_sym_table()->
                                                                               get_struct_types());
        std::shared_ptr<Struct> new_struct = Struct::generate(const_ctx, struct_type);
        ctx->get_extern_inp_sym_table()->add_struct(new_struct);
    }
    // Same for mixed structures
    uint32_t mix_struct_count = rand_val_gen->get_rand_value(p->get_min_mix_struct_count(),
                                                             p->get_max_mix_struct_count());
    for (uint32_t i = 0; i < mix_struct_count; ++i) {
        std::shared_ptr<StructType>& struct_type = rand_val_gen->get_rand_elem(ctx->get_extern_mix_sym_table()->
                                                                               get_struct_types());
        std::shared_ptr<Struct> new_struct = Struct::generate(ctx, struct_type);
        ctx->get_extern_mix_sym_table()->add_struct(new_struct);
    }
    // Same for output structures
    uint32_t out_struct_count = rand_val_gen->get_rand_value(p->get_min_out_struct_count(),
                                                             p->get_max_out_struct_count());
    for (uint32_t i = 0; i < out_struct_count; ++i) {
        std::shared_ptr<StructType>& struct_type = rand_val_gen->get_rand_elem(ctx->get_extern_out_sym_table()->
                                                                               get_struct_types());
        std::shared_ptr<Struct> new_struct = Struct::generate(ctx, struct_type);
        ctx->get_extern_out_sym_table()->add_struct(new_struct);
    }


    // Create random number of common array types
    uint32_t array_type_count = rand_val_gen->get_rand_value(p->get_min_array_type_count(),
                                                             p->get_max_array_type_count());
    if (array_type_count == 0)
        return;

    for (uint32_t i = 0; i < array_type_count; ++i) {
        //TODO: Maybe we should create one container for all struct types? And should they all be equal?
        std::shared_ptr<ArrayType> array_type = ArrayType::generate(ctx);
        ctx->get_extern_inp_sym_table()->add_array_type(array_type);
        ctx->get_extern_out_sym_table()->add_array_type(array_type);
        ctx->get_extern_mix_sym_table()->add_array_type(array_type);
    }

    // Create random number of input arrays
    uint32_t inp_array_count = rand_val_gen->get_rand_value(p->get_min_inp_array_count(),
                                                            p->get_max_inp_array_count());
    for (int i = 0; i < inp_array_count; ++i) {
        std::shared_ptr<ArrayType>& array_type = rand_val_gen->get_rand_elem(ctx->get_extern_inp_sym_table()->
                                                                             get_array_types());
        std::shared_ptr<Array> new_array = Array::generate(ctx, array_type);
        ctx->get_extern_inp_sym_table()->add_array(new_array);
    }

    // Same for mixed arrays
    uint32_t mix_array_count = rand_val_gen->get_rand_value(p->get_min_mix_array_count(),
                                                            p->get_max_mix_array_count());
    for (int i = 0; i < mix_array_count; ++i) {
        std::shared_ptr<ArrayType>& array_type = rand_val_gen->get_rand_elem(ctx->get_extern_mix_sym_table()->
                                                                             get_array_types());
        std::shared_ptr<Array> new_array = Array::generate(ctx, array_type);
        ctx->get_extern_mix_sym_table()->add_array(new_array);
    }

    // Same for output arrays
    uint32_t out_array_count = rand_val_gen->get_rand_value(p->get_min_out_array_count(),
                                                            p->get_max_out_array_count());
    for (int i = 0; i < out_array_count; ++i) {
        std::shared_ptr<ArrayType>& array_type = rand_val_gen->get_rand_elem(ctx->get_extern_out_sym_table()->
                                                                             get_array_types());
        std::shared_ptr<Array> new_array = Array::generate(ctx, array_type);
        ctx->get_extern_out_sym_table()->add_array(new_array);
    }
}

static std::string get_file_ext () {
    if (options->is_c())
        return "c";
    else if (options->is_cxx())
        return "cpp";
    std::cerr << "ERROR at " << __FILE__ << ":" << __LINE__ << ": can't detect language subset" << std::endl;
    exit(-1);
}

void Program::emit_decl () {
    std::ofstream out_file;
    out_file.open(out_folder + "/" + "init.h");
    if (options->include_valarray)
        out_file << "#include <valarray>\n\n";
    if (options->include_vector)
        out_file << "#include <vector>\n\n";
    if (options->include_array)
        out_file << "#include <array>\n\n";

    for (int i = 0; i < gen_policy.get_test_func_count(); ++i) {
        extern_inp_sym_table.at(i)->emit_variable_extern_decl(out_file);
        out_file << "\n\n";
        extern_mix_sym_table.at(i)->emit_variable_extern_decl(out_file);
        out_file << "\n\n";
        extern_out_sym_table.at(i)->emit_variable_extern_decl(out_file);
        out_file << "\n\n";
        //TODO: what if we extend struct types in mix_sym_tabl
        extern_inp_sym_table.at(i)->emit_struct_type_def(out_file);
        out_file << "\n\n";
        extern_inp_sym_table.at(i)->emit_struct_extern_decl(out_file);
        out_file << "\n\n";
        extern_mix_sym_table.at(i)->emit_struct_extern_decl(out_file);
        out_file << "\n\n";
        extern_out_sym_table.at(i)->emit_struct_extern_decl(out_file);
        out_file << "\n\n";
        extern_inp_sym_table.at(i)->emit_array_extern_decl(out_file);
        out_file << "\n\n";
        extern_mix_sym_table.at(i)->emit_array_extern_decl(out_file);
        out_file << "\n\n";
        extern_out_sym_table.at(i)->emit_array_extern_decl(out_file);
        out_file << "\n\n";
    }

    out_file.close();
}

void Program::emit_func () {
    std::ofstream out_file;
    out_file.open(out_folder + "/" + "func." + get_file_ext());
    out_file << "#include \"init.h\"\n\n";

    for (int i = 0; i < gen_policy.get_test_func_count(); ++i) {
        out_file << "void " << NameHandler::common_test_func_prefix << i << "_foo ()\n";
        functions.at(i)->emit(out_file);
        out_file << "\n";
    }
    out_file.close();
}

void Program::emit_main () {
    std::ofstream out_file;
    out_file.open(out_folder + "/" + "driver." + get_file_ext());

    // Headers
    //////////////////////////////////////////////////////////
    out_file << "#include <stdio.h>\n";
    out_file << "#include <stdlib.h>\n";
    out_file << "#include <string.h>\n";
    out_file << "#include \"init.h\"\n\n";

    // Hash
    //////////////////////////////////////////////////////////

    std::shared_ptr<ScalarVariable> seed = std::make_shared<ScalarVariable>("seed", IntegerType::init(
            Type::IntegerTypeID::ULLINT));
    std::shared_ptr<VarUseExpr> seed_use = std::make_shared<VarUseExpr>(seed);

    BuiltinType::ScalarTypedVal zero_init(Type::IntegerTypeID::ULLINT);
    zero_init.val.ullint_val = 0;
    std::shared_ptr<ConstExpr> const_init = std::make_shared<ConstExpr>(zero_init);

    std::shared_ptr<DeclStmt> seed_decl = std::make_shared<DeclStmt>(seed, const_init);
    seed_decl->emit(out_file);
    out_file << "\n\n";

    if (options->num_mode == Options::NumMode::INT) {
        out_file << "void hash(unsigned long long int *seed, unsigned long long int const v) {\n";
        out_file << "    *seed ^= v + 0x9e3779b9 + ((*seed)<<6) + ((*seed)>>2);\n";
        out_file << "}\n\n";
    }
    else if (options->num_mode == Options::NumMode::FP) {
        out_file << "FILE* res_file = NULL;\n";
        out_file << "void open_res_file(const char* file_name) {\n";
        out_file << "    const char file_suffix [] = \"_res.txt\";\n";
        out_file << "    const size_t len1 = strlen(file_name);\n";
        out_file << "    const size_t len2 = strlen(file_suffix);\n";
        out_file << "    char* result = (char*) malloc(len1+len2+1);\n";
        out_file << "    memcpy(result, file_name, len1);\n";
        out_file << "    memcpy(result + len1, file_suffix, len2 + 1);\n";
        out_file << "    res_file = fopen(result, \"w\");\n";
        out_file << "    if (res_file == NULL)\n";
        out_file << "        exit(1);\n";
        out_file << "}\n\n";

        //TODO: for FP mode seed is useless, but we have to keep it for backward compatibility
        out_file << "void hash(unsigned long long int *seed, long double v) {\n";
        out_file << "    fprintf(res_file, \"%Lf\\n\", v);\n";
        out_file << "}\n\n";

        out_file << "void close_res_file() {\n";
        out_file << "    fclose(res_file);\n";
        out_file << "}\n\n";
    }
    else
        ERROR("bad mode");

    for (int i = 0; i < gen_policy.get_test_func_count(); ++i) {
        // Definitions and initialization
        //////////////////////////////////////////////////////////
        extern_inp_sym_table.at(i)->emit_variable_def(out_file);
        out_file << "\n\n";
        extern_mix_sym_table.at(i)->emit_variable_def(out_file);
        out_file << "\n\n";
        extern_out_sym_table.at(i)->emit_variable_def(out_file);
        out_file << "\n\n";
        extern_inp_sym_table.at(i)->emit_struct_def(out_file);
        out_file << "\n\n";
        extern_mix_sym_table.at(i)->emit_struct_def(out_file);
        out_file << "\n\n";
        extern_out_sym_table.at(i)->emit_struct_def(out_file);
        out_file << "\n\n";
        extern_inp_sym_table.at(i)->emit_array_def(out_file);
        out_file << "\n\n";
        extern_mix_sym_table.at(i)->emit_array_def(out_file);
        out_file << "\n\n";
        extern_out_sym_table.at(i)->emit_array_def(out_file);

        //TODO: what if we extend struct types in mix_sym_table and out_sym_table
        extern_inp_sym_table.at(i)->emit_struct_type_static_memb_def(out_file);
        out_file << "\n\n";

        out_file << "void " << NameHandler::common_test_func_prefix << i << "_init () {\n";
        extern_inp_sym_table.at(i)->emit_struct_type_static_memb_init (out_file, "    ");
        extern_inp_sym_table.at(i)->emit_struct_init(out_file, "    ");
        extern_mix_sym_table.at(i)->emit_struct_init(out_file, "    ");
        extern_out_sym_table.at(i)->emit_struct_init(out_file, "    ");
        out_file << "}\n\n";

        // Check
        //////////////////////////////////////////////////////////
        out_file << "void " << NameHandler::common_test_func_prefix << i << "_checksum () {\n";

        extern_mix_sym_table.at(i)->emit_variable_check(out_file, "    ");
        extern_out_sym_table.at(i)->emit_variable_check(out_file, "    ");

        extern_mix_sym_table.at(i)->emit_struct_check(out_file, "    ");
        extern_out_sym_table.at(i)->emit_struct_check(out_file, "    ");

        extern_mix_sym_table.at(i)->emit_array_check (out_file, "    ");
        extern_out_sym_table.at(i)->emit_array_check (out_file, "    ");

        out_file << "}\n\n";

        out_file << "extern void " << NameHandler::common_test_func_prefix << i << "_foo ();\n\n";
    }

    // Main
    //////////////////////////////////////////////////////////
    out_file << "\n";
    out_file << "int main (int argc, char* argv []) {\n";
    if (options->num_mode == Options::NumMode::FP)
        out_file << "    open_res_file(argv[0]);\n";

    std::string tf_prefix;
    for (int i = 0; i < gen_policy.get_test_func_count(); ++i) {
        tf_prefix = NameHandler::common_test_func_prefix + std::to_string(i) + "_";
        out_file << "    " << tf_prefix << "init ();\n";
        out_file << "    " << tf_prefix << "foo ();\n";
        out_file << "    " << tf_prefix << "checksum ();\n\n";
    }

    out_file << "    printf(\"%llu\\n\", seed);\n";
    if (options->num_mode == Options::NumMode::FP)
        out_file << "    close_res_file();\n";
    out_file << "    return 0;\n";
    out_file << "}\n";

    out_file.close();
}

