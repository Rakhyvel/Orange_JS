/*  parser.c

    The parser's job is to take in a stream of tokens, and build a structure 
    that represents the program. 
    
    - The parser DOES NOT care if the structure represents a "correct" program. 
      Program validation is performed later.
    - The parser DOES care if the input token queue is in a correct fashion, 
      and will give syntax errors if it is not.
    
    Author: Joseph Shimel
    Date: 2/3/21 
*/

#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#include "./main.h"
#include "./parser.h"
#include "./lexer.h"
#include "../util/list.h"
#include "../util/map.h"
#include "../util/debug.h"

// Higher level token signatures
static const enum tokenType MODULE[] = {TOKEN_MODULE, TOKEN_IDENTIFIER, TOKEN_LBRACE};
static const enum tokenType FUNCTION[] = {TOKEN_IDENTIFIER, TOKEN_IDENTIFIER, TOKEN_LPAREN};
static const enum tokenType FUNCTION_BOX[] = {TOKEN_IDENTIFIER, TOKEN_IDENTIFIER, TOKEN_LESSER};
static const enum tokenType CONST[] = {TOKEN_CONST, TOKEN_IDENTIFIER, TOKEN_IDENTIFIER};

// Lower level token signatures
static const enum tokenType BLOCK[] = {TOKEN_LBRACE};
static const enum tokenType VARDECLARE[] = {TOKEN_IDENTIFIER, TOKEN_IDENTIFIER, TOKEN_SEMICOLON};
static const enum tokenType VARDEFINE[] = {TOKEN_IDENTIFIER, TOKEN_IDENTIFIER, TOKEN_EQUALS};
static const enum tokenType IF[] = {TOKEN_IF};
static const enum tokenType WHILE[] = {TOKEN_WHILE};
static const enum tokenType RETURN[] = {TOKEN_RETURN};
static const enum tokenType CALL[] = {TOKEN_IDENTIFIER, TOKEN_LPAREN};
static const enum tokenType INDEX[] = {TOKEN_LSQUARE};
static const enum tokenType EMPTY[] = {TOKEN_SEMICOLON};

// Private functions
static int matchComment(struct list*, struct listElem*);
static void condenseArrayIdentifiers(struct list*);
static void copyNextTokenString(struct list*, char*);
static struct variable* createVar(struct list*, struct block*);
static void parseParams(struct list*, struct map*, struct variable*);
static int matchTokens(struct list*, const enum tokenType[], int);
static struct astNode* createAST(enum astType, const char*, int);
static struct astNode* createExpressionAST(struct list*);
static struct list* nextExpression(struct list*);
static struct list* simplifyTokens(struct list*);
static struct list* infixToPostfix(struct list*);
static enum astType tokenToAST(enum tokenType);
static void assertPeek(struct list*, enum tokenType);
static void assertRemove(struct list*, enum tokenType);
static void assertOperator(enum astType, const char*, int);

/*
    Allocates and initializes the program struct */
struct program* parser_initProgram() {
    struct program* retval = (struct program*) calloc(1, sizeof(struct program));
    retval->modulesMap = map_create();
    retval->dataStructsMap = map_create();
    retval->fileMap = map_create();
    return retval;
}

/*
    Allocates and initializes a module struct */
struct module* parser_initModule(struct program* program, const char* filename, int line) {
    struct module* retval = (struct module*) calloc(1, sizeof(struct module));
    retval->functionsMap = map_create();
    retval->block = parser_initBlock(NULL);
    retval->program = program;
    retval->filename = filename;
    retval->line = line;
    return retval;
}

/*
    Allocates and initializes a function struct */
struct function* parser_initFunction(const char* filename, int line) {
    struct function* retval = (struct function*) calloc(1, sizeof(struct function));
    strcpy(retval->self.varType, "function");
    retval->self.filename = filename;
    retval->self.line = line;
    return retval;
}

/*
    Allocates and initializes a data struct */
