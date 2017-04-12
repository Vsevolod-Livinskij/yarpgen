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
#include "sym_table.h"
#include "variable.h"

using namespace rl;

std::string Type::get_name () {
    std::string ret = "";
    ret += get_spec_and_mod_str();
    ret += name;
    if (align != 0)
        ret += " __attribute__(aligned(" + std::to_string(align) + "))";
    return ret;
}

std::string Type::get_spec_and_mod_str() {
    std::string ret = "";
    ret += is_static ? "static " : "";
    switch (modifier) {
        case Mod::VOLAT:
            ret += "volatile ";
            break;
        case Mod::CONST:
            ret += "const ";
            break;
        case Mod::CONST_VOLAT:
            ret += "const volatile ";
            break;
        case Mod::NTHG:
            break;
        case Mod::MAX_MOD:
            std::cerr << "ERROR in Type::get_name: bad modifier" << std::endl;
            exit(-1);
            break;
    }
    return ret;
}

StructType::StructMember::StructMember (std::shared_ptr<Type> _type, std::string _name) : type(_type), name(_name), data(NULL) {
    if (!type->get_is_static())
        return;
    if (type->is_int_type())
        data = std::make_shared<ScalarVariable>(name, std::static_pointer_cast<IntegerType>(type));
    else if (type->is_struct_type())
        data = std::make_shared<Struct>(name, std::static_pointer_cast<StructType>(type));
    else if (type->is_array_type()) {
        //TODO: Add data for array
        //TODO: It is a stub. REWRITE IT LATER!
    }
    else {
        std::cerr << "ERROR at " << __FILE__ << ":" << __LINE__ << ": unsupported data type in StructType::StructMember::StructMember" << std::endl;
        exit(-1);
    }
}

void StructType::add_member (std::shared_ptr<Type> _type, std::string _name) {
    StructType::StructMember new_mem (_type, _name);
    if (_type->is_struct_type()) {
        nest_depth = std::static_pointer_cast<StructType>(_type)->get_nest_depth() >= nest_depth ?
                     std::static_pointer_cast<StructType>(_type)->get_nest_depth() + 1 : nest_depth;
    }
    members.push_back(std::make_shared<StructMember>(new_mem));
    shadow_members.push_back(std::make_shared<StructMember>(new_mem));
}

std::shared_ptr<StructType::StructMember> StructType::get_member (unsigned int num) {
    if (num >= members.size())
        return NULL;
    else
        return members.at(num);
}

std::string StructType::StructMember::get_definition (std::string offset) {
    std::string ret = offset + type->get_name() + " " + name;
    if (type->get_is_bit_field())
        ret += " : " + std::to_string(std::static_pointer_cast<BitField>(type)->get_bit_field_width());
    return ret;
}

std::string StructType::get_definition (std::string offset) {
    std::string ret = "";
    ret+= "struct " + name + " {\n";
    for (auto i : shadow_members) {
        ret += i->get_definition(offset + "    ") + ";\n";
    }
    ret += "};\n";
    return ret;
}

std::string StructType::get_static_memb_def (std::string offset) {
    std::string ret = "";
    for (auto i : members) {
        if (i->get_type()->get_is_static())
        ret += offset + i->get_type()->get_simple_name() + " " + name + "::" + i->get_name() + ";\n";
    }
    return ret;
}

uint64_t StructType::get_nest_struct_depth() {
    uint64_t ret = 0;
    for (const auto &member : members)
        ret = std::max(ret, member->get_type()->get_nest_struct_depth());
    return ret + 1;
}

uint64_t StructType::get_nest_array_depth() {
    uint64_t ret = 0;
    for (auto i : members)
        ret = std::max(ret, i->get_type()->get_nest_array_depth());
    return ret;
}

void StructType::dbg_dump() {
    std::cout << get_definition () << std::endl;
    std::cout << "depth: " << nest_depth << std::endl;
}

std::shared_ptr<StructType> StructType::generate (std::shared_ptr<Context> ctx) {
    std::vector<std::shared_ptr<StructType>> empty_struct_vec;
    std::vector<std::shared_ptr<ArrayType>> empty_arr_vec;
    return generate(ctx, empty_struct_vec, empty_arr_vec);
}

std::shared_ptr<ArrayType> StructType::get_suitable_array (std::vector<std::shared_ptr<ArrayType>> avail_subtypes,
                                                           uint64_t max_depth) {
    std::vector<std::shared_ptr<ArrayType>> suitable_subtypes;
    for (const auto &subtype : avail_subtypes)
        if (subtype->get_nest_struct_depth() + 1 <= max_depth)
            suitable_subtypes.push_back(subtype);

    if (suitable_subtypes.size() > 0) {
        uint64_t subtype_idx = rand_val_gen->get_rand_value(0UL, suitable_subtypes.size() - 1);
        return suitable_subtypes.at(subtype_idx);
    }
    else
        return NULL;
}

static Type::TypeID make_member_type_id_choice (std::shared_ptr<Context> ctx) {
    Type::TypeID ret = Type::MAX_TYPE_ID;
    // The problem is that sometimes we can't honestly choose member class.
    // The solution is to make the choice by hand.
    auto find_block_cond = [] (Probability<Type::TypeID> elem) {
        return (elem.get_id() == Type::ATOMIC_TYPE && elem.get_prob() > 0) ||
               (elem.get_id() == Type::ARRAY_TYPE  && elem.get_prob() > 0);
    };
    if (std::find_if(ctx->get_gen_policy()->get_member_type_id_prob().begin(),
                     ctx->get_gen_policy()->get_member_type_id_prob().end(),
                     find_block_cond) ==
                     ctx->get_gen_policy()->get_member_type_id_prob().end())
        return Type::ATOMIC_TYPE;

    while (ret != Type::ATOMIC_TYPE && ret != Type::ARRAY_TYPE)
        ret = rand_val_gen->get_rand_id(ctx->get_gen_policy()->get_member_type_id_prob());
    return ret;
}

std::shared_ptr<StructType> StructType::generate (std::shared_ptr<Context> ctx,
                                                  std::vector<std::shared_ptr<StructType>> nested_struct_types,
                                                  std::vector<std::shared_ptr<ArrayType>> arrays_with_structs) {
    Type::Mod primary_mod = ctx->get_gen_policy()->get_allowed_modifiers().at(rand_val_gen->get_rand_value<int>(0, ctx->get_gen_policy()->get_allowed_modifiers().size() - 1));

    bool primary_static_spec = false;
    if (ctx->get_gen_policy()->get_allow_static_var())
        primary_static_spec = rand_val_gen->get_rand_id(ctx->get_gen_policy()->get_static_member_prob());
    else
        primary_static_spec = false;
    //TODO: what about align?

    Type::TypeID member_class = make_member_type_id_choice(ctx);

    std::shared_ptr<Type> primary_type = NULL;
    if (member_class == Type::ARRAY_TYPE)
        primary_type = get_suitable_array(arrays_with_structs, ctx->get_gen_policy()->get_max_struct_depth());

    if (member_class == Type::ATOMIC_TYPE ||
       // If we can't choose suitable array type, we fall back to AtomicType, see above
       (primary_type == NULL && member_class == Type::ARRAY_TYPE)) {
        IntegerType::IntegerTypeID int_type_id = rand_val_gen->get_rand_id(ctx->get_gen_policy()->get_allowed_int_types());
        primary_type = IntegerType::init(int_type_id, primary_mod, primary_static_spec, 0);
    }
    else if (member_class == Type::ARRAY_TYPE) {
        //If we can't choose suitable array type, we fall back to AtomicType, see above
    }
    else {
        std::cerr << "ERROR at " << __FILE__ << ":" << __LINE__ << ": unsupported member type" << std::endl;
        exit(-1);
    }

    std::shared_ptr<StructType> struct_type = std::make_shared<StructType>(rand_val_gen->get_struct_type_name());
    int struct_member_num = rand_val_gen->get_rand_value<int>(ctx->get_gen_policy()->get_min_struct_members_num(), ctx->get_gen_policy()->get_max_struct_members_num());
    int member_num = 0;
    for (int i = 0; i < struct_member_num; ++i) {
        if (ctx->get_gen_policy()->get_allow_mix_mod_in_struct()) {
            primary_mod = ctx->get_gen_policy()->get_allowed_modifiers().at(rand_val_gen->get_rand_value<int>(0, ctx->get_gen_policy()->get_allowed_modifiers().size() - 1));;
        }

        if (ctx->get_gen_policy()->get_allow_mix_static_in_struct()) {
            primary_static_spec = ctx->get_gen_policy()->get_allow_static_members() ? rand_val_gen->get_rand_value<int>(0, 1) : false;
        }

        if (ctx->get_gen_policy()->get_allow_mix_types_in_struct()) {
            member_class = rand_val_gen->get_rand_id(ctx->get_gen_policy()->get_member_type_id_prob());
            bool add_substruct = false;
            int substruct_type_idx = 0;
            std::shared_ptr<StructType> substruct_type = NULL;
            if (member_class == Type::STRUCT_TYPE && ctx->get_gen_policy()->get_max_struct_depth() > 0 && nested_struct_types.size() > 0) {
                substruct_type_idx = rand_val_gen->get_rand_value<int>(0, nested_struct_types.size() - 1);
                substruct_type = nested_struct_types.at(substruct_type_idx);
                if (substruct_type->get_nest_depth() + 1 == ctx->get_gen_policy()->get_max_struct_depth()) {
                    add_substruct = false;
                }
                else {
                    add_substruct = true;
                }
            }
            if (add_substruct) {
                primary_type = std::make_shared<StructType>(*substruct_type);
            }
            else {
                member_class = make_member_type_id_choice(ctx);
                primary_type = NULL;
                if (member_class == Type::ARRAY_TYPE) {
                    primary_type = get_suitable_array(arrays_with_structs, ctx->get_gen_policy()->get_max_struct_depth());
                }
                if (member_class == Type::ATOMIC_TYPE ||
                   (primary_type == NULL && member_class == Type::ARRAY_TYPE)) {
                    GenPolicy::BitFieldID bit_field_dis = rand_val_gen->get_rand_id(ctx->get_gen_policy()->get_bit_field_prob());
                    if (bit_field_dis == GenPolicy::BitFieldID::UNNAMED) {
                        struct_type->add_shadow_member(BitField::generate(ctx, true));
                        continue;
                    }
                    else if (bit_field_dis == GenPolicy::BitFieldID::NAMED) {
                        primary_type = BitField::generate(ctx);
                        primary_static_spec = false; // BitField can't be static member of struct
                    }
                    else {
                        primary_type = IntegerType::generate(ctx);
                    }
                }
                else if (member_class == Type::ARRAY_TYPE) {
                }
                else {
                    std::cerr << "ERROR at " << __FILE__ << ":" << __LINE__ << ": unsupported member type" << std::endl;
                    exit(-1);
                }
            }
        }
        primary_type->set_modifier(primary_mod);
        primary_type->set_is_static(primary_static_spec);
        struct_type->add_member(primary_type, "member_" + std::to_string(rand_val_gen->get_struct_type_num()) + "_" + std::to_string(member_num++));
    }
    return struct_type;
}

