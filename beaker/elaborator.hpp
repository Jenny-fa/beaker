// Copyright (c) 2015 Andrew Sutton
// All rights reserved

#ifndef BEAKER_ELABORATOR_HPP
#define BEAKER_ELABORATOR_HPP

#include "prelude.hpp"
#include "environment.hpp"

#include <stack>
#include <unordered_map>
#include <vector>


// A scope defines a maximal lexical region of a program
// where no bindings are destroyed. A scope optionally
// assocaites a declaration with its bindings. This is
// used to maintain the current declaration context.
struct Scope : Environment<Symbol const*, Decl*>
{
  Scope()
    : decl(nullptr)
  { }

  Scope(Decl* d)
    : decl(d)
  { }
  
  Decl* decl;
};


// The scope stack maintains the current scope during
// elaboration. It adapts the more general stack to
// provide more language-specific names for those
// operations.
struct Scope_stack : Stack<Scope>
{
  Scope&       current()       { return top(); }
  Scope const& current() const { return top(); }

  Scope&       global()       { return bottom(); }
  Scope const& global() const { return bottom(); }

  Decl*          context() const;
  Module_decl*   module() const;
  Function_decl* function() const;

  void declare(Decl*);
};


// The elaborator is responsible for the annotation of
// an AST with type and other information.
class Elaborator
{
  struct Scope_sentinel;
public:
  Type const* elaborate(Expr*);
  Type const* elaborate(Literal_expr*);
  Type const* elaborate(Id_expr*);
  Type const* elaborate(Add_expr* e);
  Type const* elaborate(Sub_expr* e);
  Type const* elaborate(Mul_expr* e);
  Type const* elaborate(Div_expr* e);
  Type const* elaborate(Rem_expr* e);
  Type const* elaborate(Neg_expr* e);
  Type const* elaborate(Pos_expr* e);
  Type const* elaborate(Eq_expr* e);
  Type const* elaborate(Ne_expr* e);
  Type const* elaborate(Lt_expr* e);
  Type const* elaborate(Gt_expr* e);
  Type const* elaborate(Le_expr* e);
  Type const* elaborate(Ge_expr* e);
  Type const* elaborate(And_expr* e);
  Type const* elaborate(Or_expr* e);
  Type const* elaborate(Not_expr* e);
  Type const* elaborate(Call_expr* e);
  
  Type const* elaborate(Decl*);
  Type const* elaborate(Variable_decl*);
  Type const* elaborate(Function_decl*);
  Type const* elaborate(Parameter_decl*);
  Type const* elaborate(Module_decl*);
  
  Type const* elaborate(Stmt*);
  Type const* elaborate(Empty_stmt*);
  Type const* elaborate(Block_stmt*);
  Type const* elaborate(Return_stmt*);
  Type const* elaborate(Expression_stmt*);
  Type const* elaborate(Declaration_stmt*);


  // Found symbols.
  Function_decl* main = nullptr;

private:
  Scope_stack stack;
};


struct Elaborator::Scope_sentinel
{
  Scope_sentinel(Elaborator& e, Decl* d = nullptr)
    : elab(e)
  {
    elab.stack.push(d);
  }

  ~Scope_sentinel()
  {
    elab.stack.pop();
  }

  Elaborator& elab;
};


#endif
