   /*******************************************************/
   /*      "C" Language Integrated Production System      */
   /*                                                     */
   /*            CLIPS Version 6.50  08/25/16             */
   /*                                                     */
   /*             MULTIFIELD FUNCTIONS MODULE             */
   /*******************************************************/

/*************************************************************/
/* Purpose: Contains the code for several multifield         */
/*   functions including first$, rest$, subseq$, delete$,    */
/*   delete-member$, replace-member$, replace$, insert$,     */
/*   explode$, implode$, nth$, member$, subsetp and progn$.  */
/*                                                           */
/* Principal Programmer(s):                                  */
/*      Gary D. Riley                                        */
/*      Brian Dantes                                         */
/*      Barry Cameron                                        */
/*                                                           */
/* Contributing Programmer(s):                               */
/*                                                           */
/* Revision History:                                         */
/*                                                           */
/*      6.23: Correction for FalseSymbol/TrueSymbol. DR0859  */
/*                                                           */
/*            Changed name of variable exp to theExp         */
/*            because of Unix compiler warnings of shadowed  */
/*            definitions.                                   */
/*                                                           */
/*      6.24: Renamed BOOLEAN macro type to intBool.         */
/*                                                           */
/*            Moved ImplodeMultifield to multifld.c.         */
/*                                                           */
/*      6.30: Changed integer type/precision.                */
/*                                                           */
/*            Support for long long integers.                */
/*                                                           */
/*            Changed garbage collection algorithm.          */
/*                                                           */
/*            Fixed memory leaks when error occurred.        */
/*                                                           */
/*            Added const qualifiers to remove C++           */
/*            deprecation warnings.                          */
/*                                                           */
/*            Fixed linkage issue when DEFMODULE_CONSTRUCT   */
/*            compiler flag is set to 0.                     */
/*                                                           */
/*      6.40: Added Env prefix to GetEvaluationError and     */
/*            SetEvaluationError functions.                  */
/*                                                           */
/*            Added Env prefix to GetHaltExecution and       */
/*            SetHaltExecution functions.                    */
/*                                                           */
/*            Pragma once and other inclusion changes.       */
/*                                                           */
/*            Added support for booleans with <stdbool.h>.   */
/*                                                           */
/*            Removed use of void pointers for specific      */
/*            data structures.                               */
/*                                                           */
/*            Removed member, mv-replace, mv-subseq,         */
/*            mv-delete, str-implode, str-explode, subset,   */
/*            and nth functions.                             */
/*                                                           */
/*            UDF redesign.                                  */
/*                                                           */
/*      6.50: Fact ?var:slot references in progn$/foreach.   */
/*                                                           */
/*************************************************************/

#include "setup.h"

#if MULTIFIELD_FUNCTIONS || OBJECT_SYSTEM

#include <stdio.h>
#include <string.h>

#include "argacces.h"
#include "envrnmnt.h"
#include "exprnpsr.h"
#include "memalloc.h"
#include "multifld.h"
#include "multifun.h"
#if OBJECT_SYSTEM
#include "object.h"
#endif
#include "prcdrpsr.h"
#include "prcdrfun.h"
#include "router.h"
#if (! BLOAD_ONLY) && (! RUN_TIME)
#include "scanner.h"
#endif
#include "utility.h"

/**************/
/* STRUCTURES */
/**************/

typedef struct fieldVarStack
  {
   unsigned short type;
   void *value;
   long index;
   struct fieldVarStack *nxt;
  } FIELD_VAR_STACK;

/***************************************/
/* LOCAL INTERNAL FUNCTION DEFINITIONS */
/***************************************/

#if MULTIFIELD_FUNCTIONS
   static bool                    MVRangeCheck(long,long,long *,int);
   static void                    MultifieldPrognDriver(UDFContext *,CLIPSValue *,const char *);
#if (! BLOAD_ONLY) && (! RUN_TIME)
   static struct expr            *MultifieldPrognParser(Environment *,struct expr *,const char *);
   static struct expr            *ForeachParser(Environment *,struct expr *,const char *);
   static void                    ReplaceMvPrognFieldVars(Environment *,SYMBOL_HN *,struct expr *,int);
#endif /* (! BLOAD_ONLY) && (! RUN_TIME) */
#endif /* MULTIFIELD_FUNCTIONS */
   static void                    MVRangeError(Environment *,long,long,long,const char *);
#endif /* MULTIFIELD_FUNCTIONS || OBJECT_SYSTEM */

/***************************************/
/* LOCAL INTERNAL VARIABLE DEFINITIONS */
/***************************************/

#if MULTIFIELD_FUNCTIONS

#define MULTIFUN_DATA 10

struct multiFunctionData
  { 
   FIELD_VAR_STACK *FieldVarStack;
  };

#define MultiFunctionData(theEnv) ((struct multiFunctionData *) GetEnvironmentData(theEnv,MULTIFUN_DATA))

/**********************************************/
/* MultifieldFunctionDefinitions: Initializes */
/*   the multifield functions.                */
/**********************************************/
void MultifieldFunctionDefinitions(
  Environment *theEnv)
  {
   AllocateEnvironmentData(theEnv,MULTIFUN_DATA,sizeof(struct multiFunctionData),NULL);

#if ! RUN_TIME
   EnvAddUDF(theEnv,"first$","m",1,1,"m",FirstFunction,"FirstFunction",NULL);
   EnvAddUDF(theEnv,"rest$","m",1,1,"m",RestFunction,"RestFunction",NULL);
   EnvAddUDF(theEnv,"subseq$","m",3,3,"l;m",SubseqFunction,"SubseqFunction",NULL);
   EnvAddUDF(theEnv,"delete-member$","m",2,UNBOUNDED,"*;m",DeleteMemberFunction,"DeleteMemberFunction",NULL);
   EnvAddUDF(theEnv,"replace-member$","m",3,UNBOUNDED,"*;m",ReplaceMemberFunction,"ReplaceMemberFunction",NULL);
   EnvAddUDF(theEnv,"delete$","m",3,3,"l;m",DeleteFunction,"DeleteFunction",NULL);
   EnvAddUDF(theEnv,"replace$","m",4,UNBOUNDED,"*;m;l;l",ReplaceFunction,"ReplaceFunction",NULL);
   EnvAddUDF(theEnv,"insert$","m",3,UNBOUNDED,"*;m;l",InsertFunction,"InsertFunction",NULL);
   EnvAddUDF(theEnv,"explode$","m",1,1,"s",ExplodeFunction,"ExplodeFunction",NULL);
   EnvAddUDF(theEnv,"implode$","s",1,1,"m",ImplodeFunction,"ImplodeFunction",NULL);
   EnvAddUDF(theEnv,"nth$","synldife",2,2,";l;m",NthFunction,"NthFunction",NULL);
   EnvAddUDF(theEnv,"member$","blm",2,2,";*;m",MemberFunction,"MemberFunction",NULL);
   EnvAddUDF(theEnv,"subsetp","b",2,2,";m;m",SubsetpFunction,"SubsetpFunction",NULL);
   EnvAddUDF(theEnv,"progn$","*",0,UNBOUNDED,NULL,MultifieldPrognFunction,"MultifieldPrognFunction",NULL);
   EnvAddUDF(theEnv,"foreach","*",0,UNBOUNDED,NULL,ForeachFunction,"ForeachFunction",NULL);
#if ! BLOAD_ONLY
   AddFunctionParser(theEnv,"progn$",MultifieldPrognParser);
   AddFunctionParser(theEnv,"foreach",ForeachParser);
#endif
   FuncSeqOvlFlags(theEnv,"progn$",false,false);
   FuncSeqOvlFlags(theEnv,"foreach",false,false);
   EnvAddUDF(theEnv,"(get-progn$-field)","*",0,0,NULL,GetMvPrognField,"GetMvPrognField",NULL);
   EnvAddUDF(theEnv,"(get-progn$-index)","l",0,0,NULL,GetMvPrognIndex,"GetMvPrognIndex",NULL);
#endif
  }