//TODO: maybe we can use template instead of it?
#define CAST_CASE(new_val_memb)                                     \
switch (int_type_id) {                                              \
    case Type::IntegerTypeID::BOOL:                                 \
        new_val_memb = val.bool_val;                                \
        break;                                                      \
    case Type::IntegerTypeID::CHAR:                                 \
        new_val_memb = val.char_val;                                \
        break;                                                      \
    case Type::IntegerTypeID::UCHAR:                                \
        new_val_memb = val.uchar_val;                               \
        break;                                                      \
    case Type::IntegerTypeID::SHRT:                                 \
        new_val_memb = val.shrt_val;                                \
        break;                                                      \
    case Type::IntegerTypeID::USHRT:                                \
        new_val_memb = val.ushrt_val;                               \
        break;                                                      \
    case Type::IntegerTypeID::INT:                                  \
        new_val_memb = val.int_val;                                 \
        break;                                                      \
    case Type::IntegerTypeID::UINT:                                 \
        new_val_memb = val.uint_val;                                \
        break;                                                      \
    case Type::IntegerTypeID::LINT:                                 \
        new_val_memb = val.lint_val;                                \
        break;                                                      \
    case Type::IntegerTypeID::ULINT:                                \
        new_val_memb = val.ulint_val;                               \
        break;                                                      \
    case Type::IntegerTypeID::LLINT:                                \
        new_val_memb = val.llint_val;                               \
        break;                                                      \
    case Type::IntegerTypeID::ULLINT:                               \
        new_val_memb = val.ullint_val;                              \
        break;                                                      \
    case Type::IntegerTypeID::MAX_INT_ID:                           \
        std::cerr << "ERROR at " << __FILE__ << ":" << __LINE__ << ": unsupported int type in AtomicType::ScalarTypedVal::cast_type" << std::endl;\
        exit(-1);                                                   \
} 

AtomicType::ScalarTypedVal AtomicType::ScalarTypedVal::cast_type (Type::IntegerTypeID to_type_id) {
    ScalarTypedVal new_val = ScalarTypedVal (to_type_id);
    switch (to_type_id) {
        case Type::IntegerTypeID::BOOL:
            CAST_CASE(new_val.val.bool_val)
            break;
        case Type::IntegerTypeID::CHAR:
            CAST_CASE(new_val.val.char_val)
            break;
        case Type::IntegerTypeID::UCHAR:
            CAST_CASE(new_val.val.uchar_val)
            break;
        case Type::IntegerTypeID::SHRT:
            CAST_CASE(new_val.val.shrt_val)
            break;
        case Type::IntegerTypeID::USHRT:
            CAST_CASE(new_val.val.ushrt_val)
            break;
        case Type::IntegerTypeID::INT:
            CAST_CASE(new_val.val.int_val)
            break;
        case Type::IntegerTypeID::UINT:
            CAST_CASE(new_val.val.uint_val)
            break;
        case Type::IntegerTypeID::LINT:
            CAST_CASE(new_val.val.lint_val)
            break;
        case Type::IntegerTypeID::ULINT:
            CAST_CASE(new_val.val.ulint_val)
            break;
        case Type::IntegerTypeID::LLINT:
            CAST_CASE(new_val.val.llint_val)
            break;
        case Type::IntegerTypeID::ULLINT:
            CAST_CASE(new_val.val.ullint_val)
            break;
        case Type::IntegerTypeID::MAX_INT_ID:                           \
            std::cerr << "ERROR at " << __FILE__ << ":" << __LINE__ << ": unsupported int type in AtomicType::ScalarTypedVal::cast_type" << std::endl;
            exit(-1);
    }
    return new_val;
}

AtomicType::ScalarTypedVal AtomicType::ScalarTypedVal::pre_op (bool inc) { // Prefix
    AtomicType::ScalarTypedVal ret = *this;
    int add = inc ? 1 : -1;
    switch (int_type_id) {
        case IntegerType::IntegerTypeID::BOOL:
            std::cerr << "ERROR at " << __FILE__ << ":" << __LINE__ << ": bool is illegal in dec and inc operators in AtomicType::ScalarTypedVal::pre_op" << std::endl;
            exit(-1);
        case IntegerType::IntegerTypeID::CHAR:
            if (val.char_val == CHAR_MAX)
                ret.set_ub(UB::SignOvf);
            else
                ret.val.char_val = val.char_val + add;
            break;
        case IntegerType::IntegerTypeID::UCHAR:
            ret.val.uchar_val = val.uchar_val + add;
        case IntegerType::IntegerTypeID::SHRT:
            if (val.shrt_val == SHRT_MAX)
                ret.set_ub(UB::SignOvf);
            else
                ret.val.shrt_val = val.shrt_val + add;
            break;
        case IntegerType::IntegerTypeID::USHRT:
            ret.val.ushrt_val = val.ushrt_val + add;
        case IntegerType::IntegerTypeID::INT:
            if (val.int_val == INT_MAX)
                ret.set_ub(UB::SignOvf);
            else
                ret.val.int_val = val.int_val + add;
            break;
        case IntegerType::IntegerTypeID::UINT:
            ret.val.uint_val = val.uint_val + add;
            break;
        case IntegerType::IntegerTypeID::LINT:
            if (val.lint_val == LONG_MAX)
                ret.set_ub(UB::SignOvf);
            else
                ret.val.lint_val = val.lint_val + add;
            break;
        case IntegerType::IntegerTypeID::ULINT:
            ret.val.ulint_val = val.ulint_val + add;
            break;
        case IntegerType::IntegerTypeID::LLINT:
            if (val.llint_val == LLONG_MAX)
                ret.set_ub(UB::SignOvf);
            else
                ret.val.llint_val = val.llint_val + add;
            break;
        case IntegerType::IntegerTypeID::ULLINT:
            ret.val.ullint_val = val.ullint_val + add;
            break;
        case IntegerType::IntegerTypeID::MAX_INT_ID:
            std::cerr << "ERROR at " << __FILE__ << ":" << __LINE__ << ": perform propagate_type in AtomicType::ScalarTypedVal::pre_op" << std::endl;
            exit(-1);
    }
    return ret;
}

AtomicType::ScalarTypedVal AtomicType::ScalarTypedVal::operator- () {
    AtomicType::ScalarTypedVal ret = *this;
    switch (int_type_id) {
        case IntegerType::IntegerTypeID::BOOL:
        case IntegerType::IntegerTypeID::CHAR:
        case IntegerType::IntegerTypeID::UCHAR:
        case IntegerType::IntegerTypeID::SHRT:
        case IntegerType::IntegerTypeID::USHRT:
        case IntegerType::IntegerTypeID::MAX_INT_ID:
            std::cerr << "ERROR at " << __FILE__ << ":" << __LINE__ << ": perform propagate_type in AtomicType::ScalarTypedVal::operator-" << std::endl;
            exit(-1);
        case IntegerType::IntegerTypeID::INT:
            if (val.int_val == INT_MIN)
                ret.set_ub(UB::SignOvf);
            else
                ret.val.int_val = -val.int_val;
            break;
        case IntegerType::IntegerTypeID::UINT:
            ret.val.uint_val = -val.uint_val;
            break;
        case IntegerType::IntegerTypeID::LINT:
            if (val.lint_val == LONG_MIN)
                ret.set_ub(UB::SignOvf);
            else
                ret.val.lint_val = -val.lint_val;
            break;
        case IntegerType::IntegerTypeID::ULINT:
            ret.val.ulint_val = -val.ulint_val;
            break;
        case IntegerType::IntegerTypeID::LLINT:
            if (val.llint_val == LLONG_MIN)
                ret.set_ub(UB::SignOvf);
            else
                ret.val.llint_val = -val.llint_val;
            break;
        case IntegerType::IntegerTypeID::ULLINT:
            ret.val.ullint_val = -val.ullint_val;
            break;
    }
    return ret;

}

AtomicType::ScalarTypedVal AtomicType::ScalarTypedVal::operator~ () {
    AtomicType::ScalarTypedVal ret = *this;
    switch (int_type_id) {
        case IntegerType::IntegerTypeID::BOOL:
        case IntegerType::IntegerTypeID::CHAR:
        case IntegerType::IntegerTypeID::UCHAR:
        case IntegerType::IntegerTypeID::SHRT:
        case IntegerType::IntegerTypeID::USHRT:
        case IntegerType::IntegerTypeID::MAX_INT_ID:
            std::cerr << "ERROR at " << __FILE__ << ":" << __LINE__ << ": perform propagate_type in AtomicType::ScalarTypedVal::operator~" << std::endl;
            exit(-1);
        case IntegerType::IntegerTypeID::INT:
            ret.val.int_val = ~val.int_val;
            break;
        case IntegerType::IntegerTypeID::UINT:
            ret.val.uint_val = ~val.uint_val;
            break;
        case IntegerType::IntegerTypeID::LINT:
            ret.val.lint_val = ~val.lint_val;
            break;
        case IntegerType::IntegerTypeID::ULINT:
            ret.val.ulint_val = ~val.ulint_val;
            break;
        case IntegerType::IntegerTypeID::LLINT:
            ret.val.llint_val = ~val.llint_val;
            break;
        case IntegerType::IntegerTypeID::ULLINT:
            ret.val.ullint_val = ~val.ullint_val;
            break;
    }
    return ret;
}

uint64_t AtomicType::ScalarTypedVal::get_abs_val () {
    switch (int_type_id) {
        case IntegerType::IntegerTypeID::BOOL:
            return val.bool_val;
        case IntegerType::IntegerTypeID::CHAR:
            return std::abs(val.char_val);
        case IntegerType::IntegerTypeID::UCHAR:
            return val.uchar_val;
        case IntegerType::IntegerTypeID::SHRT:
            return std::abs(val.shrt_val);
        case IntegerType::IntegerTypeID::USHRT:
            return val.ushrt_val;
        case IntegerType::IntegerTypeID::INT:
            return std::abs(val.int_val);
        case IntegerType::IntegerTypeID::UINT:
            return val.uint_val;
        case IntegerType::IntegerTypeID::LINT:
            return std::abs(val.lint_val);
        case IntegerType::IntegerTypeID::ULINT:
            return val.ulint_val;
        case IntegerType::IntegerTypeID::LLINT:
            return std::abs(val.llint_val);
        case IntegerType::IntegerTypeID::ULLINT:
            return val.ullint_val;
        case IntegerType::IntegerTypeID::MAX_INT_ID:
            std::cerr << "ERROR at " << __FILE__ << ":" << __LINE__ << ": perform propagate_type in AtomicType::ScalarTypedVal::get_abs_val" << std::endl;
            exit(-1);
    }
    // TODO: add unreachable macro to the project.
    std::cerr << "ERROR at " << __FILE__ << ":" << __LINE__ << "reachin unreachable code in AtomicType::ScalarTypedVal::get_abs_val" << std::endl;
    exit(-1);
}

void AtomicType::ScalarTypedVal::set_abs_val (uint64_t new_val) {
    switch (int_type_id) {
        case IntegerType::IntegerTypeID::BOOL:
            val.bool_val = new_val;
            break;
        case IntegerType::IntegerTypeID::CHAR:
            val.char_val = new_val;
            break;
        case IntegerType::IntegerTypeID::UCHAR:
            val.uchar_val = new_val;
            break;
        case IntegerType::IntegerTypeID::SHRT:
            val.shrt_val = new_val;
            break;
        case IntegerType::IntegerTypeID::USHRT:
            val.ushrt_val = new_val;
            break;
        case IntegerType::IntegerTypeID::INT:
            val.int_val = new_val;
            break;
        case IntegerType::IntegerTypeID::UINT:
            val.uint_val = new_val;
            break;
        case IntegerType::IntegerTypeID::LINT:
            val.lint_val = new_val;
            break;
        case IntegerType::IntegerTypeID::ULINT:
            val.ulint_val = new_val;
            break;
        case IntegerType::IntegerTypeID::LLINT:
            val.llint_val = new_val;
            break;
        case IntegerType::IntegerTypeID::ULLINT:
            val.ullint_val = new_val;
            break;
        case IntegerType::IntegerTypeID::MAX_INT_ID:
            std::cerr << "ERROR at " << __FILE__ << ":" << __LINE__ << ": perform propagate_type in AtomicType::ScalarTypedVal::set_abs_val" << std::endl;
            exit(-1);
    }
}

