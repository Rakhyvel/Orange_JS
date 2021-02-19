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

#include "./debug.h"
#include "./parser.h"
#include "./lexer.h"
#include "./util/list.h"
#include "./util/map.h"

// Higher level token signatures
static const enum tokenType MODULE[] = {TOKEN_MODULE, TOKEN_IDENTIFIER, TOKEN_NEWLINE};
static const enum tokenType STRUCT[] = {TOKEN_STRUCT, TOKEN_IDENTIFIER};
static const enum tokenType FUNCTION[] = {TOKEN_IDENTIFIER, TOKEN_IDENTIFIER, TOKEN_LPAREN};
static const enum tokenType CONST[] = {TOKEN_CONST, TOKEN_IDENTIFIER, TOKEN_IDENTIFIER, TOKEN_ASSIGN};

// Lower level token signatures
static const enum tokenType VARDECLARE[] = {TOKEN_IDENTIFIER, TOKEN_IDENTIFIER, TOKEN_NEWLINE};
static const enum tokenType VARDEFINE[] = {TOKEN_IDENTIFIER, TOKEN_IDENTIFIER, TOKEN_ASSIGN};
static const enum tokenType IF[] = {TOKEN_IF};
static const enum tokenType WHILE[] = {TOKEN_WHILE};
static const enum tokenType RETURN[] = {TOKEN_RETURN};
static const enum tokenType CALL[] = {TOKEN_IDENTIFIER, TOKEN_LPAREN};
static const enum tokenType INDEX[] = {TOKEN_LSQUARE};

// Private functions
static int matchComment(struct list*, struct listElem*);
static void condenseArrayIdentifiers(struct list*);
static void rejectUselessNewLines(struct list*);
static void copyNextTokenString(struct list*, char*);
static void parseParams(struct list*, struct list*, struct list*);
static struct astNode* createBlockAST(struct list*);
static int matchTokens(struct list*, const enum tokenType[], int);
static struct astNode* createAST(enum astType);
static struct astNode* createExpressionAST(struct list*);
static struct list* nextExpression(struct list*);
static struct list* simplifyTokens(struct list*);
static struct list* infixToPostfix(struct list*);
static enum astType tokenToAST(enum tokenType);
static void assertPeek(struct list*, enum tokenType);
static void assertRemove(struct list*, enum tokenType);
static void assertOperator(enum astType);

/*
    Allocates and initializes the program struct */
struct program* parser_initProgram() {
    struct program* retval = (struct program*) calloc(1, sizeof(struct program));
    retval->modulesMap = map_init();
    return retval;
}

/*
    Allocates and initializes a module struct */
struct module* parser_initModule(struct program* program) {
    struct module* retval = (struct module*) calloc(1, sizeof(struct module));
    retval->functionsMap = map_init();
    retval->dataStructsMap = map_init();
    retval->globalsMap = map_init();
    retval->program = program;
    return retval;
}

/*
    Allocates and initializes a function struct */
struct function* parser_initFunction() {
    struct function* retval = (struct function*) calloc(1, sizeof(struct function));
    retval->argTypes = list_create();
    retval->argNames = list_create();
    return retval;
}

/*
    Allocates and initializes a data struct */
struct dataStruct* parser_initDataStruct() {
    struct dataStruct* retval = (struct dataStruct*) calloc(1, sizeof(struct dataStruct));
    retval->argTypes = list_create();
    retval->argNames = list_create();
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
        rejectUselessNewLines(tokenQueue);
        int isStatic = ((struct token*)queue_peek(tokenQueue))->type == TOKEN_STATIC;
        if(isStatic) free(queue_pop(tokenQueue));

        if(matchTokens(tokenQueue, MODULE, 3)) {
            struct module* module = parser_initModule(program);
            assertRemove(tokenQueue, TOKEN_MODULE);
            copyNextTokenString(tokenQueue, module->name);
            LOG("New module: %s", module->name);
            assertRemove(tokenQueue, TOKEN_NEWLINE);

            map_put(program->modulesMap, module->name, module);
            module->isStatic = isStatic;
            parser_addElements(module, tokenQueue);

            assertRemove(tokenQueue, TOKEN_END);
        } else if(((struct token*) queue_peek(tokenQueue))->type == TOKEN_EOF) {
            free(queue_pop(tokenQueue));
        } else {
            fprintf(stderr, "Unnecessary token %s found\n", ((struct token*)queue_peek(tokenQueue))->data);
            exit(1);
        }
    }   
}

