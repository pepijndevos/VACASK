%skeleton "lalr1.cc"

// Minimum version of Bison
%require  "3.3"

// Extra output file with description of states
// %verbose 

// Turn on parser instrumentation for tracing
// %define parse.trace

// Verbose parse errors
%define parse.error verbose

// Write a header file with token definitions (will change to %header)
%defines

// Namespace to use for the parser
%define api.namespace {NAMESPACE::dflparse}

// Name of the parser class
%define api.parser.class {Parser}

// Code required for the value and location types
// Goes to the top of the parser include file
%code requires{
// #define YYDEBUG 1
#include "value.h"
#include "parseroutput.h"
#include "rpneval.h"
#include "rpnexpr.h"
#include "location.h"
#include <utility>
#include <tuple>
#include <limits>
#include <iostream>
#include "common.h"

using namespace NAMESPACE;

namespace NAMESPACE::dflparse {
    class Scanner;
}

namespace NAMESPACE {
    // Stuff from library namespace
    // Init-order safe: createStatic() depends on no other dynamic initializer.
    // It touches only constant-initialized counters (nextStatic/nextOrdinary,
    // set before all dynamic init) and Meyers-singleton pools/maps (built lazily
    // on first use). As a startup-time namespace-scope static, saveCmd is interned
    // before main(), i.e. before any ordinary "save" id could exist -- the intended
    // usage documented in identifier.h (static ids created before ordinary ones).
    static Id saveCmd = Id::createStatic("save");
}

// The following definitions is missing when %locations isn't used
# ifndef YY_NULLPTR
#  if defined __cplusplus && 201103L <= __cplusplus
#   define YY_NULLPTR nullptr
#  else
#   define YY_NULLPTR 0
#  endif
# endif

struct pexpr {
    Id id; 
    Rpn expr; 
    Location loc;
};

struct paramlist {
    PTParameters params;
    std::unordered_map<Id,Location> locations;
};

struct sweeps {
    std::vector<PTSweep> sweeps;
    std::unordered_map<Id,Location> locations;
};

typedef struct subckt {
    bool isToplevel {true};
    bool hasControlBlock {false};
    PTSubcircuitDefinition def;
    PTParameters parameters;
    std::unordered_map<Id,Location> paramLoc;
} subckt;

}

// Additional arguments to parser class constructor
%parse-param {NAMESPACE::dflparse::Scanner& scanner}
// %parse-param {ParserDriver& driver}
%parse-param {ParserTables& tables}
%parse-param {Rpn* expressionPtr}
%parse-param {PTParameters* parametersPtr}
%parse-param {RpnEvaluator& evaluator}
%parse-param {Status &status}

// Add this code to the beginning of parser implementation
%code{
#include <sstream>
#include <iostream>
#include <cstdlib>
#include <fstream>

#include "dflscanner.h"

#ifdef VACASK_WITH_SPICE
// For draining foreign-format (SPICE/Spectre) includes stashed by the scanner
// into the toplevel definition (see the `output` rule).
#include "netlistrs.h"
#endif

#undef yylex
#define yylex scanner.yylex
}
 
// Type of semantic value, similar to union, allows any c++ object type
%define api.value.type variant

// Include runtime assertions to check for invalid use
// In c++ parsers it uses runtime type information (RTTI)
%define parse.assert

// Generate code for location tracking
%locations

// Location type
%define api.location.type {Location}

// Tokens, semantic types, and token names for error reporting
%token               END    0     "end of file"
%token <std::string> TITLE        "circuit title"

%token               MODEL        "model"
%token               GLOBAL       "global"
%token               GROUND       "ground"
%token               LOAD         "load"
%token               SUBCKT       "subckt"
%token               ENDS         "ends"
%token               PARAMETERS   "parameters"
%token               EMBED        "embed"
%token               CONTROL      "control"
%token               ENDC         "endc"
%token               SAVE         "save"
%token               SWEEP        "sweep"
%token               ANALYSIS     "analysis"

%token <Id>          IDENTIFIER   "identifier"

%token <Int>         INTEGER      "integer"
%token <Int>         HEXINTEGER   "hexadecimal integer"
%token <Real>        FLOAT        "floating point number"
%token <std::string> STRING       "string literal"

%token               PLUS         "+"
%token               MINUS        "-"
%token               TIMES        "*"
%token               DIVIDE       "/"
%token               POWER        "**"
%token               LBRACKET     "["
%token               RBRACKET     "]"
%token               LPAREN       "("
%token               RPAREN       ")"
%token               ASSIGN       "="
%token               COMMA        ","
%token               GREATER      ">"
%token               LESS         "<"
%token               GREATEREQ    ">="
%token               LESSEQ       "<="
%token               EQUAL        "=="
%token               NOTEQUAL     "!="
%token               QUESTION     "?"
%token               AND          "&&"
%token               OR           "||"
%token               NOT          "!"
%token               BITAND       "&"
%token               BITOR        "|"
%token               BITNOT       "~"
%token               BITEXOR      "^"
%token               BITSHIFTR    ">>"
%token               BITSHIFTL    "<<"

