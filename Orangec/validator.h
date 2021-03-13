/*  validator.h

    Author: Joseph Shimel
    Date: 2/22/21
*/

#ifndef VALIDATOR_H
#define VALIDATOR_H

#include "./symbol.h"

void validator_updateStructType(struct symbolNode*);
void validator_validate(struct symbolNode*);

#endif