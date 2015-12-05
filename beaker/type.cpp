// Copyright (c) 2015 Andrew Sutton
// All rights reserved

#include "beaker/type.hpp"
#include "beaker/decl.hpp"
#include "beaker/less.hpp"
#include "beaker/value.hpp"
#include "beaker/evaluator.hpp"

#include <set>


// Return a reference type for this type.
Type const*
Type::ref() const
{
  return get_reference_type(this);
}


// Return the non-reference type for this type.
Type const*
Type::nonref() const
{
  return this;
}


Type const*
Reference_type::ref() const
{
  return this;
}


Type const*
Reference_type::nonref() const
{
  return first;
}


Record_decl*
Record_type::declaration() const
{
  return cast<Record_decl>(decl_);
}


Scope*
Record_type::scope() const
{
  return declaration()->scope();
}



// Returns the size of the array as an
// integer value.
int
Array_type::size() const
{
  Value v = evaluate(extent());
  return v.get_integer();
}


// -------------------------------------------------------------------------- //
// Type accessors

template<typename T>
struct Type_less
{
  bool operator()(T const& a, T const& b) const
  {
    return is_less(&a, &b);
  }
};


template<typename T>
using Type_set = std::set<T, Type_less<T>>;


// Note that id types are not canonicalized.
// They don't need to be since they never
// escape elaboration.
Type const*
get_id_type(Symbol const* s)
{
  return new Id_type(s);
}


Type const*
get_boolean_type()
{
  static Boolean_type t;
  return &t;
}


Type const*
get_character_type()
{
  static Character_type t;
  return &t;
}


Type const*
get_integer_type(bool is_signed)
{
  static Integer_type st;
  static Integer_type ut(false);
    
  if (is_signed)
    return &st;
  else
    return &ut;
}

Type const*
get_short_integer_type(bool is_signed)
{
  static Short_Integer_type st;
  static Short_Integer_type ut(false);
    
  if (is_signed)
    return &st;
  else
    return &ut;
}

Type const*
get_long_integer_type(bool is_signed)
{
  static Long_Integer_type st;
  static Long_Integer_type ut(false);
    
  if (is_signed)
    return &st;
  else
    return &ut;
}

Type const*
get_integer16_type(bool is_signed)
{
  static Integer16_type st;
  static Integer16_type ut(false);
    
  if (is_signed)
    return &st;
  else
    return &ut;
}

Type const*
get_integer32_type(bool is_signed)
{
  static Integer32_type st;
  static Integer32_type ut(false);
    
  if (is_signed)
    return &st;
  else
    return &ut;
}

Type const*
get_integer64_type(bool is_signed)
{
  static Integer64_type st;
  static Integer64_type ut(false);
    
  if (is_signed)
    return &st;
  else
    return &ut;
}

Type const*
get_integer128_type(bool is_signed)
{
  static Integer128_type st;
  static Integer128_type ut(false);
    
  if (is_signed)
    return &st;
  else
    return &ut;
}

Type const*
get_float_type()
{
  static Float_type t;
  return &t;
}

Type const*
get_double_type()
{
  static Double_type t;
  return &t;
}

Type const*
get_function_type(Type_seq const& t, Type const* r)
{
  static Type_set<Function_type> fn;
  auto ins = fn.emplace(t, r);
  return &*ins.first;
}


Type const*
get_function_type(Decl_seq const& d, Type const* r)
{
  Type_seq t;
  t.reserve(d.size());
  for (Decl* d1 : d)
    t.push_back(d1->type());
  return get_function_type(t, r);
}


Type const*
get_array_type(Type const* t, Expr* n)
{
  static Type_set<Array_type> ts;
  auto ins = ts.emplace(t, n);
  return &*ins.first;
}


Type const*
get_block_type(Type const* t)
{
  static Type_set<Block_type> ts;
  auto ins = ts.emplace(t);
  return &*ins.first;
}


// FIXME: Don't allow references to non-object types.
Type const*
get_reference_type(Type const* t)
{
  static Type_set<Reference_type> ts;
  auto ins = ts.emplace(t);
  return &*ins.first;
}


Type const*
get_record_type(Record_decl* r)
{
  static Type_set<Record_type> ts;
  auto ins = ts.emplace(r);
  return &*ins.first;
}