%token               COLON        ":"
%token               SEMICOLON    ";"
%token               RIGHTARROW   "->"  // reserved for future use
// Not allowed due to conflict with <-5 (could be <- 5 or < -5)
// %token               LEFTARROW    "<-" 

%token               NEWLINE      "newline"

%token               INNETLIST    "input netlist"
%token               INEXPR       "input expression"
%token               INPARAMS     "input parameters"

%token               BLKIF        "@if"
%token               BLKELSEIF    "@elseif"
%token               BLKELSE      "@else"
%token               BLKENDIF     "@end"


// Operator associativity and precedence, lowest first
%right QUESTION
%left OR
%left AND
%left BITOR
%left BITEXOR
%left BITAND
%left EQUAL NOTEQUAL 
%left LESS GREATER GREATEREQ LESSEQ
%left BITSHIFTL BITSHIFTR
%left PLUS MINUS
%left TIMES DIVIDE
%right POWER
%right NEG NOT BITNOT
%left LPAREN RPAREN LBRACKET RBRACKET

// exprlist e  e,e
// bracketlist1 [ [1 
// commabracketlist1 [, [1, [1,2,3
// semicolonbracketlist1 [; [1; [1;2;3

// Nonterminal symbols
%type <Int>                             intnum
%type <Id>                              terminal
%type <PTIdentifierList>                terminal_list global ground keywords
%type <std::vector<Rpn>>                exprlist semexprlist colexprlist
%type <Value>                           value
%type <struct pexpr>                    parameter_expression
%type <struct paramlist>                parameter_list opt_broken_parameter_list subcktparameters 
%type <PTInstance>                      instance
%type <PTModel>                         model
%type <PTSubcircuitDefinition>          subckt
%type <struct subckt>                   subckt_build
%type <PTBlockSequence>                 condblock_build condblock
%type <PTLoad>                          load
%type <Rpn>                             expr
%type <Id>                              savestr
%type <std::vector<Id>>                 savestrlist
%type <PTSave>                          savecmd
%type <std::vector<PTSave>>             savecmd_list saves
%type <PTEmbed>                         embed
%type <Int>                             control_block_build control_block
%type <PTCommand>                       command
%type <struct sweeps>                   sweeps
%type <PTAnalysis>                      analysis pre_analysis analysis_with_params 


// Rules
%%

// Convention used throughout the actions below:
// Freshly-built temporaries are wrapped in std::move(), e.g.
//   $$ = std::move(PTInstance(...));
// even though a temporary is already an rvalue. This is a deliberate, uniform
// marker of move intent kept for consistency with the std::move() applied to
// named semantic values such as std::move($2). On a prvalue it has no effect on
// overload resolution (move-assignment is selected either way), so it is harmless
// but also provides no extra safety here -- unlike on a named lvalue, where
// omitting std::move() would silently downgrade a move to a copy.

output
  : INNETLIST subckt_build END {
    // Toplevel circuit definition
    $2.def.add(std::move($2.parameters));
#ifdef VACASK_WITH_SPICE
    // Drain foreign-format (SPICE/Spectre) includes the scanner deferred: parse
    // each via the Rust adapter and merge its models/subckts/devices into the
    // toplevel def (auto-emitting the OSDI loads they need). Runs before
    // setDefaultSubDef/verify so the merged content is committed and checked.
    {
        sim::Parser foreignParser(tables);
        for (auto& fi : tables.pendingForeign()) {
            if (!sim::mergeForeignFile(fi.path, fi.section, fi.language, $2.def, tables,
                                       foreignParser, status)) {
                YYERROR;
            }
        }
        tables.pendingForeign().clear();
    }
#endif
    tables.setDefaultSubDef(std::move($2.def));
    tables.defaultGround();
    // Verify tables (basic level 0 verifications)
    if (!(tables.verify(status))) {
        YYERROR;
    }
  }
  | INEXPR expr END {
    // Parse an expression.
    // No tables.verify() here: this path populates only *expressionPtr and
    // builds nothing in tables that would require verification.
    *expressionPtr = std::move($2);
  }
  | INPARAMS opt_broken_parameter_list END {
    // Parse a parameters list.
    // No tables.verify() here: this path populates only *parametersPtr and
    // builds nothing in tables that would require verification.
    *parametersPtr = std::move($2.params);
  }

