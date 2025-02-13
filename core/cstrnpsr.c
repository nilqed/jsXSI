   /*******************************************************/
   /*      "C" Language Integrated Production System      */
   /*                                                     */
   /*            CLIPS Version 6.40  07/30/16             */
   /*                                                     */
   /*               CONSTRAINT PARSER MODULE              */
   /*******************************************************/

/*************************************************************/
/* Purpose: Provides functions for parsing constraint        */
/*   declarations.                                           */
/*                                                           */
/* Principal Programmer(s):                                  */
/*      Gary D. Riley                                        */
/*                                                           */
/* Contributing Programmer(s):                               */
/*      Brian Dantes                                         */
/*                                                           */
/* Revision History:                                         */
/*                                                           */
/*      6.23: Changed name of variable exp to theExp         */
/*            because of Unix compiler warnings of shadowed  */
/*            definitions.                                   */
/*                                                           */
/*      6.24: Added allowed-classes slot facet.              */
/*                                                           */
/*            Renamed BOOLEAN macro type to intBool.         */
/*                                                           */
/*      6.30: Used gensprintf instead of sprintf.            */
/*                                                           */
/*            Added const qualifiers to remove C++           */
/*            deprecation warnings.                          */
/*                                                           */
/*            Slot cardinality bug fix for minimum < 0.      */
/*                                                           */
/*      6.40: Pragma once and other inclusion changes.       */
/*                                                           */
/*            Added support for booleans with <stdbool.h>.   */
/*                                                           */
/*            Removed use of void pointers for specific      */
/*            data structures.                               */
/*                                                           */
/*************************************************************/

#include <stdio.h>
#include <stdlib.h>

#include "setup.h"

#include "constant.h"
#include "cstrnchk.h"
#include "cstrnutl.h"
#include "envrnmnt.h"
#include "memalloc.h"
#include "router.h"
#include "scanner.h"
#include "sysdep.h"

#include "cstrnpsr.h"

/***************************************/
/* LOCAL INTERNAL FUNCTION DEFINITIONS */
/***************************************/

#if (! RUN_TIME) && (! BLOAD_ONLY)
   static bool                    ParseRangeCardinalityAttribute(Environment *,
                                                                 const char *,CONSTRAINT_RECORD *,
                                                                 CONSTRAINT_PARSE_RECORD *,
                                                                 const char *,bool);
   static bool                    ParseTypeAttribute(Environment *,const char *,CONSTRAINT_RECORD *);
   static void                    AddToRestrictionList(Environment *,int,CONSTRAINT_RECORD *,
                                                       CONSTRAINT_RECORD *);
   static bool                    ParseAllowedValuesAttribute(Environment *,const char *,const char *,
                                                              CONSTRAINT_RECORD *,
                                                              CONSTRAINT_PARSE_RECORD *);
   static int                     GetConstraintTypeFromAllowedName(const char *);
   static int                     GetConstraintTypeFromTypeName(const char *);
   static bool                    GetAttributeParseValue(const char *,CONSTRAINT_PARSE_RECORD *);
   static void                    SetRestrictionFlag(int,CONSTRAINT_RECORD *,bool);
   static void                    SetParseFlag(CONSTRAINT_PARSE_RECORD *,const char *);
   static void                    NoConjunctiveUseError(Environment *,const char *,const char *);
#endif

/********************************************************************/
/* CheckConstraintParseConflicts: Determines if a constraint record */
/*   has any conflicts in the attribute specifications. Returns     */
/*   true if no conflicts were detected, otherwise false.           */
/********************************************************************/
bool CheckConstraintParseConflicts(
  Environment *theEnv,
  CONSTRAINT_RECORD *constraints)
  {
   /*===================================================*/
   /* Check to see if any of the allowed-... attributes */
   /* conflict with the type attribute.                 */
   /*===================================================*/

   if (constraints->anyAllowed == true)
     { /* Do Nothing */ }
   else if (constraints->symbolRestriction &&
            (constraints->symbolsAllowed == false))
     {
      AttributeConflictErrorMessage(theEnv,"type","allowed-symbols");
      return false;
     }
   else if (constraints->stringRestriction &&
            (constraints->stringsAllowed == false))
     {
      AttributeConflictErrorMessage(theEnv,"type","allowed-strings");
      return false;
     }
   else if (constraints->integerRestriction &&
            (constraints->integersAllowed == false))
     {
      AttributeConflictErrorMessage(theEnv,"type","allowed-integers/numbers");
      return false;
     }
   else if (constraints->floatRestriction &&
            (constraints->floatsAllowed == false))
     {
      AttributeConflictErrorMessage(theEnv,"type","allowed-floats/numbers");
      return false;
     }
   else if (constraints->classRestriction &&
            (constraints->instanceAddressesAllowed == false) &&
            (constraints->instanceNamesAllowed == false))
     {
      AttributeConflictErrorMessage(theEnv,"type","allowed-classes");
      return false;
     }
   else if (constraints->instanceNameRestriction &&
            (constraints->instanceNamesAllowed == false))
     {
      AttributeConflictErrorMessage(theEnv,"type","allowed-instance-names");
      return false;
     }
   else if (constraints->anyRestriction)
     {
      struct expr *theExp;

      for (theExp = constraints->restrictionList;
           theExp != NULL;
           theExp = theExp->nextArg)
        {
         if (ConstraintCheckValue(theEnv,theExp->type,theExp->value,constraints) != NO_VIOLATION)
           {
            AttributeConflictErrorMessage(theEnv,"type","allowed-values");
            return false;
           }
        }
     }

   /*================================================================*/
   /* Check to see if range attribute conflicts with type attribute. */
   /*================================================================*/

   if ((constraints->maxValue != NULL) &&
       (constraints->anyAllowed == false))
     {
      if (((constraints->maxValue->type == INTEGER) &&
          (constraints->integersAllowed == false)) ||
          ((constraints->maxValue->type == FLOAT) &&
           (constraints->floatsAllowed == false)))
        {
         AttributeConflictErrorMessage(theEnv,"type","range");
         return false;
        }
     }

   if ((constraints->minValue != NULL) &&
       (constraints->anyAllowed == false))
     {
      if (((constraints->minValue->type == INTEGER) &&
          (constraints->integersAllowed == false)) ||
          ((constraints->minValue->type == FLOAT) &&
           (constraints->floatsAllowed == false)))
        {
         AttributeConflictErrorMessage(theEnv,"type","range");
         return false;
        }
     }

   /*=========================================*/
   /* Check to see if allowed-class attribute */
   /* conflicts with type attribute.          */
   /*=========================================*/

   if ((constraints->classList != NULL) &&
       (constraints->anyAllowed == false) &&
       (constraints->instanceNamesAllowed == false) &&
       (constraints->instanceAddressesAllowed == false))
     {
      AttributeConflictErrorMessage(theEnv,"type","allowed-class");
      return false;
     }

   /*=====================================================*/
   /* Return true to indicate no conflicts were detected. */
   /*=====================================================*/

   return true;
  }