AtomicType::ScalarTypedVal AtomicType::ScalarTypedVal::operator! () {
    AtomicType::ScalarTypedVal ret = AtomicType::ScalarTypedVal(Type::IntegerTypeID::BOOL);
    switch (int_type_id) {
        case IntegerType::IntegerTypeID::BOOL:
            ret.val.bool_val = !val.bool_val;
            break;
        case IntegerType::IntegerTypeID::CHAR:
        case IntegerType::IntegerTypeID::UCHAR:
        case IntegerType::IntegerTypeID::SHRT:
        case IntegerType::IntegerTypeID::USHRT:
        case IntegerType::IntegerTypeID::INT:
        case IntegerType::IntegerTypeID::UINT:
        case IntegerType::IntegerTypeID::LINT:
        case IntegerType::IntegerTypeID::ULINT:
        case IntegerType::IntegerTypeID::LLINT:
        case IntegerType::IntegerTypeID::ULLINT:
        case IntegerType::IntegerTypeID::MAX_INT_ID:
            std::cerr << "ERROR at " << __FILE__ << ":" << __LINE__ << ": perform propagate_type in AtomicType::ScalarTypedVal::operator!" << std::endl;
            exit(-1);
    }
    return ret;
}

AtomicType::ScalarTypedVal AtomicType::ScalarTypedVal::operator+ (ScalarTypedVal rhs) {
    AtomicType::ScalarTypedVal ret = *this;

    bool long_eq_long_long =  sizeof(long int) == sizeof(long long int);
    int64_t s_tmp = 0;
    uint64_t u_tmp = 0;

    switch (int_type_id) {
        case IntegerType::IntegerTypeID::BOOL:
        case IntegerType::IntegerTypeID::CHAR:
        case IntegerType::IntegerTypeID::UCHAR:
        case IntegerType::IntegerTypeID::SHRT:
        case IntegerType::IntegerTypeID::USHRT:
        case IntegerType::IntegerTypeID::MAX_INT_ID:
            std::cerr << "ERROR at " << __FILE__ << ":" << __LINE__ << ": perform propagate_type in AtomicType::ScalarTypedVal::operator+" << std::endl;
            exit(-1);
        case IntegerType::IntegerTypeID::INT:
            s_tmp = (long long int) val.int_val + (long long int) rhs.val.int_val;
            if (s_tmp < INT_MIN || s_tmp > INT_MAX)
                ret.set_ub(SignOvf);
            else
                ret.val.int_val = (int) s_tmp;
            break;
        case IntegerType::IntegerTypeID::UINT:
            ret.val.uint_val = val.uint_val + rhs.val.uint_val;
            break;
        case IntegerType::IntegerTypeID::LINT:
            if (!long_eq_long_long) {
                s_tmp = (long long int) val.lint_val + (long long int) rhs.val.lint_val;
                if (s_tmp < LONG_MIN || s_tmp > LONG_MAX)
                    ret.set_ub(SignOvf);
                else
                    ret.val.lint_val = (long int) s_tmp;
            }
            else {
                uint64_t ua = val.lint_val;
                uint64_t ub = rhs.val.lint_val;
                u_tmp = ua + ub;
                ua = (ua >> 63) + LONG_MAX;
                if ((int64_t) ((ua ^ ub) | ~(ub ^ u_tmp)) >= 0)
                    ret.set_ub(SignOvf);
                else
                    ret.val.lint_val = (long int) u_tmp;
            }
            break;
        case IntegerType::IntegerTypeID::ULINT:
            ret.val.ulint_val = val.ulint_val + rhs.val.ulint_val;
            break;
        case IntegerType::IntegerTypeID::LLINT:
        {
            uint64_t ua = val.llint_val;
            uint64_t ub = rhs.val.llint_val;
            u_tmp = ua + ub;
            ua = (ua >> 63) + LLONG_MAX;
            if ((int64_t) ((ua ^ ub) | ~(ub ^ u_tmp)) >= 0)
                ret.set_ub(SignOvf);
            else
                ret.val.llint_val =  val.llint_val + rhs.val.llint_val;
            break;
        }
        case IntegerType::IntegerTypeID::ULLINT:
            ret.val.ullint_val = val.ullint_val + rhs.val.ullint_val;
            break;
    }
    return ret;
}

AtomicType::ScalarTypedVal AtomicType::ScalarTypedVal::operator- (ScalarTypedVal rhs) {
    AtomicType::ScalarTypedVal ret = *this;

    bool long_eq_long_long =  sizeof(long int) == sizeof(long long int);
    int64_t s_tmp = 0;
    uint64_t u_tmp = 0;

    switch (int_type_id) {
        case IntegerType::IntegerTypeID::BOOL:
        case IntegerType::IntegerTypeID::CHAR:
        case IntegerType::IntegerTypeID::UCHAR:
        case IntegerType::IntegerTypeID::SHRT:
        case IntegerType::IntegerTypeID::USHRT:
        case IntegerType::IntegerTypeID::MAX_INT_ID:
            std::cerr << "ERROR at " << __FILE__ << ":" << __LINE__ << ": perform propagate_type in AtomicType::ScalarTypedVal::operator-" << std::endl;
            exit(-1);
        case IntegerType::IntegerTypeID::INT:
            s_tmp = (long long int) val.int_val - (long long int) rhs.val.int_val;
            if (s_tmp < INT_MIN || s_tmp > INT_MAX)
                ret.set_ub(SignOvf);
            else
                ret.val.int_val = (int) s_tmp;
            break;
        case IntegerType::IntegerTypeID::UINT:
            ret.val.uint_val = val.uint_val - rhs.val.uint_val;
            break;
        case IntegerType::IntegerTypeID::LINT:
            if (!long_eq_long_long) {
                s_tmp = (long long int) val.lint_val - (long long int) rhs.val.lint_val;
                if (s_tmp < LONG_MIN || s_tmp > LONG_MAX)
                    ret.set_ub(SignOvf);
                else
                    ret.val.lint_val = (long int) s_tmp;
            }
            else {
                uint64_t ua = val.lint_val;
                uint64_t ub = rhs.val.lint_val;
                u_tmp = ua - ub;
                ua = (ua >> 63) + LONG_MAX;
                if ((int64_t) ((ua ^ ub) & (ua ^ u_tmp)) < 0)
                    ret.set_ub(SignOvf);
                else
                    ret.val.lint_val = (long int) u_tmp;
            }
            break;
        case IntegerType::IntegerTypeID::ULINT:
            ret.val.ulint_val = val.ulint_val - rhs.val.ulint_val;
            break;
        case IntegerType::IntegerTypeID::LLINT:
        {
            uint64_t ua = val.llint_val;
            uint64_t ub = rhs.val.llint_val;
            u_tmp = ua - ub;
            ua = (ua >> 63) + LLONG_MAX;
            if ((int64_t) ((ua ^ ub) & (ua ^ u_tmp)) < 0)
                ret.set_ub(SignOvf);
            else
                ret.val.llint_val = (long long int) u_tmp;
            break;
        }
        case IntegerType::IntegerTypeID::ULLINT:
            ret.val.ullint_val = val.ullint_val - rhs.val.ullint_val;
            break;
    }
    return ret;
}

static bool check_int64_mul (int64_t a, int64_t b, int64_t* res) {
    uint64_t ret = 0;

    int8_t sign = (((a > 0) && (b > 0)) || ((a < 0) && (b < 0))) ? 1 : -1;
    uint64_t a_abs = 0;
    uint64_t b_abs = 0;

    if (a == INT64_MIN)
        // Operation "-" is undefined for "INT64_MIN", as it causes overflow.
        // But converting INT64_MIN to unsigned type yields the correct result,
        // i.e. it will be positive value -INT64_MIN.
        // See 6.3.1.3 section in C99 standart for more details (ISPC follows
        // C standard, unless it's specifically different in the language).
        a_abs = (uint64_t) INT64_MIN;
    else
        a_abs = (a > 0) ? a : -a;

    if (b == INT64_MIN)
        b_abs = (uint64_t) INT64_MIN;
    else
        b_abs = (b > 0) ? b : -b;

    uint32_t a0 = a_abs & 0xFFFFFFFF;
    uint32_t b0 = b_abs & 0xFFFFFFFF;
    uint32_t a1 = a_abs >> 32;
    uint32_t b1 = b_abs >> 32;

    if ((a1 != 0) && (b1 != 0))
        return false;

    uint64_t tmp = (((uint64_t) a1) * b0) + (((uint64_t) b1) * a0);
    if (tmp > 0xFFFFFFFF)
        return false;

    ret = (tmp << 32) + (((uint64_t) a0) * b0);
    if (ret < (tmp << 32))
        return false;

    if ((sign < 0) && (ret > (uint64_t) INT64_MIN)) {
        return false;
    } else if ((sign > 0) && (ret > INT64_MAX)) {
        return false;
    } else {
        *res = ret * sign;
    }
    return true;
}

AtomicType::ScalarTypedVal AtomicType::ScalarTypedVal::operator* (ScalarTypedVal rhs) {
    AtomicType::ScalarTypedVal ret = *this;

    bool long_eq_long_long =  sizeof(long int) == sizeof(long long int);
    int64_t s_tmp = 0;

    switch (int_type_id) {
        case IntegerType::IntegerTypeID::BOOL:
        case IntegerType::IntegerTypeID::CHAR:
        case IntegerType::IntegerTypeID::UCHAR:
        case IntegerType::IntegerTypeID::SHRT:
        case IntegerType::IntegerTypeID::USHRT:
        case IntegerType::IntegerTypeID::MAX_INT_ID:
            std::cerr << "ERROR at " << __FILE__ << ":" << __LINE__ << ": perform propagate_type in AtomicType::ScalarTypedVal::operator*" << std::endl;
            exit(-1);
        case IntegerType::IntegerTypeID::INT:
            s_tmp = (long long int) val.int_val * (long long int) rhs.val.int_val;
            if ((int) val.int_val == INT_MIN && (int) rhs.val.int_val == -1)
                ret.set_ub(SignOvfMin);
            else if (s_tmp < INT_MIN || s_tmp > INT_MAX)
                ret.set_ub(SignOvf);
            else
                ret.val.int_val = (int) s_tmp;
            break;
        case IntegerType::IntegerTypeID::UINT:
            ret.val.uint_val = val.uint_val * rhs.val.uint_val;
            break;
        case IntegerType::IntegerTypeID::LINT:
            if (val.lint_val == LONG_MIN && (long int) rhs.val.lint_val == -1)
                ret.set_ub(SignOvfMin);
            else if (!long_eq_long_long) {
                s_tmp = (long long int) val.lint_val * (long long int) rhs.val.lint_val;
                if (s_tmp < LONG_MIN || s_tmp > LONG_MAX)
                    ret.set_ub(SignOvf);
                else
                    ret.val.lint_val = (long int) s_tmp;
            }
            else {
                if (!check_int64_mul(val.lint_val, rhs.val.lint_val, &s_tmp))
                    ret.set_ub(SignOvf);
                else
                    ret.val.lint_val = (long int) s_tmp;
            }
            break;
        case IntegerType::IntegerTypeID::ULINT:
            ret.val.ulint_val = val.ulint_val * rhs.val.ulint_val;
            break;
        case IntegerType::IntegerTypeID::LLINT:
            if ((long long int) val.llint_val == LLONG_MIN && (long long int) rhs.val.llint_val == -1)
                ret.set_ub(SignOvfMin);
            else if (!check_int64_mul(val.llint_val, rhs.val.llint_val, &s_tmp))
                ret.set_ub(SignOvfMin);
            else
                ret.val.llint_val = (long long int) s_tmp;
            break;
        case IntegerType::IntegerTypeID::ULLINT:
            ret.val.ullint_val = val.ullint_val * rhs.val.ullint_val;
            break;
    }
    return ret;
}

