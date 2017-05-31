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

#include "master.h"
#include "util.h"

///////////////////////////////////////////////////////////////////////////////

using namespace yarpgen;

Master::Master (std::string _out_folder) {
    out_folder = _out_folder;
    extern_inp_sym_table = std::make_shared<SymbolTable> ();
    extern_mix_sym_table = std::make_shared<SymbolTable> ();
    extern_out_sym_table = std::make_shared<SymbolTable> ();
}

void Master::generate () {
    Context ctx (gen_policy, nullptr, Node::NodeID::MAX_STMT_ID, true);
    ctx.set_extern_inp_sym_table (extern_inp_sym_table);
    ctx.set_extern_mix_sym_table (extern_mix_sym_table);
    ctx.set_extern_out_sym_table (extern_out_sym_table);

    program = ScopeStmt::generate(std::make_shared<Context>(ctx));
}

void Master::write_file (std::string of_name, std::string data) {
    std::ofstream out_file;
    out_file.open (out_folder + "/" + of_name);
    out_file << data;
    out_file.close ();
}

static std::string get_file_ext (std::string name) {
    name += ".";
    if (options->is_c())
        return name + "c";
    else if (options->is_cxx())
        return name + "cpp";
    else if (options->is_opencl())
        return name + (name != "func." ? "c" : "cl");
    std::cerr << "ERROR at " << __FILE__ << ":" << __LINE__ << ": can't detect language subset" << std::endl;
    exit(-1);
}