/********************************************************/
/* AttributeConflictErrorMessage: Generic error message */
/*   for a constraint attribute conflict.               */
/********************************************************/
void AttributeConflictErrorMessage(
  Environment *theEnv,
  const char *attribute1,
  const char *attribute2)
  {
   PrintErrorID(theEnv,"CSTRNPSR",1,true);
   EnvPrintRouter(theEnv,WERROR,"The ");
   EnvPrintRouter(theEnv,WERROR,attribute1);
   EnvPrintRouter(theEnv,WERROR," attribute conflicts with the ");
   EnvPrintRouter(theEnv,WERROR,attribute2);
   EnvPrintRouter(theEnv,WERROR," attribute.\n");
  }

#if (! RUN_TIME) && (! BLOAD_ONLY)

/***************************************************************************/
/* InitializeConstraintParseRecord: Initializes the values of a constraint */
/*   parse record which is used to determine whether one of the standard   */
/*   constraint specifications has already been parsed.                    */
/***************************************************************************/
void InitializeConstraintParseRecord(
  CONSTRAINT_PARSE_RECORD *parsedConstraints)
  {
   parsedConstraints->type = false;
   parsedConstraints->range = false;
   parsedConstraints->allowedSymbols = false;
   parsedConstraints->allowedStrings = false;
   parsedConstraints->allowedLexemes = false;
   parsedConstraints->allowedIntegers = false;
   parsedConstraints->allowedFloats = false;
   parsedConstraints->allowedNumbers = false;
   parsedConstraints->allowedValues = false;
   parsedConstraints->allowedInstanceNames = false;
   parsedConstraints->allowedClasses = false;
   parsedConstraints->cardinality = false;
  }

/************************************************************************/
/* StandardConstraint: Returns true if the specified name is one of the */
/*   standard constraints parseable by the routines in this module.     */
/************************************************************************/
bool StandardConstraint(
  const char *constraintName)
  {
   if ((strcmp(constraintName,"type") == 0) ||
       (strcmp(constraintName,"range") == 0) ||
       (strcmp(constraintName,"cardinality") == 0) ||
       (strcmp(constraintName,"allowed-symbols") == 0) ||
       (strcmp(constraintName,"allowed-strings") == 0) ||
       (strcmp(constraintName,"allowed-lexemes") == 0) ||
       (strcmp(constraintName,"allowed-integers") == 0) ||
       (strcmp(constraintName,"allowed-floats") == 0) ||
       (strcmp(constraintName,"allowed-numbers") == 0) ||
       (strcmp(constraintName,"allowed-instance-names") == 0) ||
       (strcmp(constraintName,"allowed-classes") == 0) ||
       (strcmp(constraintName,"allowed-values") == 0))

     { return true; }

   return false;
  }

/***********************************************************************/
/* ParseStandardConstraint: Parses a standard constraint. Returns true */
/*   if the constraint was successfully parsed, otherwise false.       */
/***********************************************************************/
bool ParseStandardConstraint(
  Environment *theEnv,
  const char *readSource,
  const char *constraintName,
  CONSTRAINT_RECORD *constraints,
  CONSTRAINT_PARSE_RECORD *parsedConstraints,
  bool multipleValuesAllowed)
  {
   bool rv = false;

   /*=====================================================*/
   /* Determine if the attribute has already been parsed. */
   /*=====================================================*/

   if (GetAttributeParseValue(constraintName,parsedConstraints))
     {
      AlreadyParsedErrorMessage(theEnv,constraintName," attribute");
      return false;
     }

   /*==========================================*/
   /* If specified, parse the range attribute. */
   /*==========================================*/

   if (strcmp(constraintName,"range") == 0)
     {
      rv = ParseRangeCardinalityAttribute(theEnv,readSource,constraints,parsedConstraints,
                                          constraintName,multipleValuesAllowed);
     }

   /*================================================*/
   /* If specified, parse the cardinality attribute. */
   /*================================================*/

   else if (strcmp(constraintName,"cardinality") == 0)
     {
      rv = ParseRangeCardinalityAttribute(theEnv,readSource,constraints,parsedConstraints,
                                          constraintName,multipleValuesAllowed);
     }

   /*=========================================*/
   /* If specified, parse the type attribute. */
   /*=========================================*/

   else if (strcmp(constraintName,"type") == 0)
     { rv = ParseTypeAttribute(theEnv,readSource,constraints); }

   /*================================================*/
   /* If specified, parse the allowed-... attribute. */
   /*================================================*/

   else if ((strcmp(constraintName,"allowed-symbols") == 0) ||
            (strcmp(constraintName,"allowed-strings") == 0) ||
            (strcmp(constraintName,"allowed-lexemes") == 0) ||
            (strcmp(constraintName,"allowed-integers") == 0) ||
            (strcmp(constraintName,"allowed-floats") == 0) ||
            (strcmp(constraintName,"allowed-numbers") == 0) ||
            (strcmp(constraintName,"allowed-instance-names") == 0) ||
            (strcmp(constraintName,"allowed-classes") == 0) ||
            (strcmp(constraintName,"allowed-values") == 0))
     {
      rv = ParseAllowedValuesAttribute(theEnv,readSource,constraintName,
                                       constraints,parsedConstraints);
     }

   /*=========================================*/
   /* Remember which constraint attribute was */
   /* parsed and return the error status.     */
   /*=========================================*/

   SetParseFlag(parsedConstraints,constraintName);
   return(rv);
  }