/****************************************/
/* DeleteFunction: H/L access routine   */
/*   for the delete$ function.          */
/****************************************/
void DeleteFunction(
  Environment *theEnv,
  UDFContext *context,
  CLIPSValue *returnValue)
  {
   CLIPSValue value1, value2, value3;

   /*=======================================*/
   /* Check for the correct argument types. */
   /*=======================================*/

   if ((! UDFFirstArgument(context,MULTIFIELD_TYPE,&value1)) ||
       (! UDFNextArgument(context,INTEGER_TYPE,&value2)) ||
       (! UDFNextArgument(context,INTEGER_TYPE,&value3)))
     { return; }

   /*=================================================*/
   /* Delete the section out of the multifield value. */
   /*=================================================*/

   if (DeleteMultiValueField(theEnv,returnValue,&value1,
            mCVToInteger(&value2),mCVToInteger(&value3),"delete$") == false)/* TBD */
     {
      EnvSetEvaluationError(theEnv,true);
      EnvSetMultifieldErrorValue(theEnv,returnValue);
     }
  }

/*****************************************/
/* ReplaceFunction: H/L access routine   */
/*   for the replace$ function.          */
/*****************************************/
void ReplaceFunction(
  Environment *theEnv,
  UDFContext *context,
  CLIPSValue *returnValue)
  {
   CLIPSValue value1, value2, value3, value4;
   EXPRESSION *fieldarg;

   /*=======================================*/
   /* Check for the correct argument types. */
   /*=======================================*/

   if ((! UDFFirstArgument(context,MULTIFIELD_TYPE,&value1)) ||
       (! UDFNextArgument(context,INTEGER_TYPE,&value2)) ||
       (! UDFNextArgument(context,INTEGER_TYPE,&value3)))
     { return; }

   /*===============================*/
   /* Create the replacement value. */
   /*===============================*/

   fieldarg = GetFirstArgument()->nextArg->nextArg->nextArg;
   if (fieldarg->nextArg != NULL)
     { StoreInMultifield(theEnv,&value4,fieldarg,true); }
   else
     { EvaluateExpression(theEnv,fieldarg,&value4); }

   /*==============================================*/
   /* Replace the section in the multifield value. */
   /*==============================================*/

   if (ReplaceMultiValueField(theEnv,returnValue,&value1,(long) DOToLong(value2),
                   (long) DOToLong(value3),&value4,"replace$") == false) /* TBD */
     {
      EnvSetEvaluationError(theEnv,true);
      EnvSetMultifieldErrorValue(theEnv,returnValue);
     }
  }

/**********************************************/
/* DeleteMemberFunction: H/L access routine   */
/*   for the delete-member$ function.         */
/**********************************************/
void DeleteMemberFunction(
  Environment *theEnv,
  UDFContext *context,
  CLIPSValue *returnValue)
  {
   CLIPSValue resultValue, *delVals, tmpVal;
   int i, argCnt;
   unsigned delSize;
   long j,k;

   /*============================================*/
   /* Check for the correct number of arguments. */
   /*============================================*/

   argCnt = UDFArgumentCount(context);

   /*=======================================*/
   /* Check for the correct argument types. */
   /*=======================================*/
   
   if (! UDFFirstArgument(context,MULTIFIELD_TYPE,&resultValue))
     { return; }

   /*===================================================*/
   /* For every value specified, delete all occurrences */
   /* of those values from the multifield.              */
   /*===================================================*/

   delSize = (sizeof(CLIPSValue) * (argCnt-1));
   delVals = (CLIPSValue *) gm2(theEnv,delSize);
   for (i = 2 ; i <= argCnt ; i++)
     {
      if (! UDFNextArgument(context,ANY_TYPE,&delVals[i-2]))
        {
         rm(theEnv,delVals,delSize);
         return;
        }
     }

   while (FindDOsInSegment(delVals,argCnt-1,&resultValue,&j,&k,NULL,0))
     {
      if (DeleteMultiValueField(theEnv,&tmpVal,&resultValue,
                                j,k,"delete-member$") == false)
        {
         rm(theEnv,delVals,delSize);
         EnvSetEvaluationError(theEnv,true);
         EnvSetMultifieldErrorValue(theEnv,returnValue);
         return;
        }
      GenCopyMemory(CLIPSValue,1,&resultValue,&tmpVal);
     }
   rm(theEnv,delVals,delSize);
   GenCopyMemory(CLIPSValue,1,returnValue,&resultValue);
  }

