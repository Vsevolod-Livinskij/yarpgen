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

#pragma once

#include <iostream>
#include <climits>
#include <memory>
#include <vector>

namespace rl {

class Context;
class Data;

extern bool mode_64bit;

// DB: need general description explaining the what this class does.
// Are type objects unique, etc?
// Also methods / group of methods need comments for readability - this applies
// to this class and all its descendants.
class Type {
    public:
        enum TypeID {
            ATOMIC_TYPE,
            STRUCT_TYPE,
            MAX_TYPE_ID
        };

        // DB: in C++ it's called CV-qualifiers.
        enum Mod {
            NTHG,
            VOLAT,
            CONST,
            CONST_VOLAT,
            MAX_MOD
        };

        // DB: Why the name is "atomic"? It's a bit confusing.
        // What about "basic" or "fundamental"?
        enum AtomicTypeID {
            Integer, FP, Max_AtomicTypeID
        };

        enum IntegerTypeID {
            BOOL,
            // Note, char and signed char types are not distinguished,
            // though they are distinguished in C++ standard and char may
            // map to unsigned char in some implementaions.
            CHAR,
            UCHAR,
            SHRT,
            USHRT,
            INT,
            UINT,
            LINT,
            ULINT,
            LLINT,
            ULLINT,
            MAX_INT_ID,
        };

        Type (TypeID _id) : modifier(Mod::NTHG), is_static(false), align(0), id (_id) {}
        Type (TypeID _id, Mod _modifier, bool _is_static, uint64_t _align) :
              modifier (_modifier), is_static (_is_static), align (_align), id (_id) {}
        Type::TypeID get_type_id () { return id; }
        virtual AtomicTypeID get_atomic_type_id () { return Max_AtomicTypeID; }
        virtual IntegerTypeID get_int_type_id () { return MAX_INT_ID; }
        virtual bool get_is_signed() { return false; }
        virtual bool get_is_bit_field() { return false; }
        std::string get_name ();
        std::string get_simple_name () { return name; }
        void set_modifier (Mod _modifier) { modifier = _modifier; }
        Mod get_modifier () { return modifier; }
        void set_is_static (bool _is_static) { is_static = _is_static; }
        bool get_is_static () { return is_static; }
        void set_align (uint64_t _align) { align = _align; }
        uint64_t get_align () { return align; }

        virtual bool is_atomic_type() { return false; }
        virtual bool is_ptr_type() { return false; }
        virtual bool is_int_type() { return false; }
        virtual bool is_fp_type() { return false; }
        virtual bool is_struct_type() { return false; }

        virtual void dbg_dump() = 0;

    protected:
        std::string name;
        Mod modifier;
        bool is_static;
        uint64_t align;

    private:
        TypeID id;
};

class StructType : public Type {
    public:
        //TODO: add generator?
        struct StructMember {
            public:
                StructMember (std::shared_ptr<Type> _type, std::string _name);
                std::string get_name () { return name; }
                std::shared_ptr<Type> get_type() { return type; }
                std::shared_ptr<Data> get_data () { return data; }
                std::string get_definition (std::string offset = "");

            private:
                std::shared_ptr<Type> type;
                std::string name;

                std::shared_ptr<Data> data; //TODO: it is a stub for static members
        };

        StructType (std::string _name) : Type (Type::STRUCT_TYPE), nest_depth(0) { name = _name; }
        StructType (std::string _name, Mod _modifier, bool _is_static, uint64_t _align) :
                    Type (Type::STRUCT_TYPE, _modifier, _is_static, _align), nest_depth(0) { name = _name; }
        //TODO: it should handle nest_depth change
        void add_member (std::shared_ptr<StructMember> new_mem) { members.push_back(new_mem); shadow_members.push_back(new_mem); }
        void add_member (std::shared_ptr<Type> _type, std::string _name);
        void add_shadow_member (std::shared_ptr<Type> _type) { shadow_members.push_back(std::make_shared<StructMember>(_type, "")); }
        uint64_t get_member_count () { return members.size(); }
        uint64_t get_shadow_member_count () { return shadow_members.size(); }
        uint64_t get_nest_depth () { return nest_depth; }
        std::shared_ptr<StructMember> get_member (unsigned int num);
        std::string get_definition (std::string offset = "");
        std::string get_static_memb_def (std::string offset = "");
        bool is_struct_type() { return true; }
        void dbg_dump();
        static std::shared_ptr<StructType> generate (std::shared_ptr<Context> ctx);
        static std::shared_ptr<StructType> generate (std::shared_ptr<Context> ctx, std::vector<std::shared_ptr<StructType>> nested_struct_types);

