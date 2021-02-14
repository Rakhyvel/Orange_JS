/*  parser.c

    The compiler takes in tokens as a queue and parses them into modules,
    functions, blocks of code, and expressions. 
    
    Author: Joseph Shimel
    Date: 2/3/21 
*/

#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#include "./debug.h"
#include "./parser.h"
#include "./lexer.h"
#include "./util/list.h"
#include "./util/map.h"

// Higher level token signatures
static const enum tokenType MODULE[] = {TOKEN_MODULE, TOKEN_IDENTIFIER, TOKEN_NEWLINE};
static const enum tokenType FUNCTION[] = {TOKEN_FUNCTION, TOKEN_IDENTIFIER, TOKEN_IDENTIFIER, TOKEN_LPAREN};

// Lower level token signatures
static const enum tokenType VAR[] = {TOKEN_VAR, TOKEN_IDENTIFIER};
static const enum tokenType IF[] = {TOKEN_IF};
static const enum tokenType WHILE[] = {TOKEN_WHILE};
static const enum tokenType CALL[] = {TOKEN_IDENTIFIER, TOKEN_LPAREN};
static const enum tokenType INDEX[] = {TOKEN_IDENTIFIER, TOKEN_LSQUARE};

static void rejectUselessNewLines(struct list*);
static void copyNextTokenString(struct list*, char*);
static struct astNode* createBlockAST(struct list*);
static int matchTokens(struct list*, const enum tokenType[], int);
static struct astNode* createAST(enum astType);
static struct astNode* createExpressionAST(struct list*);
static struct list* nextExpression(struct list*);
static struct list* simplifyTokens(struct list*);
static struct list* infixToPostfix(struct list*);
static enum astType tokenToAST(enum tokenType);

/*
    Allocates and initializes the program struct */
struct program* parser_initProgram() {
    struct program* retval = (struct program*) malloc(sizeof(struct program));
    retval->modulesMap = map_init();
    return retval;
}

/*
    Allocates and initializes a module struct */
struct module* parser_initModule(struct program* program) {
    struct module* retval = (struct module*) malloc(sizeof(struct module));
    retval->functionsMap = map_init();
    retval->program = program;
    return retval;
}

/*
    Allocates and initializes a function struct */
struct function* parser_initFunction() {
    struct function* retval = (struct function*) malloc(sizeof(struct function));
    retval->argTypes = list_create();
    retval->argNames = list_create();
    return retval;
}

/*
    Consumes a token queue, adds modules, from their starting module token to
    the ending end token to the program struct.
    
    Functions, structs, and globals are also parsed. */
void parser_addModules(struct program* program, struct list* tokenQueue) {
    // Find all modules in a given token queue
    while(!list_isEmpty(tokenQueue)) {
        rejectUselessNewLines(tokenQueue);

        if(matchTokens(tokenQueue, MODULE, 3)) {
            struct module* module = parser_initModule(program);
            free(queue_pop(tokenQueue)); // Remove "module"
            copyNextTokenString(tokenQueue, module->name);
            LOG("New module: %s", module->name);
            free(queue_pop(tokenQueue)); // Remove "\n"

            map_put(program->modulesMap, module->name, module);
            parser_addFunctions(module, tokenQueue);

            free(queue_pop(tokenQueue)); // Remove "end" because modules are not true statement blocks
        } else if(((struct token*) queue_peek(tokenQueue))->type == TOKEN_EOF) {
            free(queue_pop(tokenQueue));
        } else {
            printf("Token left after module: %s\n", ((struct token*)queue_peek(tokenQueue))->data);
            ASSERT(0);
        }
    }   
}

/*
    Takes in a token queue, adds functions, structs, and globals to a given 
    module, including any internal AST's. */