AtomicType::ScalarTypedVal AtomicType::ScalarTypedVal::operator/ (ScalarTypedVal rhs) {
    AtomicType::ScalarTypedVal ret = *this;

    switch (int_type_id) {
        case IntegerType::IntegerTypeID::BOOL:
        case IntegerType::IntegerTypeID::CHAR:
        case IntegerType::IntegerTypeID::UCHAR:
        case IntegerType::IntegerTypeID::SHRT:
        case IntegerType::IntegerTypeID::USHRT:
        case IntegerType::IntegerTypeID::MAX_INT_ID:
            std::cerr << "ERROR at " << __FILE__ << ":" << __LINE__ << ": perform propagate_type in AtomicType::ScalarTypedVal::operator/" << std::endl;
            exit(-1);
        case IntegerType::IntegerTypeID::INT:
            if (rhs.val.int_val == 0) {
                ret.set_ub(ZeroDiv);
                return ret;
            }
            if ((val.int_val == INT_MIN && rhs.val.int_val == -1) ||
                (rhs.val.int_val == INT_MIN && val.int_val == -1))
                ret.set_ub(SignOvf);
            else
                ret.val.int_val = val.int_val / rhs.val.int_val;
            break;
        case IntegerType::IntegerTypeID::UINT:
            if (rhs.val.uint_val == 0) {
                ret.set_ub(ZeroDiv);
                return ret;
            }
            ret.val.uint_val = val.uint_val / rhs.val.uint_val;
            break;
        case IntegerType::IntegerTypeID::LINT:
            if (rhs.val.lint_val == 0) {
                ret.set_ub(ZeroDiv);
                return ret;
            }
            if ((val.lint_val == LONG_MIN && rhs.val.lint_val == -1) ||
                (rhs.val.lint_val == LONG_MIN && val.lint_val == -1))
                ret.set_ub(SignOvf);
            else
                ret.val.lint_val = val.lint_val / rhs.val.lint_val;
            break;
        case IntegerType::IntegerTypeID::ULINT:
            if (rhs.val.ulint_val == 0) {
                ret.set_ub(ZeroDiv);
                return ret;
            }
            ret.val.ulint_val = val.ulint_val / rhs.val.ulint_val;
            break;
        case IntegerType::IntegerTypeID::LLINT:
            if (rhs.val.llint_val == 0) {
                ret.set_ub(ZeroDiv);
                return ret;
            }
            if ((val.llint_val == LLONG_MIN && rhs.val.llint_val == -1) ||
                (rhs.val.llint_val == LLONG_MIN && val.llint_val == -1))
                ret.set_ub(SignOvf);
            else
                ret.val.llint_val = val.llint_val / rhs.val.llint_val;
            break;
        case IntegerType::IntegerTypeID::ULLINT:
            if (rhs.val.ullint_val == 0) {
                ret.set_ub(ZeroDiv);
                return ret;
            }
            ret.val.ullint_val = val.ullint_val / rhs.val.ullint_val;
            break;
    }
    return ret;
}

AtomicType::ScalarTypedVal AtomicType::ScalarTypedVal::operator% (ScalarTypedVal rhs) {
    AtomicType::ScalarTypedVal ret = *this;

    switch (int_type_id) {
        case IntegerType::IntegerTypeID::BOOL:
        case IntegerType::IntegerTypeID::CHAR:
        case IntegerType::IntegerTypeID::UCHAR:
        case IntegerType::IntegerTypeID::SHRT:
        case IntegerType::IntegerTypeID::USHRT:
        case IntegerType::IntegerTypeID::MAX_INT_ID:
            std::cerr << "ERROR at " << __FILE__ << ":" << __LINE__ << ": perform propagate_type in AtomicType::ScalarTypedVal::operator%" << std::endl;
            exit(-1);
        case IntegerType::IntegerTypeID::INT:
            if (rhs.val.int_val == 0) {
                ret.set_ub(ZeroDiv);
                return ret;
            }
            if ((val.int_val == INT_MIN && rhs.val.int_val == -1) ||
                (rhs.val.int_val == INT_MIN && val.int_val == -1))
                ret.set_ub(SignOvf);
            else
                ret.val.int_val = val.int_val % rhs.val.int_val;
            break;
        case IntegerType::IntegerTypeID::UINT:
            if (rhs.val.uint_val == 0) {
                ret.set_ub(ZeroDiv);
                return ret;
            }
            ret.val.uint_val = val.uint_val % rhs.val.uint_val;
            break;
        case IntegerType::IntegerTypeID::LINT:
            if (rhs.val.lint_val == 0) {
                ret.set_ub(ZeroDiv);
                return ret;
            }
            if ((val.lint_val == LONG_MIN && rhs.val.lint_val == -1) ||
                (rhs.val.lint_val == LONG_MIN && val.lint_val == -1))
                ret.set_ub(SignOvf);
            else
                ret.val.lint_val = val.lint_val % rhs.val.lint_val;
            break;
        case IntegerType::IntegerTypeID::ULINT:
            if (rhs.val.ulint_val == 0) {
                ret.set_ub(ZeroDiv);
                return ret;
            }
            ret.val.ulint_val = val.ulint_val % rhs.val.ulint_val;
            break;
        case IntegerType::IntegerTypeID::LLINT:
            if (rhs.val.llint_val == 0) {
                ret.set_ub(ZeroDiv);
                return ret;
            }
            if ((val.llint_val == LLONG_MIN && rhs.val.llint_val == -1) ||
                (rhs.val.llint_val == LLONG_MIN && val.llint_val == -1))
                ret.set_ub(SignOvf);
            else
                ret.val.llint_val = val.llint_val % rhs.val.llint_val;
            break;
        case IntegerType::IntegerTypeID::ULLINT:
            if (rhs.val.ullint_val == 0) {
                ret.set_ub(ZeroDiv);
                return ret;
            }
            ret.val.ullint_val = val.ullint_val % rhs.val.ullint_val;
            break;
    }
    return ret;
}

#define ScalarTypedValCmpOp(__op__)                                                                 \
AtomicType::ScalarTypedVal AtomicType::ScalarTypedVal::operator __op__ (ScalarTypedVal rhs) {       \
    AtomicType::ScalarTypedVal ret = AtomicType::ScalarTypedVal(Type::IntegerTypeID::BOOL);         \
                                                                                                    \
    switch (int_type_id) {                                                                          \
        case IntegerType::IntegerTypeID::BOOL:                                                      \
            ret.val.bool_val = val.bool_val __op__ rhs.val.bool_val;                                \
            break;                                                                                  \
        case IntegerType::IntegerTypeID::CHAR:                                                      \
            ret.val.bool_val = val.char_val __op__ rhs.val.char_val;                                \
            break;                                                                                  \
        case IntegerType::IntegerTypeID::UCHAR:                                                     \
            ret.val.bool_val = val.uchar_val __op__ rhs.val.uchar_val;                              \
            break;                                                                                  \
        case IntegerType::IntegerTypeID::SHRT:                                                      \
            ret.val.bool_val = val.shrt_val __op__ rhs.val.shrt_val;                                \
            break;                                                                                  \
        case IntegerType::IntegerTypeID::USHRT:                                                     \
            ret.val.bool_val = val.ushrt_val __op__ rhs.val.ushrt_val;                              \
            break;                                                                                  \
        case IntegerType::IntegerTypeID::INT:                                                       \
            ret.val.bool_val = val.int_val __op__ rhs.val.int_val;                                  \
            break;                                                                                  \
        case IntegerType::IntegerTypeID::UINT:                                                      \
            ret.val.bool_val = val.uint_val __op__ rhs.val.uint_val;                                \
            break;                                                                                  \
        case IntegerType::IntegerTypeID::LINT:                                                      \
            ret.val.bool_val = val.lint_val __op__ rhs.val.lint_val;                                \
            break;                                                                                  \
        case IntegerType::IntegerTypeID::ULINT:                                                     \
            ret.val.bool_val = val.ulint_val __op__ rhs.val.ulint_val;                              \
            break;                                                                                  \
        case IntegerType::IntegerTypeID::LLINT:                                                     \
            ret.val.bool_val = val.llint_val __op__ rhs.val.llint_val;                              \
            break;                                                                                  \
        case IntegerType::IntegerTypeID::ULLINT:                                                    \
            ret.val.bool_val = val.ullint_val __op__ rhs.val.ullint_val;                            \
            break;                                                                                  \
        case IntegerType::IntegerTypeID::MAX_INT_ID:                                                \
            std::cerr << "ERROR at " << __FILE__ << ":" << __LINE__ << ": perform propagate_type in AtomicType::ScalarTypedVal::operator"#__op__ << std::endl;\
            exit(-1);                                                                               \
    }                                                                                               \
    return ret;                                                                                     \
}

ScalarTypedValCmpOp(<)
ScalarTypedValCmpOp(>)
ScalarTypedValCmpOp(<=)
ScalarTypedValCmpOp(>=)
ScalarTypedValCmpOp(==)
ScalarTypedValCmpOp(!=)

#define ScalarTypedValLogOp(__op__)                                                                 \
AtomicType::ScalarTypedVal AtomicType::ScalarTypedVal::operator __op__ (ScalarTypedVal rhs) {       \
    AtomicType::ScalarTypedVal ret = AtomicType::ScalarTypedVal(Type::IntegerTypeID::BOOL);         \
                                                                                                    \
    switch (int_type_id) {                                                                          \
        case IntegerType::IntegerTypeID::BOOL:                                                      \
            ret.val.bool_val = val.int_val __op__ rhs.val.int_val;                                  \
            break;                                                                                  \
        case IntegerType::IntegerTypeID::CHAR:                                                      \
        case IntegerType::IntegerTypeID::UCHAR:                                                     \
        case IntegerType::IntegerTypeID::SHRT:                                                      \
        case IntegerType::IntegerTypeID::USHRT:                                                     \
        case IntegerType::IntegerTypeID::INT:                                                       \
        case IntegerType::IntegerTypeID::UINT:                                                      \
        case IntegerType::IntegerTypeID::LINT:                                                      \
        case IntegerType::IntegerTypeID::ULINT:                                                     \
        case IntegerType::IntegerTypeID::LLINT:                                                     \
        case IntegerType::IntegerTypeID::ULLINT:                                                    \
        case IntegerType::IntegerTypeID::MAX_INT_ID:                                                \
            std::cerr << "ERROR at " << __FILE__ << ":" << __LINE__ << ": perform propagate_type in AtomicType::ScalarTypedVal::operator"#__op__ << std::endl;\
            exit(-1);                                                                               \
    }                                                                                               \
    return ret;                                                                                     \
}

ScalarTypedValLogOp(&&)
ScalarTypedValLogOp(||)

