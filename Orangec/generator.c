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

#include "./main.h"

#include "../util/debug.h"
#include "../util/list.h"
#include "../util/map.h"

static void generateStruct(FILE*, struct dataStruct*);
static void generateGlobal(FILE*, char*, struct variable*);
static void generateFunction(FILE*, char*, struct function*);
static void printTabs(FILE*, int);
static void generateAST(FILE*, int, char*, struct astNode*);
static void generateExpression(FILE*, char*, struct astNode* );

void generator_generate(FILE* out) {
    // Each struct should be a class with only a constructor
    fprintf(out, "/*** BEGIN STRUCTS ****/\n");
    struct list* dataStructs = map_getKeyList(program->dataStructsMap);   
    struct listElem* dataStructElem;
    for(dataStructElem = list_begin(dataStructs); dataStructElem != list_end(dataStructs); dataStructElem = list_next(dataStructElem)) {
        struct dataStruct* dataStruct = map_get(program->dataStructsMap, (char*)dataStructElem->data);
        generateStruct(out, dataStruct);
    }
    fprintf(out, "\n/*** BEGIN GLOBALS ****/\n");
    struct list* modules = map_getKeyList(program->modulesMap);   
    struct listElem* moduleElem;
    // Globals
    for(moduleElem = list_begin(modules); moduleElem != list_end(modules); moduleElem = list_next(moduleElem)) {
        struct module* module = map_get(program->modulesMap, (char*)moduleElem->data);
        struct list* globals = map_getKeyList(module->block->varMap);
        struct listElem* globalElem;
        for(globalElem = list_begin(globals); globalElem != list_end(globals); globalElem = list_next(globalElem)) {
            struct variable* global = map_get(module->block->varMap, (char*)globalElem->data);
            generateGlobal(out, module->name, global);
        }
    }
    fprintf(out, "\n/*** BEGIN FUNCTIONS ****/\n");
    // Functions
    for(moduleElem = list_begin(modules); moduleElem != list_end(modules); moduleElem = list_next(moduleElem)) {
        struct module* module = map_get(program->modulesMap, (char*)moduleElem->data);
        struct list* functions = map_getKeyList(module->functionsMap);
        struct listElem* functionElem;
        for(functionElem = list_begin(functions); functionElem != list_end(functions); functionElem = list_next(functionElem)) {
            struct function* function = map_get(module->functionsMap, (char*)functionElem->data);
            generateFunction(out, module->name, function);
        }
    }
}

static void generateStruct(FILE* out, struct dataStruct* dataStruct) {
    fprintf(out, "class %s {\n\tconstructor(", dataStruct->self.name);
    struct list* fields = dataStruct->argBlock->varMap->keyList;
    struct listElem* fieldElem;
    for(fieldElem = list_begin(fields); fieldElem != list_end(fields); fieldElem = list_next(fieldElem)) {
        fprintf(out, "%s", (char*)fieldElem->data);
        if(fieldElem->next != list_end(fields)){
            fprintf(out, ", ");
        }
    }
    fprintf(out,") {\n");
    for(fieldElem = list_begin(fields); fieldElem != list_end(fields); fieldElem = list_next(fieldElem)) {
        fprintf(out, "\t\tthis.%s=%s;\n", (char*)fieldElem->data, (char*)fieldElem->data);
        
    }
    fprintf(out, "\t}\n}\n");
}

static void generateGlobal(FILE* out, char* moduleName, struct variable* variable) {
    fprintf(out, "let %s_%s;\n", moduleName, variable->name);
    // @todo implement ast and expression generation
}

static void generateFunction(FILE* out, char* moduleName, struct function* function) {
    fprintf(out, "function %s_%s(", moduleName, function->self.name);
    struct list* params = function->argBlock->varMap->keyList;
    struct listElem* paramElem;
    for(paramElem = list_begin(params); paramElem != list_end(params); paramElem = list_next(paramElem)) {
        fprintf(out, "%s", (char*)paramElem->data);
        if(paramElem->next != list_end(params)){
            fprintf(out, ", ");
        }
    }
    fprintf(out,")");
    generateAST(out, 0, moduleName, function->self.code);
}

static void printTabs(FILE* out, int tabs) {
    for(int i = 0; i < tabs; i++) {
        fprintf(out, "\t");
    }
}

static void generateAST(FILE* out, int tabs, char* moduleName, struct astNode* node) {
    if(node == NULL) return;
    printTabs(out, tabs);

    switch(node->type) {
    case AST_BLOCK: {
        fprintf(out, "{\n");
        struct listElem* elem;
        for(elem = list_begin(node->children); elem != list_end(node->children); elem = list_next(elem)) {
            generateAST(out, tabs + 1, moduleName, (struct astNode*)elem->data);
        }
        fprintf(out, "}\n");
        break;
    }
    case AST_VARDECLARE:
        fprintf(out, "let %s;\n", ((struct variable*)node->data)->name);
        break;
    case AST_VARDEFINE:
        fprintf(out, "let %s;\n", ((struct variable*)node->data)->name);
        // @todo implement generateExpression
        break;
    case AST_IF:
    case AST_IFELSE:
    case AST_WHILE:
    case AST_RETURN:
    default:
        generateExpression(out, moduleName, node);
        fprintf(out, ";\n");
        break;
    }
}

static void generateExpression(FILE* out, char* moduleName, struct astNode* node) {
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
    case AST_VAR:
        fprintf(out, "%s", (char*)node->data);
        break;
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
    case AST_MODULEACCESS:
        generateExpression(out, moduleName, node->children->head.next->next->data);
        fprintf(out, "%s", (char*)node->data);
        generateExpression(out, moduleName, node->children->head.next->data);
    default:
        break;
    }
}