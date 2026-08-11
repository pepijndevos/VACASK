#ifndef __RPNEXPR_DEFINED
#define __RPNEXPR_DEFINED

#include <variant>
#include <limits>
#include <unordered_map>
#include "value.h"
#include "identifier.h"
#include "status.h"
#include "rpnexprva.h"
#include "common.h"


namespace NAMESPACE {

class Rpn {
public:
    enum OpCode : char { 
        OpPlus, OpMinus, OpTimes, OpDivide, OpUMinus, OpPower, 
        OpEqual, OpNotEqual, OpLess, OpLessEq, OpGreater, OpGreaterEq, 
        OpBitAnd, OpBitOr, OpBitExor, OpBitNot, OpBitShiftR, OpBitShiftL, 
        OpAnd, OpOr, OpNot, OpQuestion, 
        OpSelect
    };
    enum BrFlags : char {
        BrKeepOnBranch=1, BrKeepOnNoBranch=2, BrFalse=4, BrHidden=8
    };

    typedef RpnArity Arity;
    typedef RpnJumpOffset JumpOffset;

    const static Arity manyArgs = std::numeric_limits<Arity>::max();

    // 1 byte
    typedef struct Op {
        OpCode code;
        Op(const OpCode& c) : code(c) {};
        bool operator==(OpCode op) { return code==op; };
    } Op;
    // 4 bytes
    typedef struct Identifier {
        Id name;
        Identifier(const std::string&& s) : name(std::move(s)) {};
    } Identifier;
    // 4+4 = 8 bytes
    typedef struct FunctionCall {
        Id name;
        Arity arity;
        FunctionCall(const std::string&& s, Arity a) : name(std::move(s)), arity(a) {};
    } FunctionCall;
    // 4 bytes
    typedef struct PackVec {
        Arity arity;
        PackVec(Arity a) : arity(a) {};
    } PackVec;
    // 4 bytes
    typedef struct PackList {
        Arity arity;
        PackList(Arity a) : arity(a) {};
    } PackList;
    // 4 bytes
    typedef struct Jump {
        JumpOffset offset;
        BrFlags flags;
        Jump(JumpOffset o, BrFlags f) : offset(o), flags(f) {};
    } Jump;
    // 4 bytes
    typedef struct Branch {
        JumpOffset offset;
        BrFlags flags;
        Branch(JumpOffset o, BrFlags f) : offset(o), flags(f) {};
    } Branch;
    // Empty
    typedef struct MakeBoolean {
        MakeBoolean() {};
    } MakeBoolean;
    
    enum Type : char { 
        TValue=0, TOp=1, TIdentifier=2, TFunctionCall=3, 
        TPackVec=4, TPackList=5,  
        TJump=6, TBranch=7, TMakeBoolean=8
    };

    class Entry {
    public:
        Entry           (const Entry&)  = delete;
        Entry           (      Entry&&) = default;
        Entry& operator=(const Entry&)  = delete;
        Entry& operator=(      Entry&&) = default;

        template<typename T> Entry(T&& other) : data(std::move(other)), loc_(Loc::bad) {};

        Type type() const { return Type(data.index()); };
        // Every entry kind carries its own location directly, uniformly --
        // no per-variant-alternative field, no separate index/table.
        void setLocation(const Loc& l) { loc_ = l; };
        const Loc& location() const { return loc_; };
        template<typename T> T& get() { return std::get<T>(data); };
        template<typename T> const T& get() const { return std::get<T>(data); };

        std::variant<Value, Op, Identifier, FunctionCall, PackVec, PackList, Jump, Branch, MakeBoolean> data;
        Loc loc_;
    };

    typedef std::vector<Entry> Expression;
    
    Rpn();

    Rpn           (const Rpn&)  = delete;
    Rpn           (      Rpn&&) = default;
    Rpn& operator=(const Rpn&)  = delete;
    Rpn& operator=(      Rpn&&) = default;
    
    inline auto begin() { return expr.begin(); };
    inline auto end() { return expr.end(); };
    inline auto cbegin() const { return expr.cbegin(); };
    inline auto cend() const { return expr.cend(); };

    inline bool endsWithMakeBoolean() {
        return expr.size()!=0 && expr.back().type()==Rpn::TMakeBoolean;
    };
    inline void extend(Rpn &&other) {
        for(auto it=other.begin(); it!=other.end(); ++it) {
            expr.push_back(std::move(*it));
        }
    };
    inline void extend(Entry&& other, Loc l) {
        other.setLocation(l);
        expr.push_back(std::move(other));
    };
    inline const Loc& location(const Entry& e) const {
        return e.location();
    }
    
    inline size_t size() const noexcept { return expr.size(); }

    inline const Rpn::Entry& operator[](size_t i) const { return expr[i]; }

    std::string str() const;

    bool verilogA(const std::string& discipline, const std::string& potAccess, const std::string& flowAccess, RPNBehavioralVA& behavData, Status& s=Status::ignore) const;
    
    friend std::ostream& operator<<(std::ostream& os, const Rpn& expr);

    // Replace characters that are not valid in a Verilog-A identifier with '_'
    static std::string sanitizeVariable(const std::string& s, bool atBeginning=false);

private:
    Expression expr;
    static std::unordered_map<OpCode, std::tuple<const char*, int>> opMap;

    std::string parenthesize(const std::string& s) const {
        return std::string("(")+s+")";
    };
};

DEFINE_FLAG_OPERATORS(Rpn::BrFlags);

}

#endif
