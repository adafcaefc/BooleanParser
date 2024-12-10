#include <iostream>
#include <string>
#include <vector>
#include <sstream>
#include <unordered_map>
#include <cmath>
#include <boost/algorithm/string.hpp>
#include <boost/algorithm/string/replace.hpp>

namespace Utils
{
    std::vector<bool> to_bit_vector(
        int n, 
        const unsigned int pad)
    {
        std::vector<bool> buffer;
        while (n > 0)
        {
            buffer.push_back(n % 2);
            n /= 2;
        }
        for (auto i = buffer.size(); i < pad; ++i) { buffer.push_back(false); }
        return buffer;
    }
}

// to do : make this object-oriented (?)
enum class TokenType
{
    kNull,
    kVariable,
    kTrue,
    kFalse,
    kOr,
    kAnd,
    kNot,
    kImplication,
    kBiImplication,
    kLeftBracket,
    kRightBracket,
};

using variables_map_t = std::unordered_map<std::string, bool>;

struct Token
{
    // token type with ugly enums, could be using oop...
    TokenType type;
    std::string data;
    // priority_1 = bracket, priority_2 = operation priority
    int priority_1 = 0, priority_2 = 0;
    // needed for operation, filled later (linked list)
    Token* left = nullptr;
    Token* right = nullptr;
    bool value(const variables_map_t& map = variables_map_t())
    { 
        if (this->type == TokenType::kTrue || this->type == TokenType::kFalse) { return this->type == TokenType::kTrue; }
        return map.at(this->data);
    }
    Token(const bool value) : type(value ? TokenType::kTrue : TokenType::kFalse) {}
    Token(const TokenType _type, const std::string& _data) : type(_type), data(_data) {}
    // morphing...
    void set_type(const bool value) 
    {
        this->type = value ? TokenType::kTrue : TokenType::kFalse;
        this->data = value ? "true" : "false";
    }
};

namespace Tokeniser
{
#   define TOKENISE_M(s, result) if (tokenise_impl(expression, s)) { return Token(TokenType::result, s); }

    bool tokenise_impl(
        std::string* expression,
        const std::string& token)
    {
        if (expression->find(token) == 0)
        {
            *expression = std::string(expression->begin() + token.size(), expression->end());
            return true;
        }
        return false;
    }

    Token tokenise_internal(std::string* expression)
    {
        // macro hell :>
        TOKENISE_M("true", kTrue);
        TOKENISE_M("t", kTrue);
        TOKENISE_M("false", kFalse);
        TOKENISE_M("f", kFalse);
        TOKENISE_M("&&", kAnd);
        TOKENISE_M("&", kAnd);
        TOKENISE_M("and", kAnd);
        TOKENISE_M("^", kAnd);
        TOKENISE_M("||", kOr);
        TOKENISE_M("or", kOr);
        TOKENISE_M("v", kOr);
        TOKENISE_M("~", kNot);
        TOKENISE_M("!", kNot);
        TOKENISE_M("not", kNot);
        TOKENISE_M("->", kImplication);
        TOKENISE_M("-->", kImplication);
        TOKENISE_M("<->", kBiImplication);
        TOKENISE_M("<=>", kBiImplication);
        TOKENISE_M("<-->", kBiImplication);
        TOKENISE_M("(", kLeftBracket);
        TOKENISE_M(")", kRightBracket);
        std::string current(1, expression->front());
        TOKENISE_M(current, kVariable);
    }

    std::vector<Token> tokenise(std::string* expression)
    {
        std::vector<Token> tokens;
        while (expression->size()) tokens.push_back(tokenise_internal(expression));
        return tokens;
    }

    std::vector<Token> tokenise_raw_string(const std::string& expression)
    {
        std::vector<Token> tokens;
        std::vector<std::string> space_separated;
        boost::split(space_separated, expression, boost::is_any_of(" "));
        for (auto& s : space_separated)
        {
            boost::to_lower(s);
            const auto temp = tokenise(&s);
            tokens.insert(tokens.end(), temp.begin(), temp.end());
        }
        return tokens;
    }
};

