   /*******************************************************/
   /*      "C" Language Integrated Production System      */
   /*                                                     */
   /*             CLIPS Version 6.50  07/30/16            */
   /*                                                     */
   /*                PATTERN HEADER FILE                  */
   /*******************************************************/

/*************************************************************/
/* Purpose: Provides the mechanism for recognizing and       */
/*   parsing the various types of patterns that can be used  */
/*   in the LHS of a rule. In version 6.0, the only pattern  */
/*   types provided are for deftemplate and instance         */
/*   patterns.                                               */
/*                                                           */
/* Principal Programmer(s):                                  */
/*      Gary D. Riley                                        */
/*                                                           */
/* Contributing Programmer(s):                               */
/*                                                           */
/* Revision History:                                         */
/*                                                           */
/*      6.24: Removed LOGICAL_DEPENDENCIES compilation flag. */
/*                                                           */
/*            Renamed BOOLEAN macro type to intBool.         */
/*                                                           */
/*      6.30: Added support for hashed alpha memories.       */
/*                                                           */
/*            Added const qualifiers to remove C++           */
/*            deprecation warnings.                          */
/*                                                           */
/*      6.40: Removed LOCALE definition.                     */
/*                                                           */
/*            Pragma once and other inclusion changes.       */
/*                                                           */
/*            Added support for booleans with <stdbool.h>.   */
/*                                                           */
/*            Removed use of void pointers for specific      */
/*            data structures.                               */
/*                                                           */
/*      6.50: Removed initial-fact and initial-object        */
/*            support.                                       */
/*                                                           */
/*************************************************************/

#ifndef _H_pattern

#pragma once

#define _H_pattern

#include <stdio.h>

#include "evaluatn.h"

struct patternEntityRecord
  {
   struct entityRecord base;
   void (*decrementBasisCount)(void *,void *);
   void (*incrementBasisCount)(void *,void *);
   void (*matchFunction)(void *,void *);
   bool (*synchronized)(void *,void *);
   bool (*isDeleted)(void *,void *);
  };

typedef struct patternEntityRecord PTRN_ENTITY_RECORD;
typedef struct patternEntityRecord *PTRN_ENTITY_RECORD_PTR;

struct patternEntity
  {
   struct patternEntityRecord *theInfo;
   void *dependents;
   unsigned busyCount;
   unsigned long long timeTag;
  };

typedef struct patternEntity PATTERN_ENTITY;
typedef struct patternEntity * PATTERN_ENTITY_PTR;

struct patternParser;

#ifndef _H_symbol
#include "symbol.h"
#endif
#ifndef _H_scanner
#include "scanner.h"
#endif
#ifndef _H_expressn
#include "expressn.h"
#endif
#ifndef _H_match
#include "match.h"
#endif
#ifndef _H_reorder
#include "reorder.h"
#endif
#ifndef _H_constrnt
#include "constrnt.h"
#endif

#define MAXIMUM_NUMBER_OF_PATTERNS 128

struct patternParser
  {
   const char *name;
   struct patternEntityRecord *entityType;
   int positionInArray;
   bool (*recognizeFunction)(SYMBOL_HN *);
   struct lhsParseNode *(*parseFunction)(Environment *,const char *,struct token *);
   bool (*postAnalysisFunction)(Environment *,struct lhsParseNode *);
   struct patternNodeHeader *(*addPatternFunction)(Environment *,struct lhsParseNode *);
   void (*removePatternFunction)(Environment *,struct patternNodeHeader *);
   struct expr *(*genJNConstantFunction)(void *,struct lhsParseNode *,int);
   void (*replaceGetJNValueFunction)(Environment *,struct expr *,struct lhsParseNode *,int);
   struct expr *(*genGetJNValueFunction)(Environment *,struct lhsParseNode *,int);
   struct expr *(*genCompareJNValuesFunction)(Environment *,struct lhsParseNode *,struct lhsParseNode *,bool);
   struct expr *(*genPNConstantFunction)(Environment *,struct lhsParseNode *);
   void (*replaceGetPNValueFunction)(Environment *,struct expr *,struct lhsParseNode *);
   struct expr *(*genGetPNValueFunction)(Environment *,struct lhsParseNode *);
   struct expr *(*genComparePNValuesFunction)(Environment *,struct lhsParseNode *,struct lhsParseNode *);
   void (*returnUserDataFunction)(Environment *,void *);
   void *(*copyUserDataFunction)(Environment *,void *);
   void (*markIRPatternFunction)(Environment *,struct patternNodeHeader *,int);
   void (*incrementalResetFunction)(Environment *);
   void (*codeReferenceFunction)(Environment *,void *,FILE *,int,int);
   int priority;
   struct patternParser *next;
  };

struct reservedSymbol
  {
   const char *theSymbol;
   const char *reservedBy;
   struct reservedSymbol *next;
  };

#define MAX_POSITIONS 8

#define PATTERN_DATA 19

struct patternData
  { 
   struct patternParser *ListOfPatternParsers;
   struct patternParser *PatternParserArray[MAX_POSITIONS];
   int NextPosition;
   struct reservedSymbol *ListOfReservedPatternSymbols;
   bool WithinNotCE;
   int GlobalSalience;
   bool GlobalAutoFocus;
   struct expr *SalienceExpression;
   struct patternNodeHashEntry **PatternHashTable;
   unsigned long PatternHashTableSize;
  };

#define PatternData(theEnv) ((struct patternData *) GetEnvironmentData(theEnv,PATTERN_DATA))

   void                           InitializePatterns(Environment *);
   bool                           AddPatternParser(Environment *,struct patternParser *);
   struct patternParser          *FindPatternParser(Environment *,const char *);
   void                           DetachPattern(Environment *,int,struct patternNodeHeader *);
   void                           GetNextPatternEntity(Environment *,
                                                       struct patternParser **,
                                                       struct patternEntity **);
   struct patternParser          *GetPatternParser(Environment *,int);
   struct lhsParseNode           *RestrictionParse(Environment *,const char *,struct token *,bool,
                                                   struct symbolHashNode *,short,
                                                   struct constraintRecord *,short);
   bool                           PostPatternAnalysis(Environment *,struct lhsParseNode *);
   void                           PatternNodeHeaderToCode(Environment *,FILE *,struct patternNodeHeader *,int,int);
   void                           AddReservedPatternSymbol(Environment *,const char *,const char *);
   bool                           ReservedPatternSymbol(Environment *,const char *,const char *);
   void                           ReservedPatternSymbolErrorMsg(Environment *,const char *,const char *);
   void                           AddHashedPatternNode(Environment *,void *,void *,unsigned short,void *);
   bool                           RemoveHashedPatternNode(Environment *,void *,void *,unsigned short,void *);
   void                          *FindHashedPatternNode(Environment *,void *,unsigned short,void *);

#endif /* _H_pattern */









