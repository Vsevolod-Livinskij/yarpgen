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

#include <cassert>

#include "sym_table.h"
#include "util.h"

using namespace yarpgen;


void SymbolTable::add_struct (std::shared_ptr<Struct> _struct) {
    structs.push_back(_struct);
    form_struct_member_expr(nullptr, _struct);
}

void SymbolTable::form_struct_member_expr (std::shared_ptr<MemberExpr> parent_memb_expr, std::shared_ptr<Struct> struct_var, bool ignore_const) {
    for (int j = 0; j < struct_var->get_member_count(); ++j) {
        GenPolicy gen_policy;
        if (rand_val_gen->get_rand_id(gen_policy.get_member_use_prob())) {
            std::shared_ptr<MemberExpr> member_expr;
            if (parent_memb_expr != nullptr)
                member_expr = std::make_shared<MemberExpr>(parent_memb_expr, j);
            else
                member_expr = std::make_shared<MemberExpr>(struct_var, j, options->is_opencl());

            bool is_static = struct_var->get_member(j)->get_type()->get_is_static();

            if (struct_var->get_member(j)->get_type()->is_struct_type()) {
                form_struct_member_expr(member_expr, std::static_pointer_cast<Struct>(struct_var->get_member(j)), is_static || ignore_const);
            }
            else {
                avail_members.push_back(member_expr);
                if (!is_static && !ignore_const) {
                    avail_const_members.push_back(member_expr);
                }
            }
        }
    }
}

std::string SymbolTable::emit_variable_extern_decl (std::string offset) {
    std::string ret = "";
    for (auto i : variable) {
        DeclStmt decl (i, nullptr, true);
        ret += offset + decl.emit() + "\n";
    }
    return ret;
}

std::string SymbolTable::emit_variable_def (std::string offset) {
    std::string ret = "";
    for (auto i : variable) {
        std::shared_ptr<ConstExpr> const_init = std::make_shared<ConstExpr>(i->get_init_value());

        std::shared_ptr<DeclStmt> decl = std::make_shared<DeclStmt>(i, const_init);
        ret += offset + decl->emit() + "\n";
    }
    return ret;
}

std::string SymbolTable::emit_struct_type_static_memb_def (std::string offset) {
    std::string ret = "";
    for (auto i : struct_type) {
        ret += i->get_static_memb_def() + "\n";
    }
    return ret;
}

std::string SymbolTable::emit_struct_type_def (std::string offset) {
    std::string ret = "";
    for (auto i : struct_type) {
        ret += offset + i->get_definition() + "\n";
    }
    return ret;
}

std::string SymbolTable::emit_struct_def (std::string offset) {
    std::string ret = "";
    for (auto i : structs) {
        DeclStmt decl (i, nullptr, false);
        ret += offset + decl.emit() + "\n";
    }
    return ret;
}

std::string SymbolTable::emit_struct_extern_decl (std::string offset) {
    std::string ret = "";
    for (auto i : structs) {
        DeclStmt decl (i, nullptr, true);
        ret += offset + decl.emit() + "\n";
    }
    return ret;
}

std::string SymbolTable::emit_struct_init (std::string offset) {
    std::string ret = "";
    for (auto i : structs) {
        ret += emit_single_struct_init(nullptr, i, offset);
    }
    return ret;
}

std::string SymbolTable::emit_single_struct_init (std::shared_ptr<MemberExpr> parent_memb_expr, std::shared_ptr<Struct> struct_var, std::string offset) {
    std::string ret = "";
    for (int j = 0; j < struct_var->get_member_count(); ++j) {
        std::shared_ptr<MemberExpr> member_expr;
        if  (parent_memb_expr != nullptr)
            member_expr = std::make_shared<MemberExpr>(parent_memb_expr, j);
        else
            member_expr = std::make_shared<MemberExpr>(struct_var, j);

        if (struct_var->get_member(j)->get_type()->is_struct_type()) {
            ret += emit_single_struct_init(member_expr, std::static_pointer_cast<Struct>(struct_var->get_member(j)), offset);
        }
        else {
            std::shared_ptr<ConstExpr> const_init = std::make_shared<ConstExpr>(std::static_pointer_cast<ScalarVariable>(struct_var->get_member(j))->get_init_value());
            AssignExpr assign (member_expr, const_init, false);
            ret += offset + assign.emit() + ";\n";
        }
    }
    return ret;
}