// Toplevel netlist and subcircuit definition
subckt_build
  : TITLE NEWLINE {
    // Toplevel netlist start
    tables.setTitle(std::move($1));
    $$.def = std::move(PTSubcircuitDefinition(
        Id(), // By default the name (Id) of the toplevel definition is not valid (empty)
        std::move(PTIdentifierList()), 
        @1.loc()
    ));
  }
  | SUBCKT IDENTIFIER LPAREN RPAREN NEWLINE {
    // Subcircuit definition start, no terminals
    $$.def = std::move(PTSubcircuitDefinition(
        $2, 
        std::move(PTIdentifierList()),
        @1.loc()
    ));
    // This is not the toplevel definition
    $$.isToplevel = false;
  }
  | SUBCKT IDENTIFIER LPAREN terminal_list RPAREN NEWLINE {
    // Subcircuit definition start, with terminals
    $$.def = std::move(PTSubcircuitDefinition(
        $2, 
        std::move($4), 
        @1.loc()
    ));
    // This is not the toplevel definition
    $$.isToplevel = false;
  }
  | subckt_build NEWLINE {
    // Skip NEWLINE
    $$ = std::move($1);
  }
  | subckt_build subcktparameters {
    // parameters
    for(auto it=$2.locations.begin(); it!=$2.locations.end(); ++it) {
        auto fdit = $1.paramLoc.find(it->first);
        if (fdit!=$1.paramLoc.end()) {
            status.set(Status::Redefinition, "Parameter redefinition.");
            status.extend(it->second.loc());
            status.extend("Parameter first defined here.");
            status.extend(fdit->second.loc());
            YYERROR;
        }
    }
    $$ = std::move($1);
    $$.parameters.add(std::move($2.params));
    $$.paramLoc.merge(std::move($2.locations));
  }
  | subckt_build model {
    $$ = std::move($1);
    $$.def.add(std::move($2));
  }
  | subckt_build instance {
    $$ = std::move($1);
    $$.def.add(std::move($2));
  }
  | subckt_build condblock {
    $$ = std::move($1);
    $$.def.add(std::move($2));
  }
  | subckt_build subckt {
    $$ = std::move($1);
    $$.def.add(std::move($2));
  }
  | subckt_build global {
    // global nodes, not allowed in subcircuit definition
    if (!$1.isToplevel) {
        status.set(Status::Syntax, "Global nodes allowed only in toplevel circuit.");
        status.extend(@2.loc());
        YYERROR;
    }
    $$ = std::move($1);
    for(auto it=$2.begin(); it!=$2.end(); ++it) {
        tables.addGlobal(std::move(*it));
    }
  }
  | subckt_build ground {
    // ground nodes, not allowed in subcircuit definition
    if (!$1.isToplevel) {
        status.set(Status::Syntax, "Ground nodes allowed only in toplevel circuit.");
        status.extend(@2.loc());
        YYERROR;
    }
    $$ = std::move($1);
    for(auto it=$2.begin(); it!=$2.end(); ++it) {
        tables.addGround(std::move(*it));
    }
  }
  | subckt_build load {
    // load, not allowed in subcircuit definition
    if (!$1.isToplevel) {
        status.set(Status::Syntax, "Load allowed only in toplevel circuit.");
        status.extend(@2.loc());
        YYERROR;
    }
    $$ = std::move($1);
    tables.add(std::move($2));
  }
  | subckt_build embed {
    // embed, not allowed in subcircuit definition
    if (!$1.isToplevel) {
        status.set(Status::Syntax, "Embed allowed only in toplevel circuit.");
        status.extend(@2.loc());
        YYERROR;
    }
    $$ = std::move($1);
    tables.add(std::move($2));
  }
  | subckt_build control_block {
    // control block, allow this for toplevel definition only
    if (!$1.isToplevel) {
        status.set(Status::Syntax, "Control block is not allowed inside subcircuit definition.");
        status.extend(@2.loc());
        YYERROR;
    }
    // Only one control block is allowed
    if ($1.hasControlBlock) {
        status.set(Status::Syntax, "Only one control block is allowed.");
        status.extend(@2.loc());
        YYERROR;
    }
    $$ = std::move($1);
    $$.hasControlBlock = true;
  }
  
subckt
  : subckt_build ENDS NEWLINE {
    // Subcircuit definition
    $1.def.add(std::move($1.parameters)); 
    $$ = std::move($1.def);
  }
  | subckt_build ENDS IDENTIFIER NEWLINE {
    if ($3 != $1.def.name()) {
      status.set(Status::Syntax, "ends name '"+std::string($3)+"' does not match subckt name '"+std::string($1.def.name())+"'.");
      status.extend(@3.loc());
      YYERROR;
    }
    $1.def.add(std::move($1.parameters));
    $$ = std::move($1.def);
  }

// Conditional block building
condblock_build 
  : BLKIF expr NEWLINE {
    PTBlock blk;
    $$.add(std::move($2), std::move(blk), @1.loc());
  }
  | condblock_build instance {
    $$ = std::move($1);
    // back() is always valid here: the only base production (BLKIF expr NEWLINE)
    // adds a block, and every other production keeps the sequence non-empty, so a
    // reduced condblock_build always holds >=1 block. Same for the model/condblock cases.
    $$.back().add(std::move($2));
  }
  | condblock_build model {
    $$ = std::move($1);
    $$.back().add(std::move($2));
  }
  | condblock_build condblock {
    $$ = std::move($1);
    $$.back().add(std::move($2));
  }
  | condblock_build BLKELSEIF expr NEWLINE {
    $$ = std::move($1);
    PTBlock blk;
    $$.add(std::move($3), std::move(blk), @2.loc());
  }
  | condblock_build BLKELSE NEWLINE {
    $$ = std::move($1);
    PTBlock blk;
    $$.add(std::move(Rpn()), std::move(blk), @2.loc());
  }
  | condblock_build NEWLINE {
    $$ = std::move($1);
  }
