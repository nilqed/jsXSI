   /*******************************************************/
   /*      "C" Language Integrated Production System      */
   /*                                                     */
   /*             CLIPS Version 6.40  08/25/16            */
   /*                                                     */
   /*               EVALUATION HEADER FILE                */
   /*******************************************************/

/*************************************************************/
/* Purpose: Provides routines for evaluating expressions.    */
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
/*      6.24: Renamed BOOLEAN macro type to intBool.         */
/*                                                           */
/*            Added EvaluateAndStoreInDataObject function.   */
/*                                                           */
/*      6.30: Added support for passing context information  */ 
/*            to user defined functions.                     */
/*                                                           */
/*            Added support for external address hash table  */
/*            and subtyping.                                 */
/*                                                           */
/*            Changed integer type/precision.                */
/*                                                           */
/*            Support for long long integers.                */
/*                                                           */
/*            Changed garbage collection algorithm.          */
/*                                                           */
/*            Support for DATA_OBJECT_ARRAY primitive.       */
/*                                                           */
/*            Added const qualifiers to remove C++           */
/*            deprecation warnings.                          */
/*                                                           */
/*            Converted API macros to function calls.        */
/*                                                           */
/*      6.40: Added Env prefix to GetEvaluationError and     */
/*            SetEvaluationError functions.                  */
/*                                                           */
/*            Added Env prefix to GetHaltExecution and       */
/*            SetHaltExecution functions.                    */
/*                                                           */
/*            Removed LOCALE definition.                     */
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
/*            UDF redesign.                                  */
/*                                                           */
/*************************************************************/

#ifndef _H_evaluatn

#pragma once

#define _H_evaluatn

struct entityRecord;
struct dataObject;

typedef struct dataObject CLIPSValue;
typedef struct dataObject * CLIPSValuePtr;
typedef struct expr FUNCTION_REFERENCE;

typedef void EntityPrintFunction(Environment *,const char *,void *);
typedef bool EntityEvaluationFunction(Environment *,void *,CLIPSValue *);
typedef void EntityBusyCountFunction(Environment *,void *);

#include "constant.h"
#include "symbol.h"
#include "expressn.h"

struct dataObject
  {
   void *supplementalInfo;
   unsigned short type;
   unsigned bitType;
   void *value;
   long begin;
   long end;
   struct dataObject *next;
   Environment *environment;
  };

#define C_POINTER_EXTERNAL_ADDRESS 0

#include "userdata.h"

struct entityRecord
  {
   const char *name;
   unsigned int type : 13;
   unsigned int copyToEvaluate : 1;
   unsigned int bitMap : 1;
   unsigned int addsToRuleComplexity : 1;
   EntityPrintFunction *shortPrintFunction;
   EntityPrintFunction *longPrintFunction;
   bool (*deleteFunction)(void *,void *);
   EntityEvaluationFunction *evaluateFunction;
   void *(*getNextFunction)(void *,void *);
   EntityBusyCountFunction *decrementBusyCount;
   EntityBusyCountFunction *incrementBusyCount;
   void (*propagateDepth)(void *,void *);
   void (*markNeeded)(void *,void *);
   void (*install)(void *,void *);
   void (*deinstall)(void *,void *);
   struct userData *usrData;
  };

struct externalAddressType
  {
   const  char *name;
   void (*shortPrintFunction)(Environment *,const char *,void *);
   void (*longPrintFunction)(Environment *,const char *,void *);
   bool (*discardFunction)(Environment *,void *);
   void (*newFunction)(UDFContext *,CLIPSValue *);
   bool (*callFunction)(UDFContext *,CLIPSValue *,CLIPSValue *);
  };

typedef struct entityRecord ENTITY_RECORD;
typedef struct entityRecord * ENTITY_RECORD_PTR;