/***********************************************/
/* ReplaceMemberFunction: H/L access routine   */
/*   for the replace-member$ function.         */
/***********************************************/
void ReplaceMemberFunction(
  Environment *theEnv,
  UDFContext *context,
  CLIPSValue *returnValue)
  {
   CLIPSValue resultValue,replVal,*delVals,tmpVal;
   int i,argCnt;
   unsigned delSize;
   long j,k,mink[2],*minkp;
   long replLen = 1L;

   /*============================================*/
   /* Check for the correct number of arguments. */
   /*============================================*/

   argCnt = UDFArgumentCount(context);

   /*=======================================*/
   /* Check for the correct argument types. */
   /*=======================================*/
   
   if (! UDFFirstArgument(context,MULTIFIELD_TYPE,&resultValue))
     { return; }

   if (! UDFNextArgument(context,ANY_TYPE,&replVal))
     { return; }
   if (GetType(replVal) == MULTIFIELD)
     replLen = GetDOLength(replVal);

   /*======================================================*/
   /* For the value (or values from multifield) specified, */
   /* replace all occurrences of those values with all     */
   /* values specified.                                    */
   /*======================================================*/
   
   delSize = (sizeof(CLIPSValue) * (argCnt-2));
   delVals = (CLIPSValue *) gm2(theEnv,delSize);
   
   for (i = 3 ; i <= argCnt ; i++)
     {
      if (! UDFNthArgument(context,i,ANY_TYPE,&delVals[i-3]))
        {
         rm(theEnv,delVals,delSize);
         return;
        }
     }
   minkp = NULL;
   while (FindDOsInSegment(delVals,argCnt-2,&resultValue,&j,&k,minkp,minkp ? 1 : 0))
     {
      if (ReplaceMultiValueField(theEnv,&tmpVal,&resultValue,j,k,
                                 &replVal,"replace-member$") == false)
        {
         rm(theEnv,delVals,delSize);
         EnvSetEvaluationError(theEnv,true);
         EnvSetMultifieldErrorValue(theEnv,returnValue);
         return;
        }
      GenCopyMemory(CLIPSValue,1,&resultValue,&tmpVal);
      mink[0] = 1L;
      mink[1] = j + replLen - 1L;
      minkp = mink;
     }
   rm(theEnv,delVals,delSize);
   GenCopyMemory(CLIPSValue,1,returnValue,&resultValue);
  }

/****************************************/
/* InsertFunction: H/L access routine   */
/*   for the insert$ function.          */
/****************************************/
void InsertFunction(
  Environment *theEnv,
  UDFContext *context,
  CLIPSValue *returnValue)
  {
   CLIPSValue value1, value2, value3;
   EXPRESSION *fieldarg;

   /*=======================================*/
   /* Check for the correct argument types. */
   /*=======================================*/

   if ((! UDFFirstArgument(context,MULTIFIELD_TYPE,&value1)) ||
       (! UDFNextArgument(context,INTEGER_TYPE,&value2)))
     { return; }

   /*=============================*/
   /* Create the insertion value. */
   /*=============================*/

   fieldarg = GetFirstArgument()->nextArg->nextArg;
   if (fieldarg->nextArg != NULL)
     StoreInMultifield(theEnv,&value3,fieldarg,true);
   else
     EvaluateExpression(theEnv,fieldarg,&value3);

   /*===========================================*/
   /* Insert the value in the multifield value. */
   /*===========================================*/

   if (InsertMultiValueField(theEnv,returnValue,&value1,(long) DOToLong(value2), /* TBD */
                             &value3,"insert$") == false)
     {
      EnvSetEvaluationError(theEnv,true);
      EnvSetMultifieldErrorValue(theEnv,returnValue);
     }
  }

/*****************************************/
/* ExplodeFunction: H/L access routine   */
/*   for the explode$ function.          */
/*****************************************/
void ExplodeFunction(
  Environment *theEnv,
  UDFContext *context,
  CLIPSValue *returnValue)
  {
   CLIPSValue value;
   Multifield *theMultifield;
   unsigned long end;

   /*==================================*/
   /* The argument should be a string. */
   /*==================================*/

   if (! UDFFirstArgument(context,STRING_TYPE,&value))
     { return; }

   /*=====================================*/
   /* Convert the string to a multifield. */
   /*=====================================*/

   theMultifield = StringToMultifield(theEnv,DOToString(value));
   if (theMultifield == NULL)
     {
      theMultifield = EnvCreateMultifield(theEnv,0L);
      end = 0;
     }
   else
     { end = GetMFLength(theMultifield); }

   /*========================*/
   /* Return the multifield. */
   /*========================*/

   SetpType(returnValue,MULTIFIELD);
   SetpDOBegin(returnValue,1);
   SetpDOEnd(returnValue,end);
   SetpValue(returnValue,theMultifield);
   return;
  }

/*****************************************/
/* ImplodeFunction: H/L access routine   */
/*   for the implode$ function.          */
/*****************************************/
void ImplodeFunction(
  Environment *theEnv,
  UDFContext *context,
  CLIPSValue *returnValue)
  {
   CLIPSValue theArg;

   /*======================================*/
   /* The argument should be a multifield. */
   /*======================================*/

   if (! UDFFirstArgument(context,MULTIFIELD_TYPE,&theArg))
     { return; }

   /*====================*/
   /* Return the string. */
   /*====================*/

   CVSetCLIPSString(returnValue,ImplodeMultifield(theEnv,&theArg));
  }

/****************************************/
/* SubseqFunction: H/L access routine   */
/*   for the subseq$ function.          */
/****************************************/
void SubseqFunction(
  Environment *theEnv,
  UDFContext *context,
  CLIPSValue *returnValue)
  {
   CLIPSValue theArg;
   struct multifield *theList;
   long long offset, start, end, length; /* 6.04 Bug Fix */

   /*===================================*/
   /* Get the segment to be subdivided. */
   /*===================================*/

   if (! UDFFirstArgument(context,MULTIFIELD_TYPE,&theArg))
     { return; }

   theList = (struct multifield *) DOToPointer(theArg);
   offset = GetDOBegin(theArg);
   length = GetDOLength(theArg);

   /*=============================================*/
   /* Get range arguments. If they are not within */
   /* appropriate ranges, return a null segment.  */
   /*=============================================*/

   if (! UDFNextArgument(context,INTEGER_TYPE,&theArg))
     { return; }

   start = DOToLong(theArg);

   if (! UDFNextArgument(context,INTEGER_TYPE,&theArg))
     { return; }
   end = DOToLong(theArg);

   if ((end < 1) || (end < start))
     {
      EnvSetMultifieldErrorValue(theEnv,returnValue);
      return;
     }

   /*===================================================*/
   /* Adjust lengths  to conform to segment boundaries. */
   /*===================================================*/

   if (start > length)
     {
      EnvSetMultifieldErrorValue(theEnv,returnValue);
      return;
     }
   if (end > length) end = length;
   if (start < 1) start = 1;

   /*=========================*/
   /* Return the new segment. */
   /*=========================*/

   SetpType(returnValue,MULTIFIELD);
   SetpValue(returnValue,theList);
   SetpDOEnd(returnValue,offset + end - 1);
   SetpDOBegin(returnValue,offset + start - 1);
  }