#define ScalarTypedValBitOp(__op__)                                                                 \
AtomicType::ScalarTypedVal AtomicType::ScalarTypedVal::operator __op__ (ScalarTypedVal rhs) {       \
    AtomicType::ScalarTypedVal ret = *this;                                                         \
                                                                                                    \
    switch (int_type_id) {                                                                          \
        case IntegerType::IntegerTypeID::BOOL:                                                      \
        case IntegerType::IntegerTypeID::CHAR:                                                      \
        case IntegerType::IntegerTypeID::UCHAR:                                                     \
        case IntegerType::IntegerTypeID::SHRT:                                                      \
        case IntegerType::IntegerTypeID::USHRT:                                                     \
        case IntegerType::IntegerTypeID::MAX_INT_ID:                                                \
            std::cerr << "ERROR at " << __FILE__ << ":" << __LINE__ << ": perform propagate_type in AtomicType::ScalarTypedVal::operator"#__op__ << std::endl;\
            exit(-1);                                                                               \
        case IntegerType::IntegerTypeID::INT:                                                       \
            ret.val.int_val = val.int_val __op__ rhs.val.int_val;                                   \
            break;                                                                                  \
        case IntegerType::IntegerTypeID::UINT:                                                      \
            ret.val.uint_val = val.uint_val __op__ rhs.val.uint_val;                                \
            break;                                                                                  \
        case IntegerType::IntegerTypeID::LINT:                                                      \
            ret.val.lint_val = val.lint_val __op__ rhs.val.lint_val;                                \
            break;                                                                                  \
        case IntegerType::IntegerTypeID::ULINT:                                                     \
            ret.val.ulint_val = val.ulint_val __op__ rhs.val.ulint_val;                             \
            break;                                                                                  \
        case IntegerType::IntegerTypeID::LLINT:                                                     \
            ret.val.llint_val = val.llint_val __op__ rhs.val.llint_val;                             \
            break;                                                                                  \
        case IntegerType::IntegerTypeID::ULLINT:                                                    \
            ret.val.ullint_val = val.ullint_val __op__ rhs.val.ullint_val;                          \
            break;                                                                                  \
    }                                                                                               \
    return ret;                                                                                     \
}

ScalarTypedValBitOp(&)
ScalarTypedValBitOp(|)
ScalarTypedValBitOp(^)

#define SHFT_CASE(__op__, ret_val, lhs_val)                                     \
switch (rhs.get_int_type_id()) {                                                \
    case IntegerType::IntegerTypeID::BOOL:                                      \
    case IntegerType::IntegerTypeID::CHAR:                                      \
    case IntegerType::IntegerTypeID::UCHAR:                                     \
    case IntegerType::IntegerTypeID::SHRT:                                      \
    case IntegerType::IntegerTypeID::USHRT:                                     \
    case IntegerType::IntegerTypeID::MAX_INT_ID:                                \
        std::cerr << "ERROR at " << __FILE__ << ":" << __LINE__ << ": perform propagate_type in AtomicType::ScalarTypedVal::operator__op__" << std::endl;\
        exit(-1);                                                               \
    case IntegerType::IntegerTypeID::INT:                                       \
        ret_val = lhs_val __op__ rhs.val.int_val;                               \
        break;                                                                  \
    case IntegerType::IntegerTypeID::UINT:                                      \
        ret_val = lhs_val __op__ rhs.val.uint_val;                              \
        break;                                                                  \
    case IntegerType::IntegerTypeID::LINT:                                      \
        ret_val = lhs_val __op__ rhs.val.lint_val;                              \
        break;                                                                  \
    case IntegerType::IntegerTypeID::ULINT:                                     \
        ret_val = lhs_val __op__ rhs.val.ulint_val;                             \
        break;                                                                  \
    case IntegerType::IntegerTypeID::LLINT:                                     \
        ret_val = lhs_val __op__ rhs.val.llint_val;                             \
        break;                                                                  \
    case IntegerType::IntegerTypeID::ULLINT:                                    \
        ret_val = lhs_val __op__ rhs.val.ullint_val;                            \
        break;                                                                  \
}

static uint64_t msb(uint64_t x) {
    uint64_t ret = 0;
    while (x != 0) {
        ret++;
        x = x >> 1;
    }
    return ret;
}

AtomicType::ScalarTypedVal AtomicType::ScalarTypedVal::operator<< (ScalarTypedVal rhs) {
    AtomicType::ScalarTypedVal ret = *this;

    int64_t s_lhs = 0;
    int64_t u_lhs = 0;
    int64_t s_rhs = 0;
    int64_t u_rhs = 0;
    switch (int_type_id) {
        case IntegerType::IntegerTypeID::BOOL:
        case IntegerType::IntegerTypeID::CHAR:
        case IntegerType::IntegerTypeID::UCHAR:
        case IntegerType::IntegerTypeID::SHRT:
        case IntegerType::IntegerTypeID::USHRT:
        case IntegerType::IntegerTypeID::MAX_INT_ID:
            std::cerr << "ERROR at " << __FILE__ << ":" << __LINE__ << ": perform propagate_type in AtomicType::ScalarTypedVal::operator<<" << std::endl;
            exit(-1);
        case IntegerType::IntegerTypeID::INT:
            s_lhs = val.int_val;
            break;
        case IntegerType::IntegerTypeID::UINT:
            u_lhs = val.uint_val;
            break;
        case IntegerType::IntegerTypeID::LINT:
            s_lhs = val.lint_val;
            break;
        case IntegerType::IntegerTypeID::ULINT:
            u_lhs = val.ulint_val;
            break;
        case IntegerType::IntegerTypeID::LLINT:
            s_lhs = val.llint_val;
            break;
        case IntegerType::IntegerTypeID::ULLINT:
            u_lhs = val.ullint_val;
            break;
    }

    switch (rhs.get_int_type_id()) {
        case IntegerType::IntegerTypeID::BOOL:
        case IntegerType::IntegerTypeID::CHAR:
        case IntegerType::IntegerTypeID::UCHAR:
        case IntegerType::IntegerTypeID::SHRT:
        case IntegerType::IntegerTypeID::USHRT:
        case IntegerType::IntegerTypeID::MAX_INT_ID:
            std::cerr << "ERROR at " << __FILE__ << ":" << __LINE__ << ": perform propagate_type in AtomicType::ScalarTypedVal::operator<<" << std::endl;
            exit(-1);
        case IntegerType::IntegerTypeID::INT:
            s_rhs = rhs.val.int_val;
            break;
        case IntegerType::IntegerTypeID::UINT:
            u_rhs = rhs.val.uint_val;
            break;
        case IntegerType::IntegerTypeID::LINT:
            s_rhs = rhs.val.lint_val;
            break;
        case IntegerType::IntegerTypeID::ULINT:
            u_rhs = rhs.val.ulint_val;
            break;
        case IntegerType::IntegerTypeID::LLINT:
            s_rhs = rhs.val.llint_val;
            break;
        case IntegerType::IntegerTypeID::ULLINT:
            u_rhs = rhs.val.ullint_val;
            break;
    }

    bool lhs_is_signed = IntegerType::init(int_type_id)->get_is_signed();
    bool rhs_is_signed = IntegerType::init(rhs.get_int_type_id())->get_is_signed();
    if (lhs_is_signed && (s_lhs < 0)) {
        ret.set_ub(NegShift);
        return ret;
    }
    if (rhs_is_signed && (s_rhs < 0)) {
        ret.set_ub(ShiftRhsNeg);
        return ret;
    }

    uint64_t lhs_bit_size = IntegerType::init(int_type_id)->get_bit_size();
    if (rhs_is_signed) {
        if (s_rhs >= lhs_bit_size) {
            ret.set_ub(ShiftRhsLarge);
            return ret;
        }
    }
    else {
        if (u_rhs >= lhs_bit_size) {
            ret.set_ub(ShiftRhsLarge);
            return ret;
        }
    }

    if (lhs_is_signed) {
        uint64_t max_avail_shft = lhs_bit_size - msb(s_lhs);
        if (rhs_is_signed && s_rhs >= max_avail_shft) {
            ret.set_ub(ShiftRhsLarge);
            return ret;
        }
        else if (!rhs_is_signed && u_rhs >= max_avail_shft) {
            ret.set_ub(ShiftRhsLarge);
            return ret;
        }
    }

    if (ret.has_ub())
        return ret;

    switch (int_type_id) {
        case IntegerType::IntegerTypeID::BOOL:
        case IntegerType::IntegerTypeID::CHAR:
        case IntegerType::IntegerTypeID::UCHAR:
        case IntegerType::IntegerTypeID::SHRT:
        case IntegerType::IntegerTypeID::USHRT:
        case IntegerType::IntegerTypeID::MAX_INT_ID:
            std::cerr << "ERROR at " << __FILE__ << ":" << __LINE__ << ": perform propagate_type in AtomicType::ScalarTypedVal::operator<<" << std::endl;
            exit(-1);
        case IntegerType::IntegerTypeID::INT:
            SHFT_CASE(<<, ret.val.int_val, val.int_val)
            break;
        case IntegerType::IntegerTypeID::UINT:
            SHFT_CASE(<<, ret.val.uint_val, val.uint_val)
            break;
        case IntegerType::IntegerTypeID::LINT:
            SHFT_CASE(<<, ret.val.lint_val, val.lint_val)
            break;
        case IntegerType::IntegerTypeID::ULINT:
            SHFT_CASE(<<, ret.val.ulint_val, val.ulint_val)
            break;
        case IntegerType::IntegerTypeID::LLINT:
            SHFT_CASE(<<, ret.val.llint_val, val.llint_val)
            break;
        case IntegerType::IntegerTypeID::ULLINT:
            SHFT_CASE(<<, ret.val.ullint_val, val.ullint_val)
            break;
    }
    return ret;
}