#define GetDOLength(target)       (((target).end - (target).begin) + 1)
#define GetpDOLength(target)      (((target)->end - (target)->begin) + 1)
#define GetDOBegin(target)        ((target).begin + 1)
#define GetDOEnd(target)          ((target).end + 1)
#define GetpDOBegin(target)       ((target)->begin + 1)
#define GetpDOEnd(target)         ((target)->end + 1)
#define SetDOBegin(target,val)   ((target).begin = (long) ((val) - 1))
#define SetDOEnd(target,val)     ((target).end = (long) ((val) - 1))
#define SetpDOBegin(target,val)   ((target)->begin = (long) ((val) - 1))
#define SetpDOEnd(target,val)     ((target)->end = (long) ((val) - 1))

#define EnvGetDOLength(theEnv,target)       (((target).end - (target).begin) + 1)
#define EnvGetpDOLength(theEnv,target)      (((target)->end - (target)->begin) + 1)
#define EnvGetDOBegin(theEnv,target)        ((target).begin + 1)
#define EnvGetDOEnd(theEnv,target)          ((target).end + 1)
#define EnvGetpDOBegin(theEnv,target)       ((target)->begin + 1)
#define EnvGetpDOEnd(theEnv,target)         ((target)->end + 1)
#define EnvSetDOBegin(theEnv,target,val)   ((target).begin = (long) ((val) - 1))
#define EnvSetDOEnd(theEnv,target,val)     ((target).end = (long) ((val) - 1))
#define EnvSetpDOBegin(theEnv,target,val)   ((target)->begin = (long) ((val) - 1))
#define EnvSetpDOEnd(theEnv,target,val)     ((target)->end = (long) ((val) - 1))

#define DOPToString(target) (((struct symbolHashNode *) ((target)->value))->contents)
#define DOPToDouble(target) (((struct floatHashNode *) ((target)->value))->contents)
#define DOPToFloat(target) ((float) (((struct floatHashNode *) ((target)->value))->contents))
#define DOPToLong(target) (((struct integerHashNode *) ((target)->value))->contents)
#define DOPToInteger(target) ((int) (((struct integerHashNode *) ((target)->value))->contents))
#define DOPToPointer(target)       ((target)->value)
#define DOPToExternalAddress(target) (((struct externalAddressHashNode *) ((target)->value))->externalAddress)

#define EnvDOPToString(theEnv,target) (((struct symbolHashNode *) ((target)->value))->contents)
#define EnvDOPToDouble(theEnv,target) (((struct floatHashNode *) ((target)->value))->contents)
#define EnvDOPToFloat(theEnv,target) ((float) (((struct floatHashNode *) ((target)->value))->contents))
#define EnvDOPToLong(theEnv,target) (((struct integerHashNode *) ((target)->value))->contents)
#define EnvDOPToInteger(theEnv,target) ((int) (((struct integerHashNode *) ((target)->value))->contents))
#define EnvDOPToPointer(theEnv,target)       ((target)->value)
#define EnvDOPToExternalAddress(target) (((struct externalAddressHashNode *) ((target)->value))->externalAddress)

#define DOToString(target) (((struct symbolHashNode *) ((target).value))->contents)
#define DOToDouble(target) (((struct floatHashNode *) ((target).value))->contents)
#define DOToFloat(target) ((float) (((struct floatHashNode *) ((target).value))->contents))
#define DOToLong(target) (((struct integerHashNode *) ((target).value))->contents)
#define DOToInteger(target) ((int) (((struct integerHashNode *) ((target).value))->contents))
#define DOToPointer(target)        ((target).value)
#define DOToExternalAddress(target) (((struct externalAddressHashNode *) ((target).value))->externalAddress)

#define EnvDOToString(theEnv,target) (((struct symbolHashNode *) ((target).value))->contents)
#define EnvDOToDouble(theEnv,target) (((struct floatHashNode *) ((target).value))->contents)
#define EnvDOToFloat(theEnv,target) ((float) (((struct floatHashNode *) ((target).value))->contents))
#define EnvDOToLong(theEnv,target) (((struct integerHashNode *) ((target).value))->contents)
#define EnvDOToInteger(theEnv,target) ((int) (((struct integerHashNode *) ((target).value))->contents))
#define EnvDOToPointer(theEnv,target)        ((target).value)
#define EnvDOToExternalAddress(target) (((struct externalAddressHashNode *) ((target).value))->externalAddress)