std::string Master::emit_init () {
    std::string ret = "";
    ret += "#include \"init.h\"\n\n";

    if (options->is_opencl()) {
        ret += "#include <stdlib.h>\n";
        ret += "#include <stdio.h>\n";
        ret += "#include <CL/cl.h>\n";
        ret += "\n";
        ret += "#define DATA_SIZE_MULTIPLIER (10)\n";
        ret += "#define MAX_SOURCE_SIZE (0x100000)\n";
        ret += "\n";
    }

    ret += extern_inp_sym_table->emit_variable_def() + "\n\n";
    ret += extern_mix_sym_table->emit_variable_def() + "\n\n";
    ret += extern_out_sym_table->emit_variable_def() + "\n\n";
    ret += extern_inp_sym_table->emit_struct_def() + "\n\n";
    ret += extern_mix_sym_table->emit_struct_def() + "\n\n";
    ret += extern_out_sym_table->emit_struct_def() + "\n\n";
    //TODO: what if we extand struct types in mix_sym_table and out_sym_table
    ret += extern_inp_sym_table->emit_struct_type_static_memb_def() + "\n\n";

    if (options->is_opencl()) {
        std::string OCL_std_str = "CL";
        switch (options->standard_id) {
            case Options::StandardID::OpenCL_1_1:
                OCL_std_str += "1.1";
                break;
            case Options::StandardID::OpenCL_1_2:
                OCL_std_str += "1.2";
                break;
            default:
                ERROR("unsupported OpenCL standard");
        }
        ret += "void init_opencl () {                                                              \n";
        ret += "    char fileName[] = \"./func.cl\";                                               \n";
        ret += "    FILE* fp = fopen(fileName, \"r\");                                             \n";
        ret += "    if (!fp) {                                                                     \n";
        ret += "        fprintf(stderr, \"Failed to load kernel.\\n\");                            \n";
        ret += "        exit(-1);                                                                  \n";
        ret += "    }                                                                              \n";
        ret += "    char* source_str = (char*) malloc(MAX_SOURCE_SIZE);                            \n";
        ret += "    size_t source_size = fread(source_str, 1, MAX_SOURCE_SIZE, fp);                \n";
        ret += "    fclose(fp);                                                                    \n";
        ret += "                                                                                   \n";
        ret += "    cl_platform_id platform_id;                                                    \n";
        ret += "    err = clGetPlatformIDs(1, &platform_id, NULL);                                 \n";
        ret += "    if (err != CL_SUCCESS) {                                                       \n";
        ret += "        fprintf(stderr, \"Failed to find platform.\\n\");                          \n";
        ret += "        exit(-1);                                                                  \n";
        ret += "    }                                                                              \n";
        ret += "                                                                                   \n";
        ret += "    cl_device_id device_id;                                                        \n";
        ret += "    err = clGetDeviceIDs(platform_id, CL_DEVICE_TYPE_ALL, 1, &device_id, NULL);    \n";
        ret += "    if (err != CL_SUCCESS) {                                                       \n";
        ret += "        fprintf(stderr, \"Failed to create a device group.\\n\");                  \n";
        ret += "        exit(-1);                                                                  \n";
        ret += "    }                                                                              \n";
        ret += "                                                                                   \n";
        ret += "    context = clCreateContext(0, 1, &device_id, NULL, NULL, &err);                 \n";
        ret += "    if (!context) {                                                                \n";
        ret += "        fprintf(stderr, \"Failed to create a compute context.\\n\");               \n";
        ret += "        exit(-1);                                                                  \n";
        ret += "    }                                                                              \n";
        ret += "                                                                                   \n";
        ret += "    commands = clCreateCommandQueue(context, device_id, 0, &err);                  \n";
        ret += "    if (!commands) {                                                               \n";
        ret += "        fprintf(stderr, \"Failed to create a command commands.\\n\");              \n";
        ret += "        exit(-1);                                                                  \n";
        ret += "    }                                                                              \n";
        ret += "                                                                                   \n";
        ret += "    program = clCreateProgramWithSource(context, 1, (const char **) &source_str,   \n";
        ret += "                                        (const size_t *)&source_size, &err);       \n";
        ret += "    if (!program) {                                                                \n";
        ret += "        fprintf(stderr, \"Failed to create compute program.\\n\");                 \n";
        ret += "        exit(-1);                                                                  \n";
        ret += "    }                                                                              \n";
        ret += "    free(source_str);                                                              \n";
        ret += "                                                                                   \n";
        ret += "    const char options[] = \"-cl-std=" + OCL_std_str + " -w\";                     \n";
        ret += "    err = clBuildProgram(program, 0, NULL, options, NULL, NULL);                   \n";
        ret += "    if (err != CL_SUCCESS) {                                                       \n";
        ret += "        fprintf(stderr, \"Failed to build program executable.\\n\");               \n";
        ret += "        size_t len;                                                                \n";
        ret += "        char* buffer;                                                              \n";
        ret += "        clGetProgramBuildInfo(program, device_id, CL_PROGRAM_BUILD_LOG,            \n";
        ret += "                              0, NULL, &len);                                      \n";
        ret += "        buffer = calloc(len, sizeof(char));                                        \n";
        ret += "        clGetProgramBuildInfo(program, device_id, CL_PROGRAM_BUILD_LOG,            \n";
        ret += "                              len, buffer, NULL);                                  \n";
        ret += "                                                                                   \n";
        ret += "        fprintf(stderr, \"%s\\n\", buffer);                                        \n";
        ret += "        free(buffer);                                                              \n";
        ret += "        exit(-1);                                                                  \n";
        ret += "    }                                                                              \n";
        ret += "                                                                                   \n";
        ret += "    kernel = clCreateKernel(program, \"foo\", &err);                               \n";
        ret += "    if (!kernel || err != CL_SUCCESS) {                                            \n";
        ret += "        fprintf(stderr, \"Failed to create compute kernel.\\n\");                  \n";
        ret += "        exit(-1);                                                                  \n";
        ret += "    }                                                                              \n";
        ret += "                                                                                   \n";
        ret += "    err = clGetKernelWorkGroupInfo(kernel, device_id, CL_KERNEL_WORK_GROUP_SIZE,   \n";
        ret += "                                   sizeof(local), &local, NULL);                   \n";
        ret += "    if (err != CL_SUCCESS) {                                                       \n";
        ret += "        fprintf(stderr, \"Failed to retrieve kernel work group info.\\n\");        \n";
        ret += "        exit(-1);                                                                  \n";
        ret += "    }                                                                              \n";
        ret += "                                                                                   \n";
        ret += "    count = local * DATA_SIZE_MULTIPLIER;                                          \n";
        ret += "}                                                                                  \n";
        ret += "\n";

        ret += "void foo () {                                                                      \n";
        ret += "    err = clEnqueueNDRangeKernel(commands, kernel, 1, NULL,                        \n";
        ret += "                                 &count, &local, 0, NULL, NULL);                   \n";
        ret += "    if (err) {                                                                     \n";
        ret += "        fprintf(stderr, \"Failed to execute kernel.\\n\");                         \n";
        ret += "        exit(-1);                                                                  \n";
        ret += "    }                                                                              \n";
        ret += "                                                                                   \n";
        ret += "    clFinish(commands);                                                            \n";
        ret += "}                                                                                  \n";
        ret += "\n";

        ret += "void prepare_opencl_data () {\n";
        ret += extern_inp_sym_table->emit_ocl_data_init(SymbolTable::OCL_Data_Type::RONLY, "    ") + "\n\n";
        ret += extern_mix_sym_table->emit_ocl_data_init(SymbolTable::OCL_Data_Type::RW, "    ") + "\n\n";
        ret += extern_out_sym_table->emit_ocl_data_init(SymbolTable::OCL_Data_Type::WONLY, "    ") + "\n\n";
        ret += "}\n\n";
    }

    ret += "void init () {\n";
    ret += extern_inp_sym_table->emit_struct_init ("    ");
    ret += extern_mix_sym_table->emit_struct_init ("    ");
    ret += extern_out_sym_table->emit_struct_init ("    ");
    ret += "\n";
    if (options->is_opencl()) {
        ret += "    init_opencl ();\n";
        ret += "    prepare_opencl_data ();\n";
    }
    ret += "}";

    write_file(get_file_ext("init"), ret);
    return ret;
}

