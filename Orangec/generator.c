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

static const char* systemDef = "function System_println(msg){\n\tconsole.log(msg);\n}\nfunction System_input(msg){\n\treturn window.prompt(msg);\n}";
static const char* canvasDef = "let canvas;let context;let mousePos = new Point(0, 0);canvas.addEventListener('mousemove', function(e) {var cRect = canvas.getBoundingClientRect();mousePos.x = Math.round(e.clientX - cRect.left); mousePos.y = Math.round(e.clientY - cRect.top);});function Canvas_init(id){canvas=document.getElementById(id);context=canvas.getContext('2d');}function Canvas_setColor(color){context.fillStyle='rgb('+color.r+','+color.g+','+color.b+')';}function Canvas_setStroke(w, color){context.width=w;context.strokeStyle='rgb('+color.r+','+color.g+','+color.b+')';}function Canvas_setFont(font){context.font=font;}function Canvas_drawLine(x1, y1, x2, y2){context.beginPath();context.moveTo(x1, y1);context.lineTo(x2, y2);context.stroke();}function Canvas_drawRect(x,y,w,h){context.beginPath();context.rect(x,y,w,h);context.stroke();}function Canvas_fillRect(x,y,w,h){context.fillRect(x,y,w,h);}function Canvas_drawString(text, x, y){context.fillText(text, x, y);}function Canvas_width(){return canvas.width;}function Canvas_height(){return canvas.height;}function Canvas_mouseX(){return mousePos.x;}function Canvas_mouseY(){return mousePos.y;}";

static void constructLists(struct symbolNode*, struct list*, struct list*, struct list*);
static void generateStruct(FILE*, struct symbolNode*);
static void generateGlobal(FILE*, char*, struct symbolNode*);
static void generateFunction(FILE*, char*, struct symbolNode*);
static void printTabs(FILE*, int);
static void generateAST(FILE*, int, char*, struct astNode*);
static void generateExpression(FILE*, char*, struct astNode*, int);

void generator_generate(FILE* out) {
    struct list* structList = list_create();
    struct list* globalList = list_create();
    struct list* functionList = list_create();
    constructLists(program, structList, globalList, functionList);

}

static void constructLists(struct symbolNode* node, struct list* structList, struct list* globalList, struct list* functionList) {
    struct listElem* elem = list_begin(node->children->keyList);
    for(;elem != list_end(node->children->keyList); elem = list_next(elem)) {
        LOG("%s", elem->data);
        struct symbolNode* child = (struct symbolNode*)map_get(node->children, (char*)elem->data);

        if(child->symbolType == SYMBOL_STRUCT) {
            queue_push(structList, child);
        } else if(child->symbolType == SYMBOL_VARIABLE && node->type == SYMBOL_MODULE) {
            queue_push(globalList, child);
        } else if(child->symbolType == SYMBOL_FUNCTION) {
            queue_push(functionList, child);
        }
        constructLists(child, structList, globalList, functionList);
    }
}

static void generateStruct(FILE* out, struct symbolNode* dataStruct) {
    fprintf(out, "class %s {\n\tconstructor(", dataStruct->name);
    struct list* fields = dataStruct->children->keyList;
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

static void generateGlobal(FILE* out, char* moduleName, struct symbolNode* variable) {
    fprintf(out, "let %s_%s;\n", moduleName, variable->name);
    // @todo implement ast and expression generation
}

static void generateFunction(FILE* out, char* moduleName, struct symbolNode* function) {
    fprintf(out, "function %s_%s(", moduleName, function->name);
    struct list* params = function->children->keyList;
    struct listElem* paramElem;
    for(paramElem = list_begin(params); paramElem != list_end(params); paramElem = list_next(paramElem)) {
        if(!strcmp((char*)paramElem->data, "_block")) {
            continue;
        }
        fprintf(out, "%s", (char*)paramElem->data);
        if(paramElem->next != list_end(params)){
            fprintf(out, ", ");
        }
    }
    fprintf(out,")");
    generateAST(out, 0, moduleName, function->code);
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
        fprintf(out, "let %s;\n", ((struct symbolNode*)node->data)->name);
        break;
    case AST_VARDEFINE:
        printTabs(out, tabs);
        fprintf(out, "let %s=", ((struct symbolNode*)node->data)->name);
        generateExpression(out, moduleName, ((struct symbolNode*)node->data)->code, 0);
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
        fprintf(out, ";\n");
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
        fprintf(out, "%s", (char*)node->data);
        break;
    case AST_VAR: {
        // Check to see if variable is a function, add module superscript if so
        struct symbolNode* module = map_get(program->children, moduleName);
        struct symbolNode* var = map_get(module->children, (char*)node->data);
        if(var == NULL || external) {
            fprintf(out, "%s", (char*)node->data);
        } else {
            fprintf(out, "%s_%s", moduleName, (char*)node->data);
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