AtomicType::ScalarTypedVal AtomicType::ScalarTypedVal::operator>> (ScalarTypedVal rhs) {
    AtomicType::ScalarTypedVal ret = *this;

    int64_t s_lhs = 0;
    int64_t u_lhs = 0;
    int64_t s_rhs = 0;
    int64_t u_rhs = 0;
    switch (int_type_id) {
        case IntegerType::IntegerTypeID::BOOL:
        case IntegerType::IntegerTypeID::CHAR:
        case IntegerType::IntegerTypeID::UCHAR:
        case IntegerType::IntegerTypeID::SHRT:
        case IntegerType::IntegerTypeID::USHRT:
        case IntegerType::IntegerTypeID::MAX_INT_ID:
            std::cerr << "ERROR at " << __FILE__ << ":" << __LINE__ << ": perform propagate_type in AtomicType::ScalarTypedVal::operator>>" << std::endl;
            exit(-1);
        case IntegerType::IntegerTypeID::INT:
            s_lhs = val.int_val;
            break;
        case IntegerType::IntegerTypeID::UINT:
            u_lhs = val.uint_val;
            break;
        case IntegerType::IntegerTypeID::LINT:
            s_lhs = val.lint_val;
            break;
        case IntegerType::IntegerTypeID::ULINT:
            u_lhs = val.ulint_val;
            break;
        case IntegerType::IntegerTypeID::LLINT:
            s_lhs = val.llint_val;
            break;
        case IntegerType::IntegerTypeID::ULLINT:
            u_lhs = val.ullint_val;
            break;
    }

    switch (rhs.get_int_type_id()) {
        case IntegerType::IntegerTypeID::BOOL:
        case IntegerType::IntegerTypeID::CHAR:
        case IntegerType::IntegerTypeID::UCHAR:
        case IntegerType::IntegerTypeID::SHRT:
        case IntegerType::IntegerTypeID::USHRT:
        case IntegerType::IntegerTypeID::MAX_INT_ID:
            std::cerr << "ERROR at " << __FILE__ << ":" << __LINE__ << ": perform propagate_type in AtomicType::ScalarTypedVal::operator<<" << std::endl;
            exit(-1);
        case IntegerType::IntegerTypeID::INT:
            s_rhs = rhs.val.int_val;
            break;
        case IntegerType::IntegerTypeID::UINT:
            u_rhs = rhs.val.uint_val;
            break;
        case IntegerType::IntegerTypeID::LINT:
            s_rhs = rhs.val.lint_val;
            break;
        case IntegerType::IntegerTypeID::ULINT:
            u_rhs = rhs.val.ulint_val;
            break;
        case IntegerType::IntegerTypeID::LLINT:
            s_rhs = rhs.val.llint_val;
            break;
        case IntegerType::IntegerTypeID::ULLINT:
            u_rhs = rhs.val.ullint_val;
            break;
    }

    bool lhs_is_signed = IntegerType::init(int_type_id)->get_is_signed();
    bool rhs_is_signed = IntegerType::init(rhs.get_int_type_id())->get_is_signed();
    if (lhs_is_signed && (s_lhs < 0)) {
        ret.set_ub(NegShift);
        return ret;
    }
    if (rhs_is_signed && (s_rhs < 0)) {
        ret.set_ub(ShiftRhsNeg);
        return ret;
    }

    uint64_t lhs_bit_size = IntegerType::init(int_type_id)->get_bit_size();
    if (rhs_is_signed) {
        if (s_rhs >= lhs_bit_size) {
            ret.set_ub(ShiftRhsLarge);
            return ret;
        }
    }
    else {
        if(u_rhs >= lhs_bit_size) {
            ret.set_ub(ShiftRhsLarge);
            return ret;
        }
    }

    if (ret.has_ub())
        return ret;

    switch (int_type_id) {
        case IntegerType::IntegerTypeID::BOOL:
        case IntegerType::IntegerTypeID::CHAR:
        case IntegerType::IntegerTypeID::UCHAR:
        case IntegerType::IntegerTypeID::SHRT:
        case IntegerType::IntegerTypeID::USHRT:
        case IntegerType::IntegerTypeID::MAX_INT_ID:
            std::cerr << "ERROR at " << __FILE__ << ":" << __LINE__ << ": perform propagate_type in AtomicType::ScalarTypedVal::operator>>" << std::endl;
            exit(-1);
        case IntegerType::IntegerTypeID::INT:
            SHFT_CASE(>>, ret.val.int_val, val.int_val)
            break;
        case IntegerType::IntegerTypeID::UINT:
            SHFT_CASE(>>, ret.val.uint_val, val.uint_val)
            break;
        case IntegerType::IntegerTypeID::LINT:
            SHFT_CASE(>>, ret.val.lint_val, val.lint_val)
            break;
        case IntegerType::IntegerTypeID::ULINT:
            SHFT_CASE(>>, ret.val.ulint_val, val.ulint_val)
            break;
        case IntegerType::IntegerTypeID::LLINT:
            SHFT_CASE(>>, ret.val.llint_val, val.llint_val)
            break;
        case IntegerType::IntegerTypeID::ULLINT:
            SHFT_CASE(>>, ret.val.ullint_val, val.ullint_val)
            break;
    }
    return ret;
}

template <typename T>
static void gen_rand_typed_val (T& ret, T& min, T& max) {
    ret = (T) rand_val_gen->get_rand_value<T>(min, max);
}

template <>
void gen_rand_typed_val<bool> (bool& ret, bool& min, bool& max) {
    ret = (bool) rand_val_gen->get_rand_value<int>(min, max);
}


AtomicType::ScalarTypedVal AtomicType::ScalarTypedVal::generate (std::shared_ptr<Context> ctx, AtomicType::IntegerTypeID _int_type_id) {
    std::shared_ptr<IntegerType> tmp_type = IntegerType::init (_int_type_id);
    AtomicType::ScalarTypedVal min = tmp_type->get_min();
    AtomicType::ScalarTypedVal max = tmp_type->get_max();
    return generate(ctx, min, max);
}

AtomicType::ScalarTypedVal AtomicType::ScalarTypedVal::generate (std::shared_ptr<Context> ctx, AtomicType::ScalarTypedVal min, AtomicType::ScalarTypedVal max) {
    if (min.get_int_type_id() != max.get_int_type_id()) {
        std::cerr << "ERROR at " << __FILE__ << ":" << __LINE__ << ": int type of min and int type of max ared different in AtomicType::ScalarTypedVal::generate" << std::endl;
        exit(-1);
    }
    AtomicType::ScalarTypedVal ret(min.get_int_type_id());
    switch(min.get_int_type_id()) {
        case AtomicType::BOOL:
            gen_rand_typed_val(ret.val.bool_val, min.val.bool_val, max.val.bool_val);
            break;
        case AtomicType::CHAR:
            gen_rand_typed_val(ret.val.char_val, min.val.char_val, max.val.char_val);
            break;
        case AtomicType::UCHAR:
            gen_rand_typed_val(ret.val.uchar_val, min.val.uchar_val, max.val.uchar_val);
            break;
        case AtomicType::SHRT:
            gen_rand_typed_val(ret.val.shrt_val, min.val.shrt_val, max.val.shrt_val);
            break;
        case AtomicType::USHRT:
            gen_rand_typed_val(ret.val.ushrt_val, min.val.ushrt_val, max.val.ushrt_val);
            break;
        case AtomicType::INT:
            gen_rand_typed_val(ret.val.int_val, min.val.int_val, max.val.int_val);
            break;
        case AtomicType::UINT:
            gen_rand_typed_val(ret.val.uint_val, min.val.uint_val, max.val.uint_val);
            break;
        case AtomicType::LINT:
            gen_rand_typed_val(ret.val.lint_val, min.val.lint_val, max.val.lint_val);
            break;
        case AtomicType::ULINT:
            gen_rand_typed_val(ret.val.ulint_val, min.val.ulint_val, max.val.ulint_val);
            break;
        case AtomicType::LLINT:
            gen_rand_typed_val(ret.val.llint_val, min.val.llint_val, max.val.llint_val);
            break;
        case AtomicType::ULLINT:
            gen_rand_typed_val(ret.val.ullint_val, min.val.ullint_val, max.val.ullint_val);
            break;
        case AtomicType::MAX_INT_ID:
            std::cerr << "ERROR at " << __FILE__ << ":" << __LINE__ << ": unsupported type of struct member in AtomicType::ScalarTypedVal::generate" << std::endl;
            exit(-1);
            break;
    }
    return ret;
}

std::ostream& rl::operator<< (std::ostream &out, const AtomicType::ScalarTypedVal &scalar_typed_val) {
    switch(scalar_typed_val.get_int_type_id()) {
        case AtomicType::BOOL:
            out << scalar_typed_val.val.bool_val;
            break;
        case AtomicType::CHAR:
            out << scalar_typed_val.val.char_val;
            break;
        case AtomicType::UCHAR:
            out << scalar_typed_val.val.uchar_val;
            break;
        case AtomicType::SHRT:
            out << scalar_typed_val.val.shrt_val;
            break;
        case AtomicType::USHRT:
            out << scalar_typed_val.val.ushrt_val;
            break;
        case AtomicType::INT:
            out << scalar_typed_val.val.int_val;
            break;
        case AtomicType::UINT:
            out << scalar_typed_val.val.uint_val;
            break;
        case AtomicType::LINT:
            out << scalar_typed_val.val.lint_val;
            break;
        case AtomicType::ULINT:
            out << scalar_typed_val.val.ulint_val;
            break;
        case AtomicType::LLINT:
            out << scalar_typed_val.val.llint_val;
            break;
        case AtomicType::ULLINT:
            out << scalar_typed_val.val.ullint_val;
            break;
        case AtomicType::MAX_INT_ID:
            std::cerr << "ERROR at " << __FILE__ << ":" << __LINE__ << ": unsupported type of struct member in operator<<" << std::endl;
            exit(-1);
            break;
    }
    return out;
}

std::shared_ptr<IntegerType> IntegerType::init (AtomicType::IntegerTypeID _type_id) {
    std::shared_ptr<IntegerType> ret (NULL);
    switch (_type_id) {
        case AtomicType::IntegerTypeID::BOOL:
            ret = std::make_shared<TypeBOOL> (TypeBOOL());
            break;
        case AtomicType::IntegerTypeID::CHAR:
            ret = std::make_shared<TypeCHAR> (TypeCHAR());
            break;
        case AtomicType::IntegerTypeID::UCHAR:
            ret = std::make_shared<TypeUCHAR> (TypeUCHAR());
            break;
        case AtomicType::IntegerTypeID::SHRT:
            ret = std::make_shared<TypeSHRT> (TypeSHRT());
            break;
        case AtomicType::IntegerTypeID::USHRT:
            ret = std::make_shared<TypeUSHRT> (TypeUSHRT());
            break;
        case AtomicType::IntegerTypeID::INT:
            ret = std::make_shared<TypeINT> (TypeINT());
            break;
        case AtomicType::IntegerTypeID::UINT:
            ret = std::make_shared<TypeUINT> (TypeUINT());
            break;
        case AtomicType::IntegerTypeID::LINT:
            ret = std::make_shared<TypeLINT> (TypeLINT());
            break;
        case AtomicType::IntegerTypeID::ULINT:
            ret = std::make_shared<TypeULINT> (TypeULINT());
            break;
         case AtomicType::IntegerTypeID::LLINT:
            ret = std::make_shared<TypeLLINT> (TypeLLINT());
            break;
         case AtomicType::IntegerTypeID::ULLINT:
            ret = std::make_shared<TypeULLINT> (TypeULLINT());
            break;
        case MAX_INT_ID:
            break;
    }
    return ret;
}

std::shared_ptr<IntegerType> IntegerType::init (AtomicType::IntegerTypeID _type_id, Type::Mod _modifier, bool _is_static, uint64_t _align) {
    std::shared_ptr<IntegerType> ret = IntegerType::init (_type_id);
    ret->set_modifier(_modifier);
    ret->set_is_static(_is_static);
    ret->set_align(_align);
    return ret;
}

std::shared_ptr<IntegerType> IntegerType::generate (std::shared_ptr<Context> ctx) {
    Type::Mod modifier = ctx->get_gen_policy()->get_allowed_modifiers().at(rand_val_gen->get_rand_value<int>(0, ctx->get_gen_policy()->get_allowed_modifiers().size() - 1));

    bool specifier = false;
    if (ctx->get_gen_policy()->get_allow_static_var())
        specifier = rand_val_gen->get_rand_value<int>(0, 1);
    else
        specifier = false;
    //TODO: what about align?

    IntegerType::IntegerTypeID int_type_id = (IntegerType::IntegerTypeID) rand_val_gen->get_rand_id(ctx->get_gen_policy()->get_allowed_int_types());
    return IntegerType::init(int_type_id, modifier, specifier, 0);
}


