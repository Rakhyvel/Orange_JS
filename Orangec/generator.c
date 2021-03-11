/*  generator.c

    The generator takes in a program data structure that's already been proven
    correct by the validator, and generates the code that represents that.
    Depending on the target, will either generate a Javascript file, x86, or 
    anything else i can think of.

    Author: Joseph Shimel
    Date: 3/6/21
*/

#include "./generator.h"
#include <stdlib.h>
#include <string.h>

#include "./main.h"
#include "./parser.h"

#include "../util/debug.h"
#include "../util/list.h"
#include "../util/map.h"

static void constructLists(struct symbolNode*, struct list*, struct list*, struct list*);
static void fprintb(FILE*, int);
static void generateStruct(FILE*, struct symbolNode*);
static void generateGlobal(FILE*, struct symbolNode*);
static void generateFunction(FILE*, struct symbolNode*);
static void generateAST(FILE*, int, struct astNode*);
static void generateExpression(FILE*, struct astNode*);

void generator_generate(FILE* out) {
    struct list* structList = list_create();
    struct list* globalList = list_create();
    struct list* functionList = list_create();
    constructLists(program, structList, globalList, functionList);
    struct listElem* elem = list_begin(structList);
    for(;elem != list_end(structList); elem = list_next(elem)) {
        generateStruct(out, elem->data);
        fprintf(out, "\n");
    }
    elem = list_begin(globalList);
    for(;elem != list_end(globalList); elem = list_next(elem)) {
        generateGlobal(out, elem->data);
        fprintf(out, "\n");
    }
    elem = list_begin(functionList);
    for(;elem != list_end(functionList); elem = list_next(elem)) {
        generateFunction(out, elem->data);
        fprintf(out, "\n");
    }
}

static void constructLists(struct symbolNode* node, struct list* structList, struct list* globalList, struct list* functionList) {
    struct listElem* elem = list_begin(node->children->keyList);
    for(;elem != list_end(node->children->keyList); elem = list_next(elem)) {
        struct symbolNode* child = (struct symbolNode*)map_get(node->children, (char*)elem->data);

        if(child->symbolType == SYMBOL_STRUCT) {
            queue_push(structList, child);
            LOG("%s", child->name);
        } else if(child->symbolType == SYMBOL_VARIABLE && node->symbolType == SYMBOL_MODULE) {
            queue_push(globalList, child);
        } else if(child->symbolType == SYMBOL_FUNCTION && child->code != NULL) {
            queue_push(functionList, child);
            LOG("%s", child->name);
        }
        constructLists(child, structList, globalList, functionList);
    }
}

static void fprintb(FILE* out, int num) {
    char* buf = itoa(num);
    fprintf(out, "_%s", buf);
    // free(buf);
}

static void generateStruct(FILE* out, struct symbolNode* dataStruct) {
    LOG("Generate struct");
    fprintf(out, "class ");
    fprintb(out, dataStruct->id);
    fprintf(out, " {\n\tconstructor(");
    struct list* fields = dataStruct->children->keyList;
    struct listElem* fieldElem;
    for(fieldElem = list_begin(fields); fieldElem != list_end(fields); fieldElem = list_next(fieldElem)) {
        fprintf(out, "%s", (char*)fieldElem->data);
        if(fieldElem->next != list_end(fields)){
            fprintf(out, ", ");
        }
    }
    fprintf(out,") {");
    for(fieldElem = list_begin(fields); fieldElem != list_end(fields); fieldElem = list_next(fieldElem)) {
        fprintf(out, "this.%s=%s;", (char*)fieldElem->data, (char*)fieldElem->data);
        
    }
    fprintf(out, "}\n}");
}

static void generateGlobal(FILE* out, struct symbolNode* variable) {
    LOG("Generate global");
    if(variable->code != NULL) {
        fprintf(out, "let ");
        fprintb(out, variable->id);
        fprintf(out, "=");
        generateExpression(out, variable->code);
        fprintf(out, ";");
    } else {
        fprintf(out, "let ");
        fprintb(out, variable->id);
        fprintf(out, ";");
    }
}

static void generateFunction(FILE* out, struct symbolNode* function) {
    LOG("Generate function");
    fprintf(out, "function ");
    fprintb(out, function->id);
    fprintf(out, "(");
    struct list* params = function->children->keyList;
    struct listElem* paramElem;
    for(paramElem = list_begin(params); paramElem != list_end(params); paramElem = list_next(paramElem)) {
        if(strstr((char*)paramElem->data, "_block")) {
            continue;
        }
        struct symbolNode* symbol = (struct symbolNode*)map_get(function->children, paramElem->data);
        fprintb(out, symbol->id);
        if(!(paramElem->next == list_end(params) || strstr((char*)paramElem->next->data, "_block"))){
            fprintf(out, ", ");
        }
    }
    fprintf(out,")");
    generateAST(out, 0, function->code);
}