/*
    Takes in a token queue, adds functions, structs, and globals to a given 
    module, including any internal AST's. */
void parser_addElements(struct module* module, struct list* tokenQueue) {
    rejectUselessNewLines(tokenQueue);

    // Find all functions until a function's end is reached
    while(!list_isEmpty(tokenQueue) && 
    ((struct token*) queue_peek(tokenQueue))->type != TOKEN_END) {
        rejectUselessNewLines(tokenQueue);
        int isPrivate = ((struct token*)queue_peek(tokenQueue))->type == TOKEN_PRIVATE;
        if(isPrivate) free(queue_pop(tokenQueue));

        // STRUCT
        if(matchTokens(tokenQueue, STRUCT, 2)) {
            assertRemove(tokenQueue, TOKEN_STRUCT);
            struct dataStruct* dataStruct = parser_initDataStruct();
            copyNextTokenString(tokenQueue, dataStruct->name);
            if(((struct token*) queue_peek(tokenQueue))->type == TOKEN_LSQUARE) {
                assertRemove(tokenQueue, TOKEN_LSQUARE);
                assertPeek(tokenQueue, TOKEN_IDENTIFIER);
                copyNextTokenString(tokenQueue, dataStruct->baseStruct);
                assertRemove(tokenQueue, TOKEN_RSQUARE);
            }
            LOG("New struct: %s", dataStruct->name);

            parseParams(tokenQueue, dataStruct->argTypes, dataStruct->argNames);
            map_put(module->dataStructsMap, dataStruct->name, dataStruct);
            dataStruct->module = module;
            dataStruct->program = module->program;
            dataStruct->isPrivate = isPrivate;
        }
        // FUNCTION
        else if(matchTokens(tokenQueue, FUNCTION, 3)) {
            struct function* function = parser_initFunction();
            copyNextTokenString(tokenQueue, function->returnType);
            copyNextTokenString(tokenQueue, function->name);
            LOG("New function: %s %s", function->returnType, function->name);

            parseParams(tokenQueue, function->argTypes, function->argNames);
            function->code = createBlockAST(tokenQueue);
            assertRemove(tokenQueue, TOKEN_END);
            LOG("Function %s %s's AST", function->returnType, function->name);

            parser_printAST(function->code, 0);
            map_put(module->functionsMap, function->name, function);
            function->module = module;
            function->program = module->program;
            function->isPrivate = isPrivate;
        } 
        // CONST
        else if (matchTokens(tokenQueue, CONST, 4)) {
            free(queue_pop(tokenQueue));
            struct astNode* var = parser_createAST(tokenQueue);
            var->isConstant = 1;
            map_put(module->globalsMap, var->varName, var);
            var->isPrivate = isPrivate;
        }
        // GLOBAL
        else if (matchTokens(tokenQueue, VARDECLARE, 3) || matchTokens(tokenQueue, VARDEFINE, 3)) {
            struct astNode* var = parser_createAST(tokenQueue);
            map_put(module->globalsMap, var->varName, var);
            var->isPrivate = isPrivate;
        }
        // ERROR
        else {
            fprintf(stderr, "\nUnexpected token %s\n in module %s", ((struct token*)queue_peek(tokenQueue))->data, module->name);
            exit(1);
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

    // VARIABLE
    if (matchTokens(tokenQueue, VARDECLARE, 3) || matchTokens(tokenQueue, VARDEFINE, 3)) {
        retval = createAST(AST_VARDECLARE);
        assertPeek(tokenQueue, TOKEN_IDENTIFIER);
        copyNextTokenString(tokenQueue, retval->varType);
        assertPeek(tokenQueue, TOKEN_IDENTIFIER);
        copyNextTokenString(tokenQueue, retval->varName);

        if(((struct token*)queue_peek(tokenQueue))->type == TOKEN_ASSIGN) {
            retval->type = AST_VARDEFINE;
            assertRemove(tokenQueue, TOKEN_ASSIGN);
            queue_push(retval->children, parser_createAST(tokenQueue));
        }
    }
    // IF
    else if (matchTokens(tokenQueue, IF, 1)) {
        retval = createAST(AST_IF);
        assertRemove(tokenQueue, TOKEN_IF);
        struct astNode* expression = parser_createAST(tokenQueue);
        struct astNode* body = createBlockAST(tokenQueue);
        queue_push(retval->children, expression);
        queue_push(retval->children, body);
        if(((struct token*)queue_peek(tokenQueue))->type == TOKEN_ELSE) {
            assertRemove(tokenQueue, TOKEN_ELSE);
            int removeEnd = ((struct token*)queue_peek(tokenQueue))->type != TOKEN_IF;
            queue_push(retval->children, parser_createAST(tokenQueue));
            if(removeEnd) {
                assertRemove(tokenQueue, TOKEN_END);
            }
        }
    }
    // WHILE
    else if (matchTokens(tokenQueue, WHILE, 1)) {
        retval = createAST(AST_WHILE);
        assertRemove(tokenQueue, TOKEN_WHILE);
        struct astNode* expression = parser_createAST(tokenQueue);
        struct astNode* body = createBlockAST(tokenQueue);
            assertRemove(tokenQueue, TOKEN_END);
        queue_push(retval->children, expression);
        queue_push(retval->children, body);
    }
    // RETURN
    else if (matchTokens(tokenQueue, RETURN, 1)) {
        retval = createAST(AST_RETURN);
            assertRemove(tokenQueue, TOKEN_RETURN);
        struct astNode* expression = parser_createAST(tokenQueue);
        queue_push(retval->children, expression);
    }
    // EXPRESSION
    else {
        retval = createExpressionAST(tokenQueue);
            assertRemove(tokenQueue, TOKEN_NEWLINE);
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
    if(node->varType[0] != '\0' || node->varName[0] != '\0') {
        for(int i = 0; i < n + 1; i++) printf("  ");
        LOG("%s:%s", node->varName, node->varType);
    }

    // Go through children of passed in AST node
    struct listElem* elem = NULL;
    for(elem = list_begin(node->children); elem != list_end(node->children); elem = list_next(elem)) {
        if(node->type == AST_INTLITERAL) {
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
        } else if(node->type == AST_CALL) {
            parser_printAST(((struct astNode*)elem->data), n+1);
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
    case AST_WHILE: 
        return "astType.WHILE";
    case AST_RETURN: 
        return "astType.RETURN";
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
    case AST_INTLITERAL: 
        return "astType:INTLITERAL";
    case AST_REALLITERAL: 
        return "astType:REALLITERAL";
    case AST_ARRAYLITERAL: 
        return "astType:ARRAYLITERAL";
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
    Takes in a queue of tokens, and rejects all newlines that are at the front.
    
    It is up to the caller to use this apropiately, and not damage the 
    structure of the code, since formatting matters in this language. */
static void rejectUselessNewLines(struct list* tokenQueue) {
    while(((struct token*) queue_peek(tokenQueue))->type == TOKEN_NEWLINE) {
        assertRemove(tokenQueue, TOKEN_NEWLINE);
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
    Takes in a token queue, reads in the parameters and adds both the types and
    names to lists. The lists are ordered and pairs are in the same indices.
    
    Parenthesis are removed in this function as well.*/
static void parseParams(struct list* tokenQueue, struct list* argTypes, struct list* argNames) {
    assertRemove(tokenQueue, TOKEN_LPAREN);

    // Parse parameters of function
    while(((struct token*)queue_peek(tokenQueue))->type != TOKEN_RPAREN) {
        char* argType = (char*)malloc(sizeof(char) * 255);
        char* argName = (char*)malloc(sizeof(char) * 255);
        assertPeek(tokenQueue, TOKEN_IDENTIFIER);
        copyNextTokenString(tokenQueue, argType);
        assertPeek(tokenQueue, TOKEN_IDENTIFIER);
        copyNextTokenString(tokenQueue, argName);
        queue_push(argTypes, argType);
        queue_push(argNames, argName);
    
        if(((struct token*)queue_peek(tokenQueue))->type == TOKEN_COMMA) {
            free(queue_pop(tokenQueue));
        }
        LOG("New arg: %s %s", argType, argName);
    }
    assertRemove(tokenQueue, TOKEN_RPAREN);
}

/*
    Takes in a token queue, parses out a block of statements which end with an 
    end token */
static struct astNode* createBlockAST(struct list* tokenQueue) {
    struct astNode* block = createAST(AST_BLOCK);
    rejectUselessNewLines(tokenQueue);
    // Go through statements in token queue until an end token is found
    while(!list_isEmpty(tokenQueue) && ((struct token*)queue_peek(tokenQueue))->type != TOKEN_END &&
            ((struct token*)queue_peek(tokenQueue))->type != TOKEN_ELSE) {
        queue_push(block->children, parser_createAST(tokenQueue));
        rejectUselessNewLines(tokenQueue);
    }
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
    struct astNode* retval = (struct astNode*) calloc(1, sizeof(struct astNode));
    retval->type = type;
    retval->children = list_create();
    retval->modifiers = list_create();
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
        float* realData;
        char* charData;
        astNode->type = tokenToAST(token->type);

        switch(token->type) {
        case TOKEN_INTLITERAL:
            intData = (int*)malloc(sizeof(int));
            *intData = atoi(token->data);
            queue_push(astNode->children, intData);
            stack_push(argStack, astNode);
            break;
        case TOKEN_REALLITERAL:
            realData = (float*)malloc(sizeof(float));
            *realData = atof(token->data);
            queue_push(astNode->children, realData);
            stack_push(argStack, astNode);
            break;
        case TOKEN_STRINGLITERAL:
        case TOKEN_CHARLITERAL:
            charData = (char*)malloc(sizeof(char) * 255);
            strncpy(charData, token->data, 254);
            queue_push(astNode->children, charData);
            stack_push(argStack, astNode);
            break;
        case TOKEN_IDENTIFIER:
            charData = (char*)malloc(sizeof(char) * 255);
            strncpy(charData, token->data, 254);
            queue_push(astNode->children, charData);
            stack_push(argStack, astNode);
            break;
        case TOKEN_CALL:
            // Add args of call token (which are already ASTs) to new AST node's children
            for(elem = list_begin(token->list); elem != list_end(token->list); elem = list_next(elem)) {
                queue_push(astNode->children, (struct astNode*)elem->data);
            }
            strncpy(astNode->varName, token->data, 254);
            stack_push(argStack, astNode);
            break;
        default: // Assume operator
            assertOperator(astNode->type);
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
        // CALL
        if(matchTokens(tokenQueue, CALL, 2)) {
            assertPeek(tokenQueue, TOKEN_IDENTIFIER);
            struct token* callName = ((struct token*)queue_pop(tokenQueue));
            struct token* call = lexer_createToken(TOKEN_CALL, callName->data);
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
            || token->type == TOKEN_STRINGLITERAL) {
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
    default:
        printf("Cannot directly convert %s to AST\n", lexer_tokenToString(type));
        NOT_REACHED();
    }
}

static void assertPeek(struct list* tokenQueue, enum tokenType expected) {
    enum tokenType actual = ((struct token*) queue_peek(tokenQueue))->type;
    if(actual != expected) {
        fprintf(stderr, "ERROR: unexpected token %s, expected %s\n", lexer_tokenToString(actual), lexer_tokenToString(expected));
        exit(1);
    }
}

static void assertRemove(struct list* tokenQueue, enum tokenType expected) {
    enum tokenType actual = ((struct token*) queue_peek(tokenQueue))->type;
    if(actual == expected) {
        free(queue_pop(tokenQueue));
    } else {
        fprintf(stderr, "ERROR: unexpected token %s, expected %s\n", lexer_tokenToString(actual), lexer_tokenToString(expected));
        exit(1);
    }
}

static void assertOperator(enum astType type) {
    switch(type) {
    case AST_PLUS:
    case AST_MINUS:
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
        fprintf(stderr, "ERROR: operator stack corrupted, %s was assumed to be operator!\n", parser_astToString(type));
        exit(1);
    }
}