#define CoerceToLongInteger(t,v) ((t == INTEGER) ? ValueToLong(v) : (long int) ValueToDouble(v))
#define CoerceToInteger(t,v) ((t == INTEGER) ? (int) ValueToLong(v) : (int) ValueToDouble(v))
#define CoerceToDouble(t,v) ((t == INTEGER) ? (double) ValueToLong(v) : ValueToDouble(v))

#define GetFirstArgument()           (EvaluationData(theEnv)->CurrentExpression->argList)
#define GetNextArgument(ep)          (ep->nextArg)

#define MAXIMUM_PRIMITIVES 150
#define MAXIMUM_EXTERNAL_ADDRESS_TYPES 10

#define BITS_PER_BYTE    8

#define BitwiseTest(n,b)   ((n) & (char) (1 << (b)))
#define BitwiseSet(n,b)    (n |= (char) (1 << (b)))
#define BitwiseClear(n,b)  (n &= (char) ~(1 << (b)))

#define TestBitMap(map,id)  BitwiseTest(map[(id) / BITS_PER_BYTE],(id) % BITS_PER_BYTE)
#define SetBitMap(map,id)   BitwiseSet(map[(id) / BITS_PER_BYTE],(id) % BITS_PER_BYTE)
#define ClearBitMap(map,id) BitwiseClear(map[(id) / BITS_PER_BYTE],(id) % BITS_PER_BYTE)

#define EVALUATION_DATA 44

struct evaluationData
  { 
   struct expr *CurrentExpression;
   bool EvaluationError;
   bool HaltExecution;
   int CurrentEvaluationDepth;
   int numberOfAddressTypes;
   struct entityRecord *PrimitivesArray[MAXIMUM_PRIMITIVES];
   struct externalAddressType *ExternalAddressTypes[MAXIMUM_EXTERNAL_ADDRESS_TYPES];
  };

#define EvaluationData(theEnv) ((struct evaluationData *) GetEnvironmentData(theEnv,EVALUATION_DATA))

#include "factmngr.h"
#include "object.h"

   void                           InitializeEvaluationData(Environment *);
   bool                           EvaluateExpression(Environment *,struct expr *,CLIPSValue *);
   void                           EnvSetEvaluationError(Environment *,bool);
   bool                           EnvGetEvaluationError(Environment *);
   void                           EnvSetHaltExecution(Environment *,bool);
   bool                           EnvGetHaltExecution(Environment *);
   void                           ReturnValues(Environment *,CLIPSValue *,bool);
   void                           PrintDataObject(Environment *,const char *,CLIPSValue *);
   void                           EnvSetMultifieldErrorValue(Environment *,CLIPSValue *);
   void                           ValueInstall(Environment *,CLIPSValue *);
   void                           ValueDeinstall(Environment *,CLIPSValue *);
#if DEFFUNCTION_CONSTRUCT || DEFGENERIC_CONSTRUCT
   bool                           EnvFunctionCall(Environment *,const char *,const char *,CLIPSValue *);
   bool                           FunctionCall2(Environment *,FUNCTION_REFERENCE *,const char *,CLIPSValue *);
