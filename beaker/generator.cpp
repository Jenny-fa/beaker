
#include "generator.hpp"
#include "type.hpp"
#include "expr.hpp"
#include "stmt.hpp"
#include "decl.hpp"
#include "evaluator.hpp"

#include "llvm/IR/Type.h"
#include "llvm/IR/GlobalVariable.h"
#include "llvm/IR/Function.h"
#include "llvm/IR/Constants.h"
#include "llvm/IR/Instructions.h"
#include "llvm/IR/Module.h"
#include "llvm/Support/Debug.h"

#include <iostream>


// -------------------------------------------------------------------------- //
//            Helper functions


// Attempt to insert a branch into a block
// Will not insert anything if the block already
// has a terminating instruction
void 
Generator::make_branch(llvm::BasicBlock* srcBB, llvm::BasicBlock* dstBB) 
{
  if (!srcBB->getTerminator())
    build.CreateBr(dstBB);
}


// Resolve illformed blocks within an llvm function
// These are blocks with no termination instructions.
//
// This can be caused by short-curcuiting if-then-stmt like:
//
// def foo(x : int) -> int {
//    if (x == 1)
//      return x;
// }
//
// The block merging back into the control will have no terminators.
// Resolve them by inserting the terminator instruction 'unreachable'
//
void
Generator::resolve_illformed_blocks(llvm::Function* fn)
{
  // maintain the old insert block
  auto prev = build.GetInsertBlock();

  for (llvm::Function::iterator i = fn->begin(), e = fn->end(); i != e; ++i) {
    // if no terminator inject an unreachable instruction
    if (!i->getTerminator()) {
      build.SetInsertPoint(i);
      build.CreateUnreachable();
    }
  }

  // reset the old insertion block
  build.SetInsertPoint(prev);
}


// -------------------------------------------------------------------------- //
// Mapping of types
//
// The type generator transforms a beaker type into
// its correspondiong LLVM type.


llvm::Type*
Generator::get_type(Type const* t)
{
  struct Fn
  {
    Generator& g;
    llvm::Type* operator()(Boolean_type const* t) const { return g.get_type(t); }
    llvm::Type* operator()(Integer_type const* t) const { return g.get_type(t); }
    llvm::Type* operator()(Function_type const* t) const { return g.get_type(t); }
    llvm::Type* operator()(Reference_type const* t) const { return g.get_type(t); }
  };
  return apply(t, Fn{*this});
}


// Return the 1 bit integer type.
llvm::Type*
Generator::get_type(Boolean_type const*)
{
  return build.getInt1Ty();
}


// Return the 32 bit integer type.
llvm::Type*
Generator::get_type(Integer_type const*)
{
  return build.getInt32Ty();
}


// Return a function type.
llvm::Type*
Generator::get_type(Function_type const* t)
{
  std::vector<llvm::Type*> ts;
  ts.reserve(t->parameter_types().size());
  for (Type const* t1 : t->parameter_types())
    ts.push_back(get_type(t1));
  llvm::Type* r = get_type(t->return_type());
  return llvm::FunctionType::get(r, ts, false);
}


// Translate reference types into pointer types in the
// generic address space.
//
// TODO: Actually do this?
llvm::Type*
Generator::get_type(Reference_type const* t)
{
  llvm::Type* t1 = get_type(t->type());
  return llvm::PointerType::getUnqual(t1);
}


// -------------------------------------------------------------------------- //
// Code generation for expressions
//
// An expression is transformed into a sequence instructions whose
// intermediate results are saved in registers.

