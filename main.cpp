#include <cctype>
#include <deque>
#include <fstream>
#include <iostream>
#include <memory>
#include <sstream>
#include <string>
#include <unordered_map>
#include <vector>

// Goal:
// Create a very very basic and somewhat crappy DSL for plotting ascii graphs.

enum struct TokenType {
  Integer,
  String,
  Identifier,
  Semi,
  LParen,
  RParen,
  Comma,
};

struct Token {
  std::string value;
  TokenType type;
  std::string ToString() const {
    return "Token: (" + value + ") :: " + TypeToString(type);
  }
  static std::string TypeToString(const TokenType &type) {
    switch (type) {
    case TokenType::Integer:
      return "Integer";
    case TokenType::String:
      return "String";
    case TokenType::Identifier:
      return "Identifier";
    case TokenType::Semi:
      return "Semi";
    case TokenType::LParen:
      return "LParen";
    case TokenType::RParen:
      return "RParen";
    case TokenType::Comma:
      return "Comma";
    }
  }
};

std::deque<Token> lex(const std::string &input) {
  std::deque<Token> tokens;

  // where are we in the input string?
  size_t pos = 0;

  while (pos < input.length()) {
    char c = input[pos];

    if (c == '\n' || c == ' ' || c == '\t') {
      pos++;
      continue;
    }

    if (c == '\"') {
      pos++; // ignore the opening quote.
      c = input[pos];

      std::stringstream ss;
      while (pos < input.length() && c != '\"') {
        ss << c;
        pos++;
        c = input[pos];
      }

      pos++; // ignore the closing quote;
      tokens.push_back({ss.str(), TokenType::String});
    } else if (std::isalpha(c) || c == '_') {
      std::stringstream ss;
      while (pos < input.length() && (std::isalpha(c) || c == '_')) {
        ss << c;
        pos++;
        c = input[pos];
      }
      tokens.push_back({ss.str(), TokenType::Identifier});
      // Numbers, just ints.
    } else if (std::isdigit(c)) {
      std::stringstream ss;
      while (pos < input.length() && std::isdigit(c)) {
        ss << c;
        pos++;
        c = input[pos];
      }
      tokens.push_back({ss.str(), TokenType::Integer});
      // Operators, ( ) ,
    } else if (std::ispunct(c)) {
      if (c == '(') {
        pos++;
        tokens.push_back({"(", TokenType::LParen});
      } else if (c == ')') {
        pos++;
        tokens.push_back({")", TokenType::RParen});
      } else if (c == ',') {
        pos++;
        tokens.push_back({",", TokenType::Comma});
      } else if (c == ';') {
        pos++;
        tokens.push_back({";", TokenType::Semi});
      } else {
        std::cerr << "Invalid operator! : " << c << std::endl;
        exit(1);
      }
    } else {
      std::cerr << "Unrecognized character: " << c << std::endl;
      exit(1);
    }
  }

  return tokens;
}

template <typename T> using ptr = std::unique_ptr<T>;
template <typename T> using sptr = std::shared_ptr<T>;

struct Value {
  virtual ~Value() {}
};

struct Vec2 : Value {
  Vec2(int x, int y) : x(x), y(y) {}
  int x, y;
};
struct String : Value {
  String(std::string value) : value(std::move(value)) {}
  std::string value;
};
struct Integer : Value {
  explicit Integer(int value) : value(value) {}
  int value;
};
struct Color : Value {
  explicit Color(std::string value) : value(std::move(value)) {}
  std::string value;
};
struct Curve : Value {
  enum struct Type { Linear, Exponential } type;
  explicit Curve(Curve::Type type) : type(type) {}
};

struct Node {
  virtual ~Node() {}
  virtual sptr<Value> evaluate() = 0;
  virtual std::string ToString() const = 0;
};

