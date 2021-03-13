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

#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "./ast.h"
#include "./main.h"
#include "./parser.h"
#include "./token.h"

#include "../util/debug.h"
#include "../util/map.h"

// Higher level token signatures
static const enum tokenType MODULE[] = {TOKEN_IDENTIFIER, TOKEN_LBRACE};
static const enum tokenType STRUCT[] = {TOKEN_STRUCT, TOKEN_IDENTIFIER, TOKEN_LPAREN};
static const enum tokenType ENUM[] = {TOKEN_ENUM, TOKEN_IDENTIFIER, TOKEN_LPAREN};
static const enum tokenType VARDECLARE[] = {TOKEN_IDENTIFIER, TOKEN_IDENTIFIER, TOKEN_SEMICOLON};
static const enum tokenType PARAM_DECLARE[] = {TOKEN_IDENTIFIER, TOKEN_IDENTIFIER, TOKEN_COMMA};
static const enum tokenType ENDPARAM_DECLARE[] = {TOKEN_IDENTIFIER, TOKEN_IDENTIFIER, TOKEN_RPAREN};
static const enum tokenType VARDEFINE[] = {TOKEN_IDENTIFIER, TOKEN_IDENTIFIER, TOKEN_EQUALS};
static const enum tokenType EXTERN_VARDECLARE[] = {TOKEN_IDENTIFIER, TOKEN_COLON, TOKEN_IDENTIFIER, TOKEN_IDENTIFIER, TOKEN_SEMICOLON};
static const enum tokenType EXTERN_PARAM_DECLARE[] = {TOKEN_IDENTIFIER, TOKEN_COLON, TOKEN_IDENTIFIER, TOKEN_IDENTIFIER, TOKEN_COMMA};
static const enum tokenType EXTERN_ENDPARAM_DECLARE[] = {TOKEN_IDENTIFIER, TOKEN_COLON, TOKEN_IDENTIFIER, TOKEN_IDENTIFIER, TOKEN_RPAREN};
static const enum tokenType EXTERN_VARDEFINE[] = {TOKEN_IDENTIFIER, TOKEN_COLON, TOKEN_IDENTIFIER, TOKEN_IDENTIFIER, TOKEN_EQUALS};
static const enum tokenType EXTERN_FUNCTION[] = {TOKEN_IDENTIFIER, TOKEN_COLON, TOKEN_IDENTIFIER, TOKEN_IDENTIFIER, TOKEN_LPAREN};
static const enum tokenType FUNCTION[] = {TOKEN_IDENTIFIER, TOKEN_IDENTIFIER, TOKEN_LPAREN};
static const enum tokenType CALL[] = {TOKEN_IDENTIFIER, TOKEN_LPAREN};
static const enum tokenType VERBATIM[] = {TOKEN_VERBATIM, TOKEN_LPAREN};

static bool topMatches(struct list*, enum tokenType);
static bool matchTokens(struct list*, const enum tokenType[], int);
static const char* getTopFilename(struct list*);
static int getTopLine(struct list*);
static void copyNextTokenString(struct list*, char*);
static void parseParams(struct list*, struct symbolNode*);
static void parseEnums(struct list*, struct symbolNode*);
static void expectType(struct list*, char*);
static struct astNode* parseAST(struct list*, struct symbolNode*);
static struct astNode* parseExpression(struct list*, struct symbolNode*);
static struct list* nextExpression(struct list*);
static struct list* simplifyTokens(struct list*, struct symbolNode*);
static struct list* infixToPostfix(struct list*);
static void assertRemove(struct list*, enum tokenType);
static void assertPeek(struct list*, enum tokenType);
static void assertOperator(enum astType, const char*, int);

/*
    Removes tokens from a list that are within comments */
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

/*
    Goes through the token queue as a list, checks for array type 
    modifiers "[]". Once found, concatonates the array modifier with 
    a space and the token before it. The space is used because a legal identiier
    cannot contain spaces, thus avoiding name collisions.
    
    This is used to make array type parsing easier. For example, the type the
    variable i:
        int[] i;
    would be "int array". The brackets can be detected by the validator, and
    reasoned with. */
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

/*
    Goes through a token queue, parses out the first symbol off the front of 
    the queue, and assigns its parent to the given parent */