struct dataStruct* parser_initDataStruct(const char* filename, int line) {
    struct dataStruct* retval = (struct dataStruct*) calloc(1, sizeof(struct dataStruct));
    strcpy(retval->self.varType, "struct");
    retval->parentSet = map_create();
    retval->self.filename = filename;
    retval->self.line = line;
    return retval;
}

struct block* parser_initBlock(struct block* parent) {
    struct block* retval = (struct block*) malloc(sizeof(struct block));
    retval->varMap = map_create();
    retval->parent = parent;
    return retval;
}

/*
    Removes tokens from a list that are surrounded by comments */
void parser_removeComments(struct list* tokenQueue) {
    struct listElem* elem;
    int withinComment = 0;

    for(elem=list_begin(tokenQueue);elem!=list_end(tokenQueue);elem=list_next(elem)) {
        if(withinComment) {
            if(matchComment(tokenQueue, elem)) {
                withinComment = 0;
                elem = elem->prev;
                free(list_remove(tokenQueue, list_next(elem)));
                free(list_remove(tokenQueue, list_next(elem)));
            } else {
                elem = elem->prev;
                free(list_remove(tokenQueue, list_next(elem)));
            }
        } else {
            if(matchComment(tokenQueue, elem)) {
                withinComment = 1;
                elem = elem->prev;
                free(list_remove(tokenQueue, list_next(elem)));
                free(list_remove(tokenQueue, list_next(elem)));
            }
        }
    }
}

/*
    Consumes a token queue, adds modules, from their starting module token to
    the ending end token to the program struct.
    
    Functions, structs, and globals are also parsed. */
void parser_addModules(struct program* program, struct list* tokenQueue) {
    condenseArrayIdentifiers(tokenQueue);
    // Find all modules in a given token queue
    while(!list_isEmpty(tokenQueue)) {
        int isStatic = ((struct token*)queue_peek(tokenQueue))->type == TOKEN_STATIC;
        if(isStatic) free(queue_pop(tokenQueue));
        struct token* topToken = queue_peek(tokenQueue);
        if(matchTokens(tokenQueue, MODULE, 3)) {
            struct module* module = parser_initModule(program, topToken->filename, topToken->line);
            assertRemove(tokenQueue, TOKEN_MODULE);
            copyNextTokenString(tokenQueue, module->name);
            LOG("New module: %s", module->name);
            assertRemove(tokenQueue, TOKEN_LBRACE);

            if(map_put(program->modulesMap, module->name, module)) {
                error(module->filename, module->line, "Module \"%s\" defined in more than one place!", module->name);
            }
            module->isStatic = isStatic;
            parser_addElements(module, tokenQueue);

            assertRemove(tokenQueue, TOKEN_RBRACE);
        } else if(((struct token*) queue_peek(tokenQueue))->type == TOKEN_EOF) {
            free(queue_pop(tokenQueue));
        } else {
            error(topToken->filename, topToken->line, "Unnecessary token %s found", ((struct token*)queue_peek(tokenQueue))->data);
        }
    }   
}

/*
    Takes in a token queue, adds functions, structs, and globals to a given 
    module, including any internal AST's. */