llvm::Value*
Generator::gen(Expr const* e)
{
  struct Fn
  {
    Generator& g;
    llvm::Value* operator()(Literal_expr const* e) const { return g.gen(e); }
    llvm::Value* operator()(Id_expr const* e) const { return g.gen(e); }
    llvm::Value* operator()(Add_expr const* e) const { return g.gen(e); }
    llvm::Value* operator()(Sub_expr const* e) const { return g.gen(e); }
    llvm::Value* operator()(Mul_expr const* e) const { return g.gen(e); }
    llvm::Value* operator()(Div_expr const* e) const { return g.gen(e); }
    llvm::Value* operator()(Rem_expr const* e) const { return g.gen(e); }
    llvm::Value* operator()(Neg_expr const* e) const { return g.gen(e); }
    llvm::Value* operator()(Pos_expr const* e) const { return g.gen(e); }
    llvm::Value* operator()(Eq_expr const* e) const { return g.gen(e); }
    llvm::Value* operator()(Ne_expr const* e) const { return g.gen(e); }
    llvm::Value* operator()(Lt_expr const* e) const { return g.gen(e); }
    llvm::Value* operator()(Gt_expr const* e) const { return g.gen(e); }
    llvm::Value* operator()(Le_expr const* e) const { return g.gen(e); }
    llvm::Value* operator()(Ge_expr const* e) const { return g.gen(e); }
    llvm::Value* operator()(And_expr const* e) const { return g.gen(e); }
    llvm::Value* operator()(Or_expr const* e) const { return g.gen(e); }
    llvm::Value* operator()(Not_expr const* e) const { return g.gen(e); }
    llvm::Value* operator()(Call_expr const* e) const { return g.gen(e); }
    llvm::Value* operator()(Value_conv const* e) const { return g.gen(e); }
  };

  return apply(e, Fn{*this});
}


// Return the value corresponding to a literal expression.
llvm::Value* 
Generator::gen(Literal_expr const* e)
{
  // TODO: Write better type queries.
  //
  // TODO: Write a better interface for values.
  Value v = evaluate(e);
  if (e->type() == get_boolean_type())
    return build.getInt1(v.get_integer());
  if (e->type() == get_integer_type())
    return build.getInt32(v.get_integer());
  else
    throw std::runtime_error("cannot generate function literal");
}


// Returns the value associated with the declaration.
//
// TODO: Do we need to do anything different for function
// identifiers or not?
llvm::Value* 
Generator::gen(Id_expr const* e)
{
  return stack.lookup(e->declaration())->second;
}


llvm::Value* 
Generator::gen(Add_expr const* e)
{
  llvm::Value* l = gen(e->left());
  llvm::Value* r = gen(e->right());
  return build.CreateAdd(l, r);
}


llvm::Value* 
Generator::gen(Sub_expr const* e)
{
  llvm::Value* l = gen(e->left());
  llvm::Value* r = gen(e->right());
  return build.CreateSub(l, r);
}


llvm::Value* 
Generator::gen(Mul_expr const* e)
{
  llvm::Value* l = gen(e->left());
  llvm::Value* r = gen(e->right());
  return build.CreateMul(l, r);
}


llvm::Value* 
Generator::gen(Div_expr const* e)
{
  llvm::Value* l = gen(e->left());
  llvm::Value* r = gen(e->right());
  return build.CreateSDiv(l, r);
}


// FIXME: decide on unsigned or signed remainder
// based on types of expressions
llvm::Value* 
Generator::gen(Rem_expr const* e)
{
  llvm::Value* l = gen(e->left());
  llvm::Value* r = gen(e->right());
  return build.CreateURem(l, r);
}


llvm::Value* 
Generator::gen(Neg_expr const* e)
{
  llvm::Value* zero = build.getInt32(0);
  llvm::Value* val = gen(e->operand());
  return build.CreateSub(zero, val);
}


llvm::Value* 
Generator::gen(Pos_expr const* e)
{
  return gen(e->operand());
}


llvm::Value* 
Generator::gen(Eq_expr const* e)
{
  llvm::Value* l = gen(e->left());
  llvm::Value* r = gen(e->right());
  return build.CreateICmpEQ(l, r);
}


llvm::Value* 
Generator::gen(Ne_expr const* e)
{
  llvm::Value* l = gen(e->left());
  llvm::Value* r = gen(e->right());
  return build.CreateICmpNE(l, r);
}


