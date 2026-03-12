#include "parser/jass/jass_parser.h"

#include <charconv>
#include <cstdlib>
#include <memory>
#include <string>

namespace w3x_toolkit::parser::jass {

namespace {

// Helper to create expression nodes concisely.
ExprPtr MakeExpr(ExprKind kind, int32_t line, auto&& data) {
  auto e = std::make_shared<Expression>();
  e->kind = kind;
  e->line = line;
  e->data = std::forward<decltype(data)>(data);
  return e;
}

StmtPtr MakeStmt(StmtKind kind, int32_t line, auto&& data) {
  auto s = std::make_shared<Statement>();
  s->kind = kind;
  s->line = line;
  s->data = std::forward<decltype(data)>(data);
  return s;
}

// Recursive descent parser for JASS.
class Parser {
 public:
  explicit Parser(std::vector<Token> tokens) : tokens_(std::move(tokens)) {}

  core::Result<JassAst> Parse() {
    JassAst ast;
    SkipNl();
    while (!AtEnd()) {
      SkipNl();
      if (AtEnd()) break;
      if (Check(TokenType::kType)) {
        W3X_ASSIGN_OR_RETURN(auto td, ParseTypeDecl());
        ast.types.push_back(std::move(td));
      } else if (Check(TokenType::kGlobals)) {
        W3X_RETURN_IF_ERROR(ParseGlobals(ast.globals));
      } else if (Check(TokenType::kNative) || CheckCN()) {
        W3X_ASSIGN_OR_RETURN(auto nd, ParseNativeDecl());
        ast.natives.push_back(std::move(nd));
      } else if (Check(TokenType::kFunction) || CheckCF()) {
        W3X_ASSIGN_OR_RETURN(auto fd, ParseFunctionDecl());
        ast.functions.push_back(std::move(fd));
      } else {
        SkipLine(); SkipNl();
      }
    }
    return ast;
  }

 private:
  const Token& Cur() const { return tokens_[pos_]; }
  bool AtEnd() const { return pos_ >= tokens_.size() || Cur().type == TokenType::kEof; }
  bool Check(TokenType t) const { return !AtEnd() && Cur().type == t; }
  bool CheckCN() const { return Check(TokenType::kConstant) && pos_+1 < tokens_.size() && tokens_[pos_+1].type == TokenType::kNative; }
  bool CheckCF() const { return Check(TokenType::kConstant) && pos_+1 < tokens_.size() && tokens_[pos_+1].type == TokenType::kFunction; }
  Token Adv() { Token t = Cur(); if (!AtEnd()) ++pos_; return t; }
  bool Match(TokenType t) { if (Check(t)) { Adv(); return true; } return false; }
  core::Result<Token> Expect(TokenType t, const char* msg) {
    if (Check(t)) return Adv();
    return std::unexpected(core::Error::ParseError(std::string(msg) + " at line " + std::to_string(Cur().line)));
  }
  void SkipNl() { while (Check(TokenType::kNewline)) Adv(); }
  void SkipLine() { while (!AtEnd() && !Check(TokenType::kNewline)) Adv(); }

  core::Result<TypeDecl> ParseTypeDecl() {
    int32_t ln = Cur().line; Adv();
    W3X_ASSIGN_OR_RETURN(auto nm, Expect(TokenType::kIdentifier, "Expected type name"));
    W3X_ASSIGN_OR_RETURN(auto ex, Expect(TokenType::kExtends, "Expected 'extends'"));
    W3X_ASSIGN_OR_RETURN(auto bt, Expect(TokenType::kIdentifier, "Expected base type"));
    SkipLine();
    return TypeDecl{nm.lexeme, bt.lexeme, ln};
  }

  core::Result<void> ParseGlobals(std::vector<GlobalDecl>& out) {
    Adv(); SkipNl();
    while (!AtEnd() && !Check(TokenType::kEndglobals)) {
      SkipNl();
      if (Check(TokenType::kEndglobals)) break;
      bool cst = Match(TokenType::kConstant);
      if (!Check(TokenType::kIdentifier)) { SkipLine(); SkipNl(); continue; }
      auto tt = Adv(); bool arr = Match(TokenType::kArray);
      if (!Check(TokenType::kIdentifier)) { SkipLine(); SkipNl(); continue; }
      auto nt = Adv(); ExprPtr init;
      if (Match(TokenType::kAssign)) { auto e = ParseExpr(); if (e.has_value()) init = std::move(*e); }
      out.push_back({cst, tt.lexeme, arr, nt.lexeme, init, tt.line});
      SkipLine(); SkipNl();
    }
    Match(TokenType::kEndglobals); SkipLine();
    return {};
  }