void parser_addElements(struct module* module, struct list* tokenQueue) {

    // Find all functions until a function's end is reached
    while(!list_isEmpty(tokenQueue) && 
    ((struct token*) queue_peek(tokenQueue))->type != TOKEN_RBRACE) {
        int isPrivate = ((struct token*)queue_peek(tokenQueue))->type == TOKEN_PRIVATE;
        if(isPrivate) free(queue_pop(tokenQueue));

        struct token* topToken = queue_peek(tokenQueue);
        // FUNCTION
        if(matchTokens(tokenQueue, FUNCTION, 3) || matchTokens(tokenQueue, FUNCTION_BOX, 3)) {
            struct function* function = parser_initFunction(topToken->filename, topToken->line);
            struct block* argBlock = parser_initBlock(module->block);
            function->argBlock = argBlock;
            copyNextTokenString(tokenQueue, function->self.type);
            copyNextTokenString(tokenQueue, function->self.name);
            LOG("New function: %s %s", function->self.type, function->self.name);

            if(((struct token*) queue_peek(tokenQueue))->type == TOKEN_LESSER) { // Struct only (maybe?) 
                assertRemove(tokenQueue, TOKEN_LESSER);
                assertPeek(tokenQueue, TOKEN_IDENTIFIER);
                copyNextTokenString(tokenQueue, function->self.type);
                assertRemove(tokenQueue, TOKEN_GREATER);
            }

            parseParams(tokenQueue, function->argBlock->varMap, &function->self); // Parameters

            function->self.code = parser_createAST(tokenQueue, argBlock); // Parse AST
            if(function->self.code == NULL || function->self.code->type != AST_BLOCK) {
                error(function->self.filename, function->self.line, "Functions statements must be followed by block statements");
            }
            function->block = (struct block*)function->self.code->data; // 
            LOG("Function %s %s's AST", function->self.type, function->self.name);
            parser_printAST(function->self.code, 0);
            
            if(map_put(module->functionsMap, function->self.name, function)) {
                error(function->self.filename, function->self.line, "Function \"%s\" already defined in module \"%s\"", function->self.name, module->name);
            }
            function->module = module;
            function->program = module->program;
            function->self.isPrivate = isPrivate;

            // STRUCT DEFINITION
            if(!strcmp(function->self.type, "struct")) {
                struct dataStruct* dataStruct = parser_initDataStruct(function->self.code->filename, function->self.code->line);
                if(map_put(program->dataStructsMap, function->self.name, dataStruct)) {
                    error(function->self.filename, function->self.line, "Struct \"%s\" already defined", function->self.name, module->name);
                }
                dataStruct->definition = function;
                strcpy(function->self.type, function->self.name);
                strcpy(dataStruct->self.name, function->self.name);
            }
        }
        // GLOBAL
        else if (matchTokens(tokenQueue, VARDECLARE, 3) || matchTokens(tokenQueue, VARDEFINE, 3) || matchTokens(tokenQueue, CONST, 3)) {
            struct variable* var = createVar(tokenQueue, module->block);
            assertRemove(tokenQueue, TOKEN_SEMICOLON);
            if(map_put(module->block->varMap, var->name, var)) {
                error(topToken->filename, topToken->line, "Global \"%s\"s defined in more than one place in module", var->name, module->name);
            }
            var->isPrivate = isPrivate;
        }
        // ERROR
        else {
            error(topToken->filename, topToken->line, "\nUnexpected token %s in module %s", ((struct token*)queue_peek(tokenQueue))->data, module->name);   
        }
    }
}

/*
    Creates an AST for code given a queue of tokens. Only parses one 
    instruction per call. */
struct astNode* parser_createAST(struct list* tokenQueue, struct block* block) {
    ASSERT(tokenQueue != NULL);
    ASSERT(block != NULL);
    struct astNode* retval = NULL;
    struct token* topToken = queue_peek(tokenQueue);

