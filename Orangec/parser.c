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
static const enum tokenType MODULE[] = {TOKEN_IDENTIFIER, TOKEN_LBRACE};
static const enum tokenType STRUCT[] = {TOKEN_STRUCT, TOKEN_IDENTIFIER, TOKEN_LPAREN};
static const enum tokenType VARDECLARE[] = {TOKEN_IDENTIFIER, TOKEN_IDENTIFIER, TOKEN_SEMICOLON};
static const enum tokenType VARDEFINE[] = {TOKEN_IDENTIFIER, TOKEN_IDENTIFIER, TOKEN_EQUALS};
static const enum tokenType FUNCTION[] = {TOKEN_IDENTIFIER, TOKEN_IDENTIFIER, TOKEN_LPAREN};
static const enum tokenType CALL[] = {TOKEN_IDENTIFIER, TOKEN_LPAREN};
static int num_ids = 0;

// Private functions
static int topMatches(struct list*, enum tokenType);
static int matchTokens(struct list*, const enum tokenType[], int);
static void copyNextTokenString(struct list*, char*);
static void parseParams(struct list*, struct symbolNode*);
char* itoa(int val);
static void initSymbolPath(char*, struct symbolNode*);
static struct astNode* createAST(enum astType, const char*, int, struct symbolNode*);
static struct astNode* createExpressionAST(struct list*, struct symbolNode*);
static struct list* nextExpression(struct list*);
static struct list* simplifyTokens(struct list*, struct symbolNode*);
static struct list* infixToPostfix(struct list*);
static enum astType tokenToAST(enum tokenType);
static void assertPeek(struct list*, enum tokenType);
static void assertRemove(struct list*, enum tokenType);
static void assertOperator(enum astType, const char*, int);

/*
    Allocates and initializes the program struct */
struct symbolNode* parser_createSymbolNode(enum symbolType symbolType, struct symbolNode* parent, const char* filename, int line) {
    struct symbolNode* retval = (struct symbolNode*)calloc(1, sizeof(struct symbolNode));

    retval->symbolType = symbolType;
    retval->parent = parent;
    retval->children = map_create();
    retval->filename = filename;
    retval->line = line;

    return retval;
}

/*
    Removes tokens from a list that are surrounded by comments */
void parser_removeComments(struct list* tokenQueue) {
    struct listElem* elem;
    int withinComment = 0;
    int tokenLine = -1;

    for(elem=list_begin(tokenQueue);elem!=list_end(tokenQueue);elem=list_next(elem)) {
        if(withinComment == 1) {
            if(((struct token*)elem->data)->type == TOKEN_RBLOCK) {
                withinComment = 0;
                elem = elem->prev;
                free(list_remove(tokenQueue, list_next(elem)));
            } else {
                elem = elem->prev;
                free(list_remove(tokenQueue, list_next(elem)));
            }
        } else if(withinComment == 2) {
            if(((struct token*)elem->data)->line != tokenLine) {
                withinComment = 0;
                tokenLine = -1;
                elem = elem->prev;
            } else {
                elem = elem->prev;
                free(list_remove(tokenQueue, list_next(elem)));
            }
        } else {
            if(((struct token*)elem->data)->type == TOKEN_LBLOCK) {
                withinComment = 1;
                elem = elem->prev; // iterated at end of for loop
                free(list_remove(tokenQueue, list_next(elem)));
            } else if(((struct token*)elem->data)->type == TOKEN_DSLASH) {
                withinComment = 2;
                tokenLine = ((struct token*)elem->data)->line;
                elem = elem->prev; // iterated at end of for loop
                free(list_remove(tokenQueue, list_next(elem)));
            }
        }
    }
}