/***************************************/
/* FirstFunction: H/L access routine   */
/*   for the first$ function.          */
/***************************************/
void FirstFunction(
  Environment *theEnv,
  UDFContext *context,
  CLIPSValue *returnValue)
  {
   CLIPSValue theArg;
   Multifield *theList;

   /*===================================*/
   /* Get the segment to be subdivided. */
   /*===================================*/

   if (! UDFFirstArgument(context,MULTIFIELD_TYPE,&theArg))
     {
      EnvSetMultifieldErrorValue(theEnv,returnValue);
      return;
     }

   theList = (Multifield *) DOToPointer(theArg);

   /*=========================*/
   /* Return the new segment. */
   /*=========================*/

   SetpType(returnValue,MULTIFIELD);
   SetpValue(returnValue,theList);
   if (GetDOEnd(theArg) >= GetDOBegin(theArg))
     { SetpDOEnd(returnValue,GetDOBegin(theArg)); }
   else
     { SetpDOEnd(returnValue,GetDOEnd(theArg)); }
   SetpDOBegin(returnValue,GetDOBegin(theArg));
  }

/**************************************/
/* RestFunction: H/L access routine   */
/*   for the rest$ function.          */
/**************************************/
void RestFunction(
  Environment *theEnv,
  UDFContext *context,
  CLIPSValue *returnValue)
  {
   CLIPSValue theArg;
   Multifield *theList;

   /*===================================*/
   /* Get the segment to be subdivided. */
   /*===================================*/

   if (! UDFFirstArgument(context,MULTIFIELD_TYPE,&theArg))
     {
      EnvSetMultifieldErrorValue(theEnv,returnValue);
      return;
     }

   theList = (Multifield *) DOToPointer(theArg);

   /*=========================*/
   /* Return the new segment. */
   /*=========================*/

   SetpType(returnValue,MULTIFIELD);
   SetpValue(returnValue,theList);
   if (GetDOBegin(theArg) > GetDOEnd(theArg))
     { SetpDOBegin(returnValue,GetDOBegin(theArg)); }
   else
     { SetpDOBegin(returnValue,GetDOBegin(theArg) + 1); }
   SetpDOEnd(returnValue,GetDOEnd(theArg));
  }

/*************************************/
/* NthFunction: H/L access routine   */
/*   for the nth$ function.          */
/*************************************/
void NthFunction(
  Environment *theEnv,
  UDFContext *context,
  CLIPSValue *returnValue)
  {
   CLIPSValue value1, value2;
   struct multifield *elm_ptr;
   long long n; /* 6.04 Bug Fix */

   if ((! UDFFirstArgument(context,INTEGER_TYPE,&value1)) ||
	   (! UDFNextArgument(context,MULTIFIELD_TYPE,&value2)))
     { return; }

   n = DOToLong(value1); /* 6.04 Bug Fix */
   if ((n > GetDOLength(value2)) || (n < 1))
	 {
      mCVSetSymbol(returnValue,"nil");
	  return;
	 }

   elm_ptr = (struct multifield *) GetValue(value2);
   SetpType(returnValue,GetMFType(elm_ptr,((long) n) + GetDOBegin(value2) - 1));
   SetpValue(returnValue,GetMFValue(elm_ptr,((long) n) + GetDOBegin(value2) - 1));
  }

/* ------------------------------------------------------------------
 *    SubsetFunction:
 *               This function compares two multi-field variables
 *               to see if the first is a subset of the second. It
 *               does not consider order.
 *
 *    INPUTS:    Two arguments via argument stack. First is the sublist
 *               multi-field variable, the second is the list to be
 *               compared to. Both should be of type MULTIFIELD.
 *
 *    OUTPUTS:   True if the first list is a subset of the
 *               second, else false
 *
 *    NOTES:     This function is called from H/L with the subset
 *               command. Repeated values in the sublist must also
 *               be repeated in the main list.
 * ------------------------------------------------------------------
 */

void SubsetpFunction(
  Environment *theEnv,
  UDFContext *context,
  CLIPSValue *returnValue)
  {
   CLIPSValue item1, item2, tmpItem;
   long i,j,k; 

   if (! UDFFirstArgument(context,MULTIFIELD_TYPE,&item1))
     { return; }

   if (! UDFNextArgument(context,MULTIFIELD_TYPE,&item2))
     { return; }
 
   if (mMFLength(&item1) == 0)
     {
      mCVSetBoolean(returnValue,true);
      return;
     }
     
   if (mMFLength(&item2) == 0)
     {
      mCVSetBoolean(returnValue,false);
      return;
     }

   for (i = GetDOBegin(item1) ; i <= GetDOEnd(item1) ; i++)
     {
      SetType(tmpItem,GetMFType((struct multifield *) GetValue(item1),i));
      SetValue(tmpItem,GetMFValue((struct multifield *) GetValue(item1),i));

      if (! FindDOsInSegment(&tmpItem,1,&item2,&j,&k,NULL,0))
        {
         mCVSetBoolean(returnValue,false);
         return;
        }
     }

   mCVSetBoolean(returnValue,true);
  }

/****************************************/
/* MemberFunction: H/L access routine   */
/*   for the member$ function.          */
/****************************************/
void MemberFunction(
  Environment *theEnv,
  UDFContext *context,
  CLIPSValue *returnValue)
  {
   CLIPSValue item1, item2;
   long j, k;

   mCVSetBoolean(returnValue,false);

   if (! UDFFirstArgument(context,ANY_TYPE,&item1)) return;
   
   if (! UDFNextArgument(context,MULTIFIELD_TYPE,&item2)) return;

   if (FindDOsInSegment(&item1,1,&item2,&j,&k,NULL,0))
     {
      if (j == k)
        {
         mCVSetInteger(returnValue,j);
        }
      else
        {
         returnValue->type = MULTIFIELD;
         returnValue->value = EnvCreateMultifield(theEnv,2);
         SetMFType(returnValue->value,1,INTEGER);
         SetMFValue(returnValue->value,1,EnvAddLong(theEnv,j));
         SetMFType(returnValue->value,2,INTEGER);
         SetMFValue(returnValue->value,2,EnvAddLong(theEnv,k));
         SetpDOBegin(returnValue,1);
         SetpDOEnd(returnValue,2);
        }
     }
  }