    private:
        //TODO: it is a stub for unnamed bit fields. Nobody should know about them
        std::vector<std::shared_ptr<StructMember>> shadow_members;
        std::vector<std::shared_ptr<StructMember>> members;
        uint64_t nest_depth;
};

enum UB {
    NoUB,
    NullPtr, // nullptr ptr dereferencing
    SignOvf, // Signed overflow
    SignOvfMin, // Special case of signed overflow: INT_MIN * (-1)
    ZeroDiv, // FPE
    ShiftRhsNeg, // Shift by negative value
    ShiftRhsLarge, // // Shift by large value
    NegShift, // Shift of negative value
    NoMemeber, // Can't find member of structure
    MaxUB
};

class AtomicType : public Type {
    public:
        class ScalarTypedVal {
            public:
                union Val {
                    bool bool_val;
                    signed char char_val;
                    unsigned char uchar_val;
                    short shrt_val;
                    unsigned short ushrt_val;
                    int int_val;
                    unsigned int uint_val;
                    int lint32_val; // for 32-bit mode
                    unsigned int ulint32_val; // for 32-bit mode
                    long long int lint64_val; // for 64-bit mode
                    unsigned long long int ulint64_val; // for 64-bit mode
                    long long int llint_val;
                    unsigned long long int ullint_val;
                };

                ScalarTypedVal (AtomicType::IntegerTypeID _int_type_id) : int_type_id(_int_type_id), res_of_ub(NoUB) { val.ullint_val = 0; }
                ScalarTypedVal (AtomicType::IntegerTypeID _int_type_id, UB _res_of_ub) : int_type_id (_int_type_id), res_of_ub(_res_of_ub)  { val.ullint_val = 0; }
                Type::IntegerTypeID get_int_type_id () const { return int_type_id; }
                UB get_ub () { return res_of_ub; }
                void set_ub (UB _ub) { res_of_ub = _ub; }
                bool has_ub () { return res_of_ub != NoUB; }

                //TODO: it is a stub for shift rebuild. Can we do it better?
                uint64_t get_abs_val ();
                void set_abs_val (uint64_t new_val);

                ScalarTypedVal cast_type (Type::IntegerTypeID to_type_id);
                ScalarTypedVal operator++ (int) { return pre_op(true ); } // Postfix, but uzed also as prefix
                ScalarTypedVal operator-- (int) { return pre_op(false); }// Postfix, but uzed also as prefix
                ScalarTypedVal operator- ();
                ScalarTypedVal operator~ ();
                ScalarTypedVal operator! ();

                ScalarTypedVal operator+ (ScalarTypedVal rhs);
                ScalarTypedVal operator- (ScalarTypedVal rhs);
                ScalarTypedVal operator* (ScalarTypedVal rhs);
                ScalarTypedVal operator/ (ScalarTypedVal rhs);
                ScalarTypedVal operator% (ScalarTypedVal rhs);
                ScalarTypedVal operator< (ScalarTypedVal rhs);
                ScalarTypedVal operator> (ScalarTypedVal rhs);
                ScalarTypedVal operator<= (ScalarTypedVal rhs);
                ScalarTypedVal operator>= (ScalarTypedVal rhs);
                ScalarTypedVal operator== (ScalarTypedVal rhs);
                ScalarTypedVal operator!= (ScalarTypedVal rhs);
                ScalarTypedVal operator&& (ScalarTypedVal rhs);
                ScalarTypedVal operator|| (ScalarTypedVal rhs);
                ScalarTypedVal operator& (ScalarTypedVal rhs);
                ScalarTypedVal operator| (ScalarTypedVal rhs);
                ScalarTypedVal operator^ (ScalarTypedVal rhs);
                ScalarTypedVal operator<< (ScalarTypedVal rhs);
                ScalarTypedVal operator>> (ScalarTypedVal rhs);