void parser_addFunctions(struct module* module, struct list* tokenQueue) {
    // Find all functions until a function's end is reached
    while(!list_isEmpty(tokenQueue) && 
    ((struct token*) queue_peek(tokenQueue))->type != TOKEN_END) {
        rejectUselessNewLines(tokenQueue);

        if(matchTokens(tokenQueue, FUNCTION, 4)) {
            struct function* function = parser_initFunction();
            free(queue_pop(tokenQueue)); // Remove "function"
            copyNextTokenString(tokenQueue, function->returnType);
            copyNextTokenString(tokenQueue, function->name);
            LOG("New function: %s %s", function->returnType, function->name);

            free(queue_pop(tokenQueue)); // Remove (

            // Parse parameters of function
            while(((struct token*)queue_peek(tokenQueue))->type != TOKEN_RPAREN) {
                char* argType = (char*)malloc(sizeof(char) * 255);
                char* argName = (char*)malloc(sizeof(char) * 255);
                copyNextTokenString(tokenQueue, argType);
                copyNextTokenString(tokenQueue, argName);
                queue_push(function->argTypes, argType);
                queue_push(function->argNames, argName);
    
                if(((struct token*)queue_peek(tokenQueue))->type == TOKEN_COMMA) {
                    free(queue_pop(tokenQueue));
                }
                LOG("New arg %s %s", argType, argName);
            }
            free(queue_pop(tokenQueue)); // Remove )

            function->code = createBlockAST(tokenQueue);
            LOG("Function %s %s's AST", function->returnType, function->name);
            parser_printAST(function->code, 0);
            map_put(module->functionsMap, function->name, function);
        } else {
    
            printf("Token left after function: %s\n", ((struct token*)queue_peek(tokenQueue))->data);
            ASSERT(0);
        }
        rejectUselessNewLines(tokenQueue);
    }
}

/*
    Creates an AST for code given a queue of tokens. Only parses one 
    instruction per call. Blocks must be created using the createBlockAST()
    function. */
struct astNode* parser_createAST(struct list* tokenQueue) {
    ASSERT(tokenQueue != NULL);
    struct astNode* retval = NULL;
    rejectUselessNewLines(tokenQueue);
    // VARIABLE DEFINE
    if (matchTokens(tokenQueue, VAR, 2)) {
        retval = createAST(AST_VARDECLARE);
        copyNextTokenString(tokenQueue, retval->varType);
        copyNextTokenString(tokenQueue, retval->varName);
        // TODO: pointer and array modifiers

        if(((struct token*)queue_peek(tokenQueue))->type == TOKEN_ASSIGN) {
            retval->type = AST_VARDEFINE;
            free(queue_pop(tokenQueue)); // Remove =
            queue_push(retval->children, parser_createAST(tokenQueue));
        }
    }
    // IF
    else if (matchTokens(tokenQueue, IF, 1)) {
        retval = createAST(AST_IF);
        free(queue_pop(tokenQueue)); // Remove 'if'
        struct astNode* expression = parser_createAST(tokenQueue);
        struct astNode* body = createBlockAST(tokenQueue);
        queue_push(retval->children, expression);
        queue_push(retval->children, body);
    }
    // WHILE
    else if (matchTokens(tokenQueue, WHILE, 1)) {
        retval = createAST(AST_WHILE);
        free(queue_pop(tokenQueue)); // Remove 'while'
        struct astNode* expression = parser_createAST(tokenQueue);
        struct astNode* body = createBlockAST(tokenQueue);
        queue_push(retval->children, expression);
        queue_push(retval->children, body);
    }
    // EXPRESSION
    else {
        retval = createExpressionAST(tokenQueue);
        free(queue_pop(tokenQueue)); // Remove \n
    }
    return retval;
}

/*
    If VERBOSE is defined, prints out a given Abstract Syntax Tree */
void parser_printAST(struct astNode* node, int n) {
    #ifndef VERBOSE
    return;
    #endif

    for(int i = 0; i < n; i++) printf("  "); // print spaces
    LOG("%s", parser_astToString(node->type));
    if(node->varType[0] != '\0' || node->varName[0] != '\0') {
        for(int i = 0; i < n + 1; i++) printf("  ");
        LOG("%s:%s", node->varName, node->varType);
    }

    // Go through children of passed in AST node
    struct listElem* elem = NULL;
    for(elem = list_begin(node->children); elem != list_end(node->children); elem = list_next(elem)) {
        if(node->type == AST_NUMLITERAL) {
            for(int i = 0; i < n + 1; i++) printf("  "); // print spaces
            int* data = (int*)(elem->data);
            LOG("%d", *data);
        } else if(node->type == AST_VAR) {
            for(int i = 0; i < n + 1; i++) printf("  "); // print spaces
            LOG("%s", (char*)elem->data);
        } else if(node->type == AST_CALL) {
            parser_printAST(((struct astNode*)elem->data), n+1);
        } else{
            parser_printAST(((struct astNode*)elem->data), n+1);
        }
    }
}