std::string Master::emit_decl () {
    std::string ret = "";
    /* TODO: none of it is used currently.
     * All these headers must be added only when they are really needed.
     * Parsing these headers is costly for compile time
    ret += "#include <cstdint>\n";
    ret += "#include <array>\n";
    ret += "#include <vector>\n";
    ret += "#include <valarray>\n\n";
    */

    if (options->is_opencl()) {
        ret += "#include <stdlib.h>\n";
        ret += "#include <CL/cl.h>\n";
        ret += "\n";
    }

    ret += "void hash(unsigned long long int *seed, unsigned long long int const v);\n\n";

    ret += extern_inp_sym_table->emit_variable_extern_decl() + "\n\n";
    ret += extern_mix_sym_table->emit_variable_extern_decl() + "\n\n";
    ret += extern_out_sym_table->emit_variable_extern_decl() + "\n\n";
    //TODO: what if we extand struct types in mix_sym_tabl
    ret += extern_inp_sym_table->emit_struct_type_def() + "\n\n";
    ret += extern_inp_sym_table->emit_struct_extern_decl() + "\n\n";
    ret += extern_mix_sym_table->emit_struct_extern_decl() + "\n\n";
    ret += extern_out_sym_table->emit_struct_extern_decl() + "\n\n";

    if (options->is_opencl()) {
        ret += "int err;\n";
        ret += "size_t local;\n";
        ret += "cl_context context;\n";
        ret += "cl_command_queue commands;\n";
        ret += "cl_program program;\n";
        ret += "cl_kernel kernel;\n";
        ret += "size_t count;\n";
        ret += "\n";
        ret += extern_inp_sym_table->emit_ocl_additional_data_decl() + "\n\n";
        ret += extern_mix_sym_table->emit_ocl_additional_data_decl() + "\n\n";
        ret += extern_out_sym_table->emit_ocl_additional_data_decl() + "\n\n";
    }

    write_file("init.h", ret);
    return ret;
}