#endif
   void                           CopyDataObject(Environment *,CLIPSValue *,CLIPSValue *,int);
   void                           AtomInstall(Environment *,int,void *);
   void                           AtomDeinstall(Environment *,int,void *);
   struct expr                   *ConvertValueToExpression(Environment *,CLIPSValue *);
   unsigned long                  GetAtomicHashValue(unsigned short,void *,int);
   void                           InstallPrimitive(Environment *,struct entityRecord *,int);
   int                            InstallExternalAddressType(Environment *,struct externalAddressType *);
   void                           TransferDataObjectValues(CLIPSValue *,CLIPSValue *);
   struct expr                   *FunctionReferenceExpression(Environment *,const char *);
   bool                           GetFunctionReference(Environment *,const char *,FUNCTION_REFERENCE *);
   bool                           DOsEqual(CLIPSValue *,CLIPSValue *);
   bool                           EvaluateAndStoreInDataObject(Environment *,bool,EXPRESSION *,CLIPSValue *,bool);
   void                           MFSetNthValueF(CLIPSValue *,CLIPSInteger,CLIPSValue *);
   void                           CVCreateMultifieldF(CLIPSValue *,CLIPSInteger);
   CLIPSString                    CVToString(CLIPSValue *);
   CLIPSInteger                   CVToInteger(CLIPSValue *);
   CLIPSFloat                     CVToFloat(CLIPSValue *);
   void                           CVSetVoid(CLIPSValue *);
   void                           CVSetInteger(CLIPSValue *,CLIPSInteger);
   void                           CVSetFloat(CLIPSValue *,CLIPSFloat);
   void                           CVSetSymbol(CLIPSValue *,CLIPSString);
   void                           CVSetString(CLIPSValue *,CLIPSString);
   void                           CVSetInstanceName(CLIPSValue *,CLIPSString);
   void                           CVSetInstanceAddress(CLIPSValue *,Instance *);
   void                           CVSetFactAddress(CLIPSValue *,Fact *);
   void                           CVSetBoolean(CLIPSValue *,bool);
   bool                           CVIsType(CLIPSValue *,unsigned);
   void                           EnvCVInit(Environment *,CLIPSValue *);
   void                           UDFCVInit(UDFContext *,CLIPSValue *);
   bool                           CVIsFalseSymbol(CLIPSValue *);
   bool                           CVIsTrueSymbol(CLIPSValue *);
   CLIPSInteger                   MFLength(CLIPSValue *);
   void                           MFNthValue(CLIPSValue *,CLIPSInteger,CLIPSValue *);

#define mCVIsType(cv,cvType) ((cv)->bitType & (cvType))

#define mCVToFloat(cv) ((cv)->type == FLOAT ? (CLIPSFloat) (((struct floatHashNode *) ((cv)->value))->contents) : \
                       ((cv)->type == INTEGER) ? (CLIPSFloat) (((struct integerHashNode *) ((cv)->value))->contents) : 0.0)

#define mCVToInteger(cv) ((cv)->type == INTEGER ? (CLIPSInteger) (((struct integerHashNode *) ((cv)->value))->contents) : \
                       ((cv)->type == FLOAT) ? (CLIPSInteger) (((struct floatHashNode *) ((cv)->value))->contents) : 0LL)

#define mCVToString(cv) (((struct symbolHashNode *) ((cv)->value))->contents)

#define mCVSetVoid(cv) \
   ( (cv)->value = NULL ,  \
     (cv)->bitType = VOID_TYPE , \
     (cv)->type = RVOID )

#define mCVSetInteger(cv,iv) \
   ( (cv)->value = EnvAddLong((cv)->environment,(iv)) ,  \
     (cv)->bitType = INTEGER_TYPE , \
     (cv)->type = INTEGER )

#define mCVSetFloat(cv,fv) \
   ( (cv)->value = EnvAddDouble((cv)->environment,(fv)) ,  \
     (cv)->bitType = FLOAT_TYPE , \
     (cv)->type = FLOAT )

#define mCVSetString(cv,sv) \
   ( (cv)->value = EnvAddSymbol((cv)->environment,(sv)) , \
     (cv)->bitType = STRING_TYPE, \
     (cv)->type = STRING )

#define mCVSetSymbol(cv,sv) \
   ( (cv)->value = EnvAddSymbol((cv)->environment,(sv)) , \
     (cv)->bitType = SYMBOL_TYPE, \
     (cv)->type = SYMBOL )

#define mCVSetInstanceName(cv,iv) \
   ( (cv)->value = EnvAddSymbol((cv)->environment,(iv)) , \
     (cv)->bitType = INSTANCE_NAME_TYPE, \
     (cv)->type = INSTANCE_NAME )

#define mCVSetFactAddress(cv,fv) \
   ( (cv)->value = (fv) , \
     (cv)->bitType = FACT_ADDRESS_TYPE, \
     (cv)->type = FACT_ADDRESS )