/***********************************************************/
/* OverlayConstraint: Overlays fields of source constraint */
/* record on destination based on which fields are set in  */
/* the parsed constraint record. Assumes AddConstraint has */
/* not yet been called for the destination constraint      */
/* record.                                                 */
/***********************************************************/
void OverlayConstraint(
  Environment *theEnv,
  CONSTRAINT_PARSE_RECORD *pc,
  CONSTRAINT_RECORD *cdst,
  CONSTRAINT_RECORD *csrc)
  {
   if (pc->type == 0)
     {
      cdst->anyAllowed = csrc->anyAllowed;
      cdst->symbolsAllowed = csrc->symbolsAllowed;
      cdst->stringsAllowed = csrc->stringsAllowed;
      cdst->floatsAllowed = csrc->floatsAllowed;
      cdst->integersAllowed = csrc->integersAllowed;
      cdst->instanceNamesAllowed = csrc->instanceNamesAllowed;
      cdst->instanceAddressesAllowed = csrc->instanceAddressesAllowed;
      cdst->externalAddressesAllowed = csrc->externalAddressesAllowed;
      cdst->voidAllowed = csrc->voidAllowed;
      cdst->factAddressesAllowed = csrc->factAddressesAllowed;
     }

   if (pc->range == 0)
     {
      ReturnExpression(theEnv,cdst->minValue);
      ReturnExpression(theEnv,cdst->maxValue);
      cdst->minValue = CopyExpression(theEnv,csrc->minValue);
      cdst->maxValue = CopyExpression(theEnv,csrc->maxValue);
     }

   if (pc->allowedClasses == 0)
     {
      ReturnExpression(theEnv,cdst->classList);
      cdst->classList = CopyExpression(theEnv,csrc->classList);
     }

   if (pc->allowedValues == 0)
     {
      if ((pc->allowedSymbols == 0) &&
          (pc->allowedStrings == 0) &&
          (pc->allowedLexemes == 0) &&
          (pc->allowedIntegers == 0) &&
          (pc->allowedFloats == 0) &&
          (pc->allowedNumbers == 0) &&
          (pc->allowedInstanceNames == 0))
        {
         cdst->anyRestriction = csrc->anyRestriction;
         cdst->symbolRestriction = csrc->symbolRestriction;
         cdst->stringRestriction = csrc->stringRestriction;
         cdst->floatRestriction = csrc->floatRestriction;
         cdst->integerRestriction = csrc->integerRestriction;
         cdst->classRestriction = csrc->classRestriction;
         cdst->instanceNameRestriction = csrc->instanceNameRestriction;
         cdst->restrictionList = CopyExpression(theEnv,csrc->restrictionList);
        }
      else
        {
         if ((pc->allowedSymbols == 0) && csrc->symbolRestriction)
           {
            cdst->symbolRestriction = 1;
            AddToRestrictionList(theEnv,SYMBOL,cdst,csrc);
           }
         if ((pc->allowedStrings == 0) && csrc->stringRestriction)
           {
            cdst->stringRestriction = 1;
            AddToRestrictionList(theEnv,STRING,cdst,csrc);
           }
         if ((pc->allowedLexemes == 0) && csrc->symbolRestriction && csrc->stringRestriction)
           {
            cdst->symbolRestriction = 1;
            cdst->stringRestriction = 1;
            AddToRestrictionList(theEnv,SYMBOL,cdst,csrc);
            AddToRestrictionList(theEnv,STRING,cdst,csrc);
           }
         if ((pc->allowedIntegers == 0) && csrc->integerRestriction)
           {
            cdst->integerRestriction = 1;
            AddToRestrictionList(theEnv,INTEGER,cdst,csrc);
           }
         if ((pc->allowedFloats == 0) && csrc->floatRestriction)
           {
            cdst->floatRestriction = 1;
            AddToRestrictionList(theEnv,FLOAT,cdst,csrc);
           }
         if ((pc->allowedNumbers == 0) && csrc->integerRestriction && csrc->floatRestriction)
           {
            cdst->integerRestriction = 1;
            cdst->floatRestriction = 1;
            AddToRestrictionList(theEnv,INTEGER,cdst,csrc);
            AddToRestrictionList(theEnv,FLOAT,cdst,csrc);
           }
         if ((pc->allowedInstanceNames == 0) && csrc->instanceNameRestriction)
           {
            cdst->instanceNameRestriction = 1;
            AddToRestrictionList(theEnv,INSTANCE_NAME,cdst,csrc);
           }
        }
     }

   if (pc->cardinality == 0)
     {
      ReturnExpression(theEnv,cdst->minFields);
      ReturnExpression(theEnv,cdst->maxFields);
      cdst->minFields = CopyExpression(theEnv,csrc->minFields);
      cdst->maxFields = CopyExpression(theEnv,csrc->maxFields);
     }
  }

/**********************************************/
/* OverlayConstraintParseRecord: Performs a   */
/*   field-wise "or" of the destination parse */
/*   record with the source parse record.     */
/**********************************************/
void OverlayConstraintParseRecord(
  CONSTRAINT_PARSE_RECORD *dst,
  CONSTRAINT_PARSE_RECORD *src)
  {
   if (src->type) dst->type = true;
   if (src->range) dst->range = true;
   if (src->allowedSymbols) dst->allowedSymbols = true;
   if (src->allowedStrings) dst->allowedStrings = true;
   if (src->allowedLexemes) dst->allowedLexemes = true;
   if (src->allowedIntegers) dst->allowedIntegers = true;
   if (src->allowedFloats) dst->allowedFloats = true;
   if (src->allowedNumbers) dst->allowedNumbers = true;
   if (src->allowedValues) dst->allowedValues = true;
   if (src->allowedInstanceNames) dst->allowedInstanceNames = true;
   if (src->allowedClasses) dst->allowedClasses = true;
   if (src->cardinality) dst->cardinality = true;
  }