/*********************/
/* FindDOsInSegment: */
/*********************/
/* 6.05 Bug Fix */
bool FindDOsInSegment(
  CLIPSValue *searchDOs,
  int scnt,
  CLIPSValue *value,
  long *si,
  long *ei,
  long *excludes,
  int epaircnt)
  {
   long mul_length,slen,i,k; /* 6.04 Bug Fix */
   int j;

   mul_length = GetpDOLength(value);
   for (i = 0 ; i < mul_length ; i++)
     {
      for (j = 0 ; j < scnt ; j++)
        {
         if (GetType(searchDOs[j]) == MULTIFIELD)
           {
            slen = GetDOLength(searchDOs[j]);
            if (MVRangeCheck(i+1L,i+slen,excludes,epaircnt))
              {
               for (k = 0L ; (k < slen) && ((k + i) < mul_length) ; k++)
                 if ((GetMFType(GetValue(searchDOs[j]),k+GetDOBegin(searchDOs[j])) !=
                      GetMFType(GetpValue(value),k+i+GetpDOBegin(value))) ||
                     (GetMFValue(GetValue(searchDOs[j]),k+GetDOBegin(searchDOs[j])) !=
                      GetMFValue(GetpValue(value),k+i+GetpDOBegin(value))))
                   break;
               if (k >= slen)
                 {
                  *si = i + 1L;
                  *ei = i + slen;
                  return true;
                 }
              }
           }
         else if ((GetValue(searchDOs[j]) == GetMFValue(GetpValue(value),i + GetpDOBegin(value))) &&
                  (GetType(searchDOs[j]) == GetMFType(GetpValue(value),i + GetpDOBegin(value))) &&
                  MVRangeCheck(i+1L,i+1L,excludes,epaircnt))
           {
            *si = *ei = i+1L;
            return true;
           }
        }
     }

   return false;
  }

/*****************/
/* MVRangeCheck: */
/*****************/
static bool MVRangeCheck(
  long si,
  long ei,
  long *elist,
  int epaircnt)
{
  int i;

  if (!elist || !epaircnt)
    return true;
  for (i = 0 ; i < epaircnt ; i++)
    if (((si >= elist[i*2]) && (si <= elist[i*2+1])) ||
        ((ei >= elist[i*2]) && (ei <= elist[i*2+1])))
    return false;

  return true;
}

#if (! BLOAD_ONLY) && (! RUN_TIME)

/******************************************************/
/* MultifieldPrognParser: Parses the progn$ function. */
/******************************************************/
static struct expr *MultifieldPrognParser(
  Environment *theEnv,
  struct expr *top,
  const char *infile)
  {
   struct BindInfo *oldBindList,*newBindList,*prev;
   struct token tkn;
   struct expr *tmp;
   SYMBOL_HN *fieldVar = NULL;

   SavePPBuffer(theEnv," ");
   GetToken(theEnv,infile,&tkn);

   /* ================================
      Simple form: progn$ <mf-exp> ...
      ================================ */
   if (tkn.type != LPAREN)
     {
      top->argList = ParseAtomOrExpression(theEnv,infile,&tkn);
      if (top->argList == NULL)
        {
         ReturnExpression(theEnv,top);
         return NULL;
        }
     }
   else
     {
      GetToken(theEnv,infile,&tkn);
      if (tkn.type != SF_VARIABLE)
        {
         if (tkn.type != SYMBOL)
           goto MvPrognParseError;
         top->argList = Function2Parse(theEnv,infile,ValueToString(tkn.value));
         if (top->argList == NULL)
           {
            ReturnExpression(theEnv,top);
            return NULL;
           }
        }

      /* =========================================
         Complex form: progn$ (<var> <mf-exp>) ...
         ========================================= */
      else
        {
         fieldVar = (SYMBOL_HN *) tkn.value;
         SavePPBuffer(theEnv," ");
         top->argList = ParseAtomOrExpression(theEnv,infile,NULL);
         if (top->argList == NULL)
           {
            ReturnExpression(theEnv,top);
            return NULL;
           }
         GetToken(theEnv,infile,&tkn);
         if (tkn.type != RPAREN)
           goto MvPrognParseError;
         PPBackup(theEnv);
         /* PPBackup(theEnv); */
         SavePPBuffer(theEnv,tkn.printForm);
         SavePPBuffer(theEnv," ");
        }
     }

   if (CheckArgumentAgainstRestriction(theEnv,top->argList,MULTIFIELD_TYPE))
     goto MvPrognParseError;
     
   oldBindList = GetParsedBindNames(theEnv);
   SetParsedBindNames(theEnv,NULL);
   IncrementIndentDepth(theEnv,3);
   ExpressionData(theEnv)->BreakContext = true;
   ExpressionData(theEnv)->ReturnContext = ExpressionData(theEnv)->svContexts->rtn;
   PPCRAndIndent(theEnv);
   top->argList->nextArg = GroupActions(theEnv,infile,&tkn,true,NULL,false);
   DecrementIndentDepth(theEnv,3);
   PPBackup(theEnv);
   PPBackup(theEnv);
   SavePPBuffer(theEnv,tkn.printForm);
   if (top->argList->nextArg == NULL)
     {
      ClearParsedBindNames(theEnv);
      SetParsedBindNames(theEnv,oldBindList);
      ReturnExpression(theEnv,top);
      return NULL;
     }
   tmp = top->argList->nextArg;
   top->argList->nextArg = tmp->argList;
   tmp->argList = NULL;
   ReturnExpression(theEnv,tmp);
   newBindList = GetParsedBindNames(theEnv);
   prev = NULL;
   while (newBindList != NULL)
     {
      if ((fieldVar == NULL) ? false :
          (strcmp(ValueToString(newBindList->name),ValueToString(fieldVar)) == 0))
        {
         ClearParsedBindNames(theEnv);
         SetParsedBindNames(theEnv,oldBindList);
         PrintErrorID(theEnv,"MULTIFUN",2,false);
         EnvPrintRouter(theEnv,WERROR,"Cannot rebind field variable in function progn$.\n");
         ReturnExpression(theEnv,top);
         return NULL;
        }
      prev = newBindList;
      newBindList = newBindList->next;
     }
   if (prev == NULL)
     SetParsedBindNames(theEnv,oldBindList);
   else
     prev->next = oldBindList;
   if (fieldVar != NULL)
     ReplaceMvPrognFieldVars(theEnv,fieldVar,top->argList->nextArg,0);
   return(top);

MvPrognParseError:
   SyntaxErrorMessage(theEnv,"progn$");
   ReturnExpression(theEnv,top);
   return NULL;
  }