struct Program : Node {
  Program() {}
  std::vector<ptr<Node>> statements;
  sptr<Value> evaluate() override {
    for (auto &statement : statements) {
      statement->evaluate();
    }
    return {};
  }
  std::string ToString() const override {
    std::stringstream ss;
    for (const auto &statement : statements) {
      ss << statement->ToString() << "\n";
    }
    return ss.str();
  }
};

struct Expression : Node {
  virtual sptr<Value> evaluate() = 0;
};

auto ___ = "set_size(40, 40);\n" // char width and height of the ascii graph.
           "set_background(\"#\", BLACK);\n"
           "set_color(RED);\n"
           "plot(\"c\", (0, 0), (10, 10), LINEAR);\n"
           "plot(\"r\", (2, 15), (25, 40), EXPONENTIAL);\n";

struct Plotter;

using Args = std::vector<sptr<Value>> &;

sptr<Value> plot(Plotter &plotter, Args args);
sptr<Value> set_size(Plotter &plotter, Args args);
sptr<Value> set_background(Plotter &plotter, Args args);
sptr<Value> set_color(Plotter &plotter, Args args);

struct Plotter {
  std::string color = "";
  std::string bg_color = "\e[0;30m";
  char bg_char = ' ';
  std::vector<std::vector<char>> buffer;

  using Function = sptr<Value> (*)(Plotter &, Args);

  const std::unordered_map<std::string, Function> functions = {
      {"plot", Function(&plot)},
      {"set_size", Function(&set_size)},
      {"set_background", Function(&set_background)},
      {"set_color", Function(&set_color)},
  };
};

static Plotter plotter = {};

sptr<Value> plot(Plotter &plotter, Args args) {
  // String s, Vec2 begin, Vec2 end, Curve curve

  auto character = std::dynamic_pointer_cast<String>(args[0])->value;
  auto start = std::dynamic_pointer_cast<Vec2>(args[1]);
  auto end = std::dynamic_pointer_cast<Vec2>(args[2]);
  auto curve = std::dynamic_pointer_cast<Curve>(args[3])->type;

  const auto linear = [&]() {
    // y intercept for a linear plot should be half of the height of the graph?
    auto b = 0; // plotter.buffer.size() / 2;

    auto run = end->x - start->x;
    auto rise = end->y - start->y;

    if (
      run == 0 && rise == 0 ||
      plotter.buffer.size() == 0 ||
      plotter.buffer[0].size() == 0
    ) {
      return;
    }

    auto y_size = plotter.buffer.size();
    auto x_size = plotter.buffer[0].size();

    if (std::abs(rise) > std::abs(run)) {
      float slope = float(run) / float(rise);
      float x_intercept = float(start->x)  - (slope * float(start->y));
      auto start_y = std::min(start->y, end->y);
      auto end_y = std::max(start->y, end->y);
      for (int y = start_y; y < end_y; ++y) {
        int x = slope * (float)y + x_intercept;
        if (y < 0 || y >= y_size || x < 0 || x >= x_size) {
          continue;
        }
        plotter.buffer[y][x] = character[0];
      }
    } else {
      float slope = float(rise) / float(run);
      float y_intercept = float(start->y) - (slope * float(start->x));
      auto start_x = std::min(start->x, end->x);
      auto end_x = std::max(start->x, end->x);
      for (int x = start_x; x < end_x; ++x) {
        int y = slope * (float)x + y_intercept;
        if (y < 0 || y >= y_size || x < 0 || x >= x_size) {
          continue;
        }
        plotter.buffer[y][x] = character[0];
      }
    }

  };

  const auto exponential = [&]() {
    // y intercept for a linear plot should be half of the height of the graph?
    auto b = 0; // plotter.buffer.size() / 2;

    auto slope = (end->x - start->x) / (end->y - start->y);

    for (int y = start->y; y < end->y; ++y) {
      for (int x = start->x; x < end->x; ++x) {
        auto y_value = (slope * x + b);
        y_value *= y_value;
        if (y_value == y)
          plotter.buffer[y][x] = character[0];
      }
    }
  };

  if (curve == Curve::Type::Linear) {
    linear();
  } else {
    exponential();
  }

  return {};
}
sptr<Value> set_size(Plotter &plotter, Args args) {
  plotter.buffer.clear();
  auto size_x = std::dynamic_pointer_cast<Integer>(args[0])->value;
  auto size_y = std::dynamic_pointer_cast<Integer>(args[1])->value;
  for (auto y = 0; y < size_y; ++y) {
    std::vector<char> x_buf;
    for (auto x = 0; x < size_x; ++x) {
      x_buf.push_back(plotter.bg_char);
    }
    plotter.buffer.push_back(x_buf);
  }

  return {};
}
sptr<Value> set_background(Plotter &plotter, Args args) {
  auto character = dynamic_pointer_cast<String>(args[0]);
  auto color = dynamic_pointer_cast<Color>(args[1]);
  plotter.bg_char = character->value[0];
  plotter.bg_color = color->value;
  return {};
}
sptr<Value> set_color(Plotter &plotter, Args args) {
  // Color color
  auto color = dynamic_cast<Color *>(args[0].get());
  plotter.color = color->value;
  return {};
}