/************************************************************/
/* AddToRestrictionList: Prepends atoms of the specified    */
/* type from the source restriction list to the destination */
/************************************************************/
static void AddToRestrictionList(
  Environment *theEnv,
  int type,
  CONSTRAINT_RECORD *cdst,
  CONSTRAINT_RECORD *csrc)
  {
   struct expr *theExp,*tmp;

   for (theExp = csrc->restrictionList; theExp != NULL; theExp = theExp->nextArg)
     {
      if (theExp->type == type)
        {
         tmp = GenConstant(theEnv,theExp->type,theExp->value);
         tmp->nextArg = cdst->restrictionList;
         cdst->restrictionList = tmp;
        }
     }
  }

/*******************************************************************/
/* ParseAllowedValuesAttribute: Parses the allowed-... attributes. */
/*******************************************************************/
static bool ParseAllowedValuesAttribute(
  Environment *theEnv,
  const char *readSource,
  const char *constraintName,
  CONSTRAINT_RECORD *constraints,
  CONSTRAINT_PARSE_RECORD *parsedConstraints)
  {
   struct token inputToken;
   int expectedType, restrictionType;
   bool error = false;
   struct expr *newValue, *lastValue;
   bool constantParsed = false, variableParsed = false;
   const char *tempPtr = NULL;

   /*======================================================*/
   /* The allowed-values attribute is not allowed if other */
   /* allowed-... attributes have already been parsed.     */
   /*======================================================*/

   if ((strcmp(constraintName,"allowed-values") == 0) &&
       ((parsedConstraints->allowedSymbols) ||
        (parsedConstraints->allowedStrings) ||
        (parsedConstraints->allowedLexemes) ||
        (parsedConstraints->allowedIntegers) ||
        (parsedConstraints->allowedFloats) ||
        (parsedConstraints->allowedNumbers) ||
        (parsedConstraints->allowedInstanceNames)))
     {
      if (parsedConstraints->allowedSymbols) tempPtr = "allowed-symbols";
      else if (parsedConstraints->allowedStrings) tempPtr = "allowed-strings";
      else if (parsedConstraints->allowedLexemes) tempPtr = "allowed-lexemes";
      else if (parsedConstraints->allowedIntegers) tempPtr = "allowed-integers";
      else if (parsedConstraints->allowedFloats) tempPtr = "allowed-floats";
      else if (parsedConstraints->allowedNumbers) tempPtr = "allowed-numbers";
      else if (parsedConstraints->allowedInstanceNames) tempPtr = "allowed-instance-names";
      NoConjunctiveUseError(theEnv,"allowed-values",tempPtr);
      return false;
     }

   /*=======================================================*/
   /* The allowed-values/numbers/integers/floats attributes */
   /* are not allowed with the range attribute.             */
   /*=======================================================*/

   if (((strcmp(constraintName,"allowed-values") == 0) ||
        (strcmp(constraintName,"allowed-numbers") == 0) ||
        (strcmp(constraintName,"allowed-integers") == 0) ||
        (strcmp(constraintName,"allowed-floats") == 0)) &&
       (parsedConstraints->range))
     {
      NoConjunctiveUseError(theEnv,constraintName,"range");
      return false;
     }

   /*===================================================*/
   /* The allowed-... attributes are not allowed if the */
   /* allowed-values attribute has already been parsed. */
   /*===================================================*/

   if ((strcmp(constraintName,"allowed-values") != 0) &&
            (parsedConstraints->allowedValues))
     {
      NoConjunctiveUseError(theEnv,constraintName,"allowed-values");
      return false;
     }

   /*==================================================*/
   /* The allowed-numbers attribute is not allowed if  */
   /* the allowed-integers or allowed-floats attribute */
   /* has already been parsed.                         */
   /*==================================================*/

   if ((strcmp(constraintName,"allowed-numbers") == 0) &&
       ((parsedConstraints->allowedFloats) || (parsedConstraints->allowedIntegers)))
     {
      if (parsedConstraints->allowedFloats) tempPtr = "allowed-floats";
      else tempPtr = "allowed-integers";
      NoConjunctiveUseError(theEnv,"allowed-numbers",tempPtr);
      return false;
     }

   /*============================================================*/
   /* The allowed-integers/floats attributes are not allowed if  */
   /* the allowed-numbers attribute has already been parsed.     */
   /*============================================================*/

   if (((strcmp(constraintName,"allowed-integers") == 0) ||
        (strcmp(constraintName,"allowed-floats") == 0)) &&
       (parsedConstraints->allowedNumbers))
     {
      NoConjunctiveUseError(theEnv,constraintName,"allowed-number");
      return false;
     }

   /*==================================================*/
   /* The allowed-lexemes attribute is not allowed if  */
   /* the allowed-symbols or allowed-strings attribute */
   /* has already been parsed.                         */
   /*==================================================*/

   if ((strcmp(constraintName,"allowed-lexemes") == 0) &&
       ((parsedConstraints->allowedSymbols) || (parsedConstraints->allowedStrings)))
     {
      if (parsedConstraints->allowedSymbols) tempPtr = "allowed-symbols";
      else tempPtr = "allowed-strings";
      NoConjunctiveUseError(theEnv,"allowed-lexemes",tempPtr);
      return false;
     }

   /*===========================================================*/
   /* The allowed-symbols/strings attributes are not allowed if */
   /* the allowed-lexemes attribute has already been parsed.    */
   /*===========================================================*/

   if (((strcmp(constraintName,"allowed-symbols") == 0) ||
        (strcmp(constraintName,"allowed-strings") == 0)) &&
       (parsedConstraints->allowedLexemes))
     {
      NoConjunctiveUseError(theEnv,constraintName,"allowed-lexemes");
      return false;
     }

   /*========================*/
   /* Get the expected type. */
   /*========================*/

   restrictionType = GetConstraintTypeFromAllowedName(constraintName);
   SetRestrictionFlag(restrictionType,constraints,true);
   if (strcmp(constraintName,"allowed-classes") == 0)
     { expectedType = SYMBOL; }
   else
     { expectedType = restrictionType; }
   
   /*=================================================*/
   /* Get the last value in the restriction list (the */
   /* allowed values will be appended there).         */
   /*=================================================*/

   if (strcmp(constraintName,"allowed-classes") == 0)
     { lastValue = constraints->classList; }
   else
     { lastValue = constraints->restrictionList; }
     
   if (lastValue != NULL)
     { while (lastValue->nextArg != NULL) lastValue = lastValue->nextArg; }

   /*==================================================*/
   /* Read the allowed values and add them to the list */
   /* until a right parenthesis is encountered.        */
   /*==================================================*/

   SavePPBuffer(theEnv," ");
   GetToken(theEnv,readSource,&inputToken);

   while (inputToken.type != RPAREN)
     {
      SavePPBuffer(theEnv," ");

      /*=============================================*/
      /* Determine the type of the token just parsed */
      /* and if it is an appropriate value.          */
      /*=============================================*/

      switch(inputToken.type)
        {
         case INTEGER:
           if ((expectedType != UNKNOWN_VALUE) &&
               (expectedType != INTEGER) &&
               (expectedType != INTEGER_OR_FLOAT)) error = true;
           constantParsed = true;
           break;

         case FLOAT:
           if ((expectedType != UNKNOWN_VALUE) &&
               (expectedType != FLOAT) &&
               (expectedType != INTEGER_OR_FLOAT)) error = true;
           constantParsed = true;
           break;

         case STRING:
           if ((expectedType != UNKNOWN_VALUE) &&
               (expectedType != STRING) &&
               (expectedType != SYMBOL_OR_STRING)) error = true;
           constantParsed = true;
           break;

         case SYMBOL:
           if ((expectedType != UNKNOWN_VALUE) &&
               (expectedType != SYMBOL) &&
               (expectedType != SYMBOL_OR_STRING)) error = true;
           constantParsed = true;
           break;

#if OBJECT_SYSTEM
         case INSTANCE_NAME:
           if ((expectedType != UNKNOWN_VALUE) &&
               (expectedType != INSTANCE_NAME)) error = true;
           constantParsed = true;
           break;
#endif

         case SF_VARIABLE:
           if (strcmp(inputToken.printForm,"?VARIABLE") == 0)
             { variableParsed = true; }
           else
             {
              char tempBuffer[120];
              gensprintf(tempBuffer,"%s attribute",constraintName);
              SyntaxErrorMessage(theEnv,tempBuffer);
              return false;
             }

           break;

         default:
           {
            char tempBuffer[120];
            gensprintf(tempBuffer,"%s attribute",constraintName);
            SyntaxErrorMessage(theEnv,tempBuffer);
           }
           return false;
        }

      /*=====================================*/
      /* Signal an error if an inappropriate */
      /* value was found.                    */
      /*=====================================*/

      if (error)
        {
         PrintErrorID(theEnv,"CSTRNPSR",4,true);
         EnvPrintRouter(theEnv,WERROR,"Value does not match the expected type for the ");
         EnvPrintRouter(theEnv,WERROR,constraintName);
         EnvPrintRouter(theEnv,WERROR," attribute\n");
         return false;
        }

      /*======================================*/
      /* The ?VARIABLE argument can't be used */
      /* in conjunction with constants.       */
      /*======================================*/

      if (constantParsed && variableParsed)
        {
         char tempBuffer[120];
         gensprintf(tempBuffer,"%s attribute",constraintName);
         SyntaxErrorMessage(theEnv,tempBuffer);
         return false;
        }

      /*===========================================*/
      /* Add the constant to the restriction list. */
      /*===========================================*/

      newValue = GenConstant(theEnv,inputToken.type,inputToken.value);
      if (lastValue == NULL)
        { 
         if (strcmp(constraintName,"allowed-classes") == 0)
           { constraints->classList = newValue; }
         else
           { constraints->restrictionList = newValue; }
        }
      else
        { lastValue->nextArg = newValue; }
      lastValue = newValue;

      /*=======================================*/
      /* Begin parsing the next allowed value. */
      /*=======================================*/

      GetToken(theEnv,readSource,&inputToken);
     }

   /*======================================================*/
   /* There must be at least one value for this attribute. */
   /*======================================================*/

   if ((! constantParsed) && (! variableParsed))
     {
      char tempBuffer[120];
      gensprintf(tempBuffer,"%s attribute",constraintName);
      SyntaxErrorMessage(theEnv,tempBuffer);
      return false;
     }

   /*======================================*/
   /* If ?VARIABLE was parsed, then remove */
   /* the restrictions for the type being  */
   /* restricted.                          */
   /*======================================*/

   if (variableParsed)
     {
      switch(restrictionType)
        {
         case UNKNOWN_VALUE:
           constraints->anyRestriction = false;
           break;

         case SYMBOL:
           constraints->symbolRestriction = false;
           break;

         case STRING:
           constraints->stringRestriction = false;
           break;

         case INTEGER:
           constraints->integerRestriction = false;
           break;

         case FLOAT:
           constraints->floatRestriction = false;
           break;

         case INTEGER_OR_FLOAT:
           constraints->floatRestriction = false;
           constraints->integerRestriction = false;
           break;

         case SYMBOL_OR_STRING:
           constraints->symbolRestriction = false;
           constraints->stringRestriction = false;
           break;

         case INSTANCE_NAME:
           constraints->instanceNameRestriction = false;
           break;

         case INSTANCE_OR_INSTANCE_NAME:
           constraints->classRestriction = false;
           break;
        }
     }

   /*=====================================*/
   /* Fix up pretty print representation. */
   /*=====================================*/

   PPBackup(theEnv);
   PPBackup(theEnv);
   SavePPBuffer(theEnv,")");

   /*=======================================*/
   /* Return true to indicate the attribute */
   /* was successfully parsed.              */
   /*=======================================*/

   return true;
  }