/***********************************************/
/* ForeachParser: Parses the foreach function. */
/***********************************************/
static struct expr *ForeachParser(
  Environment *theEnv,
  struct expr *top,
  const char *infile)
  {
   struct BindInfo *oldBindList,*newBindList,*prev;
   struct token tkn;
   struct expr *tmp;
   SYMBOL_HN *fieldVar;

   SavePPBuffer(theEnv," ");
   GetToken(theEnv,infile,&tkn);

   if (tkn.type != SF_VARIABLE)
     { goto ForeachParseError; }

   fieldVar = (SYMBOL_HN *) tkn.value;
   SavePPBuffer(theEnv," ");
   top->argList = ParseAtomOrExpression(theEnv,infile,NULL);
   if (top->argList == NULL)
     {
      ReturnExpression(theEnv,top);
      return NULL;
     }

   if (CheckArgumentAgainstRestriction(theEnv,top->argList,MULTIFIELD_TYPE))
     goto ForeachParseError;
     
   oldBindList = GetParsedBindNames(theEnv);
   SetParsedBindNames(theEnv,NULL);
   IncrementIndentDepth(theEnv,3);
   ExpressionData(theEnv)->BreakContext = true;
   ExpressionData(theEnv)->ReturnContext = ExpressionData(theEnv)->svContexts->rtn;
   PPCRAndIndent(theEnv);
   top->argList->nextArg = GroupActions(theEnv,infile,&tkn,true,NULL,false);
   DecrementIndentDepth(theEnv,3);
   PPBackup(theEnv);
   PPBackup(theEnv);
   SavePPBuffer(theEnv,tkn.printForm);
   if (top->argList->nextArg == NULL)
     {
      ClearParsedBindNames(theEnv);
      SetParsedBindNames(theEnv,oldBindList);
      ReturnExpression(theEnv,top);
      return NULL;
     }
   tmp = top->argList->nextArg;
   top->argList->nextArg = tmp->argList;
   tmp->argList = NULL;
   ReturnExpression(theEnv,tmp);
   newBindList = GetParsedBindNames(theEnv);
   prev = NULL;
   while (newBindList != NULL)
     {
      if ((fieldVar == NULL) ? false :
          (strcmp(ValueToString(newBindList->name),ValueToString(fieldVar)) == 0))
        {
         ClearParsedBindNames(theEnv);
         SetParsedBindNames(theEnv,oldBindList);
         PrintErrorID(theEnv,"MULTIFUN",2,false);
         EnvPrintRouter(theEnv,WERROR,"Cannot rebind field variable in function foreach.\n");
         ReturnExpression(theEnv,top);
         return NULL;
        }
      prev = newBindList;
      newBindList = newBindList->next;
     }
   if (prev == NULL)
     SetParsedBindNames(theEnv,oldBindList);
   else
     prev->next = oldBindList;
   if (fieldVar != NULL)
     ReplaceMvPrognFieldVars(theEnv,fieldVar,top->argList->nextArg,0);
   return(top);

ForeachParseError:
   SyntaxErrorMessage(theEnv,"foreach");
   ReturnExpression(theEnv,top);
   return NULL;
  }
  
/**********************************************/
/* ReplaceMvPrognFieldVars: Replaces variable */
/*   references found in the progn$ function. */
/**********************************************/
static void ReplaceMvPrognFieldVars(
  Environment *theEnv,
  SYMBOL_HN *fieldVar,
  struct expr *theExp,
  int depth)
  {
   size_t flen;

   flen = strlen(ValueToString(fieldVar));
   while (theExp != NULL)
     {
      if ((theExp->type != SF_VARIABLE) ? false :
          (strncmp(ValueToString(theExp->value),ValueToString(fieldVar),
                   (STD_SIZE) flen) == 0))
        {
         if (ValueToString(theExp->value)[flen] == '\0')
           {
            theExp->type = FCALL;
            theExp->value = FindFunction(theEnv,"(get-progn$-field)");
            theExp->argList = GenConstant(theEnv,INTEGER,EnvAddLong(theEnv,(long long) depth));
           }
#if DEFTEMPLATE_CONSTRUCT
         else if (ValueToString(theExp->value)[flen] == ':')
           {
            size_t svlen = strlen(ValueToString(theExp->value));
            if (svlen > (flen + 1))
              {
               const char *slotName = &ValueToString(theExp->value)[flen+1];
               
               theExp->argList = GenConstant(theEnv,FCALL,(void *) FindFunction(theEnv,"(get-progn$-field)"));
               theExp->argList->argList = GenConstant(theEnv,INTEGER,EnvAddLong(theEnv,(long long) depth));

               theExp->argList->nextArg = GenConstant(theEnv,SYMBOL,EnvAddSymbol(theEnv,slotName));
               theExp->argList->nextArg->nextArg = GenConstant(theEnv,SYMBOL,theExp->value);

               theExp->type = FCALL;
               theExp->value = FindFunction(theEnv,"(slot-value)");
              }
           }
#endif
         else if (strcmp(ValueToString(theExp->value) + flen,"-index") == 0)
           {
            theExp->type = FCALL;
            theExp->value = FindFunction(theEnv,"(get-progn$-index)");
            theExp->argList = GenConstant(theEnv,INTEGER,EnvAddLong(theEnv,(long long) depth));
           }
        }
      else if (theExp->argList != NULL)
        {
         if ((theExp->type == FCALL) && ((theExp->value == (void *) FindFunction(theEnv,"progn$")) ||
                                        (theExp->value == (void *) FindFunction(theEnv,"foreach")) ))
           ReplaceMvPrognFieldVars(theEnv,fieldVar,theExp->argList,depth+1);
         else
           ReplaceMvPrognFieldVars(theEnv,fieldVar,theExp->argList,depth);
        }
      theExp = theExp->nextArg;
     }
  }

#endif /* (! BLOAD_ONLY) && (! RUN_TIME) */

/*****************************************/
/* MultifieldPrognFunction: H/L access   */
/*   routine for the progn$ function.    */
/*****************************************/
void MultifieldPrognFunction(
  Environment *theEnv,
  UDFContext *context,
  CLIPSValue *returnValue)
  {
   MultifieldPrognDriver(context,returnValue,"progn$");
  }

/***************************************/
/* ForeachFunction: H/L access routine */
/*   for the foreach function.         */
/***************************************/
void ForeachFunction(
  Environment *theEnv,
  UDFContext *context,
  CLIPSValue *returnValue)
  {
   MultifieldPrognDriver(context,returnValue,"foreach");
  }
    