struct symbolNode* parser_parseTokens(struct list* tokenQueue, struct symbolNode* parent) {
    struct symbolNode* symbolNode;
    int isPrivate = topMatches(tokenQueue, TOKEN_PRIVATE);
    if(isPrivate) free(queue_pop(tokenQueue));
    int isStatic = topMatches(tokenQueue, TOKEN_STATIC);
    if(isStatic) free(queue_pop(tokenQueue));
    int isConstant = topMatches(tokenQueue, TOKEN_CONST);
    if(isConstant) free(queue_pop(tokenQueue));

    // END OF MODULE, RETURN
    if(topMatches(tokenQueue, TOKEN_RBRACE)) {
        return NULL;
    }
    // END OF PARAM LIST, RETURN
    if(topMatches(tokenQueue, TOKEN_RPAREN)) {
        return NULL;
    }
    // MODULE
    else if(matchTokens(tokenQueue, MODULE, 2)) {
        symbolNode = symbol_create(SYMBOL_MODULE, parent, getTopFilename(tokenQueue), getTopLine(tokenQueue));
        symbolNode->isStatic = isStatic;
        copyNextTokenString(tokenQueue, symbolNode->name);
        assertRemove(tokenQueue, TOKEN_LBRACE);
        struct symbolNode* child;
        while((child = parser_parseTokens(tokenQueue, symbolNode)) != NULL) {
            if(map_put(symbolNode->children, child->name, child)) {
                error(child->filename, child->line, "%s already defined in module %s", child->name, symbolNode->name);
            }
        }
        assertRemove(tokenQueue, TOKEN_RBRACE);
        LOG("Module %s created %d", symbolNode->name, symbolNode->symbolType);
    }
    // STRUCT
    else if(matchTokens(tokenQueue, STRUCT, 3)) {
        symbolNode = symbol_create(SYMBOL_STRUCT, parent, getTopFilename(tokenQueue), getTopLine(tokenQueue));
        symbolNode->isPrivate = isPrivate;
        symbolNode->isStatic = 1;
        assertRemove(tokenQueue, TOKEN_STRUCT);
        copyNextTokenString(tokenQueue, symbolNode->name);
        strcpy(symbolNode->type, symbolNode->name);
        char* buf = itoa(symbolNode->id);
        strcat(symbolNode->type, "#");
        strcat(symbolNode->type, buf);
        ASSERT(!map_put(typeMap, symbolNode->type, symbolNode));
        parseParams(tokenQueue, symbolNode);
        LOG("Struct %s created", symbolNode->name);
    }
    // ENUM
    else if(matchTokens(tokenQueue, ENUM, 3)) {
        symbolNode = symbol_create(SYMBOL_ENUM, parent, getTopFilename(tokenQueue), getTopLine(tokenQueue));
        symbolNode->isPrivate = isPrivate;
        symbolNode->isStatic = 1;
        symbolNode->isConstant = 1;
        assertRemove(tokenQueue, TOKEN_ENUM);
        copyNextTokenString(tokenQueue, symbolNode->name);
        strcpy(symbolNode->type, symbolNode->name);
        char* buf = itoa(symbolNode->id);
        strcat(symbolNode->type, "#");
        strcat(symbolNode->type, buf);
        ASSERT(!map_put(typeMap, symbolNode->type, symbolNode)); // impossible for collision to happen
        parseEnums(tokenQueue, symbolNode);
        LOG("Enum %s created", symbolNode->name);
    }
    // VARIABLE DEFINITION
    else if(matchTokens(tokenQueue, VARDEFINE, 3) || matchTokens(tokenQueue, EXTERN_VARDEFINE, 5)) {
        symbolNode = symbol_create(SYMBOL_VARIABLE, parent, getTopFilename(tokenQueue), getTopLine(tokenQueue));
        symbolNode->isPrivate = isPrivate;
        symbolNode->isConstant = isConstant;
        symbolNode->isStatic = parent->isStatic;
        expectType(tokenQueue, symbolNode->type);
        copyNextTokenString(tokenQueue, symbolNode->name);
        assertRemove(tokenQueue, TOKEN_EQUALS);
        symbolNode->code = parseAST(tokenQueue, symbolNode);
        assertRemove(tokenQueue, TOKEN_SEMICOLON);
        LOG("Variable definition %s created", symbolNode->name);
    // VARIABLE DECLARATION
    } else if(matchTokens(tokenQueue, VARDECLARE, 3) || matchTokens(tokenQueue, EXTERN_VARDECLARE, 5)) {
        symbolNode = symbol_create(SYMBOL_VARIABLE, parent, getTopFilename(tokenQueue), getTopLine(tokenQueue));
        symbolNode->isPrivate = isPrivate;
        symbolNode->isConstant = isConstant;
        symbolNode->isStatic = parent->isStatic || parent->symbolType == SYMBOL_MODULE;
        expectType(tokenQueue, symbolNode->type);
        copyNextTokenString(tokenQueue, symbolNode->name);
        assertRemove(tokenQueue, TOKEN_SEMICOLON);
        LOG("Variable declaration %s created", symbolNode->name);
    // PARAM DECLARATION
    } else if(matchTokens(tokenQueue, PARAM_DECLARE, 3) || matchTokens(tokenQueue, ENDPARAM_DECLARE, 3) ||
            matchTokens(tokenQueue, EXTERN_PARAM_DECLARE, 5) || matchTokens(tokenQueue, EXTERN_ENDPARAM_DECLARE, 5)) {
        symbolNode = symbol_create(SYMBOL_VARIABLE, parent, getTopFilename(tokenQueue), getTopLine(tokenQueue));
        symbolNode->isPrivate = isPrivate;
        symbolNode->isConstant = isConstant;
        symbolNode->isStatic = parent->isStatic || parent->symbolType == SYMBOL_MODULE;
        expectType(tokenQueue, symbolNode->type);
        copyNextTokenString(tokenQueue, symbolNode->name);
        LOG("Param %s created", symbolNode->name);    
    // FUNCTION DECLARATION
    } else if(matchTokens(tokenQueue, FUNCTION, 3) || matchTokens(tokenQueue, EXTERN_FUNCTION, 5)) {
        symbolNode = symbol_create(SYMBOL_FUNCTION, parent, getTopFilename(tokenQueue), getTopLine(tokenQueue));
        symbolNode->isPrivate = isPrivate;
        symbolNode->isConstant = isConstant;
        symbolNode->isStatic = parent->isStatic;
        expectType(tokenQueue, symbolNode->type);
        copyNextTokenString(tokenQueue, symbolNode->name);
        parseParams(tokenQueue, symbolNode);
        if(topMatches(tokenQueue, TOKEN_EQUALS)) {
            symbolNode->isDeclared = 1;
            assertRemove(tokenQueue, TOKEN_EQUALS);
            symbolNode->code = parseAST(tokenQueue, symbolNode);
            assertRemove(tokenQueue, TOKEN_SEMICOLON);
        } else if(topMatches(tokenQueue, TOKEN_LBRACE)) {
            symbolNode->isDeclared = 1;
            symbolNode->code = parseAST(tokenQueue, symbolNode);
        } else {
            symbolNode->symbolType = SYMBOL_FUNCTIONPTR;
            struct symbolNode* anon = symbol_create(SYMBOL_BLOCK, parent, getTopFilename(tokenQueue), getTopLine(tokenQueue));
            strcpy(anon->name, "_block_anon"); // put here so that parameter length cmp is easy
            ASSERT(!map_put(symbolNode->children, anon->name, anon));
        }
        LOG("Function %s created", symbolNode->name);
    } else if(topMatches(tokenQueue, TOKEN_EOF)){
        return NULL;
    } else {
        error(getTopFilename(tokenQueue), getTopLine(tokenQueue), "Unexpected token %s", token_toString(((struct token*) queue_peek(tokenQueue))->type));
    }
    return symbolNode;
}