llvm::Value* 
Generator::gen(Lt_expr const* e)
{
  llvm::Value* l = gen(e->left());
  llvm::Value* r = gen(e->right());
  return build.CreateICmpSLT(l, r);
}


llvm::Value* 
Generator::gen(Gt_expr const* e)
{
  llvm::Value* l = gen(e->left());
  llvm::Value* r = gen(e->right());
  return build.CreateICmpSGT(l, r);
}


llvm::Value* 
Generator::gen(Le_expr const* e)
{
  llvm::Value* l = gen(e->left());
  llvm::Value* r = gen(e->right());
  return build.CreateICmpSLE(l, r);
}


llvm::Value* 
Generator::gen(Ge_expr const* e)
{
  llvm::Value* l = gen(e->left());
  llvm::Value* r = gen(e->right());
  return build.CreateICmpSGE(l, r);
}


llvm::Value* 
Generator::gen(And_expr const* e)
{
  llvm::Value* l = gen(e->left());
  llvm::Value* r = gen(e->right());
  return build.CreateAnd(l, r);
}


llvm::Value* 
Generator::gen(Or_expr const* e)
{
  llvm::Value* l = gen(e->left());
  llvm::Value* r = gen(e->right());
  return build.CreateOr(l, r);
}


// Logical not is a simple XOR with the value true
// 1 xor 1 = 0
// 0 xor 1 = 1
llvm::Value* 
Generator::gen(Not_expr const* e)
{
  llvm::Value* one = build.getTrue();
  llvm::Value* operand = gen(e->operand());
  return build.CreateXor(one, operand);
}


llvm::Value* 
Generator::gen(Call_expr const* e)
{
  if (Id_expr const* id = as<Id_expr>(e->target())) {
    llvm::Value* fn = stack.lookup(id->declaration())->second;

    std::vector<llvm::Value*> argsV;
    for (auto arg : e->arguments()) {
      llvm::Value* argi = gen(arg);
      argsV.push_back(argi);
    }

    return build.CreateCall(fn, argsV, "calltmp");
  } 
  
  throw std::runtime_error("unqualified id in call expr");
}


llvm::Value*
Generator::gen(Value_conv const* e)
{
  llvm::Value* v = gen(e->source());
  return build.CreateLoad(v);
}


// -------------------------------------------------------------------------- //
// Code generation for statements
//
// The statement generator is responsible for
// the generation of statements at block scope.

void
Generator::gen(Stmt const* s)
{
  struct Fn
  {
    Generator& g;
    void operator()(Empty_stmt const* s) { g.gen(s); }
    void operator()(Block_stmt const* s) { g.gen(s); }
    void operator()(Assign_stmt const* s) { g.gen(s); }
    void operator()(Return_stmt const* s) { g.gen(s); }
    void operator()(If_then_stmt const* s) { g.gen(s); }
    void operator()(If_else_stmt const* s) { g.gen(s); }
    void operator()(While_stmt const* s) { g.gen(s); }
    void operator()(Break_stmt const* s) { g.gen(s); }
    void operator()(Continue_stmt const* s) { g.gen(s); }
    void operator()(Expression_stmt const* s) { g.gen(s); }
    void operator()(Declaration_stmt const* s) { g.gen(s); }
  };
  apply(s, Fn{*this});
}


void
Generator::gen(Empty_stmt const* s)
{
  // do nothing
}


// Generate code for a sequence of statements.
// Note that this does not correspond to a basic
// block since we don't need any terminators
// in the following program.
//
//    {
//      { ; }
//    }
//
// We only need new blocks for specific control
// flow concepts.
void
Generator::gen(Block_stmt const* s)
{
  for (Stmt const* s1 : s->statements())
    gen(s1);
}


void
Generator::gen(Assign_stmt const* s)
{
  llvm::Value* lhs = gen(s->object());
  llvm::Value* rhs = gen(s->value());
  build.CreateStore(rhs, lhs);
}