    // BLOCK
    if(matchTokens(tokenQueue, BLOCK, 1)) {
        retval = createAST(AST_BLOCK, topToken->filename, topToken->line);
        retval->data = parser_initBlock(block);
        assertRemove(tokenQueue, TOKEN_LBRACE);
        while(!list_isEmpty(tokenQueue) && ((struct token*)queue_peek(tokenQueue))->type != TOKEN_RBRACE) {
            queue_push(retval->children, parser_createAST(tokenQueue, retval->data));
        }
        assertRemove(tokenQueue, TOKEN_RBRACE);
    }
    // VARIABLE
    else if (matchTokens(tokenQueue, VARDECLARE, 3) || matchTokens(tokenQueue, VARDEFINE, 3) || matchTokens(tokenQueue, CONST, 3)) {
        retval = createAST(AST_VARDEFINE, topToken->filename, topToken->line);
        struct variable* var = createVar(tokenQueue, block);
        if(var->code == NULL) {
            retval->type = AST_VARDECLARE;
        }
        retval->data = var;
        map_put(block->varMap, var->name, var);
        // @todo create an assertDoesntExist which recursively goes up the blocks until null, asserting that the var doesnt exist
        assertRemove(tokenQueue, TOKEN_SEMICOLON);
    }
    // IF
    else if (matchTokens(tokenQueue, IF, 1)) {
        retval = createAST(AST_IF, topToken->filename, topToken->line);
        assertRemove(tokenQueue, TOKEN_IF);
        struct astNode* expression = parser_createAST(tokenQueue, block);
        struct astNode* body = parser_createAST(tokenQueue, block);
        if(body == NULL || body->type != AST_BLOCK) {
            error(retval->filename, retval->line, "If statements must be followed by block statements");
        }
        queue_push(retval->children, expression);
        queue_push(retval->children, body);
        if(((struct token*)queue_peek(tokenQueue))->type == TOKEN_ELSE) {
            assertRemove(tokenQueue, TOKEN_ELSE);
            retval->type = AST_IFELSE;
            if(((struct token*)queue_peek(tokenQueue))->type == TOKEN_IF) {
                queue_push(retval->children, parser_createAST(tokenQueue, block));
            } else {
                queue_push(retval->children, parser_createAST(tokenQueue, block));
            }
        }
    }
    // WHILE
    else if (matchTokens(tokenQueue, WHILE, 1)) {
        retval = createAST(AST_WHILE, topToken->filename, topToken->line);
        assertRemove(tokenQueue, TOKEN_WHILE);
        struct astNode* expression = parser_createAST(tokenQueue, block);
        struct astNode* body = parser_createAST(tokenQueue, block);
        if(body == NULL || body->type != AST_BLOCK) {
            error(retval->filename, retval->line, "While statements must be followed by block statements");
        }
        queue_push(retval->children, expression);
        queue_push(retval->children, body);
    }
    // RETURN
    else if (matchTokens(tokenQueue, RETURN, 1)) {
        retval = createAST(AST_RETURN, topToken->filename, topToken->line);
        assertRemove(tokenQueue, TOKEN_RETURN);
        struct astNode* expression = createExpressionAST(tokenQueue);
        queue_push(retval->children, expression);
    }
    else if (matchTokens(tokenQueue, EMPTY, 1)) {
        assertRemove(tokenQueue, TOKEN_SEMICOLON);
    }
    // EXPRESSION
    else {
        retval = createExpressionAST(tokenQueue);
    }
    return retval;
}

/*
    If VERBOSE is defined, prints out a given Abstract Syntax Tree */
void parser_printAST(struct astNode* node, int n) {
    #ifndef VERBOSE
    return;
    #else
    for(int i = 0; i < n; i++) printf("  "); // print spaces
    LOG("%s", parser_astToString(node->type));

    // Go through children of passed in AST node
    struct listElem* elem = NULL;
    for(elem = list_begin(node->children); elem != list_end(node->children); elem = list_next(elem)) {
        if(elem->data == NULL) {
            continue;
        } else if(node->type == AST_INTLITERAL) {
            for(int i = 0; i < n + 1; i++) printf("  "); // print spaces
            int* data = (int*)(elem->data);
            LOG("%d", *data);
        } else if(node->type == AST_REALLITERAL) {
            for(int i = 0; i < n + 1; i++) printf("  "); // print spaces
            float* data = (float*)(elem->data);
            LOG("%f", *data);
        } else if(node->type == AST_CHARLITERAL || node->type == AST_STRINGLITERAL) {
            for(int i = 0; i < n + 1; i++) printf("  "); // print spaces
            LOG("%s", (char*)(elem->data));
        } else if(node->type == AST_VAR) {
            for(int i = 0; i < n + 1; i++) printf("  "); // print spaces
            LOG("%s", (char*)elem->data);
        } else{
            parser_printAST(((struct astNode*)elem->data), n+1);
        }
    }
    #endif
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
    case AST_IFELSE: 
        return "astType.IFELSE";
    case AST_WHILE: 
        return "astType.WHILE";
    case AST_RETURN: 
        return "astType.RETURN";
    case AST_ADD: 
        return "astType:PLUS";
    case AST_SUBTRACT: 
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
    case AST_INTLITERAL: 
        return "astType:INTLITERAL";
    case AST_REALLITERAL: 
        return "astType:REALLITERAL";
    case AST_ARRAYLITERAL: 
        return "astType:ARRAYLITERAL";
    case AST_TRUE: 
        return "astType:TRUE";
    case AST_FALSE: 
        return "astType:FALSE";
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
    case AST_MODULEACCESS: 
        return "astType.MODULEACCESS";
    case AST_NOP:
        return "astType:NOP";
    }
    printf("Unknown astType: %d\n", type);
    NOT_REACHED();
    return "";
}