/*
    Returns whether or not the top of the tokenQueue has the specified type. */
static bool topMatches(struct list* tokenQueue, enum tokenType type) {
    return ((struct token*)queue_peek(tokenQueue))->type == type;
}

/*
    Determines if a given token signature matches what is at the front of a 
    tokenQueue.
    
    Does NOT overrun off edge of tokenQueue, instead returns false. */
static bool matchTokens(struct list* tokenQueue, const enum tokenType sig[], int nTokens) {
    int i = 0;
    struct listElem* elem;
    for(elem = list_begin(tokenQueue); elem != list_end(tokenQueue) && i < nTokens; elem = list_next(elem), i++) {
        struct token* token = (struct token*) (elem->data);
        if(token->type != sig[i]) {
            return false;
        }
    }
    return i == nTokens;
}

/*
    Returns the filename of token at the front of the tokenQueue */
static const char* getTopFilename(struct list* tokenQueue) {
    return ((struct token*)queue_peek(tokenQueue))->filename;
}

/*
    Returns the line number of the token at the front of the tokenQueue */
static int getTopLine(struct list* tokenQueue) {
    return ((struct token*)queue_peek(tokenQueue))->line;
}

/*
    Pops a token off from the front of a queue, copies the string data of that
    token into a given string. Max size is 255 characters, including null term. */
