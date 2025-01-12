#include <llvm/IR/Value.h>
#include <llvm/IR/LLVMContext.h>
#include <llvm/IR/Function.h>
#include <llvm/IR/Type.h>
#include <llvm/IR/Verifier.h>
#include "expr.hpp"
#include "misc.hpp"
#include "base.hpp"

#ifndef _CREOL_FUNCTION
#define _CREOL_FUNCTION

namespace creol {
    /// CallExprAST - represents the function call
    class CallExprAST : public ExprAST {
        std::vector<std::unique_ptr<ExprAST>> Args;
        std::string Callee;
    
    public:
        CallExprAST(const std::string& Callee, std::vector<std::unique_ptr<ExprAST>> Args)
        : Callee(Callee), Args(std::move(Args)) {}

        llvm::Value* codegen() override;
    };
    
    /// PrototypeAST - This class represents the "prototype" for a function,
    /// which captures its name, and its arguments names.
    class PrototypeAST : public ExprAST {
        std::string Name;
        std::string TypeName;
        std::vector<std::string> Args;
    
    public:
        PrototypeAST(const std::string& TypeName, const std::string& Name, std::vector<std::string> Args)
        : TypeName(TypeName), Name(Name), Args(std::move(Args)) {}

        llvm::Function* codegen() override;

        const std::string& getName() const;
    };

    /// FunctionAST - This class represents a function definition itself.
    class FunctionAST : public ExprAST {
        std::unique_ptr<PrototypeAST> Proto;
        std::unique_ptr<ExprAST> Body;
    
    public:
        FunctionAST(std::unique_ptr<PrototypeAST> Proto, std::unique_ptr<ExprAST> Body)
        : Proto(std::move(Proto)), Body(std::move(Body)) {}

        llvm::Function* codegen();
    };
};

llvm::Value* creol::CallExprAST::codegen() {
    llvm::Function* CalleeFun = creol::TheModule->getFunction(Callee);

    if (!CalleeFun) {
        return creol::LogErrorV("Unknown function referenced!");
    } else if (CalleeFun->arg_size() != Args.size()) {
        return creol::LogErrorV("Incorrent number of arguments passed!");
    } else {
        std::vector<llvm::Value*> ArgsValues;

        for (unsigned i = 0 ; i < Args.size() ; ++i) {
            ArgsValues.push_back(Args[i]->codegen());

            if (!ArgsValues.back()) {
                return nullptr;
            }
        }

        return creol::TheBuilder->CreateCall(CalleeFun, ArgsValues, "calltmp");
    }
}

llvm::Function* creol::PrototypeAST::codegen() {
    // info: Currently only integer types arguments are received by the funtions
    // TODO: divesify arguments
    std::vector<llvm::Type*> Arguments(
        Args.size(), llvm::Type::getInt32Ty(*creol::TheContext)
    );

    llvm::FunctionType* FunType = nullptr;

    if (TypeName == "int") {
        FunType = llvm::FunctionType::get(
            llvm::Type::getInt32Ty(*creol::TheContext), Arguments, false
        );
    } else if (TypeName == "float") {
        FunType = llvm::FunctionType::get(
            llvm::Type::getFloatTy(*creol::TheContext), Arguments, false
        );
    } else if (TypeName == "bool") {
        FunType = llvm::FunctionType::get(
            llvm::Type::getInt1Ty(*creol::TheContext), Arguments, false
        );
    } else if (TypeName == "void") {
        FunType = llvm::FunctionType::get(
            llvm::Type::getVoidTy(*creol::TheContext), Arguments, false
        );
    } else {
        LogErrorV("Unknown function type!");
        return nullptr;
    }

    llvm::Function* Fun = llvm::Function::Create(
        FunType, llvm::Function::ExternalLinkage, Name, creol::TheModule.get()
    );

    unsigned idx = 0;

    for (auto& arg : Fun->args()) {
        arg.setName(Args[idx++]);
    }

    return Fun;
}

const std::string& creol::PrototypeAST::getName() const {
    return Name;
}

llvm::Function* creol::FunctionAST::codegen() {
    llvm::Function* TheFunction = creol::TheModule->getFunction(Proto->getName());
    TheFunction = TheFunction ? TheFunction : Proto->codegen();

    if (!TheFunction) {
        creol::LogErrorV("Function could not be defined");
        return nullptr;
    } else if (!TheFunction->empty()) {
        creol::LogErrorV("Function can not be redefined");
        return nullptr;
    }

    llvm::BasicBlock* BasicBlock = llvm::BasicBlock::Create(
        *creol::TheContext, "entry", TheFunction
    );

    creol::TheBuilder->SetInsertPoint(BasicBlock);

    creol::NamedValues.clear();

    for (auto& arg : TheFunction->args()) {
        creol::NamedValues[arg.getName()] = &arg;
    }

    if (llvm::Value* RetVal = Body->codegen()) {
        // Add a return value to the function
        creol::TheBuilder->CreateRet(RetVal);
        llvm::verifyFunction(*TheFunction);
        return TheFunction;
    } else {
        TheFunction->eraseFromParent();
        creol::LogErrorV("Error reading the body of the funtion!");
        return nullptr;
    }
}

#endif