static void generateAST(FILE* out, int tabs, struct astNode* node) {
    LOG("Generate AST %s", parser_astToString(node->type));
    if(node == NULL) return;

    switch(node->type) {
    case AST_BLOCK: {
        fprintf(out, "{");
        struct listElem* elem;
        for(elem = list_begin(node->children); elem != list_end(node->children); elem = list_next(elem)) {
            if(elem->data == NULL) continue; // @fix why would child be null here?
            generateAST(out, tabs + 1, (struct astNode*)elem->data);
        }
        fprintf(out, "}");
        break;
    }
    case AST_SYMBOLDEFINE: {
        struct symbolNode* symbol = (struct symbolNode*) node->data;
        if(symbol->symbolType == SYMBOL_VARIABLE) {
            generateGlobal(out, node->data);
        }
    } break;
    case AST_IF:
        fprintf(out, "if(");
        generateExpression(out, node->children->head.next->data);
        fprintf(out, ")");
        generateAST(out, tabs, node->children->head.next->next->data);
        break;
    case AST_IFELSE:
        fprintf(out, "if(");
        generateExpression(out, node->children->head.next->data);
        fprintf(out, ")");
        generateAST(out, tabs, node->children->head.next->next->data);
        fprintf(out, "else");
        generateAST(out, tabs, node->children->head.next->next->next->data);
        break;
    case AST_WHILE:
        fprintf(out, "while(");
        generateExpression(out, node->children->head.next->data);
        fprintf(out, ")");
        generateAST(out, tabs, node->children->head.next->next->data);
        break;
    case AST_RETURN:
        fprintf(out, "return ");
        generateExpression(out, node->children->head.next->data);
        fprintf(out, ";");
        break;
    default:
        generateExpression(out, node);
        fprintf(out, ";");
        break;
    }
}

static void generateExpression(FILE* out, struct astNode* node) {
    LOG("Generate expression %s", parser_astToString(node->type));

    if(node == NULL) return;
    switch(node->type){
    case AST_INTLITERAL:
        fprintf(out, "%d", *(int*)node->data);
        break;
    case AST_REALLITERAL:
        fprintf(out, "%f", *(float*)node->data);
        break;
    case AST_CHARLITERAL:
    case AST_STRINGLITERAL:
    case AST_TRUE:
    case AST_FALSE:
    case AST_NULL:
        fprintf(out, "%s", (char*)node->data);
        break;
    case AST_VAR: {
        struct symbolNode* symbol = symbol_findSymbol(node->data, node->scope);
        if(symbol == NULL) {
            fprintf(out, "%s", (char*)node->data);
        } else {
            fprintb(out, symbol->id);
        }
    } break;
    case AST_ADD:
    case AST_SUBTRACT:
    case AST_MULTIPLY:
    case AST_DIVIDE:
    case AST_ASSIGN:
    case AST_OR:
    case AST_AND:
    case AST_GREATER:
    case AST_LESSER:
    case AST_GREATEREQUAL:
    case AST_LESSEREQUAL:
    case AST_IS:
    case AST_ISNT:
    case AST_DOT:
        generateExpression(out, node->children->head.next->next->data);
        fprintf(out, "%s", (char*)node->data);
        generateExpression(out, node->children->head.next->data);
        break;
    case AST_MODULEACCESS:
        generateExpression(out, node->children->head.next->data);
        break;
    case AST_CALL: {
        if(strstr(node->data, " array")) {
            fprintf(out, "Array(");
        } else {
            LOG("%s", (char*)node->data);
            struct symbolNode* symbol = symbol_findSymbol(node->data, node->scope);
            if(symbol == NULL) {
                symbol = map_get(structMap, node->data);
            }
            fprintb(out, symbol->id);
            fprintf(out, "(");
        }
        struct listElem* elem;
        for(elem = list_begin(node->children); elem != list_end(node->children); elem = list_next(elem)) {
            generateExpression(out, (struct astNode*)elem->data);
            if(elem->next != list_end(node->children)){
                fprintf(out, ", ");
            }
        }
        fprintf(out, ")");
        break;
    }
    case AST_INDEX:
        generateExpression(out, node->children->head.next->next->data);
        fprintf(out, "[");
        generateExpression(out, node->children->head.next->data);
        fprintf(out, "]");
        break;
    case AST_NEW: {
        struct astNode* rightAST = node->children->head.next->data;
        fprintf(out, "new ");
        if(rightAST->type == AST_CALL) {
            generateExpression(out, node->children->head.next->data);
        } else if(rightAST->type == AST_INDEX) {
            fprintf(out, "Array(%d)", *(int*)(((struct astNode*)rightAST->children->head.next->data)->data));
        }
    }break;
    case AST_CAST: {
        struct listElem* elem;
        for(elem = list_begin(node->children); elem != list_end(node->children); elem = list_next(elem)) {
            generateExpression(out, (struct astNode*)elem->data);
        }
    } break;
    case AST_VERBATIM: {
        struct listElem* elem;
        for(elem = list_begin(node->children); elem != list_end(node->children); elem = list_next(elem)) {
            struct astNode* child = (struct astNode*)elem->data;
            if(child->type == AST_STRINGLITERAL) {
                fprintf(out, child->data);
            } else {
                generateExpression(out, child);
            }
        }
    } break;
    case AST_FREE:
    default:
        break;
    }
}