/***********************************************************/
/* NoConjunctiveUseError: Generic error message indicating */
/*   that two attributes can't be used in conjunction.     */
/***********************************************************/
static void NoConjunctiveUseError(
  Environment *theEnv,
  const char *attribute1,
  const char *attribute2)
  {
   PrintErrorID(theEnv,"CSTRNPSR",3,true);
   EnvPrintRouter(theEnv,WERROR,"The ");
   EnvPrintRouter(theEnv,WERROR,attribute1);
   EnvPrintRouter(theEnv,WERROR," attribute cannot be used\n");
   EnvPrintRouter(theEnv,WERROR,"in conjunction with the ");
   EnvPrintRouter(theEnv,WERROR,attribute2);
   EnvPrintRouter(theEnv,WERROR," attribute.\n");
  }

/**************************************************/
/* ParseTypeAttribute: Parses the type attribute. */
/**************************************************/
static bool ParseTypeAttribute(
  Environment *theEnv,
  const char *readSource,
  CONSTRAINT_RECORD *constraints)
  {
   bool typeParsed = false;
   bool variableParsed = false;
   int theType;
   struct token inputToken;

   /*======================================*/
   /* Continue parsing types until a right */
   /* parenthesis is encountered.          */
   /*======================================*/

   SavePPBuffer(theEnv," ");
   for (GetToken(theEnv,readSource,&inputToken);
        inputToken.type != RPAREN;
        GetToken(theEnv,readSource,&inputToken))
     {
      SavePPBuffer(theEnv," ");

      /*==================================*/
      /* If the token is a symbol then... */
      /*==================================*/

      if (inputToken.type == SYMBOL)
        {
         /*==============================================*/
         /* ?VARIABLE can't be used with type constants. */
         /*==============================================*/

         if (variableParsed == true)
           {
            SyntaxErrorMessage(theEnv,"type attribute");
            return false;
           }

         /*========================================*/
         /* Check for an appropriate type constant */
         /* (e.g. SYMBOL, FLOAT, INTEGER, etc.).   */
         /*========================================*/

         theType = GetConstraintTypeFromTypeName(ValueToString(inputToken.value));
         if (theType < 0)
           {
            SyntaxErrorMessage(theEnv,"type attribute");
            return false;
           }

         /*==================================================*/
         /* Change the type restriction flags to reflect the */
         /* type restriction. If the type restriction was    */
         /* already specified, then a error is generated.    */
         /*==================================================*/

         if (SetConstraintType(theType,constraints))
           {
            SyntaxErrorMessage(theEnv,"type attribute");
            return false;
           }

         constraints->anyAllowed = false;

         /*===========================================*/
         /* Remember that a type constant was parsed. */
         /*===========================================*/

         typeParsed = true;
        }

      /*==============================================*/
      /* Otherwise if the token is a variable then... */
      /*==============================================*/

      else if (inputToken.type == SF_VARIABLE)
        {
         /*========================================*/
         /* The only variable allowd is ?VARIABLE. */
         /*========================================*/

         if (strcmp(inputToken.printForm,"?VARIABLE") != 0)
           {
            SyntaxErrorMessage(theEnv,"type attribute");
            return false;
           }

         /*===================================*/
         /* ?VARIABLE can't be used more than */
         /* once or with type constants.      */
         /*===================================*/

         if (typeParsed || variableParsed)
           {
            SyntaxErrorMessage(theEnv,"type attribute");
            return false;
           }

         /*======================================*/
         /* Remember that a variable was parsed. */
         /*======================================*/

         variableParsed = true;
        }

      /*====================================*/
      /* Otherwise this is an invalid value */
      /* for the type attribute.            */
      /*====================================*/

       else
        {
         SyntaxErrorMessage(theEnv,"type attribute");
         return false;
        }
     }

   /*=====================================*/
   /* Fix up pretty print representation. */
   /*=====================================*/

   PPBackup(theEnv);
   PPBackup(theEnv);
   SavePPBuffer(theEnv,")");

   /*=======================================*/
   /* The type attribute must have a value. */
   /*=======================================*/

   if ((! typeParsed) && (! variableParsed))
     {
      SyntaxErrorMessage(theEnv,"type attribute");
      return false;
     }

   /*===========================================*/
   /* Return true indicating the type attibuted */
   /* was successfully parsed.                  */
   /*===========================================*/

   return true;
  }