                static ScalarTypedVal generate (std::shared_ptr<Context> ctx, AtomicType::IntegerTypeID _int_type_id);
                static ScalarTypedVal generate (std::shared_ptr<Context> ctx, ScalarTypedVal min, ScalarTypedVal max);

                Val val;

            private:
                ScalarTypedVal pre_op (bool inc);

                AtomicType::IntegerTypeID int_type_id;
                UB res_of_ub;
        };

        AtomicType (AtomicTypeID at_id) : Type (Type::ATOMIC_TYPE), bit_size (0), suffix(""), atomic_id (at_id) {}
        AtomicType (AtomicTypeID at_id, Mod _modifier, bool _is_static, uint64_t _align) :
                    Type (Type::ATOMIC_TYPE, _modifier, _is_static, _align), bit_size (0), suffix(""), atomic_id (at_id) {}
        AtomicTypeID get_atomic_type_id () { return atomic_id; }
        uint64_t get_bit_size () { return bit_size; }
        std::string get_suffix () { return suffix; }
        bool is_atomic_type() { return true; }

    protected:
        unsigned int bit_size;
        std::string suffix;

    private:
        AtomicTypeID atomic_id;
};

std::ostream& operator<< (std::ostream &out, const AtomicType::ScalarTypedVal &scalar_typed_val);

class IntegerType : public AtomicType {
    public:
        IntegerType (IntegerTypeID it_id) : AtomicType (AtomicTypeID::Integer), is_signed (false), min(it_id), max(it_id), int_type_id (it_id) {}
        IntegerType (IntegerTypeID it_id, Mod _modifier, bool _is_static, uint64_t _align) :
                     AtomicType (AtomicTypeID::Integer, _modifier, _is_static, _align),
                     is_signed (false), min(it_id), max(it_id), int_type_id (it_id) {}
        static std::shared_ptr<IntegerType> init (AtomicType::IntegerTypeID _type_id);
        static std::shared_ptr<IntegerType> init (AtomicType::IntegerTypeID _type_id, Mod _modifier, bool _is_static, uint64_t _align);
        IntegerTypeID get_int_type_id () { return int_type_id; }
        static bool can_repr_value (AtomicType::IntegerTypeID A, AtomicType::IntegerTypeID B); // if type B can represent all of the values of the type A
        static AtomicType::IntegerTypeID get_corr_unsig (AtomicType::IntegerTypeID _type_id);
        bool get_is_signed () { return is_signed; }
        AtomicType::ScalarTypedVal get_min () { return min; }
        AtomicType::ScalarTypedVal get_max () { return max; }
        bool is_int_type() { return true; }
        static std::shared_ptr<IntegerType> generate (std::shared_ptr<Context> ctx);

    protected:
        bool is_signed;
        AtomicType::ScalarTypedVal min;
        AtomicType::ScalarTypedVal max;

    private:
        IntegerTypeID int_type_id;
};

class BitField : public IntegerType {
    public:
        BitField (IntegerTypeID it_id, uint64_t _bit_size) : IntegerType(it_id) { init_type(it_id, _bit_size); }
        BitField (IntegerTypeID it_id, uint64_t _bit_size, Mod _modifier) : IntegerType(it_id, _modifier, false, 0) { init_type(it_id, _bit_size); }
        bool get_is_bit_field() { return true; }
        static bool can_fit_in_int (AtomicType::ScalarTypedVal val, bool is_unsigned);
        uint64_t get_bit_field_width() { return bit_field_width; }
        static std::shared_ptr<BitField> generate (std::shared_ptr<Context> ctx, bool is_unnamed = false);
        void dbg_dump ();