// When we hit a return stmt we
// 1. generate the code for the return value
// 2. store the temp value into the return variable
// 3. branch to the return block
void
Generator::gen(Return_stmt const* s)
{
  // get the current block
  auto curr_block = build.GetInsertBlock();

  // check that this will be the only branch
  // in the block

  llvm::Value* v = gen(s->value());
  build.CreateStore(v, ret_var);
  make_branch(curr_block, ret_block);
}


// To generate the if-then
// You need to introduce the 
void
Generator::gen(If_then_stmt const* s)
{
  llvm::Value* cond = gen(s->condition());

  cond = build.CreateICmpEQ(cond, build.getTrue(), "ifcond");

  llvm::Function* fn = build.GetInsertBlock()->getParent();

  // create then block
  llvm::BasicBlock* then = llvm::BasicBlock::Create(cxt, "then", fn);
  // create an empty else block
  llvm::BasicBlock* merge = llvm::BasicBlock::Create(cxt, "cont", fn);
  // create the branch
  build.CreateCondBr(cond, then, merge);

  // emit the 'then' block
  build.SetInsertPoint(then);
  gen(s->body());
  make_branch(build.GetInsertBlock(), merge);
  // reset the block back to where it should be
  then = build.GetInsertBlock();

  // fn->getBasicBlockList().push_back(merge);

  // set the insertion point to the merge block
  build.SetInsertPoint(merge);
  merge = build.GetInsertBlock();
}


void
Generator::gen(If_else_stmt const* s)
{
  llvm::Value* cond = gen(s->condition());

  cond = build.CreateICmpEQ(cond, build.getTrue(), "ifcond");

  llvm::Function* fn = build.GetInsertBlock()->getParent();

  // create then block
  llvm::BasicBlock* then = llvm::BasicBlock::Create(cxt, "then", fn);
  // create an empty else block
  llvm::BasicBlock* el = llvm::BasicBlock::Create(cxt, "else", fn);
  // create a merge block
  llvm::BasicBlock* merge = llvm::BasicBlock::Create(cxt, "ifcont", fn);
  // create the branch
  build.CreateCondBr(cond, then, el);

  // emit the 'then' block
  build.SetInsertPoint(then);
  gen(s->true_branch());
  make_branch(build.GetInsertBlock(), merge);
  // apparently codegen of 'then' can change the current block, update then for the PHI
  then = build.GetInsertBlock();

  // emit the else block
  // FIXME: apparently theese push back lines (even though they are in the tutorial)
  // cause double frees with the environment. Not sure why but removing these solves the problem.
  // fn->getBasicBlockList().push_back(el);

  build.SetInsertPoint(el);
  gen(s->false_branch());
  make_branch(build.GetInsertBlock(), merge);
  // branch back to merge
  el = build.GetInsertBlock();

  // emit the merge block
  // FIXME: apparently theese push back lines (even though they are in the tutorial)
  // cause double frees with the environment. Not sure why but removing these solves the problem.
  // fn->getBasicBlockList().push_back(merge);
  build.SetInsertPoint(merge);
  merge = build.GetInsertBlock();
}


void
Generator::gen(While_stmt const* s)
{
  llvm::Function* fn = build.GetInsertBlock()->getParent();

  // create while block
  llvm::BasicBlock* before_while = llvm::BasicBlock::Create(cxt, "before_while", fn);
  llvm::BasicBlock* while_ = llvm::BasicBlock::Create(cxt, "while", fn);
  llvm::BasicBlock* after_while = llvm::BasicBlock::Create(cxt, "after_while", fn);

  // push the entry and exit
  loop_entry_stack.push(before_while);
  loop_exit_stack.push(after_while);

  // emit a branch to the loop entry
  make_branch(build.GetInsertBlock(), before_while);

  // emit the block which evaluates the condition
  build.SetInsertPoint(before_while);
  // generate the value for the cond
  llvm::Value* cond = gen(s->condition());
  // convert cond to boolean i1
  cond = build.CreateICmpEQ(cond, build.getTrue(), "whilecond");
  build.CreateCondBr(cond, while_, after_while);

  // emit the 'while' block
  build.SetInsertPoint(while_);
  gen(s->body());
  // generate branch back to condition testing block
  make_branch(build.GetInsertBlock(), before_while);
  // apparently codegen of 'while' can change the current block, update then for the PHI
  while_ = build.GetInsertBlock();

  // emit the rest of the code in after_while
  build.SetInsertPoint(after_while);
  after_while = build.GetInsertBlock();

  // pop the entry and exit
  loop_entry_stack.pop();
  loop_exit_stack.pop();
}