/***************************************************************************/
/* ParseRangeCardinalityAttribute: Parses the range/cardinality attribute. */
/***************************************************************************/
static bool ParseRangeCardinalityAttribute(
  Environment *theEnv,
  const char *readSource,
  CONSTRAINT_RECORD *constraints,
  CONSTRAINT_PARSE_RECORD *parsedConstraints,
  const char *constraintName,
  bool multipleValuesAllowed)
  {
   struct token inputToken;
   bool range;
   const char *tempPtr = NULL;

   /*=================================*/
   /* Determine if we're parsing the  */
   /* range or cardinality attribute. */
   /*=================================*/

   if (strcmp(constraintName,"range") == 0)
     {
      parsedConstraints->range = true;
      range = true;
     }
   else
     {
      parsedConstraints->cardinality = true;
      range = false;
     }

   /*===================================================================*/
   /* The cardinality attribute can only be used with multifield slots. */
   /*===================================================================*/

   if ((range == false) &&
       (multipleValuesAllowed == false))
     {
      PrintErrorID(theEnv,"CSTRNPSR",5,true);
      EnvPrintRouter(theEnv,WERROR,"The cardinality attribute ");
      EnvPrintRouter(theEnv,WERROR,"can only be used with multifield slots.\n");
      return false;
     }

   /*====================================================*/
   /* The range attribute is not allowed with the        */
   /* allowed-values/numbers/integers/floats attributes. */
   /*====================================================*/

   if ((range == true) &&
       (parsedConstraints->allowedValues ||
        parsedConstraints->allowedNumbers ||
        parsedConstraints->allowedIntegers ||
        parsedConstraints->allowedFloats))
     {
      if (parsedConstraints->allowedValues) tempPtr = "allowed-values";
      else if (parsedConstraints->allowedIntegers) tempPtr = "allowed-integers";
      else if (parsedConstraints->allowedFloats) tempPtr = "allowed-floats";
      else if (parsedConstraints->allowedNumbers) tempPtr = "allowed-numbers";
      NoConjunctiveUseError(theEnv,"range",tempPtr);
      return false;
     }

   /*==========================*/
   /* Parse the minimum value. */
   /*==========================*/

   SavePPBuffer(theEnv," ");
   GetToken(theEnv,readSource,&inputToken);
   if ((inputToken.type == INTEGER) || ((inputToken.type == FLOAT) && range))
     {
      if (range)
        {
         ReturnExpression(theEnv,constraints->minValue);
         constraints->minValue = GenConstant(theEnv,inputToken.type,inputToken.value);
        }
      else
        {
         if (ValueToLong(inputToken.value) < 0LL)
           {
            PrintErrorID(theEnv,"CSTRNPSR",6,true);
            EnvPrintRouter(theEnv,WERROR,"Minimum cardinality value must be greater than or equal to zero\n");
            return false;
           }

         ReturnExpression(theEnv,constraints->minFields);
         constraints->minFields = GenConstant(theEnv,inputToken.type,inputToken.value);
        }
     }
   else if ((inputToken.type == SF_VARIABLE) && (strcmp(inputToken.printForm,"?VARIABLE") == 0))
     { /* Do nothing. */ }
   else
     {
      char tempBuffer[120];
      gensprintf(tempBuffer,"%s attribute",constraintName);
      SyntaxErrorMessage(theEnv,tempBuffer);
      return false;
     }

   /*==========================*/
   /* Parse the maximum value. */
   /*==========================*/

   SavePPBuffer(theEnv," ");
   GetToken(theEnv,readSource,&inputToken);
   if ((inputToken.type == INTEGER) || ((inputToken.type == FLOAT) && range))
     {
      if (range)
        {
         ReturnExpression(theEnv,constraints->maxValue);
         constraints->maxValue = GenConstant(theEnv,inputToken.type,inputToken.value);
        }
      else
        {
         ReturnExpression(theEnv,constraints->maxFields);
         constraints->maxFields = GenConstant(theEnv,inputToken.type,inputToken.value);
        }
     }
   else if ((inputToken.type == SF_VARIABLE) && (strcmp(inputToken.printForm,"?VARIABLE") == 0))
     { /* Do nothing. */ }
   else
     {
      char tempBuffer[120];
      gensprintf(tempBuffer,"%s attribute",constraintName);
      SyntaxErrorMessage(theEnv,tempBuffer);
      return false;
     }

   /*================================*/
   /* Parse the closing parenthesis. */
   /*================================*/

   GetToken(theEnv,readSource,&inputToken);
   if (inputToken.type != RPAREN)
     {
      SyntaxErrorMessage(theEnv,"range attribute");
      return false;
     }

   /*====================================================*/
   /* Minimum value must be less than the maximum value. */
   /*====================================================*/

   if (range)
     {
      if (CompareNumbers(theEnv,constraints->minValue->type,
                         constraints->minValue->value,
                         constraints->maxValue->type,
                         constraints->maxValue->value) == GREATER_THAN)
        {
         PrintErrorID(theEnv,"CSTRNPSR",2,true);
         EnvPrintRouter(theEnv,WERROR,"Minimum range value must be less than\n");
         EnvPrintRouter(theEnv,WERROR,"or equal to the maximum range value\n");
         return false;
        }
     }
   else
     {
      if (CompareNumbers(theEnv,constraints->minFields->type,
                         constraints->minFields->value,
                         constraints->maxFields->type,
                         constraints->maxFields->value) == GREATER_THAN)
        {
         PrintErrorID(theEnv,"CSTRNPSR",2,true);
         EnvPrintRouter(theEnv,WERROR,"Minimum cardinality value must be less than\n");
         EnvPrintRouter(theEnv,WERROR,"or equal to the maximum cardinality value\n");
         return false;
        }
     }

   /*====================================*/
   /* Return true to indicate that the   */
   /* attribute was successfully parsed. */
   /*====================================*/

   return true;
  }