/*******************************************/
/* MultifieldPrognDriver: Driver routine   */
/*   for the progn$ and foreach functions. */
/******************************************/
static void MultifieldPrognDriver(
  UDFContext *context,
  CLIPSValue *returnValue,
  const char *functionName)
  {
   EXPRESSION *theExp;
   CLIPSValue argval;
   long i, end; /* 6.04 Bug Fix */
   FIELD_VAR_STACK *tmpField;
   struct CLIPSBlock gcBlock;
   Environment *theEnv = context->environment;
   
   tmpField = get_struct(theEnv,fieldVarStack);
   tmpField->type = SYMBOL;
   tmpField->value = EnvFalseSymbol(theEnv);
   tmpField->nxt = MultiFunctionData(theEnv)->FieldVarStack;
   MultiFunctionData(theEnv)->FieldVarStack = tmpField;
   returnValue->type = SYMBOL;
   returnValue->value = EnvFalseSymbol(theEnv);
   
   if (! UDFFirstArgument(context,MULTIFIELD_TYPE,&argval))
     {
      MultiFunctionData(theEnv)->FieldVarStack = tmpField->nxt;
      rtn_struct(theEnv,fieldVarStack,tmpField);
      returnValue->type = SYMBOL;
      returnValue->value = EnvFalseSymbol(theEnv);
      return;
     }
     
   CLIPSBlockStart(theEnv,&gcBlock);

   end = GetDOEnd(argval);
   for (i = GetDOBegin(argval) ; i <= end ; i++)
     {
      tmpField->type = GetMFType(argval.value,i);
      tmpField->value = GetMFValue(argval.value,i);
      /* tmpField->index = i; */
      tmpField->index = (i - GetDOBegin(argval)) + 1; 
      for (theExp = GetFirstArgument()->nextArg ; theExp != NULL ; theExp = theExp->nextArg)
        {
         EvaluateExpression(theEnv,theExp,returnValue);
        
         if (EvaluationData(theEnv)->HaltExecution || ProcedureFunctionData(theEnv)->BreakFlag || ProcedureFunctionData(theEnv)->ReturnFlag)
           {
            ProcedureFunctionData(theEnv)->BreakFlag = false;
            if (EvaluationData(theEnv)->HaltExecution)
              {
               returnValue->type = SYMBOL;
               returnValue->value = EnvFalseSymbol(theEnv);
              }
            MultiFunctionData(theEnv)->FieldVarStack = tmpField->nxt;
            rtn_struct(theEnv,fieldVarStack,tmpField);
            CLIPSBlockEnd(theEnv,&gcBlock,returnValue);
            return;
           }

         /*===================================*/
         /* Garbage collect if this isn't the */
         /* last evaluation of the progn$.    */
         /*===================================*/
         
         if ((i < end) || (theExp->nextArg != NULL))
           {
            CleanCurrentGarbageFrame(theEnv,NULL);
            CallPeriodicTasks(theEnv);
           }
        }
     }
     
   ProcedureFunctionData(theEnv)->BreakFlag = false;
   MultiFunctionData(theEnv)->FieldVarStack = tmpField->nxt;
   rtn_struct(theEnv,fieldVarStack,tmpField);
   
   CLIPSBlockEnd(theEnv,&gcBlock,returnValue);
   CallPeriodicTasks(theEnv);
  }

/*******************/
/* GetMvPrognField */
/*******************/
void GetMvPrognField(
  Environment *theEnv,
  UDFContext *context,
  CLIPSValue *returnValue)
  {
   int depth;
   FIELD_VAR_STACK *tmpField;

   depth = ValueToInteger(GetFirstArgument()->value);
   tmpField = MultiFunctionData(theEnv)->FieldVarStack;
   while (depth > 0)
     {
      tmpField = tmpField->nxt;
      depth--;
     }
   returnValue->type = tmpField->type;
   returnValue->value = tmpField->value;
  }

/*******************/
/* GetMvPrognIndex */
/*******************/
void GetMvPrognIndex(
  Environment *theEnv,
  UDFContext *context,
  CLIPSValue *returnValue)
  {
   int depth;
   FIELD_VAR_STACK *tmpField;

   depth = ValueToInteger(GetFirstArgument()->value);
   tmpField = MultiFunctionData(theEnv)->FieldVarStack;
   while (depth > 0)
     {
      tmpField = tmpField->nxt;
      depth--;
     }
   mCVSetInteger(returnValue,tmpField->index);
  }

#endif /* MULTIFIELD_FUNCTIONS */

#if OBJECT_SYSTEM || MULTIFIELD_FUNCTIONS

/**************************************************************************
  NAME         : ReplaceMultiValueField
  DESCRIPTION  : Performs a replace on the src multi-field value
                   storing the results in the dst multi-field value
  INPUTS       : 1) The destination value buffer
                 2) The source value (can be NULL)
                 3) Beginning of index range
                 4) End of range
                 5) The new field value
  RETURNS      : True if successful, false otherwise
  SIDE EFFECTS : Allocates and sets a ephemeral segment (even if new
                   number of fields is 0)
                 Src value segment is not changed
  NOTES        : index is NOT guaranteed to be valid
                 src is guaranteed to be a multi-field variable or NULL
 **************************************************************************/
bool ReplaceMultiValueField(
  Environment *theEnv,
  CLIPSValue *dst,
  CLIPSValue *src,
  long rb,
  long re,
  CLIPSValue *field,
  const char *funcName)
  {
   long i,j,k;
   struct field *deptr;
   struct field *septr;
   long srclen,dstlen;

   srclen = ((src != NULL) ? (src->end - src->begin + 1) : 0);
   if ((re < rb) ||
	   (rb < 1) || (re < 1) ||
	   (rb > srclen) || (re > srclen))
	 {
	  MVRangeError(theEnv,rb,re,srclen,funcName);
	  return false;
	 }
   rb = src->begin + rb - 1;
   re = src->begin + re - 1;
   if (field->type == MULTIFIELD)
	 dstlen = srclen + GetpDOLength(field) - (re-rb+1);
   else
	 dstlen = srclen + 1 - (re-rb+1);
   dst->type = MULTIFIELD;
   dst->begin = 0;
   dst->value = EnvCreateMultifield(theEnv,dstlen);
   SetpDOEnd(dst,dstlen);
   for (i = 0 , j = src->begin ; j < rb ; i++ , j++)
	 {
	  deptr = &((struct multifield *) dst->value)->theFields[i];
	  septr = &((struct multifield *) src->value)->theFields[j];
	  deptr->type = septr->type;
	  deptr->value = septr->value;
	 }
   if (field->type != MULTIFIELD)
	 {
	  deptr = &((struct multifield *) dst->value)->theFields[i++];
	  deptr->type = field->type;
	  deptr->value = field->value;
	 }
   else
	 {
	  for (k = field->begin ; k <= field->end ; k++ , i++)
		{
		 deptr = &((struct multifield *) dst->value)->theFields[i];
		 septr = &((struct multifield *) field->value)->theFields[k];
		 deptr->type = septr->type;
		 deptr->value = septr->value;
		}
	 }
   while (j < re)
	 j++;
   for (j++ ; i < dstlen ; i++ , j++)
	 {
	  deptr = &((struct multifield *) dst->value)->theFields[i];
	  septr = &((struct multifield *) src->value)->theFields[j];
	  deptr->type = septr->type;
	  deptr->value = septr->value;
	 }
   return true;
  }