static void copyNextTokenString(struct list* tokenQueue, char* dest) {
    assertPeek(tokenQueue, TOKEN_IDENTIFIER);
    struct token* nextToken = (struct token*) queue_pop(tokenQueue);
    strncat(dest, nextToken->data, 254);
    free(nextToken);
}

/*
    Converts integers to base 36 ascii. Used for text representation of UID's

    Implementation from http://www.strudel.org.uk/itoa/ */
char* itoa(int val) {
	char* buf = (char*)calloc(1, 32);
	int i = 30;
	for(; val && i ; --i, val /= 36)
		buf[i] = "0123456789abcdefghijklmnopqrstuvwxyz"[val % 36];
	return &buf[i+1];
}

/*
    Takes in a token queue, reads in the parameters and them as the children 
    to a parent symbol. Used for function parameters and struct fields.
    
    Parenthesis are removed in this function as well.*/
static void parseParams(struct list* tokenQueue, struct symbolNode* parent) {
    assertRemove(tokenQueue, TOKEN_LPAREN);
    // Parse parameters of function
    while(!topMatches(tokenQueue, TOKEN_RPAREN)) {
        struct symbolNode* param = parser_parseTokens(tokenQueue, parent);
        if(map_put(parent->children, param->name, param)) {
            error(getTopFilename(tokenQueue), getTopLine(tokenQueue), "Parameter \"%s\" defined in more than one place", param->name);
        }
        param->isDeclared = 1;
    
        if(topMatches(tokenQueue, TOKEN_COMMA)) {
            free(queue_pop(tokenQueue));
        } else if(!topMatches(tokenQueue, TOKEN_RPAREN)){
            error(getTopFilename(tokenQueue), getTopLine(tokenQueue), "Unexpected token %s in parameter list", ((struct token*)queue_peek(tokenQueue))->data);
        }
        LOG("New arg: %s %s", param->type, param->name);
    }
    assertRemove(tokenQueue, TOKEN_RPAREN);
}

/*
    Takes in a token queue, reads in a list of enumerations, and adds them as
    children symbols to a parent symbol. */
static void parseEnums(struct list* tokenQueue, struct symbolNode* parent) {
    assertRemove(tokenQueue, TOKEN_LPAREN);
    // Parse enum names for enums
    while(!topMatches(tokenQueue, TOKEN_RPAREN)) {
        struct symbolNode* num = symbol_create(SYMBOL_VARIABLE, parent, getTopFilename(tokenQueue), getTopLine(tokenQueue));
        num->isConstant = 1;
        copyNextTokenString(tokenQueue, num->name);
        strcpy(num->type, parent->type);
        if(map_put(parent->children, num->name, num)) {
            error(num->filename, num->line, "Enum \"%s\" defined in more than one place", num->name);
        }
        num->isDeclared = 1;
    
        if(topMatches(tokenQueue, TOKEN_COMMA)) {
            free(queue_pop(tokenQueue));
        } else if(!topMatches(tokenQueue, TOKEN_RPAREN)){
            error(getTopFilename(tokenQueue), getTopLine(tokenQueue), "Unexpected token %s in enum", ((struct token*)queue_peek(tokenQueue))->data);
        }
    }
    assertRemove(tokenQueue, TOKEN_RPAREN);
}

/*
    Creates an AST for code given a queue of tokens. Only parses one 
    instruction per call. */