  std::vector<Parameter> ParseParams() {
    std::vector<Parameter> p;
    while (Check(TokenType::kIdentifier)) {
      auto tt = Adv();
      if (!Check(TokenType::kIdentifier)) break;
      auto nt = Adv();
      p.push_back({tt.lexeme, nt.lexeme});
      if (!Match(TokenType::kComma)) break;
    }
    return p;
  }

  core::Result<std::string> ParseTakesReturns(std::vector<Parameter>& params) {
    W3X_ASSIGN_OR_RETURN(auto tk, Expect(TokenType::kTakes, "Expected 'takes'"));
    if (!Check(TokenType::kNothing)) params = ParseParams(); else Adv();
    W3X_ASSIGN_OR_RETURN(auto rt, Expect(TokenType::kReturns, "Expected 'returns'"));
    std::string ret = "nothing";
    if (Check(TokenType::kNothing)) Adv();
    else if (Check(TokenType::kIdentifier)) ret = Adv().lexeme;
    return ret;
  }

  core::Result<NativeDecl> ParseNativeDecl() {
    bool cst = Match(TokenType::kConstant); int32_t ln = Cur().line; Adv();
    W3X_ASSIGN_OR_RETURN(auto nm, Expect(TokenType::kIdentifier, "Expected native name"));
    std::vector<Parameter> params;
    W3X_ASSIGN_OR_RETURN(auto ret, ParseTakesReturns(params));
    SkipLine();
    return NativeDecl{cst, nm.lexeme, std::move(params), ret, ln};
  }

  core::Result<FunctionDecl> ParseFunctionDecl() {
    bool cst = Match(TokenType::kConstant); int32_t ln = Cur().line; Adv();
    W3X_ASSIGN_OR_RETURN(auto nm, Expect(TokenType::kIdentifier, "Expected function name"));
    std::vector<Parameter> params;
    W3X_ASSIGN_OR_RETURN(auto ret, ParseTakesReturns(params));
    SkipLine(); SkipNl();
    std::vector<LocalDecl> locals;
    while (Check(TokenType::kLocal)) {
      auto ld = ParseLocal(); if (ld.has_value()) locals.push_back(std::move(*ld));
      SkipLine(); SkipNl();
    }
    auto body = ParseBlock();
    Match(TokenType::kEndfunction); SkipLine();
    return FunctionDecl{cst, nm.lexeme, std::move(params), ret, std::move(locals), std::move(body), ln};
  }

  core::Result<LocalDecl> ParseLocal() {
    int32_t ln = Cur().line; Adv();
    if (!Check(TokenType::kIdentifier))
      return std::unexpected(core::Error::ParseError("Expected type at line " + std::to_string(ln)));
    auto tt = Adv(); bool arr = Match(TokenType::kArray);
    W3X_ASSIGN_OR_RETURN(auto nt, Expect(TokenType::kIdentifier, "Expected local name"));
    ExprPtr init;
    if (Match(TokenType::kAssign)) { auto e = ParseExpr(); if (e.has_value()) init = std::move(*e); }
    return LocalDecl{tt.lexeme, arr, nt.lexeme, init, ln};
  }

  // Statements
  core::Result<StmtPtr> ParseStmt() {
    if (Check(TokenType::kCall) || Check(TokenType::kDebug)) return ParseCallStmt();
    if (Check(TokenType::kSet)) return ParseSetStmt();
    if (Check(TokenType::kReturn)) return ParseReturnStmt();
    if (Check(TokenType::kExitwhen)) return ParseExitwhenStmt();
    if (Check(TokenType::kIf)) return ParseIfStmt();
    if (Check(TokenType::kLoop)) return ParseLoopStmt();
    SkipLine(); return StmtPtr{nullptr};
  }

  core::Result<StmtPtr> ParseCallStmt() {
    int32_t ln = Cur().line;
    if (Check(TokenType::kDebug)) Adv();
    Adv();
    if (!Check(TokenType::kIdentifier)) { SkipLine(); return StmtPtr{nullptr}; }
    auto name = Adv().lexeme; Match(TokenType::kLeftParen);
    auto args = ParseArgList(); Match(TokenType::kRightParen);
    return MakeStmt(StmtKind::kCall, ln, CallStmt{std::move(name), std::move(args)});
  }

  core::Result<StmtPtr> ParseSetStmt() {
    int32_t ln = Cur().line; Adv();
    if (!Check(TokenType::kIdentifier)) { SkipLine(); return StmtPtr{nullptr}; }
    auto name = Adv().lexeme;
    if (Match(TokenType::kLeftBracket)) {
      auto idx = OptExpr(); Match(TokenType::kRightBracket); Match(TokenType::kAssign);
      auto val = OptExpr();
      return MakeStmt(StmtKind::kSetArray, ln, SetArrayStmt{std::move(name), idx, val});
    }
    Match(TokenType::kAssign); auto val = OptExpr();
    return MakeStmt(StmtKind::kSet, ln, SetStmt{std::move(name), val});
  }