void
Generator::gen(Break_stmt const* s)
{
  if (!loop_entry_stack.empty()) {
    llvm::BasicBlock* exit_ = loop_exit_stack.top();
    make_branch(build.GetInsertBlock(), exit_);
  }
}


void
Generator::gen(Continue_stmt const* s)
{
  if (!loop_entry_stack.empty()) {
    llvm::BasicBlock* reentry = loop_entry_stack.top();
    make_branch(build.GetInsertBlock(), reentry);
  }
}


void
Generator::gen(Expression_stmt const* s)
{
  gen(s->expression());
}


void
Generator::gen(Declaration_stmt const* s)
{
  gen(s->declaration());
}


// -------------------------------------------------------------------------- //
// Code generation for declarations
//
// TODO: We can't generate all of the code for a module
// in a single pass. We probably need to break this up
// into a number of smaller declaration generators. For
// example, generators that:
//
//    - produce declarations
//    - produce global initializers
//    - produce global destructors
//    - other stuff
//
// In, it might not be worthwhile to have a number
// of sub-generators that refer to the top-level
// generator.

void
Generator::gen(Decl const* d)
{
  struct Fn
  {
    Generator& g;
    void operator()(Variable_decl const* d) { return g.gen(d); }
    void operator()(Function_decl const* d) { return g.gen(d); }
    void operator()(Parameter_decl const* d) { return g.gen(d); }
    void operator()(Module_decl const* d) { return g.gen(d); }
  };
  return apply(d, Fn{*this});
}


// We actually cannot generate local variables
// by injecting them at the beginning block
// because assignment to memory from prior memory would produce 
// illformed code
void
Generator::gen_local(Variable_decl const* d)
{
  // get the containing function
  llvm::Function* fn = build.GetInsertBlock()->getParent();

  // get the entry block
  llvm::BasicBlock& entry_block = fn->getEntryBlock();

  // store the old insert point
  auto prev = build.GetInsertBlock();

  // if the entry block is empty
  if (entry_block.empty())
    build.SetInsertPoint(&entry_block);
  else
    build.SetInsertPoint(locals_insert_pt);

  // generate the alloca
  llvm::Type* t = get_type(d->type());
  String const& name = d->name()->spelling();
  llvm::Value* local = build.CreateAlloca(t, nullptr, name);
  // generate the initializer first
  llvm::Value* init = gen(d->init());
  // generate the store
  build.CreateStore(init, local);

  stack.top().bind(d, local);

  // reset the insert point
  build.SetInsertPoint(prev);
}


void
Generator::gen_global(Variable_decl const* d)
{
  String const&   name = d->name()->spelling();
  llvm::Type*     type = build.getInt32Ty();

  // FIXME: Handle initialization correctly.
  llvm::Constant* init = llvm::ConstantAggregateZero::get(type);

  // Build the global variable, automatically adding
  // it to the module.
  llvm::GlobalVariable* var = new llvm::GlobalVariable(
    *mod,                                  // owning module
    type,                                  // type
    false,                                 // is constant
    llvm::GlobalVariable::ExternalLinkage, // linkage,
    init,                                  // initializer
    name                                   // name
  );

  // Create a binding for the new variable.
  stack.top().bind(d, var);
}