struct TokenisedExpression
{
private:
    std::vector<Token> tokens, raw_tokens;
    variables_map_t variables;
    using variable_pair_t = std::pair<const std::string, bool>;

    void proccess_internal()
    {
        std::vector<Token> new_tokens;
        int state = 0;
        for (auto& token : this->raw_tokens)
        {
            // increase the priority of not operator
            if (token.type == TokenType::kNot) { token.priority_2 = 1; }
            // parse brackets
            if (token.type == TokenType::kLeftBracket) 
            { 
                state += 1; 
            }
            else if (token.type == TokenType::kRightBracket) 
            { 
                state -= 1; 
            }
            else
            {
                token.priority_1 = state;
                new_tokens.push_back(token);
            }
        }
        this->tokens = new_tokens;
    }

    void load_variables()
    {
        // create variable map
        for (auto& token : this->tokens)
        {
            if (token.type != TokenType::kVariable) continue;
            this->variables[token.data] = false;
        }
    }

    void procces_block_link(std::vector<Token>* block)
    {
        // write the linked list
        for (auto i = 0u; i < block->size(); ++i)
        {
            if (i == 0u)
            {
                block->at(i).right = &block->at(i + 1u);
            }
            else if (i + 1u == block->size())
            {
                block->at(i).left = &block->at(i - 1u);
            }
            else
            {
                block->at(i).left = &block->at(i - 1u);
                block->at(i).right = &block->at(i + 1u);
            }
        }
    }

    // helper function to get the current variable value
    bool variable_value(const std::string& variable) { return this->variables[variable]; }

    // to do : throw an exception when function fail to parse expression
    // [p v q v r] still crashes...
    void proccess_block(std::vector<Token>* block)
    {
        std::vector<Token> block_copy = *block;
        std::vector<Token> block_new;
        this->procces_block_link(&block_copy);
        int max_priority = -1;
        for (auto& token : block_copy)
        {
            // this could be made using oop
            // struct IToken 
            // {
            //   virtual bool is_operator() { return true; }; 
            //   ...
            // }
            switch (token.type)
            {
            case TokenType::kNot:
            case TokenType::kOr:
            case TokenType::kAnd:
            case TokenType::kImplication:
            case TokenType::kBiImplication:
                if (max_priority < token.priority_2) { max_priority = token.priority_2; }
                break;
            default:
                break;
            }
        }
        for (auto& token : block_copy)
        {
            // this could be made using oop
            // struct IToken 
            // {
            //   virtual bool process() = 0; 
            //   ...
            // }
            // struct OrToken : public IToken 
            // {
            //   bool process(const variables_map_t& map) override
            //   {
            //     return this->left->value(map) || this->right->value(map);
            //   }
            //   ...
            // }
            if (token.priority_2 != max_priority) continue;
            switch (token.type)
            {
            case TokenType::kNot:
                if (token.right->type == TokenType::kNull) { break; }
                token.set_type(!token.right->value(this->variables));
                token.right->type = TokenType::kNull;
                break;
            case TokenType::kOr:
                if (token.left->type == TokenType::kNull || token.right->type == TokenType::kNull) { break; }
                token.set_type(token.left->value(this->variables) || token.right->value(this->variables));
                token.left->type = TokenType::kNull;
                token.right->type = TokenType::kNull;
                break;
            case TokenType::kAnd:
                if (token.left->type == TokenType::kNull || token.right->type == TokenType::kNull) { break; }
                token.set_type(token.left->value(this->variables) && token.right->value(this->variables));
                token.left->type = TokenType::kNull;
                token.right->type = TokenType::kNull;
                break;
            case TokenType::kImplication:
                if (token.left->type == TokenType::kNull || token.right->type == TokenType::kNull) { break; }
                token.set_type(!token.left->value(this->variables) || token.right->value(this->variables));
                token.left->type = TokenType::kNull;
                token.right->type = TokenType::kNull;
                break;
                break;
            case TokenType::kBiImplication:
                if (token.left->type == TokenType::kNull || token.right->type == TokenType::kNull) { break; }
                token.set_type((!token.left->value(this->variables) || token.right->value(this->variables)) && (!token.right->value(this->variables) || token.left->value(this->variables)));
                token.left->type = TokenType::kNull;
                token.right->type = TokenType::kNull;
                break;
            default:
                break;
            }
        }
        for (auto& token : block_copy)
        {
            if (token.type != TokenType::kNull) { block_new.push_back(token); }
        }
        *block = block_new;
    }