  core::Result<StmtPtr> ParseReturnStmt() {
    int32_t ln = Cur().line; Adv(); ExprPtr v;
    if (!Check(TokenType::kNewline) && !AtEnd()) v = OptExpr();
    return MakeStmt(StmtKind::kReturn, ln, ReturnStmt{v});
  }

  core::Result<StmtPtr> ParseExitwhenStmt() {
    int32_t ln = Cur().line; Adv();
    return MakeStmt(StmtKind::kExitwhen, ln, ExitwhenStmt{OptExpr()});
  }

  core::Result<StmtPtr> ParseIfStmt() {
    int32_t ln = Cur().line; Adv();
    auto cond = OptExpr(); Match(TokenType::kThen); SkipLine(); SkipNl();
    IfStmt is; is.if_block = {cond, ParseBlock()};
    while (Check(TokenType::kElseif)) {
      Adv(); auto ec = OptExpr(); Match(TokenType::kThen); SkipLine(); SkipNl();
      is.elseif_blocks.push_back({ec, ParseBlock()});
    }
    if (Check(TokenType::kElse)) { Adv(); SkipLine(); SkipNl(); is.else_block = IfBlock{{}, ParseBlock()}; }
    Match(TokenType::kEndif);
    return MakeStmt(StmtKind::kIf, ln, std::move(is));
  }

  core::Result<StmtPtr> ParseLoopStmt() {
    int32_t ln = Cur().line; Adv(); SkipLine(); SkipNl();
    auto body = ParseBlock(); Match(TokenType::kEndloop);
    return MakeStmt(StmtKind::kLoop, ln, LoopStmt{std::move(body)});
  }

  bool IsBlockEnd() const {
    return AtEnd() || Check(TokenType::kEndif) || Check(TokenType::kElseif) ||
           Check(TokenType::kElse) || Check(TokenType::kEndloop) || Check(TokenType::kEndfunction);
  }

  std::vector<StmtPtr> ParseBlock() {
    std::vector<StmtPtr> stmts;
    while (!IsBlockEnd()) {
      SkipNl(); if (IsBlockEnd()) break;
      auto s = ParseStmt();
      if (s.has_value() && s.value()) stmts.push_back(std::move(*s));
      SkipLine(); SkipNl();
    }
    return stmts;
  }

  ExprPtr OptExpr() { auto e = ParseExpr(); return e.has_value() ? std::move(*e) : nullptr; }

  std::vector<ExprPtr> ParseArgList() {
    std::vector<ExprPtr> args;
    if (Check(TokenType::kRightParen) || AtEnd()) return args;
    if (auto e = ParseExpr(); e.has_value()) args.push_back(std::move(*e));
    while (Match(TokenType::kComma))
      if (auto e = ParseExpr(); e.has_value()) args.push_back(std::move(*e));
    return args;
  }

  // Expression parsing with precedence climbing.
  core::Result<ExprPtr> ParseExpr() { return ParseOr(); }

  core::Result<ExprPtr> ParseOr() {
    W3X_ASSIGN_OR_RETURN(auto l, ParseAnd());
    while (Check(TokenType::kOr)) { int32_t ln = Cur().line; Adv(); W3X_ASSIGN_OR_RETURN(auto r, ParseAnd()); l = MakeBin("or", std::move(l), std::move(r), ln); }
    return l;
  }
  core::Result<ExprPtr> ParseAnd() {
    W3X_ASSIGN_OR_RETURN(auto l, ParseCmp());
    while (Check(TokenType::kAnd)) { int32_t ln = Cur().line; Adv(); W3X_ASSIGN_OR_RETURN(auto r, ParseCmp()); l = MakeBin("and", std::move(l), std::move(r), ln); }
    return l;
  }
  core::Result<ExprPtr> ParseCmp() {
    W3X_ASSIGN_OR_RETURN(auto l, ParseAdd());
    while (Check(TokenType::kEqual)||Check(TokenType::kNotEqual)||Check(TokenType::kLess)||Check(TokenType::kLessEqual)||Check(TokenType::kGreater)||Check(TokenType::kGreaterEqual)) {
      auto op = Adv(); W3X_ASSIGN_OR_RETURN(auto r, ParseAdd()); l = MakeBin(op.lexeme, std::move(l), std::move(r), op.line);
    }
    return l;
  }
  core::Result<ExprPtr> ParseAdd() {
    W3X_ASSIGN_OR_RETURN(auto l, ParseMul());
    while (Check(TokenType::kPlus)||Check(TokenType::kMinus)) {
      auto op = Adv(); W3X_ASSIGN_OR_RETURN(auto r, ParseMul()); l = MakeBin(op.lexeme, std::move(l), std::move(r), op.line);
    }
    return l;
  }
  core::Result<ExprPtr> ParseMul() {
    W3X_ASSIGN_OR_RETURN(auto l, ParseUnary());
    while (Check(TokenType::kStar)||Check(TokenType::kSlash)||Check(TokenType::kPercent)) {
      auto op = Adv(); W3X_ASSIGN_OR_RETURN(auto r, ParseUnary()); l = MakeBin(op.lexeme, std::move(l), std::move(r), op.line);
    }
    return l;
  }
  core::Result<ExprPtr> ParseUnary() {
    if (Check(TokenType::kNot)) { int32_t ln = Cur().line; Adv(); W3X_ASSIGN_OR_RETURN(auto o, ParseUnary()); return MakeExpr(ExprKind::kUnary, ln, UnaryExpr{"not", std::move(o)}); }
    if (Check(TokenType::kMinus)) { int32_t ln = Cur().line; Adv(); W3X_ASSIGN_OR_RETURN(auto o, ParseUnary()); return MakeExpr(ExprKind::kUnary, ln, UnaryExpr{"-", std::move(o)}); }
    return ParsePrimary();
  }