;

// End of conditional block
condblock
  : condblock_build BLKENDIF NEWLINE {
    $$ = std::move($1);
  }
;

terminal
  : IDENTIFIER { 
    $$ = $1;
  }
  | INTEGER { 
    $$ = Id(std::to_string($1)); 
  }

terminal_list
  : terminal { 
    $$.push_back(PTParsedIdentifier(std::move($1), @1.loc())); 
  }
  | terminal_list terminal { 
    $$ = std::move($1); 
    $$.push_back(PTParsedIdentifier(std::move($2), @2.loc())); 
  } 

intnum
  : INTEGER { $$ = $1; } 
  | HEXINTEGER { $$ = $1; } 

value
  : intnum { $$ = Int($1); }
  | FLOAT { $$ = Real($1); }
  | STRING { $$ = Value(std::move($1)); }

expr
  : value { $$.extend(std::move($1), @1.loc()); }
  | IDENTIFIER { 
    $$.extend(Rpn::Identifier(std::move($1)), @1.loc()); 
  }
  | expr PLUS expr { 
    $$.extend(std::move($1)); 
    $$.extend(std::move($3)); 
    $$.extend(Rpn::Op(Rpn::OpPlus), @2.loc()); 
  }
  | expr MINUS expr { 
    $$.extend(std::move($1)); 
    $$.extend(std::move($3)); 
    $$.extend(Rpn::Op(Rpn::OpMinus), @2.loc()); 
  }
  | expr TIMES expr { 
    $$.extend(std::move($1)); 
    $$.extend(std::move($3)); 
    $$.extend(Rpn::Op(Rpn::OpTimes), @2.loc());  
  }
  | expr DIVIDE expr { 
    $$.extend(std::move($1)); 
    $$.extend(std::move($3)); 
    $$.extend(Rpn::Op(Rpn::OpDivide), @2.loc());  
  }
  | expr POWER expr { 
    $$.extend(std::move($1)); 
    $$.extend(std::move($3)); 
    $$.extend(Rpn::Op(Rpn::OpPower), @2.loc());  
  }
  | expr EQUAL expr { 
    $$.extend(std::move($1)); 
    $$.extend(std::move($3)); 
    $$.extend(Rpn::Op(Rpn::OpEqual), @2.loc());  
  }
  | expr NOTEQUAL expr { 
    $$.extend(std::move($1)); 
    $$.extend(std::move($3)); 
    $$.extend(Rpn::Op(Rpn::OpNotEqual), @2.loc());  
  }
  | expr LESS expr { 
    $$.extend(std::move($1)); 
    $$.extend(std::move($3)); 
    $$.extend(Rpn::Op(Rpn::OpLess), @2.loc());  
  }
  | expr LESSEQ expr { 
    $$.extend(std::move($1)); 
    $$.extend(std::move($3)); 
    $$.extend(Rpn::Op(Rpn::OpLessEq), @2.loc());  
  }
  | expr GREATER expr { 
    $$.extend(std::move($1)); 
    $$.extend(std::move($3)); 
    $$.extend(Rpn::Op(Rpn::OpGreater), @2.loc());  
  }
  | expr GREATEREQ expr { 
    $$.extend(std::move($1)); 
    $$.extend(std::move($3)); 
    $$.extend(Rpn::Op(Rpn::OpGreaterEq), @2.loc());  
  }
  | expr BITAND expr { 
    $$.extend(std::move($1)); 
    $$.extend(std::move($3)); 
    $$.extend(Rpn::Op(Rpn::OpBitAnd), @2.loc());  
  }
  | expr BITOR expr { 
    $$.extend(std::move($1)); 
    $$.extend(std::move($3)); 
    $$.extend(Rpn::Op(Rpn::OpBitOr), @2.loc());  
  }
  | expr BITEXOR expr { 
    $$.extend(std::move($1)); 
    $$.extend(std::move($3)); 
    $$.extend(Rpn::Op(Rpn::OpBitExor), @2.loc());  
  }
  | expr BITSHIFTR expr { 
    $$.extend(std::move($1)); 
    $$.extend(std::move($3)); 
    $$.extend(Rpn::Op(Rpn::OpBitShiftR), @2.loc());  
  }
  | expr BITSHIFTL expr { 
    $$.extend(std::move($1)); 
    $$.extend(std::move($3)); 
    $$.extend(Rpn::Op(Rpn::OpBitShiftL), @2.loc());  
  }
  | expr AND expr { 
    // short circuit (a && b) translation to RPN
    //         a
    //         makeboolean
    //         branchiffalse end
    //         b
    //         makeboolean
    //         op(and) // does nothing during execution, needed by formatting
    //   end:
    auto needsConversion = !$3.endsWithMakeBoolean();
    $$.extend(std::move($1));
    $$.extend(Rpn::MakeBoolean(), @2.loc());
    auto branchPos = $$.size();
    auto branchOffset = $3.size()+(needsConversion?1:0)+2;
    $$.extend(Rpn::Branch(branchOffset, Rpn::BrFalse|Rpn::BrKeepOnBranch|Rpn::BrHidden), @2.loc());
    $$.extend(std::move($3));
    if (needsConversion) {
      $$.extend(Rpn::MakeBoolean(), @2.loc());
    }
    $$.extend(Rpn::Op(Rpn::OpAnd), @2.loc());
    // The BrFalse branch must land just past op(and), i.e. at the end of the
    // sequence built here. Catch silently-wrong offset arithmetic.
    if (branchPos+branchOffset != $$.size()) {
        status.set(Status::Unsupported, "Internal error: short-circuit '&&' branch offset is wrong.");
        status.extend(@2.loc());
        YYERROR;
    }
  }
  | expr OR expr { 
    // short circuit (a || b) translation to RPN
    //         a
    //         makeboolean
    //         branchiftrue end
    //         b
    //         makeboolean
    //         op(or) // does nothing during execution, needed by formatting
    //   end:
    auto needsConversion = !$3.endsWithMakeBoolean();
    $$.extend(std::move($1));
    $$.extend(Rpn::MakeBoolean(), @2.loc());
    auto branchPos = $$.size();
    auto branchOffset = $3.size()+(needsConversion?1:0)+2;
    $$.extend(Rpn::Branch(branchOffset, Rpn::BrKeepOnBranch|Rpn::BrHidden), @2.loc());
    $$.extend(std::move($3));
    if (needsConversion) {
      $$.extend(Rpn::MakeBoolean(), @2.loc());
    }
    $$.extend(Rpn::Op(Rpn::OpOr), @2.loc());
    // The BrTrue branch must land just past op(or), i.e. at the end of the
    // sequence built here. Catch silently-wrong offset arithmetic.
    if (branchPos+branchOffset != $$.size()) {
        status.set(Status::Unsupported, "Internal error: short-circuit '||' branch offset is wrong.");
        status.extend(@2.loc());
        YYERROR;
    }
  }
  | expr QUESTION expr COLON expr %prec QUESTION {
    // Ternary operator a?b:c, translation to RPN
    //        a ($1)
    //        branchiffalse false 
    //        b ($3)
    //        jump end 
    // false: c ($5)
    // end:   op(question) // does nothing during execution, needed by formatting
    $$.extend(std::move($1));
    auto branchPos = $$.size();
    auto branchOffset = $3.size()+2;
    $$.extend(Rpn::Branch(branchOffset, Rpn::BrFalse|Rpn::BrHidden), @2.loc());
    $$.extend(std::move($3));
    auto jumpPos = $$.size();
    auto jumpOffset = $5.size()+2;
    $$.extend(Rpn::Jump(jumpOffset, Rpn::BrHidden), @2.loc());
    auto falsePos = $$.size();
    $$.extend(std::move($5));
    $$.extend(Rpn::Op(Rpn::OpQuestion), @2.loc());
    // The BrFalse branch must land on c ($5); the jump must land just past
    // op(question). Catch silently-wrong offset arithmetic.
    if (branchPos+branchOffset != falsePos || jumpPos+jumpOffset != $$.size()) {
        status.set(Status::Unsupported, "Internal error: ternary branch/jump offset is wrong.");
        status.extend(@2.loc());
        YYERROR;
    }
  }
  | BITNOT expr { 
    $$.extend(std::move($2)); 
    $$.extend(Rpn::Op(Rpn::OpBitNot), @1.loc());  
  }
  | NOT expr { 
    $$.extend(std::move($2)); 
    $$.extend(Rpn::Op(Rpn::OpNot), @1.loc());  
  }
  | MINUS expr %prec NEG { 
    $$.extend(std::move($2)); 
    $$.extend(Rpn::Op(Rpn::OpUMinus), @1.loc());  
  }
  | PLUS expr %prec NEG { 
    $$.extend(std::move($2)); 
  }
  | LPAREN expr RPAREN{ 
    $$.extend(std::move($2)); 
  }
  | IDENTIFIER LPAREN RPAREN { 
    // Function call, no arguments
    $$.extend(Rpn::FunctionCall(std::move($1), 0), @1.loc()); 
  }
  | IDENTIFIER LPAREN exprlist RPAREN {
    // Function call with arguments
    // Arity is stored as Rpn::Arity (uint32_t); reject counts that would not fit.
    if ($3.size() > std::numeric_limits<Rpn::Arity>::max()) {
        status.set(Status::BadArguments, "Function call has too many arguments.");
        status.extend(@1.loc());
        YYERROR;
    }
    for(Rpn::Arity i=0; i<$3.size(); i++) {
        $$.extend(std::move($3[i]));
    }
    $$.extend(Rpn::FunctionCall(std::move($1), static_cast<Rpn::Arity>($3.size())), @1.loc());
  }
  | LBRACKET RBRACKET {
    // Empty vector of type Int
    $$.extend(Rpn::PackVec(0), @1.loc()); 
  }
  | LBRACKET COMMA RBRACKET {
    // Empty vector of type Int
    $$.extend(Rpn::PackVec(0), @1.loc()); 
  }
  | LBRACKET exprlist RBRACKET {
    // Pack values in a vector, flatten lists into a vector
    // [,] is an empty Int vector
    if ($2.size() > std::numeric_limits<Rpn::Arity>::max()) {
        status.set(Status::BadArguments, "Vector has too many elements.");
        status.extend(@1.loc());
        YYERROR;
    }
    for(Rpn::Arity i=0; i<$2.size(); i++) {
        $$.extend(std::move($2[i]));
    }
    $$.extend(Rpn::PackVec(static_cast<Rpn::Arity>($2.size())), @1.loc());
  }
  | LBRACKET SEMICOLON RBRACKET {
    // Empty list
    $$.extend(Rpn::PackList(0), @1.loc()); 
  }
  | LBRACKET expr SEMICOLON RBRACKET {
    // List with single element
    // Pack values in a list, keep members that are lists themselves intact
    // This produces a list of lists of ...
    $$.extend(std::move($2)); 
    $$.extend(Rpn::PackList(1), @1.loc()); 
  }
  | LBRACKET semexprlist RBRACKET {
    // List with two or more elements
    // Pack values in a list, keep members that are lists themselves intact
    // This produces a list of lists of ...
    if ($2.size() > std::numeric_limits<Rpn::Arity>::max()) {
        status.set(Status::BadArguments, "List has too many elements.");
        status.extend(@1.loc());
        YYERROR;
    }
    for(Rpn::Arity i=0; i<$2.size(); i++) {
        $$.extend(std::move($2[i]));
    }
    $$.extend(Rpn::PackList(static_cast<Rpn::Arity>($2.size())), @1.loc());
  }
  | LBRACKET COLON RBRACKET {
    // Empty list
    $$.extend(Rpn::PackList(0), @1.loc()); 
  }
  | LBRACKET expr COLON RBRACKET {
    // List with single element unpacked
    // Merge scalars and lists in one list
    $$.extend(std::move($2)); 
    $$.extend(Rpn::MergeList(1), @1.loc()); 
  }
  | LBRACKET colexprlist RBRACKET {
    // List with two or more elements unpacked
    // Merge scalars and lists in one list
    if ($2.size() > std::numeric_limits<Rpn::Arity>::max()) {
        status.set(Status::BadArguments, "List has too many elements.");
        status.extend(@1.loc());
        YYERROR;
    }
    for(Rpn::Arity i=0; i<$2.size(); i++) {
        $$.extend(std::move($2[i]));
    }
    $$.extend(Rpn::MergeList(static_cast<Rpn::Arity>($2.size())), @1.loc());
  }
  
  | expr LBRACKET expr RBRACKET {
    // Vector and list selector
    $$.extend(std::move($1)); 
    $$.extend(std::move($3)); 
    $$.extend(Rpn::Op(Rpn::OpSelect), @2.loc()); 
  }