std::string SymbolTable::emit_struct_check (std::string offset) {
    std::string ret = "";
    for (auto i : structs) {
        ret += emit_single_struct_check(nullptr, i, offset);
    }
    return ret;
}

std::string SymbolTable::emit_single_struct_check (std::shared_ptr<MemberExpr> parent_memb_expr, std::shared_ptr<Struct> struct_var, std::string offset) {
    std::string ret = "";
    for (int j = 0; j < struct_var->get_member_count(); ++j) {
        std::shared_ptr<MemberExpr> member_expr;
        if  (parent_memb_expr != nullptr)
            member_expr = std::make_shared<MemberExpr>(parent_memb_expr, j);
        else
            member_expr = std::make_shared<MemberExpr>(struct_var, j);

        if (struct_var->get_member(j)->get_type()->is_struct_type())
            ret += emit_single_struct_check(member_expr, std::static_pointer_cast<Struct>(struct_var->get_member(j)), offset);
        else
            ret += offset + "hash(&seed, " + member_expr->emit() + ");\n";
    }
    return ret;
}

std::string SymbolTable::emit_variable_check (std::string offset) {
    std::string ret = "";
    for (auto i = variable.begin(); i != variable.end(); ++i) {
        ret += offset + "hash(&seed, " + (*i)->get_name() + ");\n";
    }
    return ret;
}

std::string SymbolTable::emit_ocl_additional_data_decl (std::string offset) {
    std::string ret = "";
    auto ocl_data_decl_printer = [offset] (std::string var_name, std::string type_name) -> std::string {
        return offset + "cl_mem " + var_name + "_ocl;\n" +
               offset + type_name + "* " + var_name + "_arr;\n";
    };
    for (auto const &i : variable)
        ret += ocl_data_decl_printer(i->get_name(), i->get_type()->get_simple_name());
    for (auto const &i : structs)
        ret += ocl_data_decl_printer(i->get_name(), i->get_type()->get_simple_name());

    return ret;
}

static unsigned long int kernel_arg_count = 0;

std::string SymbolTable::emit_ocl_data_init (OCL_Data_Type ocl_data_type, std::string offset) {
    std::string ocl_data_type_str = "";
    switch (ocl_data_type) {
        case RONLY:
            ocl_data_type_str = "CL_MEM_READ_ONLY";
            break;
        case WONLY:
            ocl_data_type_str = "CL_MEM_WRITE_ONLY";
            break;
        case RW:
            ocl_data_type_str = "CL_MEM_READ_WRITE";
            break;
        default:
            ERROR("can't detect OpenCL data type");
    }
    std::string ret = "";
    auto call_printer_for_all_data = [this] (auto printer) -> std::string {
        std::string tmp_ret = "";
        for (auto const &i : variable)
            tmp_ret += printer(i->get_name(), i->get_type()->get_simple_name());
        for (auto const &i : structs)
            tmp_ret += printer(i->get_name(), i->get_type()->get_simple_name());
        return tmp_ret;
    };

    auto ocl_data_alloc_printer = [offset, ocl_data_type_str] (std::string var_name, std::string type_name) -> std::string {
        return offset + var_name + "_ocl = clCreateBuffer(context," + ocl_data_type_str + ", " +
               "sizeof(" + type_name + ") * count, NULL, NULL);\n" +
               offset + var_name + "_arr = calloc(count, sizeof(" + type_name + "));\n";
    };
    ret += call_printer_for_all_data(ocl_data_alloc_printer);

    ret += offset + "for (int i = 0; i < count; ++i) {\n";
    auto arr_init_printer = [offset] (std::string var_name, std::string type_name) -> std::string {
        return offset + "    " + var_name + "_arr [i] = " + var_name + ";\n";
    };
    ret += call_printer_for_all_data(arr_init_printer);
    ret += offset + "}\n";

    auto ocl_data_to_kernel_printer = [offset] (std::string var_name, std::string type_name) -> std::string {
        return offset + "clEnqueueWriteBuffer(commands, " + var_name + "_ocl, CL_TRUE, 0, " +
               "sizeof(" + type_name + ") * count, " + var_name + "_arr, 0, NULL, NULL);\n" +
               offset + "clSetKernelArg(kernel, " + std::to_string(kernel_arg_count++) + ", sizeof(cl_mem), " +
               "&" + var_name + "_ocl);\n";
    };
    ret += call_printer_for_all_data(ocl_data_to_kernel_printer);
    return ret;
}

