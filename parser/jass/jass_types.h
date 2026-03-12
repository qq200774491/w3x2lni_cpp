#ifndef W3X_TOOLKIT_PARSER_JASS_JASS_TYPES_H_
#define W3X_TOOLKIT_PARSER_JASS_JASS_TYPES_H_

#include <cstdint>
#include <memory>
#include <optional>
#include <string>
#include <variant>
#include <vector>

namespace w3x_toolkit::parser::jass {

// ============================================================================
// Tokens
// ============================================================================

enum class TokenType : uint8_t {
  // Literals
  kIntegerLiteral,
  kRealLiteral,
  kStringLiteral,
  kIdentifier,

  // Keywords
  kFunction,
  kEndfunction,
  kIf,
  kThen,
  kElse,
  kElseif,
  kEndif,
  kLoop,
  kEndloop,
  kExitwhen,
  kReturn,
  kCall,
  kSet,
  kLocal,
  kType,
  kExtends,
  kGlobals,
  kEndglobals,
  kNative,
  kConstant,
  kTakes,
  kReturns,
  kNothing,
  kArray,
  kNot,
  kAnd,
  kOr,
  kNull,
  kTrue,
  kFalse,
  kDebug,

  // Operators and punctuation
  kPlus,
  kMinus,
  kStar,
  kSlash,
  kPercent,
  kEqual,       // ==
  kNotEqual,    // !=
  kLess,
  kLessEqual,
  kGreater,
  kGreaterEqual,
  kAssign,      // =
  kLeftParen,
  kRightParen,
  kLeftBracket,
  kRightBracket,
  kComma,

  // Special
  kNewline,
  kEof,
};

struct Token {
  TokenType type = TokenType::kEof;
  std::string lexeme;
  int32_t line = 0;
  int32_t column = 0;
};

// ============================================================================
// AST node types
// ============================================================================

// Forward declarations for all AST nodes.
struct Expression;
struct Statement;

// An expression is represented as a variant-based tagged union.
// Using shared_ptr for recursive data structures.
using ExprPtr = std::shared_ptr<Expression>;
using StmtPtr = std::shared_ptr<Statement>;

// A function parameter.
struct Parameter {
  std::string type_name;
  std::string name;
};

// Type declaration: type <name> extends <base>
struct TypeDecl {
  std::string name;
  std::string base_type;
  int32_t line = 0;
};

// Global variable declaration.
struct GlobalDecl {
  bool is_constant = false;
  std::string type_name;
  bool is_array = false;
  std::string name;
  ExprPtr initializer;  // may be null
  int32_t line = 0;
};

// Native function declaration.
struct NativeDecl {
  bool is_constant = false;
  std::string name;
  std::vector<Parameter> parameters;  // empty if "takes nothing"
  std::string return_type;            // "nothing" if void
  int32_t line = 0;
};

// Local variable declaration inside a function.
struct LocalDecl {
  std::string type_name;
  bool is_array = false;
  std::string name;
  ExprPtr initializer;  // may be null
  int32_t line = 0;
};

// ============================================================================
// Expressions
// ============================================================================

enum class ExprKind : uint8_t {
  kIntegerLiteral,
  kRealLiteral,
  kStringLiteral,
  kBoolLiteral,
  kNullLiteral,
  kVariable,
  kArrayAccess,
  kFunctionCall,
  kFunctionRef,
  kUnary,
  kBinary,
};

struct IntegerLiteralExpr {
  int32_t value = 0;
};

struct RealLiteralExpr {
  std::string text;  // keep original text to avoid precision issues
};

struct StringLiteralExpr {
  std::string value;
};

struct BoolLiteralExpr {
  bool value = false;
};

struct NullLiteralExpr {};

struct VariableExpr {
  std::string name;
};

struct ArrayAccessExpr {
  std::string name;
  ExprPtr index;
};

struct FunctionCallExpr {
  std::string name;
  std::vector<ExprPtr> arguments;
};

struct FunctionRefExpr {
  std::string name;
};

struct UnaryExpr {
  std::string op;  // "not" or "-"
  ExprPtr operand;
};

struct BinaryExpr {
  std::string op;  // "+", "-", "*", "/", "%", "and", "or", ==, !=, <, <=, >, >=
  ExprPtr left;
  ExprPtr right;
};

struct Expression {
  ExprKind kind;
  int32_t line = 0;
  std::variant<
      IntegerLiteralExpr,
      RealLiteralExpr,
      StringLiteralExpr,
      BoolLiteralExpr,
      NullLiteralExpr,
      VariableExpr,
      ArrayAccessExpr,
      FunctionCallExpr,
      FunctionRefExpr,
      UnaryExpr,
      BinaryExpr> data;
};

// ============================================================================
// Statements
// ============================================================================

enum class StmtKind : uint8_t {
  kSet,
  kSetArray,
  kCall,
  kIf,
  kLoop,
  kReturn,
  kExitwhen,
};

struct SetStmt {
  std::string name;
  ExprPtr value;
};

struct SetArrayStmt {
  std::string name;
  ExprPtr index;
  ExprPtr value;
};

struct CallStmt {
  std::string name;
  std::vector<ExprPtr> arguments;
};

struct IfBlock {
  ExprPtr condition;  // null for 'else' block
  std::vector<StmtPtr> body;
};

struct IfStmt {
  IfBlock if_block;
  std::vector<IfBlock> elseif_blocks;
  std::optional<IfBlock> else_block;
};

struct LoopStmt {
  std::vector<StmtPtr> body;
};

struct ReturnStmt {
  ExprPtr value;  // may be null for bare return
};

struct ExitwhenStmt {
  ExprPtr condition;
};

struct Statement {
  StmtKind kind;
  int32_t line = 0;
  std::variant<
      SetStmt,
      SetArrayStmt,
      CallStmt,
      IfStmt,
      LoopStmt,
      ReturnStmt,
      ExitwhenStmt> data;
};

// ============================================================================
// Function declaration (with body)
// ============================================================================

struct FunctionDecl {
  bool is_constant = false;
  std::string name;
  std::vector<Parameter> parameters;
  std::string return_type;  // "nothing" if void
  std::vector<LocalDecl> locals;
  std::vector<StmtPtr> body;
  int32_t line = 0;
};

// ============================================================================
// Top-level program AST
// ============================================================================

struct JassAst {
  std::vector<TypeDecl> types;
  std::vector<GlobalDecl> globals;
  std::vector<NativeDecl> natives;
  std::vector<FunctionDecl> functions;
};

}  // namespace w3x_toolkit::parser::jass

#endif  // W3X_TOOLKIT_PARSER_JASS_JASS_TYPES_H_