/*
    Determines if the list element is the beginning of a comment delimiter */
static int matchComment(struct list* list, struct listElem* elem) {
    if(elem != list_end(list) && list_next(elem) != list_end(list)) {
        if( ((struct token*)elem->data)->type == TOKEN_TILDE &&  
            ((struct token*)list_next(elem)->data)->type == TOKEN_TILDE) {
            return 1;
        } else {
            return 0;
        }
    } else {
        return 0;
    }
}

static void condenseArrayIdentifiers(struct list* tokenQueue) {
    struct listElem* elem;
    for(elem = list_begin(tokenQueue); elem != list_end(tokenQueue); elem = list_next(elem)) {
        if(list_next(elem) != list_end(tokenQueue) &&
        ((struct token*)elem->data)->type == TOKEN_IDENTIFIER &&
        ((struct token*)(list_next(elem)->data))->type == TOKEN_ARRAY ) {
            free(list_remove(tokenQueue, list_next(elem)));
            strncat(((struct token*)elem->data)->data, " array", 255);
            elem = elem->prev;
        }
    }
}

/*
    Pops a token off from the front of a queue, copies the string data of that
    token into a given string. Max size is 255 characters, including null term. */
static void copyNextTokenString(struct list* tokenQueue, char* dest) {
    assertPeek(tokenQueue, TOKEN_IDENTIFIER);
    struct token* nextToken = (struct token*) queue_pop(tokenQueue);
    strncpy(dest, nextToken->data, 254);
    free(nextToken);
}

/*
    Takes in a token queue, parses out a variable from it. Variables are marked constant
    automatically if needed */
static struct variable* createVar(struct list* tokenQueue, struct block* block) {
    struct variable* retval = (struct variable*)malloc(sizeof(struct variable));
    strcpy(retval->varType, "variable");
    struct token* topToken = queue_peek(tokenQueue);
    retval->filename = topToken->filename;
    retval->line = topToken->line;

    if(((struct token*)queue_peek(tokenQueue))->type == TOKEN_CONST) {
        retval->isConstant = 1;
        free(queue_pop(tokenQueue));
    }

    assertPeek(tokenQueue, TOKEN_IDENTIFIER);
    copyNextTokenString(tokenQueue, retval->type);
    assertPeek(tokenQueue, TOKEN_IDENTIFIER);
    copyNextTokenString(tokenQueue, retval->name);

    if(((struct token*)queue_peek(tokenQueue))->type == TOKEN_EQUALS) {
        assertRemove(tokenQueue, TOKEN_EQUALS);
        retval->code = parser_createAST(tokenQueue, block);
    } else if(((struct token*)queue_peek(tokenQueue))->type == TOKEN_SEMICOLON) {
        retval->code = NULL;
    }
    return retval;
}

/*
    Takes in a token queue, reads in the parameters and adds both the types and
    names to lists. The lists are ordered and pairs are in the same indices.
    
    Parenthesis are removed in this function as well.*/