  core::Result<ExprPtr> ParsePrimary() {
    int32_t ln = Cur().line;
    if (Match(TokenType::kLeftParen)) { auto inner = ParseExpr(); Match(TokenType::kRightParen); return inner; }
    if (Check(TokenType::kFunction)) { Adv(); if (Check(TokenType::kIdentifier)) { auto n = Adv().lexeme; return MakeExpr(ExprKind::kFunctionRef, ln, FunctionRefExpr{std::move(n)}); } }
    if (Match(TokenType::kNull)) return MakeExpr(ExprKind::kNullLiteral, ln, NullLiteralExpr{});
    if (Match(TokenType::kTrue)) return MakeExpr(ExprKind::kBoolLiteral, ln, BoolLiteralExpr{true});
    if (Match(TokenType::kFalse)) return MakeExpr(ExprKind::kBoolLiteral, ln, BoolLiteralExpr{false});
    if (Check(TokenType::kIntegerLiteral)) {
      auto t = Adv(); int32_t v = 0;
      auto [p, ec] = std::from_chars(t.lexeme.data(), t.lexeme.data()+t.lexeme.size(), v, 10);
      if (ec != std::errc{} && t.lexeme.size()>2 && (t.lexeme[1]=='x'||t.lexeme[1]=='X'))
        std::from_chars(t.lexeme.data()+2, t.lexeme.data()+t.lexeme.size(), v, 16);
      return MakeExpr(ExprKind::kIntegerLiteral, ln, IntegerLiteralExpr{v});
    }
    if (Check(TokenType::kRealLiteral)) { auto t = Adv(); return MakeExpr(ExprKind::kRealLiteral, ln, RealLiteralExpr{t.lexeme}); }
    if (Check(TokenType::kStringLiteral)) { auto t = Adv(); return MakeExpr(ExprKind::kStringLiteral, ln, StringLiteralExpr{t.lexeme}); }
    if (Check(TokenType::kIdentifier)) {
      auto t = Adv();
      if (Check(TokenType::kLeftParen)) { Adv(); auto a = ParseArgList(); Match(TokenType::kRightParen); return MakeExpr(ExprKind::kFunctionCall, ln, FunctionCallExpr{t.lexeme, std::move(a)}); }
      if (Check(TokenType::kLeftBracket)) { Adv(); auto i = OptExpr(); Match(TokenType::kRightBracket); return MakeExpr(ExprKind::kArrayAccess, ln, ArrayAccessExpr{t.lexeme, i}); }
      return MakeExpr(ExprKind::kVariable, ln, VariableExpr{t.lexeme});
    }
    return std::unexpected(core::Error::ParseError("Unexpected token '" + Cur().lexeme + "' at line " + std::to_string(Cur().line)));
  }

  ExprPtr MakeBin(const std::string& op, ExprPtr l, ExprPtr r, int32_t ln) {
    return MakeExpr(ExprKind::kBinary, ln, BinaryExpr{op, std::move(l), std::move(r)});
  }

  std::vector<Token> tokens_;
  size_t pos_ = 0;
};

}  // namespace

core::Result<JassAst> ParseJass(std::string_view source) {
  W3X_ASSIGN_OR_RETURN(auto tokens, Tokenize(source));
  Parser parser(std::move(tokens));
  return parser.Parse();
}

}  // namespace w3x_toolkit::parser::jass