/******************************************************************/
/* GetConstraintTypeFromAllowedName: Returns the type restriction */
/*   associated with an allowed-... attribute.                    */
/******************************************************************/
static int GetConstraintTypeFromAllowedName(
  const char *constraintName)
  {
   if (strcmp(constraintName,"allowed-values") == 0) return(UNKNOWN_VALUE);
   else if (strcmp(constraintName,"allowed-symbols") == 0) return(SYMBOL);
   else if (strcmp(constraintName,"allowed-strings") == 0) return(STRING);
   else if (strcmp(constraintName,"allowed-lexemes") == 0) return(SYMBOL_OR_STRING);
   else if (strcmp(constraintName,"allowed-integers") == 0) return(INTEGER);
   else if (strcmp(constraintName,"allowed-numbers") == 0) return(INTEGER_OR_FLOAT);
   else if (strcmp(constraintName,"allowed-instance-names") == 0) return(INSTANCE_NAME);
   else if (strcmp(constraintName,"allowed-classes") == 0) return(INSTANCE_OR_INSTANCE_NAME);
   else if (strcmp(constraintName,"allowed-floats") == 0) return(FLOAT);

   return(-1);
  }

/*******************************************************/
/* GetConstraintTypeFromTypeName: Converts a type name */
/*   to its equivalent integer type restriction.       */
/*******************************************************/
static int GetConstraintTypeFromTypeName(
  const char *constraintName)
  {
   if (strcmp(constraintName,"SYMBOL") == 0) return(SYMBOL);
   else if (strcmp(constraintName,"STRING") == 0) return(STRING);
   else if (strcmp(constraintName,"LEXEME") == 0) return(SYMBOL_OR_STRING);
   else if (strcmp(constraintName,"INTEGER") == 0) return(INTEGER);
   else if (strcmp(constraintName,"FLOAT") == 0) return(FLOAT);
   else if (strcmp(constraintName,"NUMBER") == 0) return(INTEGER_OR_FLOAT);
   else if (strcmp(constraintName,"INSTANCE-NAME") == 0) return(INSTANCE_NAME);
   else if (strcmp(constraintName,"INSTANCE-ADDRESS") == 0) return(INSTANCE_ADDRESS);
   else if (strcmp(constraintName,"INSTANCE") == 0) return(INSTANCE_OR_INSTANCE_NAME);
   else if (strcmp(constraintName,"EXTERNAL-ADDRESS") == 0) return(EXTERNAL_ADDRESS);
   else if (strcmp(constraintName,"FACT-ADDRESS") == 0) return(FACT_ADDRESS);

   return(-1);
  }

