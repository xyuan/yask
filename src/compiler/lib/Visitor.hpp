/*****************************************************************************

YASK: Yet Another Stencil Kernel
Copyright (c) 2014-2018, Intel Corporation

Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the "Software"), to
deal in the Software without restriction, including without limitation the
rights to use, copy, modify, merge, publish, distribute, sublicense, and/or
sell copies of the Software, and to permit persons to whom the Software is
furnished to do so, subject to the following conditions:

* The above copyright notice and this permission notice shall be included in
  all copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS
IN THE SOFTWARE.

*****************************************************************************/

//////////// Generic expression-visitor class /////////////

#ifndef VISITOR_HPP
#define VISITOR_HPP

#include "Expr.hpp"

namespace yask {

    // Base class for an Expr-tree visitor.
    // Visitation returns a string.
    // By default, visitors return empty strings or the last accepted result.
    class ExprVisitor {
    protected:
        bool _visitEqualsLhs = false; // whether to visit LHS of EQUALS.

    public:
        virtual ~ExprVisitor() { }

        // By default, leaf-node visitors do nothing.
        virtual string visit(ConstExpr* ce) { return ""; }
        virtual string visit(CodeExpr* ce) { return ""; }
        virtual string visit(IndexExpr* ie) { return ""; }
        virtual string visit(GridPoint* gp) { return ""; } // does NOT visit arg exprs by default.

        // By default, a unary visitor visits its operand.
        virtual string visit(UnaryNumExpr* ue) {
            return ue->getRhs()->accept(this);
        }
        virtual string visit(UnaryNum2BoolExpr* ue) {
            return ue->getRhs()->accept(this);
        }
        virtual string visit(UnaryBoolExpr* ue) {
            return ue->getRhs()->accept(this);
        }

        // By default, a binary visitor visits its operands.
        virtual string visit(BinaryNumExpr* be) {
            be->getLhs()->accept(this);
            return be->getRhs()->accept(this);
        }
        virtual string visit(BinaryNum2BoolExpr* be) {
            be->getLhs()->accept(this);
            return be->getRhs()->accept(this);
        }
        virtual string visit(BinaryBoolExpr* be) {
            be->getLhs()->accept(this);
            return be->getRhs()->accept(this);
        }

        // By default, commutative and function visitors visit their operands.
        virtual string visit(CommutativeExpr* ce) {
            auto& ops = ce->getOps();
            string res;
            for (auto ep : ops)
                res = ep->accept(this);
            return res;
        }
        virtual string visit(FuncExpr* ce) {
            auto& ops = ce->getOps();
            string res;
            for (auto ep : ops)
                res = ep->accept(this);
            return res;
        }
        virtual string visit(EqualsExpr* ee) {

            // Visit LHS if flag is set.
            if (_visitEqualsLhs)
                ee->getLhs()->accept(this);
            return ee->getRhs()->accept(this);

            // Conditions are not visited.
        }
    };

} // namespace yask.

#endif