#define mCVSetInstanceAddress(cv,iv) \
   ( (cv)->value = (iv) , \
     (cv)->bitType = INSTANCE_ADDRESS_TYPE, \
     (cv)->type = INSTANCE_ADDRESS )

#define mCVSetBoolean(cv,bv) \
   ( (cv)->value = ((bv) ? SymbolData((cv)->environment)->TrueSymbolHN : \
                           SymbolData((cv)->environment)->FalseSymbolHN ) , \
     (cv)->bitType = (SYMBOL_TYPE | BOOLEAN_TYPE) , \
     (cv)->type = SYMBOL )

#define mEnvCVInit(env,rv) ((rv)->environment = env)

#define mUDFCVInit(context,rv) ((rv)->environment = (context)->environment)

#define mCVIsFalseSymbol(cv) (((cv)->type == SYMBOL) &&  ((cv)->value == SymbolData((cv)->environment)->FalseSymbolHN))

#define mCVIsTrueSymbol(cv) (((cv)->type == SYMBOL) &&  ((cv)->value == SymbolData((cv)->environment)->TrueSymbolHN))

#define mMFLength(cv)   ((cv)->type == MULTIFIELD ? (((cv)->end - (cv)->begin) + 1) : 0)

#define mMFNthValue(mf,n,rv) \
   ( (rv)->type = (((struct field *) ((struct multifield *) ((mf)->value))->theFields)[((mf)->begin + n) - 1].type) , \
     (rv)->value = (((struct field *) ((struct multifield *) ((mf)->value))->theFields)[((mf)->begin + n) - 1].value) , \
     (rv)->bitType = (1 << (rv)->type) , \
     (rv)->environment = (mf)->environment )

/******/

#define CVType(cv) ((cv)->bitType)

#define CVSetCLIPSValue(v1,v2) ((v1)->type = (v2)->type, (v1)->value = (v2)->value, (v1)->bitType = (v2)->bitType)

#define CVToRawValue(cv) ((cv)->value)

#define CVSetCLIPSSymbol(cv,sv) \
   ( (cv)->value = (sv) , \
     (cv)->bitType = SYMBOL_TYPE, \
     (cv)->type = SYMBOL )

#define CVSetCLIPSString(cv,sv) \
   ( (cv)->value = (sv) , \
     (cv)->bitType = STRING_TYPE, \
     (cv)->type = STRING )

#define CVSetCLIPSInstanceName(cv,iv) \
   ( (cv)->value = (iv) , \
     (cv)->bitType = INSTANCE_NAME_TYPE, \
     (cv)->type = INSTANCE_NAME )

#define CVSetExternalAddress(cv,iv,ivt) \
   ( (cv)->value = EnvAddExternalAddress((cv)->environment,iv,ivt) , \
     (cv)->bitType = EXTERNAL_ADDRESS_TYPE, \
     (cv)->type = EXTERNAL_ADDRESS )

#define CVSetRawValue(cv,rv) \
   ( (cv)->value = (rv)  )


#define MFSetNthValue(mf,n,nv) \
   ( \
     ((struct field *) ((struct multifield *) ((mf)->value))->theFields)[((mf)->begin + n) - 1].type = (nv)->type , \
     ((struct field *) ((struct multifield *) ((mf)->value))->theFields)[((mf)->begin + n) - 1].value = (nv)->value \
   )

#define CVCreateMultifield(mf,size) \
   ( (mf)->value = EnvCreateMultifield((mf)->environment,size) , \
     (mf)->bitType = MULTIFIELD_TYPE, \
     (mf)->type = MULTIFIELD, \
     (mf)->begin = 0, \
     (mf)->end = ((size) - 1) )

#define MFSetRange(mf,b,e) \
   ( (mf)->end = (mf)->begin + (e) - 1, \
     (mf)->begin = (mf)->begin + (b) - 1 )

#define CVSetValue(tv,sv) \
   ( (tv)->type = (sv)->type , \
     (tv)->value = (sv)->value , \
     (tv)->bitType = (sv)->bitType , \
     (tv)->begin = (sv)->begin , \
     (tv)->end = (sv)->end )

#endif /* _H_evaluatn */