    private:
        void init_type(IntegerTypeID it_id, uint64_t _bit_size);
        uint64_t bit_field_width;
};

class TypeBOOL : public IntegerType {
    public:
        TypeBOOL () : IntegerType(AtomicType::IntegerTypeID::BOOL) { init_type (); }
        TypeBOOL (Mod _modifier, bool _is_static, uint64_t _align) :
                  IntegerType(AtomicType::IntegerTypeID::BOOL, _modifier, _is_static, _align) { init_type (); }
        void dbg_dump ();

    private:
        void init_type () {
            name = "bool";
            suffix = "";
            min.val.bool_val = false;
            max.val.bool_val = true;
            bit_size = sizeof (bool) * CHAR_BIT;
            is_signed = false;
        }
};

class TypeCHAR : public IntegerType {
    public:
        TypeCHAR () : IntegerType(AtomicType::IntegerTypeID::CHAR) { init_type (); }
        TypeCHAR (Mod _modifier, bool _is_static, uint64_t _align) :
                  IntegerType(AtomicType::IntegerTypeID::CHAR, _modifier, _is_static, _align) { init_type (); }
        void dbg_dump ();

    private:
        void init_type () {
            name = "signed char";
            suffix = "";
            min.val.char_val = SCHAR_MIN;
            max.val.char_val = SCHAR_MAX;
            bit_size = sizeof (char) * CHAR_BIT;
            is_signed = true;
        }
};

class TypeUCHAR : public IntegerType {
    public:
        TypeUCHAR () : IntegerType(AtomicType::IntegerTypeID::UCHAR) { init_type (); }
        TypeUCHAR (Mod _modifier, bool _is_static, uint64_t _align) :
                   IntegerType(AtomicType::IntegerTypeID::UCHAR, _modifier, _is_static, _align) { init_type (); }
        void dbg_dump ();

    private:
        void init_type () {
            name = "unsigned char";
            suffix = "";
            min.val.uchar_val = 0;
            max.val.uchar_val = UCHAR_MAX;
            bit_size = sizeof (unsigned char) * CHAR_BIT;
            is_signed = false;
        }
};

class TypeSHRT : public IntegerType {
    public:
        TypeSHRT () : IntegerType(AtomicType::IntegerTypeID::SHRT) { init_type (); }
        TypeSHRT (Mod _modifier, bool _is_static, uint64_t _align) :
                  IntegerType(AtomicType::IntegerTypeID::SHRT, _modifier, _is_static, _align) { init_type (); }
        void dbg_dump ();

    private:
        void init_type () {
            name = "short";
            suffix = "";
            min.val.shrt_val = SHRT_MIN;
            max.val.shrt_val = SHRT_MAX;
            bit_size = sizeof (short) * CHAR_BIT;
            is_signed = true;
        }
};

class TypeUSHRT : public IntegerType {
    public:
        TypeUSHRT () : IntegerType(AtomicType::IntegerTypeID::USHRT) { init_type (); }
        TypeUSHRT (Mod _modifier, bool _is_static, uint64_t _align) :
                   IntegerType(AtomicType::IntegerTypeID::USHRT, _modifier, _is_static, _align) { init_type (); }
        void dbg_dump ();

    private:
        void init_type () {
            name = "unsigned short";
            suffix = "";
            min.val.ushrt_val = 0;
            max.val.ushrt_val = USHRT_MAX;
            bit_size = sizeof (unsigned short) * CHAR_BIT;
            is_signed = false;
        }
};

class TypeINT : public IntegerType {
    public:
        TypeINT () : IntegerType(AtomicType::IntegerTypeID::INT) { init_type (); }
        TypeINT (Mod _modifier, bool _is_static, uint64_t _align) :
                 IntegerType(AtomicType::IntegerTypeID::INT, _modifier, _is_static, _align) { init_type (); }
        void dbg_dump ();