    void process_blocks(std::vector<std::vector<Token>>* blocks)
    {
        const auto blocks_copy = *blocks;
        int max_priority = blocks_copy.front().front().priority_1;
        for (auto& block : blocks_copy)
        {
            if (max_priority < block.front().priority_1) { max_priority = block.front().priority_1; }
        }

        for (auto& block : blocks_copy)
        {
            if (block.front().priority_1 == max_priority)
            {
                while (block.size() > 1)
                {
                    this->proccess_block(const_cast<std::vector<Token>*>(&block));
                }
                const_cast<std::vector<Token>*>(&block)->front().priority_1--;
            }
        }

        int current_priority = blocks_copy.front().front().priority_1;
        std::vector<std::vector<Token>> blocks_new;
        std::vector<Token> block_new;

        for (auto& block : blocks_copy)
        {
            for (auto& token : block)
            {
                if (token.priority_1 != current_priority)
                {
                    current_priority = token.priority_1;
                    blocks_new.push_back(block_new);
                    block_new.clear();
                }
                block_new.push_back(token);
            }
        }
        blocks_new.push_back(block_new);
        *blocks = blocks_new;
    }

    bool run_expression()
    {
        std::vector<std::vector<Token>> blocks;
        std::vector<Token> block;
        int current_priority = this->tokens.front().priority_1;
        for (auto& token : this->tokens)
        {
            if (token.priority_1 != current_priority)
            {
                current_priority = token.priority_1;
                blocks.push_back(block);
                block.clear();
            }
            block.push_back(token);
        }
        blocks.push_back(block);
        while (blocks.size() > 1 || blocks.front().size() > 1) { process_blocks(&blocks); }
        return blocks.front().front().value();
    }

    void print_all_expression()
    {
        std::cout << ' ';
        for (auto& variable : this->variables)
        {
            std::cout << "   " << variable.first << "\t|";
        }
        std::cout << "   ";
        for (auto& token : this->raw_tokens)
        {
            std::cout << token.data << ' ';
        }
        std::cout << "\n------------------------------------------------------------------------------------------------\n";

        std::vector<variable_pair_t*> map_pairs;
        for (auto& entry : this->variables) { map_pairs.push_back(const_cast<variable_pair_t*>(&entry)); }

        for (auto i = 0u; i < std::pow(2, this->variables.size()); i++)
        {
            const auto bits = Utils::to_bit_vector(i, map_pairs.size());
            for (auto j = 0u; j < map_pairs.size(); ++j)
            {
                map_pairs.at(j)->second = bits.at(j);
            }
            std::cout << ' ';
            for (auto& variable : this->variables)
            {
                std::cout << "   " << (variable.second ? 'T' : 'F') << "\t|";
            }
            std::cout << "   " << (run_expression() ? 'T' : 'F') << '\n';
        }
    }

public:
    TokenisedExpression(const std::string& expression)
    {
        this->raw_tokens = Tokeniser::tokenise_raw_string(expression);
        this->proccess_internal();
        this->load_variables();
        this->print_all_expression();
    }
};

int main()
{
        std::string input;
        std::cout << "Input expression : ";
        std::getline(std::cin, input);
        TokenisedExpression exp(input);
        std::cout << '\n';
}