// Comma separated exprlist is always in parentheses or brackets, 
// no need to handle NEWLINE. 
// List has always at least one expression. 
exprlist
  : expr {
    // Single expresion
    $$.push_back(std::move($1));
  }
  | exprlist COMMA expr {
    // Multiple expressions
    $$ = std::move($1);
    $$.push_back(std::move($3));
  }

// Semicolon separated expression list is always in brackets, 
// no need to handle NEWLINE. 
// List has always at least two expressions. 
semexprlist
  : expr SEMICOLON expr {
    $$.push_back(std::move($1));
    $$.push_back(std::move($3));
  }
  | semexprlist SEMICOLON expr {
    $$ = std::move($1);
    $$.push_back(std::move($3));
  }

// Colon separated expression list is always in brackets, 
// no need to handle NEWLINE. 
// List has always at least two expressions. 
colexprlist
  : expr COLON expr {
    $$.push_back(std::move($1));
    $$.push_back(std::move($3));
  }
  | colexprlist COLON expr {
    $$ = std::move($1);
    $$.push_back(std::move($3));
  }

parameter_expression
  : IDENTIFIER ASSIGN expr { 
    $$.id = $1;
    $$.expr = std::move($3);
    $$.loc = @1;
  }

parameter_list
  : parameter_expression {
    $$.locations[$1.id] = $1.loc;
    if (evaluator.isConstant($1.expr)) {
        Value v;
        RpnEvaluationNetlistContext ctx;
        if (!evaluator.evaluate($1.expr, v, ctx, status)) {
            YYERROR;
        }
        $$.params.add(PTParameterValue($1.id, std::move(v), @1.loc()));
    } else {
        $$.params.add(PTParameterExpression($1.id, std::move($1.expr), @1.loc()));
    }
  }
  | parameter_list parameter_expression {
    $$ = std::move($1);
    auto it = $$.locations.find($2.id);
    if (it!=$$.locations.end()) {
        status.set(Status::Redefinition, "Parameter redefinition.");
        status.extend(@2.loc());
        status.extend("Parameter first defined here.");
        status.extend(it->second.loc());
        YYERROR;
    }
    $$.locations[$2.id] = $2.loc;
    if (evaluator.isConstant($2.expr)) {
        Value v;
        RpnEvaluationNetlistContext ctx;
        if (!evaluator.evaluate($2.expr, v, ctx, status)) {
            YYERROR;
        }
        $$.params.add(PTParameterValue($2.id, std::move(v), @2.loc()));
    } else {
        $$.params.add(PTParameterExpression($2.id, std::move($2.expr), @2.loc()));
    }
  }