/*
    Converts an AST type to a string */
char* parser_astToString(enum astType type) {
    switch(type) {
    case AST_BLOCK: 
        return "astType:BLOCK";
    case AST_VARDEFINE: 
        return "astType.VARDEFINE";
    case AST_VARDECLARE: 
        return "astType.VARDECLARE";
    case AST_IF: 
        return "astType.IF";
    case AST_WHILE: 
        return "astType.WHILE";
    case AST_PLUS: 
        return "astType:PLUS";
    case AST_MINUS: 
        return "astType:MINUS";
    case AST_MULTIPLY: 
        return "astType:MULTIPLY";
    case AST_DIVIDE: 
        return "astType:DIVIDE";
    case AST_AND: 
        return "astType.AND";
    case AST_OR: 
        return "astType.OR";
    case AST_GREATER: 
        return "astType.GREATER";
    case AST_LESSER: 
        return "astType.LESSER";
    case AST_IS:
        return "astType.IS";
    case AST_ISNT: 
        return "astType.ISNT";
    case AST_ASSIGN: 
        return "astType:ASSIGN";
    case AST_INDEX: 
        return "astType.INDEX";
    case AST_NUMLITERAL: 
        return "astType:NUMLITERAL";
    case AST_CALL:
        return "astType.CALL";
    case AST_VAR: 
        return "astType.VAR";
    case AST_STRINGLITERAL: 
        return "astType.STRINGLITERAL";
    case AST_CHARLITERAL: 
        return "astType.CHARLITERAL";
    case AST_DOT: 
        return "astType.DOT";
    case AST_NOP:
        return "astType:NOP";
    }
    printf("Unknown astType: %d\n", type);
    NOT_REACHED();
    return "";
}

/*
    Takes in a queue of tokens, and rejects all newlines that are at the front.
    
    It is up to the caller to use this apropiately, and not damage the 
    structure of the code, since formatting matters in this language. */
static void rejectUselessNewLines(struct list* tokenQueue) {
    while(((struct token*) queue_peek(tokenQueue))->type == TOKEN_NEWLINE) {
        free(queue_pop(tokenQueue)); // Remove \n
    }
}

/*
    Pops a token off from the front of a queue, copies the string data of that
    token into a given string. Max size is 255 characters, including null term. */
static void copyNextTokenString(struct list* tokenQueue, char* dest) {
    struct token* nextToken = (struct token*) queue_pop(tokenQueue);
    strncpy(dest, nextToken->data, 254);
    free(nextToken);
}

/*
    Takes in a token queue, parses out a block of statements which end with an 
    end token */
static struct astNode* createBlockAST(struct list* tokenQueue) {
    struct astNode* block = createAST(AST_BLOCK);
    
    // Go through statements in token queue until an end token is found
    while(!list_isEmpty(tokenQueue) && ((struct token*)queue_peek(tokenQueue))->type != TOKEN_END) {
        queue_push(block->children, parser_createAST(tokenQueue));
        rejectUselessNewLines(tokenQueue);
    }
    free(queue_pop(tokenQueue)); // Remove "end"
    return block;
}

/*
    Determines if a given token signature matches what is at the front of a 
    tokenQueue.
    
    Does NOT run off edge of tokenQueue, instead returns false. */
static int matchTokens(struct list* tokenQueue, const enum tokenType sig[], int nTokens) {
    int i = 0;
    struct listElem* elem;
    for(elem = list_begin(tokenQueue); elem != list_end(tokenQueue) && i < nTokens; elem = list_next(elem), i++) {
        struct token* token = (struct token*) (elem->data);
        if(token->type != sig[i]) {
            return 0;
        }
    }
    return i == nTokens;
}

/*
    Allocates and initializes an Abstract Syntax Tree node, with the proper type */
