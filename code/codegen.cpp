#include "codegen.hpp"

namespace Code {

void Codegen_v::visit(AST::Expr*) { std::exit(0); }

void Codegen_v::visitNumber(AST::NumberExpr* e) {
  // numeric constants represented with ConstantFP,
  // APFloat ~ "Arbitrary Precision floating point constants"
  cur_val = llvm::ConstantFP::get(ctx, llvm::APFloat(e->val()));
}

void Codegen_v::visitVariable(AST::VariableExpr* e) {
  llvm::Value* v = named_vs[e->name()];
  if (!v)
    err::log_error("unknown variable name");
  cur_val = v;
}

void Codegen_v::visitBinary(AST::BinaryExpr* e) {
  e->lhs()->accept(*this);
  llvm::Value* l = cur_val;
  e->lhs()->accept(*this);
  llvm::Value* r = cur_val;

  if (!l || !r) {
    cur_val = nullptr;
    return;
  }

  switch (e->op()) {
  case '+':
    cur_val = builder.CreateFAdd(l, r, "addtmp");
    return;
  case '-':
    cur_val = builder.CreateFAdd(l, r, "subtmp");
    return;
  case '*':
    cur_val = builder.CreateFAdd(l, r, "multmp");
    return;
  case '<':
    // fcmp returns an `i1` => conversion needed to 0.0 or 1.0
    // uitofp takes a value and a fp type to cast to
    l       = builder.CreateFCmpULT(l, r, "cmptmp");
    cur_val = builder.CreateUIToFP(l, llvm::Type::getDoubleTy(ctx), "booltmp");
    return;
  default:
    err::log_error("invalid binary operator");
    cur_val = nullptr;
    return;
  }
}

void Codegen_v::visitCall(AST::CallExpr* e) {
  // lookup the name in global module table
  llvm::Function* cf = module->getFunction(e->callee());
  if (!cf) {
    err::log_error("unknown function referenced");
    cur_val = nullptr;
    return;
  }

  auto args = e->args();
  // argument mismatch error
  if (cf->arg_size() != args.size()) {
    err::log_error("incorrect # arguments passed");
    cur_val = nullptr;
    return;
  }

  std::vector<llvm::Value*> vargs;
  for (size_t i = 0; i != args.size(); i++) {
    args[i]->accept(*this);
    if (!cur_val) {
      cur_val = nullptr;
      return;
    }
    vargs.push_back(cur_val);
  }
  cur_val = builder.CreateCall(cf, vargs, "calltmp");
}

void Codegen_v::visitPrototype(AST::Prototype* p) {
  auto args = p->args();
  // make the function type
  std::vector<llvm::Type*> doubles(args.size(), llvm::Type::getDoubleTy(ctx));

  llvm::FunctionType* fty =
      llvm::FunctionType::get(llvm::Type::getDoubleTy(ctx), doubles, false);

  llvm::Function* f = llvm::Function::Create(
      fty, llvm::Function::ExternalLinkage, p->name(), module.get());

  // set name of funcs argument according to names given in proto
  size_t idx = 0;
  for (auto& a : f->args())
    a.setName(args[idx++]);

  cur_val = f;
}

void Codegen_v::visitFunction(AST::Function* fun) {
  // check for existing function from previous 'extern' declarations
  auto proto        = fun->proto();
  llvm::Function* f = module->getFunction(proto->name());

  if (f) {
    if (f->arg_size() != proto->args().size()) {
      err::log_error("conflicting function declarations.");
      return;
    }
    // if prior parse proto has differnt arg names we need to rename them
    auto it = proto->args().begin();
    for (auto& arg : f->args())
      arg.setName(*it++);
  } else {
    proto->accept(*this);
    f = static_cast<llvm::Function*>(cur_val);
  }
  if (!f)
    return;

  if (!f->empty()) { // assert function has no body before we start
    err::log_error("function cannot be defined");
    return;
  }

  // create new basic bloc for insertion
  llvm::BasicBlock* bb = llvm::BasicBlock::Create(ctx, "entry", f);
  builder.SetInsertPoint(bb);

  // record the function arguments in the named_vs
  named_vs.clear(); // QUESTION: why clear() tho?
  for (auto& a : f->args())
    named_vs[a.getName()] = &a;

  fun->body()->accept(*this);
  if (llvm::Value* retv = cur_val) {
    builder.CreateRet(retv);
    // validate generate code;
    llvm::verifyFunction(*f);
    cur_val = f;
    return;
  }
  f->eraseFromParent();
  cur_val = nullptr;
  return;
}
} // namespace Code