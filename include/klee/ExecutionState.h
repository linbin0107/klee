//===-- ExecutionState.h ----------------------------------------*- C++ -*-===//
//
//                     The KLEE Symbolic Virtual Machine
//
// This file is distributed under the University of Illinois Open Source
// License. See LICENSE.TXT for details.
//
//===----------------------------------------------------------------------===//

#ifndef KLEE_EXECUTIONSTATE_H
#define KLEE_EXECUTIONSTATE_H

#include "klee/Constraints.h"
#include "klee/Expr.h"

#include "klee/AddressSpace.h"
#include "klee/Internal/Module/KInstIterator.h"

#include "klee/BitfieldSimplifier.h"
#include "klee/Solver.h"
#include "klee/SolverManager.h"
#include "klee/util/Assignment.h"
#include "IAddressSpaceNotification.h"

#include <map>
#include <set>
#include <vector>

namespace klee {
class Array;
class CallPathNode;
struct Cell;
struct KFunction;
struct KInstruction;
class MemoryObject;
struct InstructionInfo;

llvm::raw_ostream &operator<<(llvm::raw_ostream &os, const MemoryMap &mm);

struct StackFrame {
    KInstIterator caller;
    KFunction *kf;
    CallPathNode *callPathNode;

    std::vector<const MemoryObject *> allocas;
    Cell *locals;

    // For vararg functions: arguments not passed via parameter are
    // stored (packed tightly) in a local (alloca) memory object. This
    // is setup to match the way the front-end generates vaarg code (it
    // does not pass vaarg through as expected). VACopy is lowered inside
    // of intrinsic lowering.
    MemoryObject *varargs;

    StackFrame(KInstIterator caller, KFunction *kf);
    StackFrame(const StackFrame &s);
    ~StackFrame();
};

class ExecutionState : public IAddressSpaceNotification {
    friend class AddressSpace;

public:
    typedef std::vector<StackFrame> stack_ty;

private:
    // unsupported, use copy constructor
    ExecutionState &operator=(const ExecutionState &);
    std::map<std::string, std::string> fnAliases;

public:
    bool fakeState;

    // pc - pointer to current instruction stream
    KInstIterator pc, prevPC;
    stack_ty stack;
    ConstraintManager constraints;
    AddressSpace addressSpace;

    // XXX: get this out of here
    mutable double queryCost;

    /// Disables forking, set by user code.
    bool forkDisabled;

    /// ordered list of symbolics: used to generate test cases.
    //
    // FIXME: Move to a shared list structure (not critical).
    std::vector<std::pair<const MemoryObject *, const Array *>> symbolics;

    // Maps a KLEE variable name to the real variable name.
    // The KLEE name is stripped from any special characters to make
    // it suitable to send to the constraint solver.
    klee::ImmutableMap<std::string, std::string> variableNameMapping;

    Assignment *concolics;

    unsigned incomingBBIndex;

    std::string getFnAlias(std::string fn);
    void addFnAlias(std::string old_fn, std::string new_fn);
    void removeFnAlias(std::string fn);

private:
    /// Simplifier user to simplify expressions when adding them
    static BitfieldSimplifier s_simplifier;

    ExecutionState() : fakeState(false), addressSpace(this) {
    }

protected:
    virtual ExecutionState *clone();
    virtual void addressSpaceChange(const MemoryObject *mo, const ObjectState *oldState, ObjectState *newState);

    virtual void addressSpaceObjectSplit(const ObjectState *oldObject, const std::vector<ObjectState *> &newObjects);

public:
    // Fired whenever an object becomes all concrete or gets at least one symbolic byte.
    // Only fired in the context of a memory operation (load/store)
    virtual void addressSpaceSymbolicStatusChange(ObjectState *object, bool becameConcrete);

public:
    ExecutionState(KFunction *kf);

    // XXX total hack, just used to make a state so solver can
    // use on structure
    ExecutionState(const std::vector<ref<Expr>> &assumptions);

    virtual ~ExecutionState();

    ExecutionState *branch();

    void pushFrame(KInstIterator caller, KFunction *kf);
    void popFrame();

    void addSymbolic(const MemoryObject *mo, const Array *array) {
        symbolics.push_back(std::make_pair(mo, array));
    }

    ///
    /// \brief Add a constraints to the state
    ///
    /// Note: it is very important for the caller to check the return
    /// value of this function. An error while adding a constraint
    /// can lead to incorrect execution.
    ///
    /// \param e the constraint to add
    /// \param recomputeConcolics whether to compute a new set of input values if the new
    /// constraint is valid but does not evaluate to true with the current set of concolic values
    /// \return true if the constraint was successfully added, false otherwise
    ///
    virtual bool addConstraint(const ref<Expr> &e, bool recomputeConcolics = false) __attribute__((warn_unused_result));

    ///
    /// \brief Compute a set of concrete inputs for the given constraints
    /// \param mgr the constraints
    /// \param assignment the concrete inputs
    /// \return true if computation was successful, false if there is no solution
    /// or some other error occured.
    ///
    bool solve(const ConstraintManager &mgr, Assignment &assignment);

    virtual bool merge(const ExecutionState &b);

    void printStack(KInstruction *target, std::stringstream &msg) const;

    bool getSymbolicSolution(std::vector<std::pair<std::string, std::vector<unsigned char>>> &res);

    ref<Expr> simplifyExpr(const ref<Expr> &e) const;

    static BitfieldSimplifier &getSimplifier() {
        return s_simplifier;
    }

    ref<ConstantExpr> toConstant(ref<Expr> e, const std::string &reason);
    ref<ConstantExpr> toConstantSilent(ref<Expr> e);

    /// Return a unique constant value for the given expression in the
    /// given state, if it has one (i.e. it provably only has a single
    /// value). Otherwise return the original expression.
    ref<Expr> toUnique(ref<Expr> &e);

    void dumpQuery(llvm::raw_ostream &os) const;

    std::shared_ptr<TimingSolver> solver() const;

    Cell &getArgumentCell(KFunction *kf, unsigned index);
    Cell &getDestCell(KInstruction *target);

    void bindLocal(KInstruction *target, ref<Expr> value);
    void bindArgument(KFunction *kf, unsigned index, ref<Expr> value);
    void stepInstruction();

    ObjectState *bindObject(const MemoryObject *mo, bool isLocal, const Array *array = nullptr);
};
}

#endif
