   /*******************************************************/
   /*      "C" Language Integrated Production System      */
   /*                                                     */
   /*             CLIPS Version 6.40  08/25/16            */
   /*                                                     */
   /*                CONSTRAINT HEADER FILE               */
   /*******************************************************/

/*************************************************************/
/* Purpose: Provides functions for creating and removing     */
/*   constraint records, adding them to the contraint hash   */
/*   table, and enabling and disabling static and dynamic    */
/*   constraint checking.                                    */
/*                                                           */
/* Principal Programmer(s):                                  */
/*      Gary D. Riley                                        */
/*                                                           */
/* Contributing Programmer(s):                               */
/*                                                           */
/* Revision History:                                         */
/*                                                           */
/*      6.23: Correction for FalseSymbol/TrueSymbol. DR0859  */
/*                                                           */
/*      6.24: Added allowed-classes slot facet.              */
/*                                                           */
/*            Renamed BOOLEAN macro type to intBool.         */
/*                                                           */
/*      6.30: Removed conditional code for unsupported       */
/*            compilers/operating systems (IBM_MCW and       */
/*            MAC_MCW).                                      */
/*                                                           */
/*            Changed integer type/precision.                */
/*                                                           */
/*            Converted API macros to function calls.        */
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
/*            ALLOW_ENVIRONMENT_GLOBALS no longer supported. */
/*                                                           */
/*            Static constraint checking is always enabled.  */
/*                                                           */
/*            UDF redesign.                                  */
/*                                                           */
/*************************************************************/

#ifndef _H_constrnt

#pragma once

#define _H_constrnt

struct constraintRecord;
typedef struct constraintRecord CONSTRAINT_RECORD;

#include "evaluatn.h"

struct constraintRecord
  {
   unsigned int anyAllowed : 1;
   unsigned int symbolsAllowed : 1;
   unsigned int stringsAllowed : 1;
   unsigned int floatsAllowed : 1;
   unsigned int integersAllowed : 1;
   unsigned int instanceNamesAllowed : 1;
   unsigned int instanceAddressesAllowed : 1;
   unsigned int externalAddressesAllowed : 1;
   unsigned int factAddressesAllowed : 1;
   unsigned int voidAllowed : 1;
   unsigned int anyRestriction : 1;
   unsigned int symbolRestriction : 1;
   unsigned int stringRestriction : 1;
   unsigned int floatRestriction : 1;
   unsigned int integerRestriction : 1;
   unsigned int classRestriction : 1;
   unsigned int instanceNameRestriction : 1;
   unsigned int multifieldsAllowed : 1;
   unsigned int singlefieldsAllowed : 1;
   unsigned short bsaveIndex;
   struct expr *classList;
   struct expr *restrictionList;
   struct expr *minValue;
   struct expr *maxValue;
   struct expr *minFields;
   struct expr *maxFields;
   struct constraintRecord *multifield;
   struct constraintRecord *next;
   int bucket;
   int count;
  };

#define SIZE_CONSTRAINT_HASH  167

#define CONSTRAINT_DATA 43

struct constraintData
  { 
   struct constraintRecord **ConstraintHashtable;
   bool DynamicConstraintChecking;
#if (BLOAD || BLOAD_ONLY || BLOAD_AND_BSAVE) && (! RUN_TIME)
   struct constraintRecord *ConstraintArray;
   long int NumberOfConstraints;
#endif
  };

#define ConstraintData(theEnv) ((struct constraintData *) GetEnvironmentData(theEnv,CONSTRAINT_DATA))

   void                           InitializeConstraints(Environment *);
   void                           GDCCommand(Environment *,UDFContext *,CLIPSValue *);
   void                           SDCCommand(Environment *,UDFContext *,CLIPSValue *);
   bool                           EnvSetDynamicConstraintChecking(Environment *,bool);
   bool                           EnvGetDynamicConstraintChecking(Environment *);
#if (! BLOAD_ONLY) && (! RUN_TIME)
   unsigned long                  HashConstraint(struct constraintRecord *);
   struct constraintRecord       *AddConstraint(Environment *,struct constraintRecord *);
#endif
#if (! RUN_TIME)
   void                           RemoveConstraint(Environment *,struct constraintRecord *);
#endif

#endif