static struct astNode* parseAST(struct list* tokenQueue, struct symbolNode* scope) {
    ASSERT(tokenQueue != NULL);
    ASSERT(scope != NULL);
    struct astNode* retval = NULL;

    // BLOCK
    if(topMatches(tokenQueue, TOKEN_LBRACE)) {
        retval = ast_create(AST_BLOCK, getTopFilename(tokenQueue), getTopLine(tokenQueue), scope);
        struct symbolNode* symbolNode = symbol_create(SYMBOL_BLOCK, scope, getTopFilename(tokenQueue), getTopLine(tokenQueue));
        retval->data = symbolNode;
        symbolNode->isStatic = scope->isStatic;
        strcpy(symbolNode->name, "_block");
        strcat(symbolNode->name, itoa(symbolNode->id)); // done to prevent collisions with other blocks in scope
        strcpy(symbolNode->type, scope->type); // blocks have same type as parent, to aid in validating return values
        ASSERT(!map_put(scope->children, symbolNode->name, symbolNode));
        assertRemove(tokenQueue, TOKEN_LBRACE);
        while(!list_isEmpty(tokenQueue) && !topMatches(tokenQueue, TOKEN_RBRACE)) {
            queue_push(retval->children, parseAST(tokenQueue, retval->data));
        }
        assertRemove(tokenQueue, TOKEN_RBRACE);
    }
    // SYMBOL DEFINITION/DECLARATION
    else if (matchTokens(tokenQueue, VARDECLARE, 3) || 
             matchTokens(tokenQueue, VARDEFINE, 3) || 
             matchTokens(tokenQueue, FUNCTION, 3) || 
             matchTokens(tokenQueue, STRUCT, 3) || 
             matchTokens(tokenQueue, ENUM, 3) || 
             matchTokens(tokenQueue, EXTERN_VARDECLARE, 5) || 
             matchTokens(tokenQueue, EXTERN_VARDEFINE, 5)) {
        retval = ast_create(AST_SYMBOLDEFINE, getTopFilename(tokenQueue), getTopLine(tokenQueue), scope);
        struct symbolNode* symbolNode = parser_parseTokens(tokenQueue, scope);
        retval->data = symbolNode;
        if(map_put(scope->children, symbolNode->name, symbolNode)) {
            error(symbolNode->filename, symbolNode->line, "Symbol %s already defined in this scope", symbolNode->name);
        }
    }
    // IF
    else if (topMatches(tokenQueue, TOKEN_IF)) {
        retval = ast_create(AST_IF, getTopFilename(tokenQueue), getTopLine(tokenQueue), scope);
        assertRemove(tokenQueue, TOKEN_IF);
        struct astNode* expression = parseAST(tokenQueue, scope);
        struct astNode* body = parseAST(tokenQueue, scope);
        if(body == NULL || body->type != AST_BLOCK) {
            error(retval->filename, retval->line, "If statements must be followed by block statements");
        }
        queue_push(retval->children, expression);
        queue_push(retval->children, body);
        if(topMatches(tokenQueue, TOKEN_ELSE)) {
            assertRemove(tokenQueue, TOKEN_ELSE);
            retval->type = AST_IFELSE;
            if(topMatches(tokenQueue, TOKEN_IF)) {
                queue_push(retval->children, parseAST(tokenQueue, scope));
            } else {
                queue_push(retval->children, parseAST(tokenQueue, scope));
            }
        }
    }
    // WHILE
    else if (topMatches(tokenQueue, TOKEN_WHILE)) {
        retval = ast_create(AST_WHILE, getTopFilename(tokenQueue), getTopLine(tokenQueue), scope);
        assertRemove(tokenQueue, TOKEN_WHILE);
        struct astNode* expression = parseAST(tokenQueue, scope);
        struct astNode* body = parseAST(tokenQueue, scope);
        if(body == NULL || body->type != AST_BLOCK) {
            error(retval->filename, retval->line, "While statements must be followed by block statements");
        }
        queue_push(retval->children, expression);
        queue_push(retval->children, body);
    }
    // RETURN
    else if (topMatches(tokenQueue, TOKEN_RETURN)) {
        retval = ast_create(AST_RETURN, getTopFilename(tokenQueue), getTopLine(tokenQueue), scope);
        assertRemove(tokenQueue, TOKEN_RETURN);
        struct astNode* expression = parseExpression(tokenQueue, scope);
        queue_push(retval->children, expression);
    }
    else if (topMatches(tokenQueue, TOKEN_SEMICOLON)) {
        assertRemove(tokenQueue, TOKEN_SEMICOLON);
    }
    // EXPRESSION
    else {
        retval = parseExpression(tokenQueue, scope);
    }
    return retval;
}