struct Call : Expression {
  Call(const std::string &function_name, std::vector<ptr<Node>> &&arguments)
      : function_name(std::move(function_name)),
        arguments(std::move(arguments)) {}

  std::string function_name;
  std::vector<ptr<Node>> arguments;
  sptr<Value> evaluate() override {
    Plotter::Function func;
    if (plotter.functions.contains(function_name)) {
      func = plotter.functions.at(function_name);
    } else {
      std::cerr << "Use of undeclared function: " << function_name << std::endl;
      exit(1);
    }
    std::vector<sptr<Value>> args;
    for (const auto &arg : arguments) {
      args.push_back(arg->evaluate());
    }
    return func(plotter, args);
  }
  std::string ToString() const override {
    std::stringstream ss;
    ss << "Function Call: " << function_name << " ";
    ss << "(";
    for (const auto &arg : arguments) {
      ss << arg->ToString();
      if (arg != arguments.back())
        ss << ", ";
    }
    ss << ")";
    return ss.str();
  }
};
struct Identifier : Expression {
  std::string value;
  explicit Identifier(std::string value) : value(std::move(value)) {}

  // TODO: perform a lookup for constants!
  // We'll need a Color value type.
  // We'll need a Curve value type.
  sptr<Value> evaluate() override {
    static std::unordered_map<std::string, sptr<Value>> symbols{
        // COLORS
        {"BLACK", std::make_shared<Color>("\e[0;30m")},
        {"WHITE", std::make_shared<Color>("\e[1;37m")},
        {"RED", std::make_shared<Color>("\e[0;31m")},
        {"GREEN", std::make_shared<Color>("\e[0;32m")},

        // CURVES
        {"EXPONENTIAL", std::make_shared<Curve>(Curve::Type::Exponential)},
        {"LINEAR", std::make_shared<Curve>(Curve::Type::Linear)},
    };
    if (symbols.contains(value)) {
      return symbols[value];
    }
    std::cerr << "Use of undeclared symbol :: " << value << std::endl;
    exit(1);
  }
  std::string ToString() const override { return "Identifier: " + value; }
};
struct StringNode : Expression {
  std::string value;
  explicit StringNode(std::string value) : value(std::move(value)) {}
  sptr<Value> evaluate() override { return std::make_shared<String>(value); }
  std::string ToString() const override { return "String: " + value; }
};
struct IntegerNode : Expression {
  int value;
  explicit IntegerNode(const std::string &value) : value(std::stoi(value)) {}
  sptr<Value> evaluate() override { return std::make_shared<Integer>(value); }
  std::string ToString() const override {
    return "Integer: " + std::to_string(value);
  }
};