// Generate code for a variable declaration. Note that
// code generation depends heavily on context. Globals
// and locals are very different.
//
// TODO: If we add class/record types, then we also
// need to handle member variables as well. Maybe.
void
Generator::gen(Variable_decl const* d)
{
  if (is_global_variable(d))
    return gen_global(d);
  else
    return gen_local(d);
}


void
Generator::gen(Function_decl const* d)
{
  String const& name = d->name()->spelling();
  llvm::Type*   type = get_type(d->type());

  // Build the function.
  llvm::FunctionType* ftype = llvm::cast<llvm::FunctionType>(type);
  llvm::Function* fn = llvm::Function::Create(
    ftype,                           // function type
    llvm::Function::ExternalLinkage, // linkage
    name,                            // name
    mod);                            // owning module

  // Create a new binding for the variable.
  stack.top().bind(d, fn);

  // Establish a new binding environment for declarations
  // related to this function.
  Symbol_sentinel scope(*this);
  
  // Build the argument list. Note that 
  {
    auto ai = fn->arg_begin();
    auto pi = d->parameters().begin();
    while (ai != fn->arg_end()) {
      Decl const* p = *pi;
      llvm::Argument* a = &*ai;
      a->setName(p->name()->spelling());

      // Create an initial name binding for the
      // function parameter. Note that we're
      // going to overwrite this when we create
      // locals for each parameter.
      stack.top().bind(p, a);

      ++ai;
      ++pi;
    }
  }

  // Build the entry point for the function
  // and make that the insertion point.
  //
  // TODO: We probably need a stack of blocks
  // so that we know where we are.
  llvm::BasicBlock* b = llvm::BasicBlock::Create(cxt, "b", fn);
  build.SetInsertPoint(b);

  // build the return block for the function
  // it doesnt matter where the block appears
  // as long as it is the last thing called
  // llvm::BasicBlock* ret = llvm::BasicBlock::Create(cxt, "return", fn);

  // Generate a local variable for each of the variables.
  for (Decl const* p : d->parameters())
    gen(p);

  // create a variable to store the return
  llvm::Type* t = get_type(d->return_type());
  ret_var = build.CreateAlloca(t);
  // create the return block
  ret_block = llvm::BasicBlock::Create(cxt, "return");

  // generate an insertion point for all other local variables
  // this will get deleted later
  locals_insert_pt = ret_var;

  // Generate the body of the function.
  gen(d->body());

  // generate the final return
  fn->getBasicBlockList().push_back(ret_block);
  build.SetInsertPoint(ret_block);
  llvm::Value* ret_val = build.CreateLoad(ret_var);
  build.CreateRet(ret_val);

  // handle illformed blocks
  resolve_illformed_blocks(fn);

  // delete the local variable insertion point
  locals_insert_pt = nullptr;
  // erase the pointer to the return value
  ret_var = nullptr;
  ret_block = nullptr;
}


void
Generator::gen(Parameter_decl const* d)
{
  llvm::Type* t = get_type(d->type());
  llvm::Value* a = stack.top().get(d).second;
  llvm::Value* v = build.CreateAlloca(t);
  stack.top().rebind(d, v);
  build.CreateStore(a, v);
}


void
Generator::gen(Module_decl const* d)
{
  // Establish the global binding environment.
  Symbol_sentinel scope(*this);

  // Initialize the module.
  //
  // TODO: Make the output name the ".ll" version of the
  // the input name. Although this might also depend on
  // whether we're generating IR or object code?
  assert(!mod);
  mod = new llvm::Module("a.ll", cxt);

  // Generate all top-level declarations.
  for (Decl const* d1 : d->declarations())
    gen(d1);

  // TODO: Make a second pass to generate global
  // constructors for initializers.
}


llvm::Module* 
Generator::operator()(Decl const* d)
{
  assert(is<Module_decl>(d));
  gen(d);
  return mod;
}