static void parseParams(struct list* tokenQueue, struct map* argMap, struct variable* variable) {
    assertRemove(tokenQueue, TOKEN_LPAREN);

    // Parse parameters of function
    while(((struct token*)queue_peek(tokenQueue))->type != TOKEN_RPAREN) {
        struct variable* arg = createVar(tokenQueue, NULL);
        struct token* topToken = queue_peek(tokenQueue);
        if(map_put(argMap, arg->name, arg)) {
            error(topToken->filename, topToken->line, "Parameter \"%s\" defined in more than one place in %s \"%s\"!\n", arg->name, variable->varType, variable->name);
        }
    
        if(((struct token*)queue_peek(tokenQueue))->type == TOKEN_COMMA) {
            free(queue_pop(tokenQueue));
        }
        LOG("New arg: %s %s", arg->type, arg->name);
    }
    assertRemove(tokenQueue, TOKEN_RPAREN);
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
static struct astNode* createAST(enum astType type, const char* filename, int line) {
    struct astNode* retval = (struct astNode*) calloc(1, sizeof(struct astNode));
    retval->type = type;
    retval->children = list_create();
    retval->filename = filename;
    retval->line = line;
    return retval;
}

/*
    Given a token queue, extracts the front expression, parses it into an
    Abstract Syntax Tree. */
static struct astNode* createExpressionAST(struct list* tokenQueue) {
    ASSERT(tokenQueue != NULL);
    struct astNode* astNode = NULL;
    struct token* token = NULL;
    struct list* rawExpression = nextExpression(tokenQueue);
    if(list_isEmpty(rawExpression)) {
        return NULL;
    }

    struct list* expression = infixToPostfix(simplifyTokens(rawExpression));
    struct list* argStack = list_create();
    
    while (!list_isEmpty(expression)) {
        struct listElem* elem = NULL;
        token = (struct token*)queue_pop(expression);
        astNode = createAST(AST_NOP, token->filename, token->line);
        int* intData;
        float* realData;
        char* charData;
        astNode->type = tokenToAST(token->type);

        switch(token->type) {
        case TOKEN_INTLITERAL:
            intData = (int*)malloc(sizeof(int));
            *intData = atoi(token->data);
            astNode->data = intData;
            stack_push(argStack, astNode);
            break;
        case TOKEN_REALLITERAL:
            realData = (float*)malloc(sizeof(float));
            *realData = atof(token->data);
            astNode->data = realData;
            stack_push(argStack, astNode);
            break;
        case TOKEN_CALL:
            // Add args of call token (which are already ASTs) to new AST node's children
            for(elem = list_begin(token->list); elem != list_end(token->list); elem = list_next(elem)) {
                queue_push(astNode->children, (struct astNode*)elem->data);
            }
        case TOKEN_STRINGLITERAL:
        case TOKEN_CHARLITERAL:
        case TOKEN_IDENTIFIER:
        case TOKEN_TRUE:
        case TOKEN_FALSE:
            charData = (char*)malloc(sizeof(char) * 255);
            strncpy(charData, token->data, 254);
            astNode->data = charData;
            stack_push(argStack, astNode);
            break;
        default: // Assume operator
            assertOperator(astNode->type, token->filename, token->line);
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
        // SEMICOLON EXIT
        if(nextType == TOKEN_SEMICOLON) {
            break;
        }
        // BRACE EXIT
        if(nextType == TOKEN_LBRACE) {
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
        LOG("%s", lexer_tokenToString(nextType));
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
        // CALL
        if(matchTokens(tokenQueue, CALL, 2)) {
            assertPeek(tokenQueue, TOKEN_IDENTIFIER);
            struct token* callName = ((struct token*)queue_pop(tokenQueue));
            struct token* call = lexer_createToken(TOKEN_CALL, callName->data);
            call->line = callName->line;
            free(callName);

            assertRemove(tokenQueue, TOKEN_LPAREN);
            
            // Go through each argument, create AST representing it
            while(!list_isEmpty(tokenQueue) && ((struct token*)queue_peek(tokenQueue))->type != TOKEN_RPAREN) {
                struct astNode* argAST = createExpressionAST(tokenQueue);
                queue_push(call->list, argAST);

                if(((struct token*)queue_peek(tokenQueue))->type == TOKEN_COMMA) {
                    free(queue_pop(tokenQueue));
                }
            }
            assertRemove(tokenQueue, TOKEN_RPAREN);

            queue_push(retval, call);
        } 
        // INDEX
        else if(matchTokens(tokenQueue, INDEX, 1)) {
            queue_push(retval, lexer_createToken(TOKEN_INDEX, ""));
            queue_push(retval, lexer_createToken(TOKEN_LPAREN, "("));
            assertRemove(tokenQueue, TOKEN_LSQUARE);

            // extract expression for index, add to retval list, between anonymous parens
            struct list* innerExpression = nextExpression(tokenQueue);
            struct listElem* elem;
            for(elem = list_begin(innerExpression); elem != list_end(innerExpression); elem=list_next(elem)) {
                queue_push(retval, elem->data);
            }

            assertRemove(tokenQueue, TOKEN_RSQUARE);
            queue_push(retval, lexer_createToken(TOKEN_RPAREN, ")"));
        }
        // OTHER (just add to queue)
        else {
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
        if(token->type == TOKEN_IDENTIFIER || token->type == TOKEN_INTLITERAL  || token->type == TOKEN_REALLITERAL 
            || token->type == TOKEN_CALL || token->type == TOKEN_CHARLITERAL
            || token->type == TOKEN_STRINGLITERAL || token->type == TOKEN_FALSE || token->type == TOKEN_TRUE) {
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
    operators. Must have a one-to-one mapping. TOKEN_LPAREN for example, 
    does not have a one-to-one mapping with any AST type.
    
    Called by createExpression to convert operator tokens into ASTs */
static enum astType tokenToAST(enum tokenType type) {
    switch(type) {
    case TOKEN_PLUS:
        return AST_ADD;
    case TOKEN_MINUS: 
        return AST_SUBTRACT;
    case TOKEN_STAR: 
        return AST_MULTIPLY;
    case TOKEN_SLASH: 
        return AST_DIVIDE;
    case TOKEN_EQUALS:
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
    case TOKEN_COLON:
        return AST_MODULEACCESS;
    case TOKEN_INTLITERAL:
        return AST_INTLITERAL;
    case TOKEN_REALLITERAL:
        return AST_REALLITERAL;
    case TOKEN_CHARLITERAL:
        return AST_CHARLITERAL;
    case TOKEN_STRINGLITERAL:
        return AST_STRINGLITERAL;
    case TOKEN_TRUE:
        return AST_FALSE;
    case TOKEN_FALSE:
        return AST_FALSE;
    default:
        printf("Cannot directly convert %s to AST\n", lexer_tokenToString(type));
        NOT_REACHED();
    }
}

/*
    Verifies that the next token is what is expected. Errors otherwise */
static void assertPeek(struct list* tokenQueue, enum tokenType expected) {
    struct token* topToken = (struct token*) queue_peek(tokenQueue);
    enum tokenType actual = topToken->type;
    if(actual != expected) {
        error(topToken->filename, topToken->line, "Unexpected token %s, expected %s", lexer_tokenToString(actual), lexer_tokenToString(expected));
    }
}

/*
    Verifies that the next token is what is expected, and removes it. Errors
    otherwise */
static void assertRemove(struct list* tokenQueue, enum tokenType expected) {
    struct token* topToken = (struct token*) queue_peek(tokenQueue);
    enum tokenType actual = topToken->type;
    if(actual == expected) {
        free(queue_pop(tokenQueue));
    } else {
        error(topToken->filename, topToken->line, "Unexpected token %s, expected %s", lexer_tokenToString(actual), lexer_tokenToString(expected));
    }
}

/*
    Verifies that the next token is an operator. Errors otherwise */
static void assertOperator(enum astType type, const char* filename, int line) {
    switch(type) {
    case AST_ADD:
    case AST_SUBTRACT:
    case AST_MULTIPLY:
    case AST_DIVIDE:
    case AST_ASSIGN:
    case AST_IS: 
    case AST_ISNT: 
    case AST_GREATER:
    case AST_LESSER:
    case AST_AND:
    case AST_OR: 
    case AST_DOT:
    case AST_INDEX:
    case AST_MODULEACCESS:
        return;
    default:
        error(filename, line, "Operator stack corrupted, %s was assumed to be operator!\n", parser_astToString(type));
    }
}