/**************************************************************/
/* GetAttributeParseValue: Returns a boolean value indicating */
/*   whether a specific attribute has already been parsed.    */
/**************************************************************/
static bool GetAttributeParseValue(
  const char *constraintName,
  CONSTRAINT_PARSE_RECORD *parsedConstraints)
  {
   if (strcmp(constraintName,"type") == 0)
     { return(parsedConstraints->type); }
   else if (strcmp(constraintName,"range") == 0)
     { return(parsedConstraints->range); }
   else if (strcmp(constraintName,"cardinality") == 0)
     { return(parsedConstraints->cardinality); }
   else if (strcmp(constraintName,"allowed-values") == 0)
     { return(parsedConstraints->allowedValues); }
   else if (strcmp(constraintName,"allowed-symbols") == 0)
     { return(parsedConstraints->allowedSymbols); }
   else if (strcmp(constraintName,"allowed-strings") == 0)
     { return(parsedConstraints->allowedStrings); }
   else if (strcmp(constraintName,"allowed-lexemes") == 0)
     { return(parsedConstraints->allowedLexemes); }
   else if (strcmp(constraintName,"allowed-instance-names") == 0)
     { return(parsedConstraints->allowedInstanceNames); }
   else if (strcmp(constraintName,"allowed-classes") == 0)
     { return(parsedConstraints->allowedClasses); }
   else if (strcmp(constraintName,"allowed-integers") == 0)
     { return(parsedConstraints->allowedIntegers); }
   else if (strcmp(constraintName,"allowed-floats") == 0)
     { return(parsedConstraints->allowedFloats); }
   else if (strcmp(constraintName,"allowed-numbers") == 0)
     { return(parsedConstraints->allowedNumbers); }

   return true;
  }

/**********************************************************/
/* SetRestrictionFlag: Sets the restriction flag of a     */
/*   constraint record indicating whether a specific      */
/*   type has an associated allowed-... restriction list. */
/**********************************************************/
static void SetRestrictionFlag(
  int restriction,
  CONSTRAINT_RECORD *constraints,
  bool value)
  {
   switch (restriction)
     {
      case UNKNOWN_VALUE:
         constraints->anyRestriction = value;
         break;

      case SYMBOL:
         constraints->symbolRestriction = value;
         break;

      case STRING:
         constraints->stringRestriction = value;
         break;

      case INTEGER:
         constraints->integerRestriction = value;
         break;

      case FLOAT:
         constraints->floatRestriction = value;
         break;

      case INTEGER_OR_FLOAT:
         constraints->integerRestriction = value;
         constraints->floatRestriction = value;
         break;

      case SYMBOL_OR_STRING:
         constraints->symbolRestriction = value;
         constraints->stringRestriction = value;
         break;

      case INSTANCE_NAME:
         constraints->instanceNameRestriction = value;
         break;

      case INSTANCE_OR_INSTANCE_NAME:
         constraints->classRestriction = value;
         break;
     }
  }

/********************************************************************/
/* SetParseFlag: Sets the flag in a parsed constraints data         */
/*  structure indicating that a specific attribute has been parsed. */
/********************************************************************/
static void SetParseFlag(
  CONSTRAINT_PARSE_RECORD *parsedConstraints,
  const char *constraintName)
  {
   if (strcmp(constraintName,"range") == 0)
     { parsedConstraints->range = true; }
   else if (strcmp(constraintName,"type") == 0)
     { parsedConstraints->type = true; }
   else if (strcmp(constraintName,"cardinality") == 0)
     { parsedConstraints->cardinality = true; }
   else if (strcmp(constraintName,"allowed-symbols") == 0)
     { parsedConstraints->allowedSymbols = true; }
   else if (strcmp(constraintName,"allowed-strings") == 0)
     { parsedConstraints->allowedStrings = true; }
   else if (strcmp(constraintName,"allowed-lexemes") == 0)
     { parsedConstraints->allowedLexemes = true; }
   else if (strcmp(constraintName,"allowed-integers") == 0)
     { parsedConstraints->allowedIntegers = true; }
   else if (strcmp(constraintName,"allowed-floats") == 0)
     { parsedConstraints->allowedFloats = true; }
   else if (strcmp(constraintName,"allowed-numbers") == 0)
     { parsedConstraints->allowedNumbers = true; }
   else if (strcmp(constraintName,"allowed-values") == 0)
     { parsedConstraints->allowedValues = true; }
   else if (strcmp(constraintName,"allowed-classes") == 0)
     { parsedConstraints->allowedClasses = true; }
  }

#endif /* (! RUN_TIME) && (! BLOAD_ONLY) */