bool IntegerType::can_repr_value (AtomicType::IntegerTypeID a, AtomicType::IntegerTypeID b) {
    // This function is used for different conversion rules, so it can be called only after integral promotion
    std::shared_ptr<IntegerType> B = std::static_pointer_cast<IntegerType>(init(b));
    bool int_eq_long = sizeof(int) == sizeof(long int);
    bool long_eq_long_long =  sizeof(long int) == sizeof(long long int);
    switch (a) {
        case INT:
            return B->get_is_signed();
        case UINT:
            if (B->get_int_type_id() == INT)
                return false;
            if (B->get_int_type_id() == LINT)
                return !int_eq_long;
            return true;
        case LINT:
            if (!B->get_is_signed())
                return false;
            if (B->get_int_type_id() == INT)
                return int_eq_long;
            return true;
        case ULINT:
            switch (B->get_int_type_id()) {
                case INT:
                    return false;
                case UINT:
                    return int_eq_long;
                case LINT:
                    return false;
                case ULINT:
                    return true;
                case LLINT:
                    return !long_eq_long_long;
                case ULLINT:
                    return true;
                default:
                    std::cerr << "ERROR: Type::can_repr_value in case ULINT" << std::endl;
            }
        case LLINT:
            switch (B->get_int_type_id()) {
                case INT:
                case UINT:
                   return false;
                case LINT:
                    return long_eq_long_long;
                case ULINT:
                   return false;
                case LLINT:
                    return true;
                case ULLINT:
                   return false;
                default:
                    std::cerr << "ERROR: Type::can_repr_value in case ULINT" << std::endl;
            }
        case ULLINT:
            switch (B->get_int_type_id()) {
                case INT:
                case UINT:
                case LINT:
                   return false;
                case ULINT:
                   return long_eq_long_long;
                case LLINT:
                   return false;
                case ULLINT:
                    return true;
                default:
                    std::cerr << "ERROR: Type::can_repr_value in case ULINT" << std::endl;
            }
        default:
            std::cerr << "ERROR: Type::can_repr_value" << std::endl;
            return false;
    }
}

AtomicType::IntegerTypeID IntegerType::get_corr_unsig (AtomicType::IntegerTypeID _type_id) {
    // This function is used for different conversion rules, so it can be called only after integral promotion
    switch (_type_id) {
        case INT:
        case UINT:
            return UINT;
        case LINT:
        case ULINT:
            return ULINT;
        case LLINT:
        case ULLINT:
            return ULLINT;
        default:
            std::cerr << "ERROR: Type::get_corr_unsig" << std::endl;
            return MAX_INT_ID;
    }
}

void BitField::init_type (IntegerTypeID it_id, uint64_t _bit_size) {
    std::shared_ptr<IntegerType> base_type = IntegerType::init(it_id);
    name = base_type->get_simple_name();
    suffix = base_type->get_suffix();
    is_signed = base_type->get_is_signed();
    bit_size = _bit_size;
    bit_field_width = _bit_size;
    min = base_type->get_min();
    max = base_type->get_max();
    if (bit_size >= base_type->get_bit_size()) {
        bit_size = base_type->get_bit_size();
        return;
    }

    uint64_t act_max = 0;
    if (is_signed)
        act_max = pow(2, bit_size - 1) - 1;
    else
        act_max = pow(2, bit_size) - 1;
    int64_t act_min = -((int64_t) act_max) - 1;

    switch (it_id) {
        case AtomicType::IntegerTypeID::BOOL:
            min.val.bool_val = false;
            max.val.bool_val = true;
            break;
        case AtomicType::IntegerTypeID::CHAR:
            min.val.char_val = act_min;
            max.val.char_val = (int64_t) act_max;
            break;
        case AtomicType::IntegerTypeID::UCHAR:
            min.val.uchar_val = 0;
            max.val.uchar_val = act_max;
            break;
        case AtomicType::IntegerTypeID::SHRT:
            min.val.shrt_val = act_min;
            max.val.shrt_val = (int64_t) act_max;
            break;
        case AtomicType::IntegerTypeID::USHRT:
            min.val.ushrt_val = 0;
            max.val.ushrt_val = act_max;
            break;
        case AtomicType::IntegerTypeID::INT:
            min.val.int_val = act_min;
            max.val.int_val = (int64_t) act_max;
            break;
        case AtomicType::IntegerTypeID::UINT:
            min.val.uint_val = 0;
            max.val.uint_val = act_max;
            break;
        case AtomicType::IntegerTypeID::LINT:
            min.val.lint_val = act_min;
            max.val.lint_val = (int64_t) act_max;
            break;
        case AtomicType::IntegerTypeID::ULINT:
            min.val.ulint_val = 0;
            max.val.ulint_val = act_max;
            break;
         case AtomicType::IntegerTypeID::LLINT:
            min.val.llint_val = act_min;
            max.val.llint_val = (int64_t) act_max;
            break;
         case AtomicType::IntegerTypeID::ULLINT:
            min.val.ullint_val = 0;
            max.val.ullint_val = act_max;
            break;
        case MAX_INT_ID:
            std::cerr << "ERROR at " << __FILE__ << ":" << __LINE__ << ": unsupported int type in BitField" << std::endl;
            exit(-1);
            break;
    }
}

template <class T>
static std::string dbg_dump_helper (std::string name, int id, T min, T max, uint32_t bit_size, bool is_signed) {
    std::string ret = "";
    ret += "name: " + name + "\n";
    ret += "int_type_id: " + std::to_string(id) + "\n";
    ret += "min: " + std::to_string((T)min) + "\n";
    ret += "max: " + std::to_string((T)max) + "\n";
    ret += "bit_size: " + std::to_string(bit_size) + "\n";
    ret += "is_signed: " + std::to_string(is_signed) + "\n";
    return ret;
}

void TypeBOOL::dbg_dump () {
    std::cout << dbg_dump_helper<bool>(get_name(), get_int_type_id(), min.val.bool_val, max.val.bool_val, bit_size, is_signed) << std::endl;
}

void TypeCHAR::dbg_dump () {
    std::cout << dbg_dump_helper<char>(get_name(), get_int_type_id(), min.val.char_val, max.val.char_val, bit_size, is_signed) << std::endl;
}

void TypeUCHAR::dbg_dump () {
    std::cout << dbg_dump_helper<unsigned char>(get_name(), get_int_type_id(), min.val.uchar_val, max.val.uchar_val, bit_size, is_signed) << std::endl;
}

void TypeSHRT::dbg_dump () {
    std::cout << dbg_dump_helper<short>(get_name(), get_int_type_id(), min.val.shrt_val, max.val.shrt_val, bit_size, is_signed) << std::endl;
}

void TypeUSHRT::dbg_dump () {
    std::cout << dbg_dump_helper<unsigned short>(get_name(), get_int_type_id(), min.val.ushrt_val, max.val.ushrt_val, bit_size, is_signed) << std::endl;
}

void TypeINT::dbg_dump () {
    std::cout << dbg_dump_helper<int>(get_name(), get_int_type_id(), min.val.int_val, max.val.int_val, bit_size, is_signed) << std::endl;
}

void TypeUINT::dbg_dump () {
    std::cout << dbg_dump_helper<unsigned int>(get_name(), get_int_type_id(), min.val.uint_val, max.val.uint_val, bit_size, is_signed) << std::endl;
}

void TypeLINT::dbg_dump () {
    std::cout << dbg_dump_helper<long int>(get_name(), get_int_type_id(), min.val.lint_val, max.val.lint_val, bit_size, is_signed) << std::endl;
}

void TypeULINT::dbg_dump () {
    std::cout << dbg_dump_helper<unsigned long int>(get_name(), get_int_type_id(), min.val.ulint_val, max.val.ulint_val, bit_size, is_signed) << std::endl;
}

void TypeLLINT::dbg_dump () {
    std::cout << dbg_dump_helper<long long int>(get_name(), get_int_type_id(), min.val.llint_val, max.val.llint_val, bit_size, is_signed) << std::endl;
}

void TypeULLINT::dbg_dump () {
    std::cout << dbg_dump_helper<unsigned long long int>(get_name(), get_int_type_id(), min.val.ullint_val, max.val.ullint_val, bit_size, is_signed) << std::endl;
}

std::shared_ptr<BitField> BitField::generate (std::shared_ptr<Context> ctx, bool is_unnamed) {
    Type::Mod modifier = ctx->get_gen_policy()->get_allowed_modifiers().at(rand_val_gen->get_rand_value<int>(0, ctx->get_gen_policy()->get_allowed_modifiers().size() - 1));
    IntegerType::IntegerTypeID int_type_id = (IntegerType::IntegerTypeID) rand_val_gen->get_rand_id(ctx->get_gen_policy()->get_allowed_int_types());
    std::shared_ptr<IntegerType> tmp_int_type = IntegerType::init(int_type_id);
    uint64_t min_bit_size = is_unnamed ? 0 : (tmp_int_type->get_bit_size() / ctx->get_gen_policy()->get_min_bit_field_size());
    //TODO: it cause different result for LLVM and GCC. See pr70733
//    uint64_t max_bit_size = tmp_int_type->get_bit_size() * ctx->get_gen_policy()->get_max_bit_field_size();
     std::shared_ptr<IntegerType> int_type = IntegerType::init(Type::IntegerTypeID::INT);
    uint64_t max_bit_size = int_type->get_bit_size();

    uint64_t bit_size = rand_val_gen->get_rand_value<uint64_t>(min_bit_size, max_bit_size);
    return std::make_shared<BitField>(int_type_id, bit_size, modifier);
}

bool BitField::can_fit_in_int (AtomicType::ScalarTypedVal val, bool is_unsigned) {
    std::shared_ptr<IntegerType> tmp_type = IntegerType::init(is_unsigned ? Type::IntegerTypeID::UINT : Type::IntegerTypeID::INT);
    bool val_is_unsig = false;
    int64_t s_val = 0;
    uint64_t u_val = 0;

    switch (val.get_int_type_id()) {
        case IntegerType::IntegerTypeID::BOOL:
        case IntegerType::IntegerTypeID::CHAR:
        case IntegerType::IntegerTypeID::UCHAR:
        case IntegerType::IntegerTypeID::SHRT:
        case IntegerType::IntegerTypeID::USHRT:
        case IntegerType::IntegerTypeID::INT:
            return true;
            break;
        case IntegerType::IntegerTypeID::UINT:
            val_is_unsig = true;
            u_val = val.val.uint_val;
            break;
        case IntegerType::IntegerTypeID::LINT:
            val_is_unsig = false;
            s_val = val.val.lint_val;
            break;
        case IntegerType::IntegerTypeID::ULINT:
            val_is_unsig = true;
            u_val = val.val.ulint_val;
            break;
        case IntegerType::IntegerTypeID::LLINT:
            val_is_unsig = false;
            s_val = val.val.llint_val;
            break;
        case IntegerType::IntegerTypeID::ULLINT:
            val_is_unsig = true;
            u_val = val.val.ullint_val;
            break;
        case IntegerType::IntegerTypeID::MAX_INT_ID:
            std::cerr << "ERROR at " << __FILE__ << ":" << __LINE__ << ": unsupported int type in BitField" << std::endl;
            exit(-1);
    }

    int64_t s_min = 0;
    uint64_t u_min = 0;
    int64_t s_max = 0;
    uint64_t u_max = 0;

    if (is_unsigned) {
        u_min = tmp_type->get_min().val.uint_val;
        u_max = tmp_type->get_max().val.uint_val;
    }
    else {
        s_min = tmp_type->get_min().val.int_val;
        s_max = tmp_type->get_max().val.int_val;
    }

    if (val_is_unsig) {
        if (is_unsigned)
            return (u_min <= u_val) && (u_val <= u_max);
        else
            return (s_min <= u_val) && (u_val <= s_max);
    }
    else {
        if (is_unsigned)
            return (u_min <= s_val) && (s_val <= u_max);
        else
            return (s_min <= s_val) && (s_val <= s_max);
    }
}