static struct astNode* createAST(enum astType type) {
    struct astNode* retval = (struct astNode*) malloc(sizeof(struct astNode));
    retval->type = type;
    retval->children = list_create();
    return retval;
}

/*
    Given a token queue, extracts the front expression, parses it into an
    Abstract Syntax Tree. */
static struct astNode* createExpressionAST(struct list* tokenQueue) {
    ASSERT(tokenQueue != NULL);
    struct astNode* astNode = NULL;
    struct token* token = NULL;
    struct list* expression = infixToPostfix(simplifyTokens(nextExpression(tokenQueue)));
    struct list* argStack = list_create();
    
    while (!list_isEmpty(expression)) {
        struct listElem* elem = NULL;
        token = (struct token*)queue_pop(expression);
        astNode = createAST(AST_NOP);
        int* intData;
        char* charData;

        switch(token->type) {
        case TOKEN_NUMLITERAL:
            astNode->type = AST_NUMLITERAL;
            intData = (int*)malloc(sizeof(int));
            *intData = atoi(token->data);
            queue_push(astNode->children, intData);
            stack_push(argStack, astNode);
            break;
        case TOKEN_IDENTIFIER:
            astNode->type = AST_VAR;
            charData = (char*)malloc(sizeof(char) * 255);
            strncpy(charData, token->data, 254);
            queue_push(astNode->children, charData);
            stack_push(argStack, astNode);
            break;
        case TOKEN_CALL:
            astNode->type = AST_CALL;
            // Add args of call token (which are already ASTs) to new AST node's children
            for(elem = list_begin(token->list); elem != list_end(token->list); elem = list_next(elem)) {
                queue_push(astNode->children, (struct astNode*)elem->data);
            }
            strncpy(astNode->varName, token->data, 254);
            stack_push(argStack, astNode);
            break;
        default: // Assume operator
            astNode->type = tokenToAST(token->type);
            queue_push(astNode->children, stack_pop(argStack)); // Right
            queue_push(astNode->children, stack_pop(argStack)); // Left
            stack_push(argStack, astNode);
            break;
        }
        free(token);
    }
    return stack_peek(argStack);
}

/*
    Takes a queue of tokens, pops off the first expression and returns as a new
    queue. */
static struct list* nextExpression(struct list* tokenQueue) {
    ASSERT(tokenQueue != NULL);

    struct list* retval = list_create();
    int depth = 0;
    enum tokenType nextType;

    // Go through tokens, pick out the first expression. Stop when state determines expression is over
    while(!list_isEmpty(tokenQueue)) {
        nextType = ((struct token*)queue_peek(tokenQueue))->type;
        if(nextType == TOKEN_LPAREN || nextType == TOKEN_LSQUARE) {
            depth++;
        } else if(nextType == TOKEN_RPAREN || nextType == TOKEN_RSQUARE) {
            depth--;
        }
        // COMMA EXIT
        if(depth == 0 && nextType == TOKEN_COMMA) {
            break;
        }
        // NEWLINE EXIT
        if(nextType == TOKEN_NEWLINE) {
            break;
        }
        // END OF FILE EXIT
        if(nextType == TOKEN_EOF) {
            break;
        }
        // DEPTH EXIT
        if(depth < 0) {
            break;
        }
        queue_push(retval, queue_pop(tokenQueue));
    }
    return retval;
}

/*
    Takes in a queue representing an expression, and if it can, transforms 
    function calls/index token structures into proper call/index tokens */
static struct list* simplifyTokens(struct list* tokenQueue) {
    struct list* retval = list_create();