struct Vector2Node : Expression {
  Vector2Node(const std::string &x, const std::string &y)
      : x(std::stoi(x)), y(std::stoi(y)) {}
  int x, y;
  sptr<Value> evaluate() override { return std::make_shared<Vec2>(x, y); }

  std::string ToString() const override {
    return "Vector2: (x: " + std::to_string(x) + ", y: " + std::to_string(y) +
           ")";
  }
};

struct Parser {
  std::deque<Token> tokens;
  Parser(std::deque<Token> &&tokens) : tokens(tokens) {}

  Token expect(TokenType ttype) {
    if (tokens.front().type != ttype) {
      std::cerr << "Unexpected token: " << Token::TypeToString(ttype)
                << std::endl;
      exit(1);
    }
    auto tok = tokens.front();
    tokens.pop_front();
    return tok;
  }
  Token peek() const { return tokens.front(); }
  bool next_is(TokenType ttype) const { return peek().type == ttype; }
  Token eat() {
    auto tok = tokens.front();
    tokens.pop_front();
    return tok;
  }

  // return a program with child nodes.
  ptr<Program> parse() {
    auto program = std::make_unique<Program>();
    while (tokens.size() > 0) {
      program->statements.push_back(parse_statement());
    }
    return program;
  }
  ptr<Node> parse_statement() {
    // we always read our tokens & pop tokens from the FRONT of our deque.
    auto next = tokens.front();

    switch (next.type) {
    case TokenType::Identifier: {
      auto call = parse_call();
      expect(TokenType::Semi);
      return call;
    }
    default: {
      std::cout << "Unexpected token " << next.ToString() << std::endl;
      exit(1);
    }
    }
  }
  ptr<Call> parse_call() {
    auto name = expect(TokenType::Identifier).value;
    expect(TokenType::LParen);
    std::vector<ptr<Node>> args;
    while (!tokens.empty() && !next_is(TokenType::RParen)) {
      args.push_back(parse_expr());
      if (next_is(TokenType::Comma)) {
        eat();
      }
    }
    expect(TokenType::RParen);
    return std::make_unique<Call>(name, std::move(args));
  }
  ptr<Expression> parse_expr() {
    auto next = peek();
    switch (next.type) {
    case TokenType::LParen: {
      eat(); // eat LParen.
      auto x = expect(TokenType::Integer).value;
      expect(TokenType::Comma);
      auto y = expect(TokenType::Integer).value;
      expect(TokenType::RParen);
      return std::make_unique<Vector2Node>(x, y);
    }
    case TokenType::Integer:
      eat();
      return std::make_unique<IntegerNode>(next.value);
    case TokenType::String:
      eat();
      return std::make_unique<StringNode>(next.value);
    case TokenType::Identifier:
      eat();
      return std::make_unique<Identifier>(next.value);
    case TokenType::Semi:
    case TokenType::RParen:
    case TokenType::Comma:
      std::cerr << "Unexpected token!: " << next.ToString() << std::endl;
      exit(1);
    }
  }
};