/*
    Copies the type at the front of the tokenQueue to a given string. If the 
    type is external (IE contains the : operator), the type will have the 
    form module$type. */
static void expectType(struct list* tokenQueue, char* dst) {
    copyNextTokenString(tokenQueue, dst);
    if(topMatches(tokenQueue, TOKEN_COLON)) {
        assertRemove(tokenQueue, TOKEN_COLON);
        strcat(dst, "$");    // The dollar sign is a special char
                             // signifies that the type is a composite, and should be updated later by validator_updateStructType
        copyNextTokenString(tokenQueue, dst);
    }
}

/*
    Given a token queue, extracts the front expression, parses it into an
    Abstract Syntax Tree. */
static struct astNode* parseExpression(struct list* tokenQueue, struct symbolNode* scope) {
    LOG("Create Expression AST");
    ASSERT(tokenQueue != NULL);
    struct astNode* astNode = NULL;
    struct token* token = NULL;
    struct list* rawExpression = nextExpression(tokenQueue);
    if(list_isEmpty(rawExpression)) {
        error(getTopFilename(tokenQueue), getTopLine(tokenQueue), "Expected expression");
    }

    struct list* expression = infixToPostfix(simplifyTokens(rawExpression, scope));
    struct list* argStack = list_create();
    
    while (!list_isEmpty(expression)) {
        struct listElem* elem = NULL;
        token = (struct token*)queue_pop(expression);
        astNode = ast_create(AST_NOP, token->filename, token->line, scope);
        int* intData;
        float* realData;
        char* charData;
        astNode->type = ast_tokenToAST(token->type);

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
        case TOKEN_VERBATIM:
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
            ASSERT(!list_isEmpty(argStack)); // if fails, can indicate token was not put onto argstack
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
        LOG("%s %s", token_toString(nextType), ((struct token*)queue_peek(tokenQueue))->data);
        queue_push(retval, queue_pop(tokenQueue));
    }
    LOG("end Next Expression");
    return retval;
}

/*
    Takes in a queue representing an expression, and if it can, transforms 
    function calls/index token structures into proper call/index tokens 
    
    This is useful later, infix->postfix translation is done. For expressions
    that taken more than one token, such as calls, indexes, and casts, its 
    easier to convert them to a single anonymous token, and then convert to
    postfix as normal. 
    
    Calls:      IDENT ( ARG , ARG , ... )       ->      CALL(name) : {AST_ARG, AST_ARG, ... } 
    Verbatim:   VERBATIM ( ARG , ARG , ... )    ->      VERBATIM : {AST_ARG, AST_ARG, ... } 
    Indexes:    IDENT [ EXPR. ]                 ->      IDENT INDEX ( EXPR. )
    Cast:       CAST ( IDENT )                  ->      CAST(type)

    Other tokens are just added to the retval queue
    */
static struct list* simplifyTokens(struct list* tokenQueue, struct symbolNode* scope) {
    LOG("Simplify Tokens");
    struct list* retval = list_create();

