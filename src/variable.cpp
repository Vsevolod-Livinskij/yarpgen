/*
Copyright (c) 2015-2016, Intel Corporation

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

#include "type.h"
#include "variable.h"
#include "sym_table.h"

using namespace rl;

void Struct::allocate_members() {
    std::shared_ptr<StructType> struct_type = std::static_pointer_cast<StructType>(type);
    // TODO: struct member can be not only integer
    for (int i = 0; i < struct_type->get_num_of_members(); ++i) {
        std::shared_ptr<StructType::StructMember> cur_member = struct_type->get_member(i);
        if (cur_member->get_type()->is_int_type()) {
            if (struct_type->get_member(i)->get_type()->get_is_static()) {
//                std::cout << struct_type->get_member(i)->get_definition() << std::endl;
//                std::cout << struct_type->get_member(i)->get_data().get() << std::endl;
                members.push_back(struct_type->get_member(i)->get_data());
            }
            else {
                std::shared_ptr<IntegerType> int_mem_type = std::static_pointer_cast<IntegerType> (struct_type->get_member(i)->get_type());
                ScalarVariable new_mem (struct_type->get_member(i)->get_name(), int_mem_type);
                members.push_back(std::make_shared<ScalarVariable>(new_mem));
            }
        }
        else if (cur_member->get_type()->is_struct_type()) {
            if (struct_type->get_member(i)->get_type()->get_is_static()) {
//                std::cout << struct_type->get_member(i)->get_definition() << std::endl;
//                std::cout << struct_type->get_member(i)->get_data().get() << std::endl;
                members.push_back(struct_type->get_member(i)->get_data());
            }
            else {
                Struct new_struct (cur_member->get_name(), std::static_pointer_cast<StructType>(cur_member->get_type()));
                members.push_back(std::make_shared<Struct>(new_struct));
            }
        }
        else if (cur_member->get_type()->is_array_type()) {
            //TODO: It is a stub. REWRITE IT LATER!
        }
        else {
            std::cerr << "ERROR at " << __FILE__ << ":" << __LINE__ << ": unsupported type of struct member in Struct::allocate_members" << std::endl;
            exit(-1);
        }
    }
}

std::shared_ptr<Data> Struct::get_member (unsigned int num) {
    if (num >= members.size())
        return NULL;
    else
        return members.at(num);
}

void Struct::dbg_dump () {
    std::cout << "type ";
    type->dbg_dump();
    std::cout << "name: " << name << std::endl;
    std::cout << "modifier: " << type->get_modifier() << std::endl;
    std::cout << "members ";
    for (auto i : members) {
        i->dbg_dump();
    }
}

std::shared_ptr<Struct> Struct::generate (std::shared_ptr<Context> ctx) {
    //TODO: what about nested structs? StructType::generate need it. Should it take it itself from context?
    std::shared_ptr<Struct> ret = std::make_shared<Struct>(rand_val_gen->get_struct_var_name(), StructType::generate(ctx));
    ret->generate_members_init(ctx);
    return ret;
}

std::shared_ptr<Struct> Struct::generate (std::shared_ptr<Context> ctx, std::shared_ptr<StructType> struct_type) {
    std::shared_ptr<Struct> ret = std::make_shared<Struct>(rand_val_gen->get_struct_var_name(), struct_type);
    ret->generate_members_init(ctx);
    return ret;
}

void Struct::generate_members_init(std::shared_ptr<Context> ctx) {
    for (int i = 0; i < get_num_of_members(); ++i) {
        if (get_member(i)->get_type()->is_struct_type()) {
            std::static_pointer_cast<Struct>(get_member(i))->generate_members_init(ctx);
        }
        else if (get_member(i)->get_type()->is_int_type()) {
            std::shared_ptr<IntegerType> member_type = std::static_pointer_cast<IntegerType>(get_member(i)->get_type());
            AtomicType::ScalarTypedVal init_val = AtomicType::ScalarTypedVal::generate(ctx, member_type->get_min(), member_type->get_max());
            std::static_pointer_cast<ScalarVariable>(get_member(i))->set_init_value(init_val);
        }
        else {
            std::cerr << "ERROR at " << __FILE__ << ":" << __LINE__ << ": unsupported type of struct member in Struct::generate_members_init" << std::endl;
            exit(-1);
        }
    }
}

ScalarVariable::ScalarVariable (std::string _name, std::shared_ptr<IntegerType> _type) : Data (_name, _type, Data::VarClassID::VAR),
                    min(_type->get_int_type_id()), max(_type->get_int_type_id()),
                    init_val(_type->get_int_type_id()), cur_val(_type->get_int_type_id()) {
    min = _type->get_min();
    max = _type->get_max();
    init_val = _type->get_min();
    cur_val = _type->get_min();
    was_changed = false;
}

void ScalarVariable::dbg_dump () {
    std::cout << "type ";
    type->dbg_dump();
    std::cout << "name: " << name << std::endl;
    std::cout << "modifier: " << type->get_modifier() << std::endl;
    std::cout << "init_value: " << init_val << std::endl;
    std::cout << "was_changed " << was_changed << std::endl;
    std::cout << "cur_value: " << cur_val << std::endl;
    std::cout << "min: " << min << std::endl;
    std::cout << "max: " << max << std::endl;
}

std::shared_ptr<ScalarVariable> ScalarVariable::generate(std::shared_ptr<Context> ctx) {
    std::shared_ptr<ScalarVariable> ret = std::make_shared<ScalarVariable> (rand_val_gen->get_scalar_var_name(), IntegerType::generate(ctx));
    ret->set_init_value(AtomicType::ScalarTypedVal::generate(ctx, ret->get_type()->get_int_type_id()));
    return ret;
}

Array::Array(std::string _name, std::shared_ptr<ArrayType> _type, std::shared_ptr<Data> init_val) :
                  Data (_name, _type, Data::VarClassID::ARRAY), base_data(init_val) {
    std::shared_ptr<ArrayType> arr_type = std::static_pointer_cast<ArrayType>(type);
    std::shared_ptr<Type> base_type = arr_type->get_base_type();
    //TODO: can make this loop more optimal, but do we need it?
    for (uint32_t i = 0; i < arr_type->get_size(); ++i) {
        if (!base_type->is_array_type()) {
            if (base_type->get_type_id() != base_data->get_type()->get_type_id() ||
                //TODO: Is it strictly enough to ensure that types are equal?
                base_type->get_name() != base_data->get_type()->get_name()) {
                std::cerr << "ERROR at " << __FILE__ << ":" << __LINE__ << ": array base type and init type should be equal" << std::endl;
                exit(-1);
            }
            //TODO: we have a mess with base_data name, but who cares?
            data.push_back(base_data);
        }
        else {
            std::shared_ptr<ArrayType> base_arr_type = std::static_pointer_cast<ArrayType>(base_type);
            data.push_back(std::make_shared<Array>(name + " [" + std::to_string(i) + "]", base_arr_type, base_data));
        }
    }
}

void Array::dbg_dump() {
    std::cout << "name: " << name << std::endl;
    std::cout << "array type: " << std::endl;
    type->dbg_dump();
    std::cout << "base data: " << std::endl;
    base_data->dbg_dump();
    std::cout << "elements: " << std::endl;
    for (auto i : data)
        i->dbg_dump();
}

std::shared_ptr<Array> Array::generate(std::shared_ptr<Context> ctx, std::shared_ptr<ArrayType> arr_type) {
    std::shared_ptr<Type> base_init_type = arr_type->get_lower_base_type();
    std::shared_ptr<Data> init_val = NULL;
    if (base_init_type->is_struct_type())
        init_val = std::make_shared<Struct>("", std::static_pointer_cast<StructType>(base_init_type));
    else if (base_init_type->is_int_type())
        init_val = std::make_shared<ScalarVariable>("", std::static_pointer_cast<IntegerType>(base_init_type));
    return std::make_shared<Array>(rand_val_gen->get_array_var_name(), arr_type, init_val);
}