    private:
        void init_type () {
            name = "int";
            suffix = "";
            min.val.int_val = INT_MIN;
            max.val.int_val = INT_MAX;
            bit_size = sizeof (int) * CHAR_BIT;
            is_signed = true;
        }
};

class TypeUINT : public IntegerType {
    public:
        TypeUINT () : IntegerType(AtomicType::IntegerTypeID::UINT) { init_type (); }
        TypeUINT (Mod _modifier, bool _is_static, uint64_t _align) :
                  IntegerType(AtomicType::IntegerTypeID::UINT, _modifier, _is_static, _align) { init_type (); }
        void dbg_dump ();

    private:
        void init_type () {
            name = "unsigned int";
            suffix = "U";
            min.val.uint_val = 0;
            max.val.uint_val = UINT_MAX;
            bit_size = sizeof (unsigned int) * CHAR_BIT;
            is_signed = false;
        }
};

class TypeLINT : public IntegerType {
    public:
        TypeLINT () : IntegerType(AtomicType::IntegerTypeID::LINT) { init_type (); }
        TypeLINT (Mod _modifier, bool _is_static, uint64_t _align) :
                  IntegerType(AtomicType::IntegerTypeID::LINT, _modifier, _is_static, _align) { init_type (); }
        void dbg_dump ();

    private:
        void init_type () {
            name = "long int";
            suffix = "L";
            if (mode_64bit) {
                bit_size = sizeof(long long int) * CHAR_BIT;
                min.val.lint64_val = LLONG_MIN;
                max.val.lint64_val = LLONG_MAX;
            }
            else {
                bit_size = sizeof(int) * CHAR_BIT;
                min.val.lint32_val = INT_MIN;
                max.val.lint32_val = INT_MAX;
            }
            is_signed = true;
        }
};

class TypeULINT : public IntegerType {
    public:
        TypeULINT () : IntegerType(AtomicType::IntegerTypeID::ULINT) { init_type (); }
        TypeULINT (Mod _modifier, bool _is_static, uint64_t _align) :
                   IntegerType(AtomicType::IntegerTypeID::ULINT, _modifier, _is_static, _align) { init_type (); }
        void dbg_dump ();

    private:
        void init_type () {
            name = "unsigned long int";
            suffix = "UL";
            if (mode_64bit) {
                bit_size = sizeof (unsigned long long int) * CHAR_BIT;
                min.val.ulint64_val = 0;
                max.val.ulint64_val = ULLONG_MAX;
            }
            else {
                bit_size = sizeof(unsigned int) * CHAR_BIT;
                min.val.ulint32_val = 0;
                max.val.ulint32_val = UINT_MAX;
            }
            is_signed = false;
        }
};

class TypeLLINT : public IntegerType {
    public:
        TypeLLINT () : IntegerType(AtomicType::IntegerTypeID::LLINT) { init_type (); }
        TypeLLINT (Mod _modifier, bool _is_static, uint64_t _align) :
                   IntegerType(AtomicType::IntegerTypeID::LLINT, _modifier, _is_static, _align) { init_type (); }
        void dbg_dump ();

    private:
        void init_type () {
            name = "long long int";
            suffix = "LL";
            min.val.llint_val = LLONG_MIN;
            max.val.llint_val = LLONG_MAX;
            bit_size = sizeof (long long int) * CHAR_BIT;
            is_signed = true;
        }
};

class TypeULLINT : public IntegerType {
    public:
        TypeULLINT () : IntegerType(AtomicType::IntegerTypeID::ULLINT) { init_type (); }
        TypeULLINT (Mod _modifier, bool _is_static, uint64_t _align) :
                    IntegerType(AtomicType::IntegerTypeID::ULLINT, _modifier, _is_static, _align) { init_type (); }
        void dbg_dump ();

    private:
        void init_type () {
            name = "unsigned long long int";
            suffix = "ULL";
            min.val.ullint_val = 0;
            max.val.ullint_val = ULLONG_MAX;
            bit_size = sizeof (unsigned long long int) * CHAR_BIT;
            is_signed = false;
        }
};
}