std::string SymbolTable::emit_ocl_kernel_args () {
    std::string ret = "";
    auto ocl_kernel_args_printer = [] (std::string var_name, std::string type_name) -> std::string {
        return "__global " + type_name + "* " + var_name + ", ";
    };
    for (auto const &i : variable)
        ret += ocl_kernel_args_printer(i->get_name(), i->get_type()->get_simple_name());
    for (auto const &i : structs)
        ret += ocl_kernel_args_printer(i->get_name(), i->get_type()->get_simple_name());
    if (ret.length() > 0)
        ret.erase(ret.length() - 2);
    return ret;
}

std::string SymbolTable::emit_ocl_single_struct_check (std::string base_string, std::string memb_path,
                                                       std::shared_ptr<Struct> struct_var, std::string offset) {
    std::string ret = "";
    for (int j = 0; j < struct_var->get_member_count(); ++j) {
        std::shared_ptr<Data> member = struct_var->get_member(j);
        if (member->get_type()->is_struct_type())
            ret += emit_ocl_single_struct_check(base_string, memb_path + "." + member->get_name(),
                                                std::static_pointer_cast<Struct>(member), offset);
        else
            ret += offset + "fail |= " + base_string + "_arr [i]" + memb_path + "." + member->get_name() +
                   " != " + base_string + "_arr [i - 1]" + memb_path + "." + member->get_name() + ";\n";
    }
    return ret;
}

std::string SymbolTable::emit_ocl_data_check (std::string offset) {
    std::string ret = "";
    auto call_printer_for_all_data = [this] (auto printer) -> std::string {
        std::string tmp_ret = "";
        for (auto const &i : variable)
            tmp_ret += printer(i->get_name(), i->get_type()->get_simple_name());
        for (auto const &i : structs)
            tmp_ret += printer(i->get_name(), i->get_type()->get_simple_name());
        return tmp_ret;
    };

    auto ocl_buf_read_printer = [offset] (std::string var_name, std::string type_name) -> std::string {
        return offset + "clEnqueueReadBuffer(commands, " + var_name + "_ocl, CL_TRUE, 0, " +
               "sizeof(" + type_name + ") * count, " + var_name + "_arr, 0, NULL, NULL );\n";
    };
    ret += call_printer_for_all_data(ocl_buf_read_printer);
    ret += "\n";

    ret += offset + "for (int i = 1; i < count; i++) {\n";
    auto arr_check_printer = [offset] (std::string var_name) -> std::string {
        return offset + "    fail |= " + var_name + "_arr [i] != " + var_name + "_arr [i - 1];\n";
    };
    for (auto const &i : variable)
        ret += arr_check_printer(i->get_name());
    for (auto const &i : structs) {
        ret += emit_ocl_single_struct_check(i->get_name(), "", i, offset + "    ");
    }
    ret += offset + "}\n\n";

    ret += offset + "if (fail) hash(&seed, fail);\n\n";

    auto ocl_data_free_printer = [offset] (std::string var_name, std::string type_name) -> std::string {
        return offset + var_name + " = " + var_name + "_arr [0];\n" +
               offset + "free (" + var_name + "_arr);\n" +
               offset + "clReleaseMemObject(" + var_name + "_ocl);\n";
    };
    ret += call_printer_for_all_data(ocl_data_free_printer);
    ret += "\n";
    return ret;
}

Context::Context (GenPolicy _gen_policy, std::shared_ptr<Context> _parent_ctx, Node::NodeID _self_stmt_id, bool _taken) {
    gen_policy = std::make_shared<GenPolicy>(_gen_policy);
    parent_ctx = _parent_ctx;
    local_sym_table = std::make_shared<SymbolTable>();
    depth = 0;
    if_depth = 0;
    self_stmt_id = _self_stmt_id;
    taken = _taken;

    if (parent_ctx != nullptr) {
        extern_inp_sym_table = parent_ctx->get_extern_inp_sym_table ();
        extern_out_sym_table = parent_ctx->get_extern_out_sym_table ();
        extern_mix_sym_table = parent_ctx->get_extern_mix_sym_table();
        depth = parent_ctx->get_depth() + 1;
        if_depth = parent_ctx->get_if_depth();
        taken &= parent_ctx->get_taken();
        //TODO: It should be parent of scope statement
        if (parent_ctx->get_self_stmt_id() == Node::NodeID::IF)
            if_depth++;
    }
}
