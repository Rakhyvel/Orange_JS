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

#include "../util/debug.h"
#include "../util/list.h"
#include "../util/map.h"

static const char* systemDef = "function System_println(msg){\n\tconsole.log(msg);\n}\nfunction System_input(msg){\n\treturn window.prompt(msg);\n}";
static const char* canvasDef = "let canvas;let context;function Canvas_init(id){canvas=document.getElementById(id);context=canvas.getContext('2d');}function Canvas_setColor(color){context.fillStyle='rgb('+color.r+','+color.g+','+color.b+')';}function Canvas_setStroke(w, color){context.width=w;context.strokeStyle='rgb('+color.r+','+color.g+','+color.b+')';}function Canvas_setFont(font){context.font=font;}function Canvas_drawLine(x1, y1, x2, y2){context.beginPath();context.moveTo(x1, y1);context.lineTo(x2, y2);context.stroke();}function Canvas_drawRect(x,y,w,h){context.beginPath();context.rect(x,y,w,h);context.stroke();}function Canvas_fillRect(x,y,w,h){context.fillRect(x,y,w,h);}function Canvas_drawString(text, x, y){context.fillText(text, x, y);}";

static void generateStruct(FILE*, struct dataStruct*);
static void generateGlobal(FILE*, char*, struct variable*);
static void generateFunction(FILE*, char*, struct function*);
static void printTabs(FILE*, int);
static void generateAST(FILE*, int, char*, struct astNode*);
static void generateExpression(FILE*, char*, struct astNode*, int);

void generator_generate(FILE* out) {
    char* startMod;
    char* startFunc;

    // Each struct should be a class with only a constructor
    fprintf(out, "/*** BEGIN STRUCTS ****/\n");
    struct list* dataStructs = map_getKeyList(program->dataStructsMap);   
    struct listElem* dataStructElem;
    for(dataStructElem = list_begin(dataStructs); dataStructElem != list_end(dataStructs); dataStructElem = list_next(dataStructElem)) {
        struct dataStruct* dataStruct = map_get(program->dataStructsMap, (char*)dataStructElem->data);
        generateStruct(out, dataStruct);
    }
    fprintf(out, "\n/*** BEGIN SYSTEM/CANVAS ****/\n");
    fprintf(out, "%s", systemDef);
    fprintf(out, "%s", canvasDef);
    fprintf(out, "\n/*** BEGIN GLOBALS ****/\n");
    struct list* modules = map_getKeyList(program->modulesMap);   
    struct listElem* moduleElem;
    // Globals
    for(moduleElem = list_begin(modules); moduleElem != list_end(modules); moduleElem = list_next(moduleElem)) {
        struct module* module = map_get(program->modulesMap, (char*)moduleElem->data);
        if(!strcmp(module->name, "System") || !strcmp(module->name, "Canvas")) {
            continue;
        }
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
        if(!strcmp(module->name, "System") || !strcmp(module->name, "Canvas")) {
            continue;
        }
        struct list* functions = map_getKeyList(module->functionsMap);
        struct listElem* functionElem;
        for(functionElem = list_begin(functions); functionElem != list_end(functions); functionElem = list_next(functionElem)) {
            struct function* function = map_get(module->functionsMap, (char*)functionElem->data);
            if(!strcmp(function->self.name, "start")) {
                startMod=module->name;
                startFunc=function->self.name;
            }
            generateFunction(out, module->name, function);
        }
    }
    fprintf(out, "%s_%s();", startMod, startFunc);
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

    switch(node->type) {
    case AST_BLOCK: {
        fprintf(out, "{\n");
        struct listElem* elem;
        for(elem = list_begin(node->children); elem != list_end(node->children); elem = list_next(elem)) {
            generateAST(out, tabs + 1, moduleName, (struct astNode*)elem->data);
        }
        printTabs(out, tabs);
        fprintf(out, "}\n");
        break;
    }
    case AST_VARDECLARE:
        printTabs(out, tabs);
        fprintf(out, "let %s;\n", ((struct variable*)node->data)->name);
        break;
    case AST_VARDEFINE:
        printTabs(out, tabs);
        fprintf(out, "let %s=", ((struct variable*)node->data)->name);
        generateExpression(out, moduleName, ((struct variable*)node->data)->code, 0);
        fprintf(out, ";\n");
        break;
    case AST_IF:
        printTabs(out, tabs);
        fprintf(out, "if(");
        generateExpression(out, moduleName, node->children->head.next->data, 0);
        fprintf(out, ")");
        generateAST(out, tabs, moduleName, node->children->head.next->next->data);
        break;
    case AST_IFELSE:
        printTabs(out, tabs);
        fprintf(out, "if(");
        generateExpression(out, moduleName, node->children->head.next->data, 0);
        fprintf(out, ")");
        generateAST(out, tabs, moduleName, node->children->head.next->next->data);
        printTabs(out, tabs);
        fprintf(out, "else");
        generateAST(out, tabs, moduleName, node->children->head.next->next->next->data);
        break;
    case AST_WHILE:
        printTabs(out, tabs);
        fprintf(out, "while(");
        generateExpression(out, moduleName, node->children->head.next->data, 0);
        fprintf(out, ")");
        generateAST(out, tabs, moduleName, node->children->head.next->next->data);
        break;
    case AST_RETURN:
        printTabs(out, tabs);
        fprintf(out, "return ");
        generateExpression(out, moduleName, node->children->head.next->data, 0);
        break;
    default:
        printTabs(out, tabs);
        generateExpression(out, moduleName, node, 0);
        fprintf(out, ";\n");
        break;
    }
}

static void generateExpression(FILE* out, char* moduleName, struct astNode* node, int external) {
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
        generateExpression(out, moduleName, node->children->head.next->next->data, external);
        fprintf(out, "%s", (char*)node->data);
        generateExpression(out, moduleName, node->children->head.next->data, external);
        break;
    case AST_MODULEACCESS:
        generateExpression(out, moduleName, node->children->head.next->next->data, 1);
        fprintf(out, "_");
        generateExpression(out, moduleName, node->children->head.next->data, 1);
        break;
    case AST_CALL: {
        if(strstr(node->data, " array")) {
            fprintf(out, "Array(");
        } else if(!external) {
            fprintf(out, "%s_%s(", moduleName, (char*)node->data);
        } else {
            fprintf(out, "%s(", (char*)node->data);
        }
        struct listElem* elem;
        for(elem = list_begin(node->children); elem != list_end(node->children); elem = list_next(elem)) {
            generateExpression(out, moduleName, (struct astNode*)elem->data, external);
            if(elem->next != list_end(node->children)){
                fprintf(out, ", ");
            }
        }
        fprintf(out, ")");
        break;
    }
    case AST_INDEX:
        generateExpression(out, moduleName, node->children->head.next->next->data, external);
        fprintf(out, "[");
        generateExpression(out, moduleName, node->children->head.next->data, external);
        fprintf(out, "]");
        break;
    case AST_NEW: {
        struct astNode* rightAST = node->children->head.next->data;
        fprintf(out, "new ");
        if(rightAST->type == AST_CALL) {
            generateExpression(out, moduleName, node->children->head.next->data, external);
        } else if(rightAST->type == AST_INDEX) {
            fprintf(out, "Array(%d)", *(int*)(((struct astNode*)rightAST->children->head.next->data)->data));
        }
    }
    case AST_FREE:
    default:
        break;
    }
}