/**************************************************************************
  NAME         : InsertMultiValueField
  DESCRIPTION  : Performs an insert on the src multi-field value
                   storing the results in the dst multi-field value
  INPUTS       : 1) The destination value buffer
                 2) The source value (can be NULL)
                 3) The index for the change
                 4) The new field value
  RETURNS      : True if successful, false otherwise
  SIDE EFFECTS : Allocates and sets a ephemeral segment (even if new
                   number of fields is 0)
                 Src value segment is not changed
  NOTES        : index is NOT guaranteed to be valid
                 src is guaranteed to be a multi-field variable or NULL
 **************************************************************************/
bool InsertMultiValueField(
  Environment *theEnv,
  CLIPSValue *dst,
  CLIPSValue *src,
  long theIndex,
  CLIPSValue *field,
  const char *funcName)
  {
   long i,j,k;
   FIELD *deptr, *septr;
   long srclen,dstlen;

   srclen = (long) ((src != NULL) ? (src->end - src->begin + 1) : 0);
   if (theIndex < 1)
     {
      MVRangeError(theEnv,theIndex,theIndex,srclen+1,funcName);
      return false;
     }
   if (theIndex > (srclen + 1))
     theIndex = (srclen + 1);
   dst->type = MULTIFIELD;
   dst->begin = 0;
   if (src == NULL)
     {
      if (field->type == MULTIFIELD)
        {
         DuplicateMultifield(theEnv,dst,field);
         AddToMultifieldList(theEnv,(struct multifield *) dst->value);
        }
      else
        {
         dst->value = EnvCreateMultifield(theEnv,0L);
         dst->end = 0;
         deptr = &((struct multifield *) dst->value)->theFields[0];
         deptr->type = field->type;
         deptr->value = field->value;
        }
      return true;
     }
   dstlen = (field->type == MULTIFIELD) ? GetpDOLength(field) + srclen : srclen + 1;
   dst->value = EnvCreateMultifield(theEnv,dstlen);
   SetpDOEnd(dst,dstlen);
   theIndex--;
   for (i = 0 , j = src->begin ; i < theIndex ; i++ , j++)
     {
      deptr = &((struct multifield *) dst->value)->theFields[i];
      septr = &((struct multifield *) src->value)->theFields[j];
      deptr->type = septr->type;
      deptr->value = septr->value;
     }
   if (field->type != MULTIFIELD)
     {
      deptr = &((struct multifield *) dst->value)->theFields[theIndex];
      deptr->type = field->type;
      deptr->value = field->value;
      i++;
     }
   else
     {
      for (k = field->begin ; k <= field->end ; k++ , i++)
        {
         deptr = &((struct multifield *) dst->value)->theFields[i];
         septr = &((struct multifield *) field->value)->theFields[k];
         deptr->type = septr->type;
         deptr->value = septr->value;
        }
     }
   for ( ; j <= src->end ; i++ , j++)
     {
      deptr = &((struct multifield *) dst->value)->theFields[i];
      septr = &((struct multifield *) src->value)->theFields[j];
      deptr->type = septr->type;
      deptr->value = septr->value;
     }
   return true;
  }

/*******************************************************
  NAME         : MVRangeError
  DESCRIPTION  : Prints out an error messages for index
                   out-of-range errors in multi-field
                   access functions
  INPUTS       : 1) The bad range start
                 2) The bad range end
                 3) The max end of the range (min is
                     assumed to be 1)
  RETURNS      : Nothing useful
  SIDE EFFECTS : None
  NOTES        : None
 ******************************************************/
static void MVRangeError(
  Environment *theEnv,
  long brb,
  long bre,
  long max,
  const char *funcName)
  {
   PrintErrorID(theEnv,"MULTIFUN",1,false);
   EnvPrintRouter(theEnv,WERROR,"Multifield index ");
   if (brb == bre)
     PrintLongInteger(theEnv,WERROR,(long long) brb);
   else
     {
      EnvPrintRouter(theEnv,WERROR,"range ");
      PrintLongInteger(theEnv,WERROR,(long long) brb);
      EnvPrintRouter(theEnv,WERROR,"..");
      PrintLongInteger(theEnv,WERROR,(long long) bre);
     }
   EnvPrintRouter(theEnv,WERROR," out of range 1..");
   PrintLongInteger(theEnv,WERROR,(long long) max);
   if (funcName != NULL)
     {
      EnvPrintRouter(theEnv,WERROR," in function ");
      EnvPrintRouter(theEnv,WERROR,funcName);
     }
   EnvPrintRouter(theEnv,WERROR,".\n");
  }

/**************************************************************************
  NAME         : DeleteMultiValueField
  DESCRIPTION  : Performs a modify on the src multi-field value
                   storing the results in the dst multi-field value
  INPUTS       : 1) The destination value buffer
                 2) The source value (can be NULL)
                 3) The beginning index for deletion
                 4) The ending index for deletion
  RETURNS      : True if successful, false otherwise
  SIDE EFFECTS : Allocates and sets a ephemeral segment (even if new
                   number of fields is 0)
                 Src value segment is not changed
  NOTES        : index is NOT guaranteed to be valid
                 src is guaranteed to be a multi-field variable or NULL
 **************************************************************************/
bool DeleteMultiValueField(
  Environment *theEnv,
  CLIPSValue *dst,
  CLIPSValue *src,
  long rb,
  long re,
  const char *funcName)
  {
   long i,j;
   FIELD_PTR deptr,septr;
   long srclen, dstlen;

   srclen = (long) ((src != NULL) ? (src->end - src->begin + 1) : 0);
   if ((re < rb) ||
       (rb < 1) || (re < 1) ||
       (rb > srclen) || (re > srclen))
     {
      MVRangeError(theEnv,rb,re,srclen,funcName);
      return false;
     }
   dst->type = MULTIFIELD;
   dst->begin = 0;
   if (srclen == 0)
    {
     dst->value = EnvCreateMultifield(theEnv,0L);
     dst->end = -1;
     return true;
    }
   rb = src->begin + rb -1;
   re = src->begin + re -1;
   dstlen = srclen-(re-rb+1);
   SetpDOEnd(dst,dstlen);
   dst->value = EnvCreateMultifield(theEnv,dstlen);
   for (i = 0 , j = src->begin ; j < rb ; i++ , j++)
     {
      deptr = &((struct multifield *) dst->value)->theFields[i];
      septr = &((struct multifield *) src->value)->theFields[j];
      deptr->type = septr->type;
      deptr->value = septr->value;
     }
   while (j < re)
     j++;
   for (j++ ; i <= dst->end ; j++ , i++)
     {
      deptr = &((struct multifield *) dst->value)->theFields[i];
      septr = &((struct multifield *) src->value)->theFields[j];
      deptr->type = septr->type;
      deptr->value = septr->value;
     }
   return true;
  }

#endif /* OBJECT_SYSTEM || MULTIFIELD_FUNCTIONS */