void draw_plotter() {
  for (auto it = plotter.buffer.rbegin(); it != plotter.buffer.rend(); ++it) {
    auto column = *it;
    for (const auto &character : column) {
      if (character == plotter.bg_char) {
        std::cout << plotter.bg_color << character << "\e[0m";
      } else {
        std::cout << plotter.color << character << "\e[0m";
      }
    }
    std::cout << "\n";
  }
}
int main(int argc, char *argv[]) {

  std::string input;

begin:
  while (true) {
    std::cout << "\e[1;32m";
    std::cout << "[help] for available options, [exec] to run." << std::endl;

    std::cout << "\e[1;35m";
    std::string s;
    std::getline(std::cin, s);
    std::cout << "\e[0m";

    if (s == "pop") {
      size_t last_pos = input.find_last_not_of('\n');
      if (last_pos != std::string::npos) {
        size_t pos = input.find_last_of('\n', last_pos);
        if (pos != std::string::npos) {
          input.erase(pos + 1);
        } else {
          input.clear(); // If no newline is found, clear the entire string
        }
      } else {
        input.clear(); // If the string is all newlines, clear the entire string
      }
      continue;
    }

    if (s == "clear") {
      input.clear();
      goto begin;
    }

    if (s == "preview") {
      std::cout << input << std::endl;
      std::cout << "enter something to continue\n";
      std::string s;
      std::getline(std::cin, s);
      goto end;
    }

    if (s == "help") {
      std::cout << "\033[2J\033[H";
      std::cout << "Repl functions: [pop]: remove last line "
                   "inputted\n[preview]: see the current input\n[clear] to "
                   "clear the input and reset.";
      std::cout << "TYPES:\n";
      std::cout
          << "\e[1;35mCOLOR_CONSTANT\e[0m: \e[1;32mBLACK\e[0m | "
             "\e[1;32mWHITE\e[0m | \e[1;32mRED\e[0m | \e[1;32mGREEN\e[0m\n";
      std::cout << "\e[1;35mCURVE_CONSTANT\e[0m: \e[1;32mLINEAR\e[0m | "
                   "\e[1;32mEXPONENTIAL\e[0m\n";
      std::cout << "\e[1;35mVEC2\e[0m = (\e[1;32mx\e[0m,\e[1;32my\e[0m)\n";

      std::cout << "FUNCTIONS:\n";
      std::cout << "\e[1;36mset_color\e[0m(\e[1;35mCOLOR_CONSTANT\e[0m "
                   "\e[1;32mcolor\e[0m)\n"
                << "\e[1;36mset_size\e[0m(\e[1;35mINT\e[0m \e[1;32mx\e[0m, "
                   "\e[1;35mINT\e[0m \e[1;32my\e[0m)\n"
                << "\e[1;36mset_background\e[0m(\e[1;35m\"CHARACTER\"\e[0m "
                   "\e[1;32mchar\e[0m, "
                << "\e[1;35mCOLOR_CONSTANT\e[0m \e[1;32mcolor\e[0m)\n"
                << "\e[1;36mplot\e[0m(\e[1;35m\"CHARACTER\"\e[0m "
                   "\e[1;32mchar\e[0m, \e[1;35mVEC2\e[0m \e[1;32mstart\e[0m, "
                   "\e[1;35mVEC2\e[0m \e[1;32mend\e[0m, "
                   "\e[1;35mCURVE_CONSTANT\e[0m \e[1;32mcurve\e[0m)\n";
      std::cout << "enter something to continue\n";
      std::string s;
      std::getline(std::cin, s);
      goto end;
    }

    if (s == "exec") {
      break;
    }

    if (s == "file") {
      std::cout << "Enter file name:\n";
      std::string s;
      std::getline(std::cin, s);
      std::ifstream file(s);
      if (file.is_open()) {
        std::stringstream ss;
        std::string line;
        while (std::getline(file, line)) {
          ss << line << std::endl;
        }
        input += ss.str();
        file.close();
      } else {
        std::cerr << "file read err, probably doesnt exist.\n";
      }
      continue;
    }

    if (s.back() != ';')
      input += (s + ";\n");
    else
      input += (s + "\n");

  end:
    std::cout << "\033[2J\033[H";
  }

  auto tokens = lex(input);
  auto parser = Parser(std::move(tokens));
  auto root = parser.parse();

  root->evaluate();

  draw_plotter();

  std::cout << "press [enter] to continue, [done] to exit" << std::endl;
  std::string s;
  std::getline(std::cin, s);

  if (s == "done") {
    exit(0);
  }
  goto begin;
}

// "linear" plot.
/*
 #####C
 ####C#
 ###C##
 ##C###
 #C####
 C#####
*/

// "exponential" plot
/*
 #######r#############
 #######r#############
 ######rr#############
 ####rr###############
 ##rrr################
 rrr##################
*/