    // Go through each token in given expression token queue, add/reject/parse to retval
    while(!list_isEmpty(tokenQueue)) {
        // @fix unify call arg parsing and verbatim arg parsing
        // CALL
        if(matchTokens(tokenQueue, CALL, 2)) {
            struct token* callName = ((struct token*)queue_pop(tokenQueue));
            struct token* call = token_create(TOKEN_CALL, callName->data, callName->filename, callName->line);
            call->line = callName->line;
            free(callName);

            assertRemove(tokenQueue, TOKEN_LPAREN);
            
            // Go through each argument, create AST representing it
            while(!list_isEmpty(tokenQueue) && !topMatches(tokenQueue, TOKEN_RPAREN)) {
                struct astNode* argAST = parseExpression(tokenQueue, scope);
                queue_push(call->list, argAST);

                if(topMatches(tokenQueue, TOKEN_COMMA)) {
                    free(queue_pop(tokenQueue));
                }
            }
            assertRemove(tokenQueue, TOKEN_RPAREN);

            queue_push(retval, call);
        // VERBATIM
        } else if(matchTokens(tokenQueue, VERBATIM, 2)) {
            struct token* verbatim = ((struct token*)queue_pop(tokenQueue));

            assertRemove(tokenQueue, TOKEN_LPAREN);
            
            // Go through each argument, create AST representing it
            while(!list_isEmpty(tokenQueue) && !topMatches(tokenQueue, TOKEN_RPAREN)) {
                struct astNode* argAST = parseExpression(tokenQueue, scope);
                queue_push(verbatim->list, argAST);

                if(topMatches(tokenQueue, TOKEN_COMMA)) {
                    free(queue_pop(tokenQueue));
                }
            }
            assertRemove(tokenQueue, TOKEN_RPAREN);

            queue_push(retval, verbatim);
        } 
        // INDEX
        else if(topMatches(tokenQueue, TOKEN_LSQUARE)) {
            queue_push(retval, token_create(TOKEN_INDEX, "", getTopFilename(tokenQueue), getTopLine(tokenQueue)));
            queue_push(retval, token_create(TOKEN_LPAREN, "(", getTopFilename(tokenQueue), getTopLine(tokenQueue)));
            assertRemove(tokenQueue, TOKEN_LSQUARE);

            // extract expression for index, add to retval list, between anonymous parens
            struct list* innerExpression = infixToPostfix(simplifyTokens(nextExpression(tokenQueue), scope));
            struct listElem* elem;
            for(elem = list_begin(innerExpression); elem != list_end(innerExpression); elem=list_next(elem)) {
                queue_push(retval, elem->data);
            }

            assertRemove(tokenQueue, TOKEN_RSQUARE);
            queue_push(retval, token_create(TOKEN_RPAREN, ")", getTopFilename(tokenQueue), getTopLine(tokenQueue)));
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
    converts it to postfix order.
    
    Tokens are read in as they are in the file, which is in infix order. It is 
    easier to parse out the AST from a postfix queue of tokens than an infix 
    queue of tokens. 
    
    Example infix to posftix translation:
        Infix:   4 * 5 + (3 - 9) 
        Postfix: 3 9 - 4 5 * +
    */
static struct list* infixToPostfix(struct list* tokenQueue) {
    LOG("Infix to Postfix");
    struct list* retval = list_create();
    struct list* opStack = list_create();
    struct token* token = NULL;

    // Go through each token in expression token queue, rearrange to form postfix expression token queue
    while(!list_isEmpty(tokenQueue)) {
        token = ((struct token*) queue_pop(tokenQueue));
        // VALUE
        if(token->type == TOKEN_IDENTIFIER || token->type == TOKEN_INTLITERAL  
            || token->type == TOKEN_REALLITERAL || token->type == TOKEN_CALL 
            || token->type == TOKEN_CHARLITERAL 
            || token->type == TOKEN_STRINGLITERAL || token->type == TOKEN_FALSE
            || token->type == TOKEN_TRUE || token->type == TOKEN_VERBATIM) {
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
            token_precedence(token->type) <= token_precedence(((struct token*)stack_peek(opStack))->type)) {
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
    Verifies that the next token is what is expected, and removes it. Errors
    otherwise */
static void assertRemove(struct list* tokenQueue, enum tokenType expected) {
    struct token* topToken = (struct token*) queue_peek(tokenQueue);
    enum tokenType actual = topToken->type;
    if(actual == expected) {
        free(queue_pop(tokenQueue));
    } else {
        error(topToken->filename, topToken->line, "Unexpected token %s, expected %s", token_toString(actual), token_toString(expected));
    }
}

/*
    Verifies that the next token is what is expected. Errors otherwise */
static void assertPeek(struct list* tokenQueue, enum tokenType expected) {
    struct token* topToken = (struct token*) queue_peek(tokenQueue);
    enum tokenType actual = topToken->type;
    if(actual != expected) {
        error(topToken->filename, topToken->line, "Unexpected token %s, expected %s", token_toString(actual), token_toString(expected));
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
        error(filename, line, "Operator stack corrupted, %s was assumed to be operator", ast_toString(type));
    }
}