    // Go through each token in given expression token queue, add/reject/parse to retval
    while(!list_isEmpty(tokenQueue)) {
        if(matchTokens(tokenQueue, CALL, 2)) {
            struct token* callName = ((struct token*)queue_pop(tokenQueue));
            struct token* call = lexer_createToken(TOKEN_CALL, callName->data);
            free(callName);

            free(queue_pop(tokenQueue)); // Remove (
            
            // Go through each argument, create AST representing it
            while(!list_isEmpty(tokenQueue) && ((struct token*)queue_peek(tokenQueue))->type != TOKEN_RPAREN) {
                struct astNode* argAST = createExpressionAST(tokenQueue);
                queue_push(call->list, argAST);

                if(((struct token*)queue_peek(tokenQueue))->type == TOKEN_COMMA) {
                    free(queue_pop(tokenQueue));
                }
            }
            queue_pop(tokenQueue); // Remove )

            queue_push(retval, call);
        } else if(matchTokens(tokenQueue, INDEX, 2)) {
            queue_push(retval, queue_pop(tokenQueue)); // move identifier
            queue_push(retval, lexer_createToken(TOKEN_INDEX, ""));
            queue_push(retval, lexer_createToken(TOKEN_LPAREN, "("));
            free(queue_pop(tokenQueue)); // Remove [

            // extract expression for index, add to retval list, between anonymous parens
            struct list* innerExpression = nextExpression(tokenQueue);
            struct listElem* elem;
            for(elem = list_begin(innerExpression); elem != list_end(innerExpression); elem=list_next(elem)) {
                queue_push(retval, elem->data);
            }

            free(queue_pop(tokenQueue)); // Remove ]
            queue_push(retval, lexer_createToken(TOKEN_RPAREN, ")"));
        } else {
            queue_push(retval, queue_pop(tokenQueue));
        }
    }
    return retval;
}

/*
    Consumes a queue of tokens representing an expression in infix order, 
    converts it to postfix order */
static struct list* infixToPostfix(struct list* tokenQueue) {
    struct list* retval = list_create();
    struct list* opStack = list_create();
    struct token* token = NULL;

    // Go through each token in expression token queue, rearrange to form postfix expression token queue
    while(!list_isEmpty(tokenQueue)) {
        token = ((struct token*) queue_pop(tokenQueue));
        // VALUE
        if(token->type == TOKEN_IDENTIFIER || token->type == TOKEN_NUMLITERAL || token->type == TOKEN_CALL) {
            queue_push(retval, token);
        } 
        // OPEN PARENTHESIS
        else if (token->type == TOKEN_LPAREN) {
            stack_push(opStack, token);
        } 
        // CLOSE PARENTHESIS
        else if (token->type == TOKEN_RPAREN) {
            // Pop all operations from opstack to retval until original ( paren is found
            while(!list_isEmpty(opStack) && ((struct token*)stack_peek(opStack))->type != TOKEN_LPAREN) {
                queue_push(retval, stack_pop(opStack));
            }
            stack_pop(opStack); // Remove (
        } 
        // OPERATOR
        else {
            // Pop all operations from opstack until an operation of lower precedence is found
            while(!list_isEmpty(opStack) && 
            lexer_getTokenPrecedence(token->type) <= lexer_getTokenPrecedence(((struct token*)stack_peek(opStack))->type)) {
                queue_push(retval, stack_pop(opStack));
            }
            stack_push(opStack, token);
        }
    }

    // Push all remaining operators to queue
    while(!list_isEmpty(opStack)) {
        queue_push(retval, stack_pop(opStack));
    }

    return retval;
}

/*
    Used to convert between token types for operators, and ast types for 
    operators 
    
    Called by createExpression to convert operator tokens into ASTs */
static enum astType tokenToAST(enum tokenType type) {
    switch(type) {
    case TOKEN_PLUS:
        return AST_PLUS;
    case TOKEN_MINUS: 
        return AST_MINUS;
    case TOKEN_MULTIPLY: 
        return AST_MULTIPLY;
    case TOKEN_DIVIDE: 
        return AST_DIVIDE;
    case TOKEN_ASSIGN:
        return AST_ASSIGN;
	case TOKEN_IS: 
        return AST_IS;
    case TOKEN_ISNT: 
        return AST_ISNT;
    case TOKEN_GREATER: 
        return AST_GREATER;
    case TOKEN_LESSER:
        return AST_LESSER;
	case TOKEN_AND: 
        return AST_AND;
    case TOKEN_OR:
        return AST_OR;
    case TOKEN_IDENTIFIER:
        return AST_VAR;
    case TOKEN_CALL:
        return AST_CALL;
    case TOKEN_DOT:
        return AST_DOT;
    case TOKEN_INDEX:
        return AST_INDEX;
    default:
        printf("Cannot convert %s to AST\n", lexer_tokenToString(type));
        NOT_REACHED();
    }
}