void BitField::dbg_dump () {
    std::string ret = "";
    ret += "name: " + name + "\n";
    ret += "int_type_id: " + std::to_string(get_int_type_id()) + "\n";
    switch (get_int_type_id()) {
        case AtomicType::IntegerTypeID::BOOL:
            ret += "min: " + std::to_string(min.val.bool_val) + "\n";
            ret += "max: " + std::to_string(max.val.bool_val) + "\n";
            break;
        case AtomicType::IntegerTypeID::CHAR:
            ret += "min: " + std::to_string(min.val.char_val) + "\n";
            ret += "max: " + std::to_string(max.val.char_val) + "\n";
            break;
        case AtomicType::IntegerTypeID::UCHAR:
            ret += "min: " + std::to_string(min.val.uchar_val) + "\n";
            ret += "max: " + std::to_string(max.val.uchar_val) + "\n";
            break;
        case AtomicType::IntegerTypeID::SHRT:
            ret += "min: " + std::to_string(min.val.shrt_val) + "\n";
            ret += "max: " + std::to_string(max.val.shrt_val) + "\n";
            break;
        case AtomicType::IntegerTypeID::USHRT:
            ret += "min: " + std::to_string(min.val.ushrt_val) + "\n";
            ret += "max: " + std::to_string(max.val.ushrt_val) + "\n";
            break;
        case AtomicType::IntegerTypeID::INT:
            ret += "min: " + std::to_string(min.val.int_val) + "\n";
            ret += "max: " + std::to_string(max.val.int_val) + "\n";
            break;
        case AtomicType::IntegerTypeID::UINT:
            ret += "min: " + std::to_string(min.val.uint_val) + "\n";
            ret += "max: " + std::to_string(max.val.uint_val) + "\n";
            break;
        case AtomicType::IntegerTypeID::LINT:
            ret += "min: " + std::to_string(min.val.lint_val) + "\n";
            ret += "max: " + std::to_string(max.val.lint_val) + "\n";
            break;
        case AtomicType::IntegerTypeID::ULINT:
            ret += "min: " + std::to_string(min.val.ulint_val) + "\n";
            ret += "max: " + std::to_string(max.val.ulint_val) + "\n";
            break;
         case AtomicType::IntegerTypeID::LLINT:
            ret += "min: " + std::to_string(min.val.llint_val) + "\n";
            ret += "max: " + std::to_string(max.val.llint_val) + "\n";
            break;
         case AtomicType::IntegerTypeID::ULLINT:
            ret += "min: " + std::to_string(min.val.ullint_val) + "\n";
            ret += "max: " + std::to_string(max.val.ullint_val) + "\n";
            break;
        case MAX_INT_ID:
            std::cerr << "ERROR at " << __FILE__ << ":" << __LINE__ << ": unsupported int type in BitField" << std::endl;
            exit(-1);
            break;
    }
    ret += "bit_size: " + std::to_string(bit_size) + "\n";
    ret += "is_signed: " + std::to_string(is_signed) + "\n";
    std::cout << ret;
}

ArrayType::ArrayType(std::shared_ptr<Type> _base_type, size_t _size, ArrayType::Kind _kind)
        : Type (Type::ARRAY_TYPE), base_type(_base_type), size(_size), kind(_kind) {
    name = "";
    init_depth();
}

ArrayType::ArrayType(std::shared_ptr<Type> _base_type, size_t _size, ArrayType::Kind _kind, Type::Mod _modifier,
                     bool _is_static, uint64_t _align)
        : Type (Type::ARRAY_TYPE, _modifier, _is_static, _align), base_type(_base_type), size(_size), kind(_kind) {
    name = "";
    init_depth();
}

void ArrayType::init_depth() {
    depth = 1;
    if (base_type->is_array_type())
        depth += std::static_pointer_cast<ArrayType>(base_type)->get_depth();
}

uint64_t ArrayType::get_nest_struct_depth() {
    return base_type->get_nest_struct_depth();
}

uint64_t ArrayType::get_nest_array_depth() {
    uint64_t ret = 1;
    ret += base_type->get_nest_array_depth();
    return ret;
}

std::shared_ptr<Type> ArrayType::get_lower_base_type () {
    if (!base_type->is_array_type())
        return base_type;
    std::shared_ptr<ArrayType> base_arr_type = std::static_pointer_cast<ArrayType>(base_type);
    return base_arr_type->get_lower_base_type();
}

void ArrayType::dbg_dump() {
    std::string ret = "";
    ret += "kind: " + std::to_string(kind) + "\n";
    ret += "size: " + std::to_string(size) + "\n";
    ret += "depth: " + std::to_string(depth) + "\n";
    ret += "full name: " + get_name();
    if (kind == C_ARR)
        ret += " " + get_suffix();
    ret += "\n";
    ret += "suffix: " + get_suffix() + "\n";
    ret += "base type: ";
    std::cout << ret;
    base_type->dbg_dump();
    std::cout << std::endl;
}

std::string ArrayType::get_name() {
    std::string ret = "";
    ret += get_spec_and_mod_str();
    ret += get_simple_name_with_ptr_sign_ctrl();
    if (align != 0)
        ret += " __attribute__(aligned(" + std::to_string(align) + "))";
    return ret;
}

std::string ArrayType::get_simple_name_with_ptr_sign_ctrl (bool emit_ptr_sign) {
    std::string ret = "";
    emit_ptr_sign |= (kind != C_ARR);

    switch (kind) {
        case C_ARR:
            break;
        case VAL_ARR:
            ret += "std::valarray<";
            break;
        case STD_VEC:
            ret += "std::vector<";
            break;
        case STD_ARR:
            ret += "std::array<";
            break;
        case MAX_KIND:
            std::cerr << "ERROR at " << __FILE__ << ":" << __LINE__ << ": unsupported kind of array" << std::endl;
            exit(-1);
    }

    if (base_type->is_array_type())
        ret += std::static_pointer_cast<ArrayType>(base_type)->get_simple_name_with_ptr_sign_ctrl(emit_ptr_sign);
    else
        ret += base_type->get_simple_name();

    if (emit_ptr_sign &&
        base_type->is_array_type() && std::static_pointer_cast<ArrayType>(base_type)->get_kind() == C_ARR)
        ret += "*";

    switch (kind) {
        case C_ARR:
            break;
        case VAL_ARR:
        case STD_VEC:
            ret += ">";
            break;
        case STD_ARR:
            ret += ", " + std::to_string(size) + ">";
            break;
        case MAX_KIND:
            std::cerr << "ERROR at " << __FILE__ << ":" << __LINE__ << ": unsupported kind of array" << std::endl;
            exit(-1);
    }
    return ret;
}

std::string ArrayType::get_simple_name () {
    return get_simple_name_with_ptr_sign_ctrl ();
}


std::string ArrayType::get_suffix() {
    if (kind != C_ARR)
        return "";
    std::string ret = "[" + std::to_string(size) + "]";
    if (base_type->is_array_type()) {
        std::shared_ptr<ArrayType> base_array_type = std::static_pointer_cast<ArrayType>(base_type);
        std::string base_suffix = base_array_type->get_suffix();
        if (!base_suffix.empty())
            ret += " " + base_suffix;
    }
    return ret;
}

std::shared_ptr<ArrayType> ArrayType::generate(std::shared_ptr<Context> ctx) {
    std::vector<std::shared_ptr<StructType>> empty_vec;
    return generate(ctx, empty_vec);
}

std::shared_ptr<ArrayType> ArrayType::generate (std::shared_ptr<Context> ctx,
                                                std::vector<std::shared_ptr<StructType>> avail_struct_types) {
    //TODO: should we generate nested arrays here? Let it be so.
    uint64_t depth = rand_val_gen->get_rand_value(ctx->get_gen_policy()->get_min_array_depth(),
                                                  ctx->get_gen_policy()->get_max_array_depth());
    std::shared_ptr<Context> gen_ctx = std::make_shared<Context>(*ctx);
    if (!ctx->get_gen_policy()->get_allow_mix_kind()) {
        Kind primary_kind = rand_val_gen->get_rand_id(ctx->get_gen_policy()->get_array_kind_prob());
        std::vector<Probability<Kind>> new_array_kind_prob;
        new_array_kind_prob.push_back(Probability<Kind>(primary_kind, 100));

        GenPolicy tmp_gen_policy = *(ctx->get_gen_policy());
        tmp_gen_policy.set_array_kind_prob(new_array_kind_prob);
        gen_ctx->set_gen_policy(tmp_gen_policy);
    }
    Type::TypeID base_type_id = rand_val_gen->get_rand_id(ctx->get_gen_policy()->get_array_base_type_prob());
    bool add_struct_with_array = rand_val_gen->get_rand_id(ctx->get_gen_policy()->get_struct_with_array_prob());
    std::shared_ptr<ArrayType> base_array = auxiliary_generate(gen_ctx, avail_struct_types, depth,
                                                               base_type_id, add_struct_with_array);

    Type::Mod modifier = ctx->get_gen_policy()->get_allowed_modifiers().at(
            rand_val_gen->get_rand_value(0UL, ctx->get_gen_policy()->get_allowed_modifiers().size() - 1));
    base_array->set_modifier(modifier);

    bool is_static = false;
    if (ctx->get_gen_policy()->get_allow_static_var())
        is_static = rand_val_gen->get_rand_value<int>(false, true);
    base_array->set_is_static(is_static);

    //TODO: what about align?
    base_array->set_align(0);

    return base_array;
}

std::shared_ptr<StructType> ArrayType::get_suitable_struct (std::vector<std::shared_ptr<StructType>> avail_subtypes,
                                                            uint64_t exact_depth) {
    std::vector<std::shared_ptr<StructType>> suitable_subtypes;
    for (const auto &subtype : avail_subtypes)
        if (subtype->get_nest_array_depth() == exact_depth)
            suitable_subtypes.push_back(subtype);

    if (suitable_subtypes.size() > 0) {
        uint64_t subtype_idx = rand_val_gen->get_rand_value(0UL, suitable_subtypes.size() - 1);
        return suitable_subtypes.at(subtype_idx);
    } else
        return NULL;
}

std::shared_ptr<ArrayType> ArrayType::auxiliary_generate(std::shared_ptr<Context> ctx,
                                                         std::vector<std::shared_ptr<StructType>> avail_struct_types,
                                                         uint32_t left_depth,
                                                         Type::TypeID base_type_id,
                                                         bool add_struct_with_arr) {
    Kind kind = rand_val_gen->get_rand_id(ctx->get_gen_policy()->get_array_kind_prob());
    size_t size = rand_val_gen->get_rand_value(ctx->get_gen_policy()->get_min_array_size(),
                                               ctx->get_gen_policy()->get_max_array_size());
    std::shared_ptr<Type> base_type = NULL;
    if (add_struct_with_arr ||
       (left_depth == 1 && base_type_id == Type::STRUCT_TYPE))
        base_type = get_suitable_struct(avail_struct_types, left_depth - 1);

    if (left_depth == 1) {
        if (base_type_id == Type::ATOMIC_TYPE ||
           //If we can't choose suitable struct, we fall back to AtomicType
           (base_type == NULL && base_type_id == Type::STRUCT_TYPE))
            base_type = IntegerType::generate(ctx);
        else if (base_type_id == Type::STRUCT_TYPE) {
            // If we can't choose suitable struct, we fall back to AtomicType (see above)
        }
        else {
            std::cerr << "ERROR at " << __FILE__ << ":" << __LINE__ << ": unsupported base type for array" << std::endl;
            exit(-1);
        }
    }
    else
        base_type = ArrayType::auxiliary_generate(ctx, avail_struct_types, left_depth - 1, base_type_id, add_struct_with_arr);
    return std::make_shared<ArrayType>(base_type, size, kind);
}