opt_broken_parameter_list
  : parameter_list {
    $$ = std::move($1);
  }
  | LPAREN parameter_list RPAREN {
    $$ = std::move($2);
  }

instance
  : IDENTIFIER LPAREN RPAREN IDENTIFIER NEWLINE {
    // No terminals, no parameters
    $$ = std::move(PTInstance(
        $1, 
        $4, 
        std::move(PTIdentifierList()),
        @1.loc()
    ));
  }
  | IDENTIFIER LPAREN terminal_list RPAREN IDENTIFIER NEWLINE {
    // Terminals, no parameters
    $$ = std::move(PTInstance(
        $1, 
        $5, 
        std::move($3), 
        @1.loc()
    ));
  }
  | IDENTIFIER LPAREN RPAREN IDENTIFIER opt_broken_parameter_list NEWLINE {
    // No terminals, parameters
    $$ = std::move(PTInstance(
        $1, 
        $4, 
        std::move(PTIdentifierList()),
        std::move($5.params), 
        @1.loc()
    ));
  }
  | IDENTIFIER LPAREN terminal_list RPAREN IDENTIFIER opt_broken_parameter_list NEWLINE {
    // Terminals, parameters
    $$ = std::move(PTInstance(
        $1, 
        $5, 
        std::move($3), 
        std::move($6.params), 
        @1.loc()
    ));
  }
  