std::string Master::emit_func () {
    std::string ret = "";
    if (!options->is_opencl())
        ret += "#include \"init.h\"\n\n";
    else {
        ret += extern_inp_sym_table->emit_struct_type_def() + "\n\n";
        ret += "__kernel ";
    }
    ret += "void foo (";
    if (options->is_opencl()) {
        std::string tmp_ret = extern_inp_sym_table->emit_ocl_kernel_args();
        ret += tmp_ret;
        tmp_ret = extern_mix_sym_table->emit_ocl_kernel_args();
        ret += tmp_ret.length() ? ", " + tmp_ret : "";
        tmp_ret = extern_out_sym_table->emit_ocl_kernel_args();
        ret += tmp_ret.length() ? ", " + tmp_ret : "";
    }
    ret += ") {\n";
    if (options->is_opencl())
        ret += "    int gid = get_global_id(0);\n";
    ret += program->emit();
    ret += "}";
    write_file(get_file_ext("func"), ret);
    return ret;
}

std::string Master::emit_hash () {
    std::string ret = "void hash(unsigned long long int *seed, unsigned long long int const v) {\n";
    ret += "    *seed ^= v + 0x9e3779b9 + ((*seed)<<6) + ((*seed)>>2);\n";
    ret += "}\n";
    write_file(get_file_ext("hash"), ret);
    return ret;
}

std::string Master::emit_check () { // TODO: rewrite with IR
    std::string ret = "";
    ret += "#include \"init.h\"\n\n";

    ret += "unsigned long long int checksum () {\n";

    std::shared_ptr<ScalarVariable> seed = std::make_shared<ScalarVariable>("seed", IntegerType::init(Type::IntegerTypeID::ULLINT));
    std::shared_ptr<VarUseExpr> seed_use = std::make_shared<VarUseExpr>(seed);

    BuiltinType::ScalarTypedVal zero_init (IntegerType::IntegerTypeID::ULLINT);
    zero_init.val.ullint_val = 0;
    std::shared_ptr<ConstExpr> const_init = std::make_shared<ConstExpr> (zero_init);

    std::shared_ptr<DeclStmt> seed_decl = std::make_shared<DeclStmt>(seed, const_init);

    ret += seed_decl->emit("    ") + "\n";

    if (options->is_opencl()) {
        ret += "    int fail = 0;\n";

        ret += extern_mix_sym_table->emit_ocl_data_check("    ");
        ret += extern_out_sym_table->emit_ocl_data_check("    ");

        ret += "    clReleaseProgram(program);\n";
        ret += "    clReleaseKernel(kernel);\n";
        ret += "    clReleaseCommandQueue(commands);\n";
        ret += "    clReleaseContext(context);\n";
        ret += "\n\n";
    }

    ret += extern_mix_sym_table->emit_variable_check ("    ");
    ret += extern_out_sym_table->emit_variable_check ("    ");

    ret += extern_mix_sym_table->emit_struct_check ("    ");
    ret += extern_out_sym_table->emit_struct_check ("    ");

    ret += "    return seed;\n";
    ret += "}";
    write_file(get_file_ext("check"), ret);
    return ret;
}

std::string Master::emit_main () {
    std::string ret = "";
    ret += "#include <stdio.h>\n";
    ret += "#include \"init.h\"\n\n";
    ret += "extern void init ();\n";
    ret += "extern void foo ();\n";
    ret += "extern unsigned long long int checksum ();\n\n";
    ret += "int main () {\n";
    ret += "    init ();\n";
    ret += "    foo ();\n";
    ret += "    printf(\"%llu\\n\", checksum ());";
    ret += "    return 0;\n";
    ret += "}";
    write_file(get_file_ext("driver"), ret);
    return ret;
}

