/*****************************************************************************

YASK: Yet Another Stencil Kernel
Copyright (c) 2014-2016, Intel Corporation

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

//////////// Generic expression-printer classes /////////////

#ifndef PRINT_HPP
#define PRINT_HPP

#include "ExprUtils.hpp"

using namespace std;

// Base class to define methods for printing.
class PrintHelper {
    int _varNum;                // current var number.

protected:
    const CounterVisitor* _cv;  // counter info.
    string _varPrefix;          // first part of var name.
    string _varType;            // type, if any, of var.
    string _linePrefix;         // prefix for each line.
    string _lineSuffix;         // suffix for each line.

public:
    PrintHelper(const CounterVisitor* cv,
                const string& varPrefix,
                const string& varType,
                const string& linePrefix,
                const string& lineSuffix) :
        _varNum(1), _cv(cv),
        _varPrefix(varPrefix), _varType(varType),
        _linePrefix(linePrefix), _lineSuffix(lineSuffix) { }

    virtual ~PrintHelper() { }

    virtual const string& getVarType() const { return _varType; }
    virtual void setVarType(const string& varType) { _varType = varType; }
    virtual const string& getLinePrefix() const { return _linePrefix; }
    virtual const string& getLineSuffix() const { return _lineSuffix; }
    const CounterVisitor* getCounters() const { return _cv; }

    // Return count from counter visitor.
    int getCount(Expr* ep) {
        if (!_cv)
            return 0;
        return _cv->getCount(ep);
    }

    // Return number of times this node is shared.
    int getNumCommon(Expr* ep) {
        if (!_cv)
            return 0;
        int c = _cv->getCount(ep);
        return (c <= 1) ? 0 : c-1;
    }
    
    // Make and return next var name.
    virtual string makeVarName() {
        ostringstream oss;
        oss << _varPrefix << _varNum++;
        return oss.str();
    }

    // Return any code expression.
    // The 'os' parameter is provided for derived types that
    // need to write intermediate code to a stream.
    virtual string addCodeExpr(ostream& os, const string& code) {
        return code;
    }

    // Return a constant expression.
    // The 'os' parameter is provided for derived types that
    // need to write intermediate code to a stream.
    virtual string addConstExpr(ostream& os, double v) {
        ostringstream oss;
        oss << v;
        return oss.str();
    }

    // Return a parameter reference.
    virtual string readFromParam(ostream& os, const GridPoint& pp) {
        string str = pp.getName();
        if (pp.size())
            str += "(" + pp.makeValStr() + ")";
        return str;
    }
    
    // Return a grid reference.
    // The 'os' parameter is provided for derived types that
    // need to write intermediate code to a stream.
    virtual string readFromPoint(ostream& os, const GridPoint& gp) {
        return gp.makeStr();
    }

    // Update a grid point.
    // The 'os' parameter is provided for derived types that
    // need to write intermediate code to a stream.
    virtual string writeToPoint(ostream& os, const GridPoint& gp, const string& val) {
        return gp.makeStr() + " = " + val;
    }
};

// Base class for a print visitor.
class PrintVisitorBase : public ExprVisitor {

protected:
    ostream& _os;               // used for printing intermediate results as needed.
    PrintHelper& _ph;           // used to format items for printing.

    // After visiting an expression, the part of the result not written to _os
    // is stored in _exprStr.
    string _exprStr;

public:
    // os is used for printing intermediate results as needed.
    PrintVisitorBase(ostream& os, PrintHelper& ph) :
        _os(os), _ph(ph) { }

    virtual ~PrintVisitorBase() { }

    // Get unwritten result expression, if any.
    virtual string getExprStr() const {
        return _exprStr;
    }

    // Get unwritten result expression, if any.
    // Clear result.
    virtual string getExprStrAndClear() {
        string v = _exprStr;
        _exprStr = "";
        return v;
    }
};

// Outputs a simple, human-readable version of the AST
// in a top-down fashion. Expressions will be written to 'os',
// and anything 'left over' will be left in '_exprStr'.
class PrintVisitorTopDown : public PrintVisitorBase {
    int _numCommon;
    
public:
    PrintVisitorTopDown(ostream& os, PrintHelper& ph) :
        PrintVisitorBase(os, ph), _numCommon(0) { }

    // Get the number of shared nodes found after this visitor
    // has been accepted.
    int getNumCommon() const { return _numCommon; }
    
    // A grid or parameter read.
    virtual void visit(GridPoint* gp) {
        if (gp->isParam())
            _exprStr += _ph.readFromParam(_os, *gp);
        else
            _exprStr += _ph.readFromPoint(_os, *gp);
        _numCommon += _ph.getNumCommon(gp);
    }

    // A constant.
    virtual void visit(ConstExpr* ce) {
        _exprStr += _ph.addConstExpr(_os, ce->getVal());
        _numCommon += _ph.getNumCommon(ce);
    }

    // Some code.
    virtual void visit(CodeExpr* ce) {
        _exprStr += _ph.addCodeExpr(_os, ce->getCode());
        _numCommon += _ph.getNumCommon(ce);
    }

    // A generic unary operator.
    virtual void visit(UnaryExpr* ue) {
        _exprStr += ue->getOpStr();
        ue->getRhs()->accept(this);
        _numCommon += _ph.getNumCommon(ue);
    }

    // A generic binary operator.
    virtual void visit(BinaryExpr* be) {
        _exprStr += "(";
        be->getLhs()->accept(this); // adds LHS to _exprStr.
        _exprStr += " " + be->getOpStr() + " ";
        be->getRhs()->accept(this); // adds RHS to _exprStr.
        _exprStr += ")";
        _numCommon += _ph.getNumCommon(be);
    }

    // A commutative operator.
    virtual void visit(CommutativeExpr* ce) {
        _exprStr += "(";
        ExprPtrVec& ops = ce->getOps();
        int opNum = 0;
        for (auto ep : ops) {
            if (opNum > 0)
                _exprStr += " " + ce->getOpStr() + " ";
            ep->accept(this);   // adds operand to _exprStr;
            opNum++;
        }
        _exprStr += ")";
        _numCommon += _ph.getNumCommon(ce);
    }

    // An equals operator.
    virtual void visit(EqualsExpr* ee) {

        // Get RHS and clear expr.
        ee->getRhs()->accept(this); // writes to _exprStr;
        string rhs = getExprStrAndClear();

        // Write statement with embedded rhs.
        GridPointPtr gpp = ee->getLhs();
        _os << _ph.getLinePrefix() << _ph.writeToPoint(_os, *gpp, rhs) << _ph.getLineSuffix();

        // note: _exprStr is now empty.
        // note: no need to update num-common.
    }
};

// Outputs a simple, human-readable version of the AST in a bottom-up
// fashion with multiple expressions, each assigned to a temp var.  The
// maxPoints parameter controls the size of each separate expression. Within
// each expression, a top-down visitor is used.
class PrintVisitorBottomUp : public PrintVisitorBase {

protected:
    // max points to print top-down before printing bottom-up.
    int _maxPoints;  

    // map sub-expressions to var names.
    map<Expr*, string> _tempVars;

    // Declare a new temp var.
    // Set _exprStr to it.
    // Print LHS of assignment to it.
    // Return stream to continue w/RHS.
    virtual ostream& makeNextTempVar(Expr* ex) {
        _exprStr = _ph.makeVarName();
        _tempVars[ex] = _exprStr;
        _os << endl << " // " << _exprStr << " = " << ex->makeStr() << "." << endl;
        _os << _ph.getLinePrefix() << _ph.getVarType() << " " << _exprStr << " = ";
        return _os;
    }

public:
    // os is used for printing intermediate results as needed.
    PrintVisitorBottomUp(ostream& os, PrintHelper& ph,
                         int maxPoints) :
        PrintVisitorBase(os, ph), _maxPoints(maxPoints) { }

    // make a new top-down visitor with the same print helper.
    virtual PrintVisitorTopDown* newPrintVisitorTopDown() {
        return new PrintVisitorTopDown(_os, _ph);
    }

    // Look for existing var.
    // Then, use top-down method for simple exprs.
    // Return true if successful.
    // FIXME: this causes all nodes in an expr above a certain
    // point to fail top-down because it looks at the original expr,
    // not the one with temp vars.
    virtual bool tryTopDown(Expr* ex, bool leaf = false) {

        // First, determine whether this expr has already been evaluated.
        auto p = _tempVars.find(ex);
        if (p != _tempVars.end()) {

            // if so, just use the existing var.
            _exprStr = p->second;
            return true;
        }
        
        // Use top down if <= maxPoints points in ex.
        if (leaf || ex->getNumNodes() <= _maxPoints) {

            // use a top-down printer to render the expr.
            PrintVisitorTopDown* topDown = newPrintVisitorTopDown();
            ex->accept(topDown);

            // were there any common sub-exprs found?
            bool ok = topDown->getNumCommon() == 0;

            // if no CSEs, use the resulting expression.
            if (ok)
                _exprStr = topDown->getExprStr();
            
            // if a leaf node is common, make a var for it.
            else if (leaf) {
                makeNextTempVar(ex) << topDown->getExprStr() << _ph.getLineSuffix();
                ok = true;
            }

            delete topDown;
            return ok;
        }

        return false;
    }

    // A grid or param point: just set expr.
    virtual void visit(GridPoint* gp) {
        tryTopDown(gp, true);
    }

    // A constant: just set expr.
    virtual void visit(ConstExpr* ce) {
        tryTopDown(ce, true);
    }

    // Code: just set expr.
    virtual void visit(CodeExpr* ce) {
        tryTopDown(ce, true);
    }

    // A unary operator.
    // Try top-down on RHS.
    virtual void visit(UnaryExpr* ue) {
        if (tryTopDown(ue))
            return;

        ue->getRhs()->accept(this); // sets _exprStr.
        string rhs = getExprStrAndClear();
        makeNextTempVar(ue) << ue->getOpStr() << ' ' << rhs << _ph.getLineSuffix();
    }

    // A binary operator.
    // Use top-down if whole expr small enough;
    // otherwise, get LHS and RHS; print statement; make new var for result.
    virtual void visit(BinaryExpr* be) {
        if (tryTopDown(be))
            return;

        be->getLhs()->accept(this); // sets _exprStr.
        string lhs = getExprStrAndClear();
        be->getRhs()->accept(this); // sets _exprStr.
        string rhs = getExprStrAndClear();
        makeNextTempVar(be) << lhs << ' ' << be->getOpStr() << ' ' << rhs << _ph.getLineSuffix();
    }

    // A commutative operator.
    // Use top-down if expr small enough;
    // otherwise, get ops; print statement(s); make new var(s) for result.
    virtual void visit(CommutativeExpr* ce) {
        if (tryTopDown(ce))
            return;

        ExprPtrVec& ops = ce->getOps();
        string lhs;
        int opNum = 0;
        for (auto ep : ops) {

            // eval the operand; sets _exprStr.            
            ep->accept(this);
            string opStr = getExprStrAndClear();

            // first operand.
            if (opNum == 0)
                lhs = opStr;    // just save for next iteration.

            // subsequent operands.
            // makes separate assignment for each one.
            // result is kept as LHS of next one.
            else {
                makeNextTempVar(ce) << lhs << ' ' << ce->getOpStr() << ' ' <<
                    opStr << _ph.getLineSuffix();
                lhs = getExprStr(); // result used in next iteration, if any.
            }
            opNum++;
        }

        // note: _exprStr contains result of last operation.
    }

    // An equality.
    virtual void visit(EqualsExpr* ee) {

        // Eval RHS.
        Expr* rp = ee->getRhs().get();
        rp->accept(this); // sets _exprStr.
        string rhs = _exprStr;

        // Assign RHS to a temp var.
        makeNextTempVar(rp) << rhs << _ph.getLineSuffix(); // sets _exprStr.
        string tmp = getExprStrAndClear();

        // Write temp var to grid.
        GridPointPtr gpp = ee->getLhs();
        _os << endl << " // Save result to " << gpp->makeStr() << ":" << endl;
        _os << _ph.getLinePrefix() << _ph.writeToPoint(_os, *gpp, tmp) << _ph.getLineSuffix();

        // note: _exprStr is now empty.
    }
    
};


// Outputs a POV-Ray input file.
class POVRayPrintVisitor : public ExprVisitor {
protected:
    ostream& _os;
    vector<string> _colors;
    int _numPts;

public:
    POVRayPrintVisitor(ostream& os) : _os(os), _numPts(0) {

        // Make a rainbow.
        _colors.push_back("Red");
        _colors.push_back("Orange");
        _colors.push_back("Yellow");
        _colors.push_back("Green");
        _colors.push_back("Blue");
        _colors.push_back("Violet");

        // NB: could also calculate the hue and use CHSL2RGB().
    }

    virtual int getNumPoints() const {
        return _numPts;
    }

    // Only want to visit the RHS of an equation.
    virtual void visit(EqualsExpr* ee) {
        ee->getRhs()->accept(this);      
    }
    
    // A point: output it.
    virtual void visit(GridPoint* gp) {
        _numPts++;

        // Pick a color based on its distance.
        size_t ci = gp->max();
        ci %= _colors.size();
        
        _os << "point(" + _colors[ci] + ", " << gp->makeValStr() << ")" << endl;
    }
};

#endif