void parser_condenseArrayIdentifiers(struct list* tokenQueue) {
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

struct symbolNode* parser_parseTokens(struct list* tokenQueue, struct symbolNode* parent) {
    struct symbolNode* symbolNode;
    int isPrivate = topMatches(tokenQueue, TOKEN_PRIVATE);
    if(isPrivate) free(queue_pop(tokenQueue));
    int isStatic = topMatches(tokenQueue, TOKEN_STATIC);
    if(isStatic) free(queue_pop(tokenQueue));
    int isConstant = topMatches(tokenQueue, TOKEN_CONST);
    if(isConstant) free(queue_pop(tokenQueue));

    struct token* topToken = queue_peek(tokenQueue);
    // END OF MODULE, RETURN
    if(topMatches(tokenQueue, TOKEN_RBRACE)) {
        return NULL;
    }
    // MODULE
    else if(matchTokens(tokenQueue, MODULE, 2)) {
        symbolNode = parser_createSymbolNode(SYMBOL_MODULE, parent, topToken->filename, topToken->line);
        symbolNode->isStatic = isStatic;
        copyNextTokenString(tokenQueue, symbolNode->name);
        assertRemove(tokenQueue, TOKEN_LBRACE);
        struct symbolNode* child;
        while((child = parser_parseTokens(tokenQueue, symbolNode)) != NULL) {
            map_put(symbolNode->children, child->name, child);
        }
        assertRemove(tokenQueue, TOKEN_RBRACE);
        LOG("Module %s created %d", symbolNode->name, symbolNode->symbolType);
    }
    // STRUCT
    else if(matchTokens(tokenQueue, STRUCT, 3)) {
        symbolNode = parser_createSymbolNode(SYMBOL_STRUCT, parent, topToken->filename, topToken->line);
        symbolNode->isPrivate = isPrivate;
        symbolNode->isStatic = 1;
        assertRemove(tokenQueue, TOKEN_STRUCT);
        copyNextTokenString(tokenQueue, symbolNode->name);
        strcpy(symbolNode->type, symbolNode->name);
        parseParams(tokenQueue, symbolNode);
        LOG("Struct %s created", symbolNode->name);
    }
    // VARIABLE DEFINITION
    else if(matchTokens(tokenQueue, VARDEFINE, 3)) {
        symbolNode = parser_createSymbolNode(SYMBOL_VARIABLE, parent, topToken->filename, topToken->line);
        symbolNode->isPrivate = isPrivate;
        symbolNode->isConstant = isConstant;
        symbolNode->isStatic = parent->isStatic;
        copyNextTokenString(tokenQueue, symbolNode->type);
        copyNextTokenString(tokenQueue, symbolNode->name);
        assertRemove(tokenQueue, TOKEN_EQUALS);
        symbolNode->code = parser_createAST(tokenQueue, symbolNode);
        assertRemove(tokenQueue, TOKEN_SEMICOLON);
        LOG("Variable %s created", symbolNode->name);
    // VARIABLE DECLARATION
    } else if(matchTokens(tokenQueue, VARDECLARE, 3)) {
        symbolNode = parser_createSymbolNode(SYMBOL_VARIABLE, parent, topToken->filename, topToken->line);
        symbolNode->isPrivate = isPrivate;
        symbolNode->isConstant = isConstant;
        symbolNode->isStatic = parent->isStatic || parent->symbolType == SYMBOL_MODULE;
        copyNextTokenString(tokenQueue, symbolNode->type);
        copyNextTokenString(tokenQueue, symbolNode->name);
        assertRemove(tokenQueue, TOKEN_SEMICOLON);
        LOG("Variable %s created", symbolNode->name);
    // FUNCTION DECLARATION
    } else if(matchTokens(tokenQueue, FUNCTION, 3)) {
        symbolNode = parser_createSymbolNode(SYMBOL_FUNCTION, parent, topToken->filename, topToken->line);
        symbolNode->isPrivate = isPrivate;
        symbolNode->isConstant = isConstant;
        symbolNode->isStatic = parent->isStatic;
        copyNextTokenString(tokenQueue, symbolNode->type);
        copyNextTokenString(tokenQueue, symbolNode->name);
        parseParams(tokenQueue, symbolNode);
        if(topMatches(tokenQueue, TOKEN_EQUALS)) {
            assertRemove(tokenQueue, TOKEN_EQUALS);
            symbolNode->code = parser_createAST(tokenQueue, symbolNode);
            assertRemove(tokenQueue, TOKEN_SEMICOLON);
        } else if(topMatches(tokenQueue, TOKEN_LBRACE)) {
            symbolNode->code = parser_createAST(tokenQueue, symbolNode);
        } else {
            // error
        }
        LOG("Function %s created", symbolNode->name);
    } else if(topMatches(tokenQueue, TOKEN_EOF)){
        return NULL;
    } else {
        printf("%s\n", lexer_tokenToString(((struct token*) queue_peek(tokenQueue))->type));
    }
    initSymbolPath(symbolNode->path, symbolNode);
    return symbolNode;
}

/*
    Creates an AST for code given a queue of tokens. Only parses one 
    instruction per call. */
struct astNode* parser_createAST(struct list* tokenQueue, struct symbolNode* scope) {
    ASSERT(tokenQueue != NULL);
    ASSERT(scope != NULL);
    struct astNode* retval = NULL;
    struct token* topToken = queue_peek(tokenQueue);

    // BLOCK
    if(topMatches(tokenQueue, TOKEN_LBRACE)) {
        retval = createAST(AST_BLOCK, topToken->filename, topToken->line, scope);
        retval->data = parser_createSymbolNode(SYMBOL_BLOCK, scope, topToken->filename, topToken->line);
        strcpy(((struct symbolNode*)retval->data)->name, "_block");
        strcat(((struct symbolNode*)retval->data)->name, itoa(num_ids++));
        strcpy(((struct symbolNode*)retval->data)->type, scope->type);
        map_put(scope->children, ((struct symbolNode*)retval->data)->name, retval->data);
        assertRemove(tokenQueue, TOKEN_LBRACE);
        while(!list_isEmpty(tokenQueue) && !topMatches(tokenQueue, TOKEN_RBRACE)) {
            queue_push(retval->children, parser_createAST(tokenQueue, retval->data));
        }
        assertRemove(tokenQueue, TOKEN_RBRACE);
    }
    // SYMBOL DEFINITION/DECLARATION
    else if (matchTokens(tokenQueue, VARDECLARE, 3) || 
             matchTokens(tokenQueue, VARDEFINE, 3) || 
             matchTokens(tokenQueue, FUNCTION, 3) || 
             matchTokens(tokenQueue, STRUCT, 3)) {
        LOG("HELLO");
        retval = createAST(AST_SYMBOLDEFINE, topToken->filename, topToken->line, scope);
        struct symbolNode* symbolNode = parser_parseTokens(tokenQueue, scope);
        retval->data = symbolNode;
        map_put(scope->children, symbolNode->name, symbolNode);
    }
    // IF
    else if (topMatches(tokenQueue, TOKEN_IF)) {
        retval = createAST(AST_IF, topToken->filename, topToken->line, scope);
        assertRemove(tokenQueue, TOKEN_IF);
        struct astNode* expression = parser_createAST(tokenQueue, scope);
        struct astNode* body = parser_createAST(tokenQueue, scope);
        if(body == NULL || body->type != AST_BLOCK) {
            error(retval->filename, retval->line, "If statements must be followed by block statements");
        }
        queue_push(retval->children, expression);
        queue_push(retval->children, body);
        if(topMatches(tokenQueue, TOKEN_ELSE)) {
            assertRemove(tokenQueue, TOKEN_ELSE);
            retval->type = AST_IFELSE;
            if(topMatches(tokenQueue, TOKEN_IF)) {
                queue_push(retval->children, parser_createAST(tokenQueue, scope));
            } else {
                queue_push(retval->children, parser_createAST(tokenQueue, scope));
            }
        }
    }
    // WHILE
    else if (topMatches(tokenQueue, TOKEN_WHILE)) {
        retval = createAST(AST_WHILE, topToken->filename, topToken->line, scope);
        assertRemove(tokenQueue, TOKEN_WHILE);
        struct astNode* expression = parser_createAST(tokenQueue, scope);
        struct astNode* body = parser_createAST(tokenQueue, scope);
        if(body == NULL || body->type != AST_BLOCK) {
            error(retval->filename, retval->line, "While statements must be followed by block statements");
        }
        queue_push(retval->children, expression);
        queue_push(retval->children, body);
    }
    // RETURN
    else if (topMatches(tokenQueue, TOKEN_RETURN)) {
        retval = createAST(AST_RETURN, topToken->filename, topToken->line, scope);
        assertRemove(tokenQueue, TOKEN_RETURN);
        struct astNode* expression = createExpressionAST(tokenQueue, scope);
        queue_push(retval->children, expression);
    }
    else if (topMatches(tokenQueue, TOKEN_SEMICOLON)) {
        assertRemove(tokenQueue, TOKEN_SEMICOLON);
    }
    // EXPRESSION
    else {
        retval = createExpressionAST(tokenQueue, scope);
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
    if(node->type == AST_SYMBOLDEFINE) {
        for(int i = 0; i < n; i++) printf("  ");
        LOG("%s %s =", ((struct symbolNode*)node->data)->type, (((struct symbolNode*)node->data)->name));
        parser_printAST(((struct symbolNode*)node->data)->code, n+1);
    } else if(node->type == AST_INTLITERAL) {
        for(int i = 0; i < n; i++) printf("  "); 
        printf("^-: "); // print spaces
        int* data = (int*)(node->data);
        LOG("%d", *data);
    } else if(node->type == AST_REALLITERAL) {
        for(int i = 0; i < n; i++) printf("  "); 
        printf("^-: "); // print spaces
        float* data = (float*)(node->data);
        LOG("%f", *data);
    } else if(node->type == AST_CHARLITERAL || node->type == AST_STRINGLITERAL) {
        for(int i = 0; i < n; i++) printf("  "); 
        printf("^-: "); // print spaces
        LOG("%s", (char*)(node->data));
    } else if(node->type == AST_VAR) {
        for(int i = 0; i < n; i++) printf("  "); 
        printf("^-: "); // print spaces
        LOG("%s", (char*)node->data);
    }

    // Go through children of passed in AST node
    struct listElem* elem = NULL;
    for(elem = list_begin(node->children); elem != list_end(node->children); elem = list_next(elem)) {
        if(elem->data == NULL) {
            continue;
        } else {
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
    case AST_SYMBOLDEFINE: 
        return "astType.SYMBOLDEFINE";
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
    case AST_CAST: 
        return "astType.CAST";
    case AST_NEW: 
        return "astType.NEW";
    case AST_FREE: 
        return "astType.FREE";
    case AST_GREATER: 
        return "astType.GREATER";
    case AST_LESSER: 
        return "astType.LESSER";
    case AST_GREATEREQUAL: 
        return "astType.GREATEREQUAL";
    case AST_LESSEREQUAL: 
        return "astType.LESSEREQUAL";
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
    case AST_NULL: 
        return "astType:NULL";
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

static int topMatches(struct list* tokenQueue, enum tokenType type) {
    return ((struct token*)queue_peek(tokenQueue))->type == type;
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
    Implementation from http://www.strudel.org.uk/itoa/ */
char* itoa(int val) {
	char* buf = (char*)malloc(32);
	int i = 30;
	for(; val && i ; --i, val /= 36)
		buf[i] = "0123456789abcdefghijklmnopqrstuvwxyz"[val % 36];
	return &buf[i+1];
}

/*
    Takes in a token queue, reads in the parameters and adds both the types and
    names to lists. The lists are ordered and pairs are in the same indices.
    
    Parenthesis are removed in this function as well.*/
static void parseParams(struct list* tokenQueue, struct symbolNode* parent) {
    assertRemove(tokenQueue, TOKEN_LPAREN);
    // Parse parameters of function
    while(!topMatches(tokenQueue, TOKEN_RPAREN)) {
        struct symbolNode* param = parser_createSymbolNode(SYMBOL_VARIABLE, parent, parent->filename, parent->line);
        copyNextTokenString(tokenQueue, param->type);
        if(!topMatches(tokenQueue, TOKEN_COMMA)) {
            copyNextTokenString(tokenQueue, param->name);
        } else {
            strcpy(param->name, "_undef");
            strcat(param->name, itoa(num_ids++));
            free(queue_pop(tokenQueue));
        }
        struct token* topToken = queue_peek(tokenQueue);
        if(map_put(parent->children, param->name, param)) {
            error(topToken->filename, topToken->line, "Parameter \"%s\" defined in more than one place", param->name);
        }
    
        if(topMatches(tokenQueue, TOKEN_COMMA)) {
            free(queue_pop(tokenQueue));
        }
        LOG("New arg: %s %s", param->type, param->name);
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

static void initSymbolPath(char* path, struct symbolNode* node) {
    switch (node->symbolType) {
    case SYMBOL_PROGRAM:
        break;
    default:
        initSymbolPath(path, node->parent);
        strcat(path, "_");
        strcat(path, node->name);
        break;
    }
}

/*
    Allocates and initializes an Abstract Syntax Tree node, with the proper type */
static struct astNode* createAST(enum astType type, const char* filename, int line, struct symbolNode* scope) {
    struct astNode* retval = (struct astNode*) calloc(1, sizeof(struct astNode));
    retval->type = type;
    retval->children = list_create();
    retval->filename = filename;
    retval->line = line;
    retval->scope = scope;
    return retval;
}

/*
    Given a token queue, extracts the front expression, parses it into an
    Abstract Syntax Tree. */
static struct astNode* createExpressionAST(struct list* tokenQueue, struct symbolNode* scope) {
    LOG("Create Expression AST");
    ASSERT(tokenQueue != NULL);
    struct astNode* astNode = NULL;
    struct token* token = NULL;
    struct list* rawExpression = nextExpression(tokenQueue);
    if(list_isEmpty(rawExpression)) {
        return NULL;
    }

    struct list* expression = infixToPostfix(simplifyTokens(rawExpression, scope));
    struct list* argStack = list_create();
    
    while (!list_isEmpty(expression)) {
        struct listElem* elem = NULL;
        token = (struct token*)queue_pop(expression);
        astNode = createAST(AST_NOP, token->filename, token->line, scope);
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
        case TOKEN_NULL:
            charData = (char*)malloc(sizeof(char) * 255);
            strncpy(charData, token->data, 254);
            astNode->data = charData;
            stack_push(argStack, astNode);
            break;
        default: // Assume operator
            charData = (char*)malloc(sizeof(char) * 255);
            strncpy(charData, token->data, 254);
            astNode->data = charData;
            assertOperator(astNode->type, token->filename, token->line);
            queue_push(astNode->children, stack_pop(argStack)); // Right
            if(astNode->type != AST_CAST && astNode->type != AST_NEW && astNode->type != AST_FREE) { // Don't do left side for unary operators
                queue_push(astNode->children, stack_pop(argStack)); // Left
            }
            stack_push(argStack, astNode);
            break;
        }
        free(token);
    }
    LOG("end Create Expression AST");
    return stack_peek(argStack);
}

/*
    Takes a queue of tokens, pops off the first expression and returns as a new
    queue. */
static struct list* nextExpression(struct list* tokenQueue) {
    LOG("Next expression");
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
        LOG("%s %s", lexer_tokenToString(nextType), ((struct token*)queue_peek(tokenQueue))->data);
        queue_push(retval, queue_pop(tokenQueue));
    }
    LOG("end Next Expression");
    return retval;
}

/*
    Takes in a queue representing an expression, and if it can, transforms 
    function calls/index token structures into proper call/index tokens */
static struct list* simplifyTokens(struct list* tokenQueue, struct symbolNode* scope) {
    LOG("Simplify Tokens");
    struct list* retval = list_create();

    // Go through each token in given expression token queue, add/reject/parse to retval
    while(!list_isEmpty(tokenQueue)) {
        // CALL
        if(matchTokens(tokenQueue, CALL, 2)) {
            assertPeek(tokenQueue, TOKEN_IDENTIFIER);
            struct token* callName = ((struct token*)queue_pop(tokenQueue));
            struct token* call = lexer_createToken(TOKEN_CALL, callName->data, callName->filename, callName->line);
            call->line = callName->line;
            free(callName);

            assertRemove(tokenQueue, TOKEN_LPAREN);
            
            // Go through each argument, create AST representing it
            while(!list_isEmpty(tokenQueue) && !topMatches(tokenQueue, TOKEN_RPAREN)) {
                struct astNode* argAST = createExpressionAST(tokenQueue, scope);
                queue_push(call->list, argAST);

                if(topMatches(tokenQueue, TOKEN_COMMA)) {
                    free(queue_pop(tokenQueue));
                }
            }
            assertRemove(tokenQueue, TOKEN_RPAREN);

            queue_push(retval, call);
        } 
        // INDEX
        else if(topMatches(tokenQueue, TOKEN_LSQUARE)) {
            struct token* topToken = queue_peek(tokenQueue);
            queue_push(retval, lexer_createToken(TOKEN_INDEX, "", topToken->filename, topToken->line));
            queue_push(retval, lexer_createToken(TOKEN_LPAREN, "(", topToken->filename, topToken->line));
            assertRemove(tokenQueue, TOKEN_LSQUARE);

            // extract expression for index, add to retval list, between anonymous parens
            struct list* innerExpression = infixToPostfix(simplifyTokens(nextExpression(tokenQueue), scope));
            struct listElem* elem;
            for(elem = list_begin(innerExpression); elem != list_end(innerExpression); elem=list_next(elem)) {
                queue_push(retval, elem->data);
            }

            assertRemove(tokenQueue, TOKEN_RSQUARE);
            queue_push(retval, lexer_createToken(TOKEN_RPAREN, ")", topToken->filename, topToken->line));
        }
        // CAST
        else if(topMatches(tokenQueue, TOKEN_CAST)) {
            struct token* cast = queue_pop(tokenQueue);
            assertRemove(tokenQueue, TOKEN_LPAREN);
            struct token* type = queue_pop(tokenQueue);
            strcpy(cast->data, type->data);
            assertRemove(tokenQueue, TOKEN_RPAREN);
            free(type);
            queue_push(retval, cast);
        }
        // OTHER (just add to queue)
        else {
            queue_push(retval, queue_pop(tokenQueue));
        }
    }
    LOG("end Simplify Tokens");
    return retval;
}

/*
    Consumes a queue of tokens representing an expression in infix order, 
    converts it to postfix order */
static struct list* infixToPostfix(struct list* tokenQueue) {
    LOG("Infix to Postfix");
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
            ASSERT(!list_isEmpty(opStack));
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
    LOG("end Infix to Postfix");
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
    case TOKEN_GREATEREQUAL: 
        return AST_GREATEREQUAL;
    case TOKEN_LESSEREQUAL:
        return AST_LESSEREQUAL;
	case TOKEN_AND: 
        return AST_AND;
    case TOKEN_OR:
        return AST_OR;
    case TOKEN_CAST:
        return AST_CAST;
    case TOKEN_NEW:
        return AST_NEW;
    case TOKEN_FREE:
        return AST_FREE;
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
    case TOKEN_NULL:
        return AST_NULL;
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
    case AST_GREATEREQUAL:
    case AST_LESSEREQUAL:
    case AST_AND:
    case AST_OR: 
    case AST_CAST: 
    case AST_NEW: 
    case AST_FREE: 
    case AST_DOT:
    case AST_INDEX:
    case AST_MODULEACCESS:
        return;
    default:
        error(filename, line, "Operator stack corrupted, %s was assumed to be operator", parser_astToString(type));
    }
}