model
  : MODEL IDENTIFIER IDENTIFIER NEWLINE {
    $$ = std::move(PTModel(
        $2, 
        $3, 
        @1.loc()
    ));
    $$.add(std::move(PTParameters()));
  }
  | MODEL IDENTIFIER IDENTIFIER opt_broken_parameter_list NEWLINE {
    $$ = std::move(PTModel(
        $2, 
        $3, 
        std::move($4.params), 
        @1.loc()
    ));
  }

subcktparameters
  : PARAMETERS opt_broken_parameter_list NEWLINE {
    $$ = std::move($2);
  }

embed
  : EMBED STRING STRING {
    $$ = std::move(PTEmbed(std::move($2), std::move($3), @1.loc())); 
  }

savestr
  : terminal {
    $$ = $1;
  }
  | STRING {
    $$ = $1; 
  }

savestrlist
  : savestr {
    $$.push_back(std::move($1));
  }
  | savestrlist COMMA savestr {
    $$ = std::move($1);
    $$.push_back(std::move($3));
  }

savecmd
  : IDENTIFIER { // LPAREN RPAREN {
    $$ = std::move(PTSave($1, @1.loc()));
  }
  | IDENTIFIER LPAREN savestrlist RPAREN {
    if ($3.size()>2) {
        status.set(Status::BadArguments, "Save directive accepts at most 2 arguments.");
        status.extend(@1.loc());
        YYERROR;
    } else if ($3.size()==2) {
        $$ = std::move(PTSave($1, $3[0], $3[1], @1.loc()));
    } else {
        $$ = std::move(PTSave($1, $3[0], @1.loc()));
    }
  }

savecmd_list 
  : savecmd {
    $$.push_back(std::move($1));
  }
  | savecmd_list savecmd {
    $$ = std::move($1);
    $$.push_back(std::move($2));
  }

saves
  : savecmd_list {
    $$ = std::move($1);
  }
  | LPAREN savecmd_list RPAREN {
    $$ = std::move($2);
  }

global
  : GLOBAL terminal_list NEWLINE {  
    $$ = std::move($2); 
  }
  | GLOBAL LPAREN terminal_list RPAREN NEWLINE {  
    $$ = std::move($3); 
  }

ground
  : GROUND terminal_list NEWLINE {  
    $$ = std::move($2); 
  }
  | GROUND LPAREN terminal_list RPAREN NEWLINE {  
    $$ = std::move($3); 
  }

