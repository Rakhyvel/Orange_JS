/*  validator.h

    Author: Joseph Shimel
    Date: 2/22/21
*/

#ifndef VALIDATOR_H
#define VALIDATOR_H

#include "./parser.h"

void validator_extendStructs(struct program*);
void validator_validate(struct program*);

#endif