load
  : LOAD STRING NEWLINE {
    $$ = std::move(PTLoad(std::move($2), @1.loc()));
  }
  | LOAD STRING opt_broken_parameter_list NEWLINE {
    if ($3.params.expressionCount()>0) {
      status.set(Status::BadArguments, "Only constant expressions are allowed here.");
      status.extend($3.params.expressions()[0].location());
      YYERROR;
    }
    $$ = std::move(PTLoad(std::move($2), std::move($3.params), @1.loc()));
  }
  
sweeps
  : SWEEP IDENTIFIER opt_broken_parameter_list {
    Id id = $2;
    $$.sweeps.push_back(PTSweep(id, std::move($3.params), @1.loc()));
    $$.locations.insert({id, @1});
  }
  | sweeps NEWLINE {
    $$ = std::move($1);
  }
  | sweeps SWEEP IDENTIFIER opt_broken_parameter_list {
    $$ = std::move($1);
    Id id = $3;
    auto [it, inserted] = $$.locations.insert({id, @2});
    if (!inserted) {
        status.set(Status::Redefinition, "Sweep does not have a unique name.");
        status.extend(@2.loc());
        status.extend("The name was first used here.");
        status.extend(it->second.loc());
        YYERROR;
    }
    $$.sweeps.push_back(PTSweep(id, std::move($4.params), @2.loc()));
  }

pre_analysis
  : ANALYSIS IDENTIFIER IDENTIFIER {
    $$ = std::move(PTAnalysis($2, $3, @1.loc()));
  }
  | sweeps ANALYSIS IDENTIFIER IDENTIFIER {
    $$ = std::move(PTAnalysis($3, $4, @2.loc()));
    $$.add(std::move($1.sweeps));
  }

analysis_with_params
  : pre_analysis opt_broken_parameter_list {
    $$ = std::move($1);
    $$.add(std::move($2.params));
  }

analysis
  : pre_analysis {
    $$ = std::move($1);
  }
  | analysis_with_params {
    $$ = std::move($1);
  }

keywords
  : IDENTIFIER {
    $$.push_back(PTParsedIdentifier(std::move($1), @1.loc())); 
  }
  | keywords IDENTIFIER {
    $$ = std::move($1);
    $$.push_back(PTParsedIdentifier(std::move($2), @2.loc())); 
  }

command
  : IDENTIFIER {
    $$ = std::move(PTCommand(@1.loc(), $1));
  }
  | IDENTIFIER keywords {
    $$ = std::move(PTCommand(@1.loc(), $1));
    $$.set(std::move($2));
  }
  | IDENTIFIER LPAREN exprlist RPAREN {
    $$ = std::move(PTCommand(@1.loc(), $1));
    $$.set(std::move($3));
  }
  | IDENTIFIER keywords LPAREN exprlist RPAREN {
    $$ = std::move(PTCommand(@1.loc(), $1));
    $$.set(std::move($2));
    $$.set(std::move($4));
  }
  | IDENTIFIER opt_broken_parameter_list {
    $$ = std::move(PTCommand(@1.loc(), $1));
    $$.set(std::move($2.params));
  }
  | IDENTIFIER keywords opt_broken_parameter_list {
    $$ = std::move(PTCommand(@1.loc(), $1));
    $$.set(std::move($2));
    $$.set(std::move($3.params));
  }
  | IDENTIFIER LPAREN exprlist RPAREN opt_broken_parameter_list {
    $$ = std::move(PTCommand(@1.loc(), $1));
    $$.set(std::move($3));
    $$.set(std::move($5.params));
  }
  | IDENTIFIER keywords LPAREN exprlist RPAREN opt_broken_parameter_list {
    $$ = std::move(PTCommand(@1.loc(), $1));
    $$.set(std::move($2));
    $$.set(std::move($4));
    $$.set(std::move($6.params));
  }

control_block_build
  : CONTROL NEWLINE {
  }
  | control_block_build NEWLINE {
  }
  | control_block_build SAVE saves NEWLINE {
    // This has to be defined separately because 
    // the syntax of save command is different 
    // from the rest of commands. 
    // Exception to the std::move() convention above: this is a local
    // initialization, not a move into a sink. The prvalue is left bare so
    // guaranteed copy elision constructs cmd in place; wrapping it in
    // std::move() would defeat the elision and force an extra move.
    auto cmd = PTCommand(@2.loc(), saveCmd);
    PTSaves s;
    s.add(std::move($3));
    cmd.set(std::move(s));
    tables.addCommand(std::move(cmd));
  }
  | control_block_build analysis NEWLINE {
    // Analysis also has a special syntax. 
    tables.addCommand(std::move($2));
  } 
  | control_block_build command NEWLINE {
    tables.addCommand(std::move($2));
  }
  
control_block
  : control_block_build ENDC NEWLINE {
  }

%%

namespace NAMESPACE::dflparse {

// Error reporting
void Parser::error( const Parser::location_type &l, const std::string &err_message ) {
   status.set(Status::Syntax, ("Parser "+err_message));
   status.extend(l.loc());
} 

}
