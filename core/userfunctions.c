   /*******************************************************/
   /*      "C" Language Integrated Production System      */
   /*                                                     */
   /*            CLIPS Version 6.40  07/30/16             */
   /*                                                     */
   /*                USER FUNCTIONS MODULE                */
   /*******************************************************/

/*************************************************************/
/* Purpose:                                                  */
/*                                                           */
/* Principal Programmer(s):                                  */
/*      Gary D. Riley                                        */
/*                                                           */
/* Contributing Programmer(s):                               */
/*                                                           */
/* Revision History:                                         */
/*                                                           */
/*      6.24: Created file to seperate UserFunctions and     */
/*            EnvUserFunctions from main.c.                  */
/*                                                           */
/*      6.30: Removed conditional code for unsupported       */
/*            compilers/operating systems (IBM_MCW,          */
/*            MAC_MCW, and IBM_TBC).                         */
/*                                                           */
/*            Removed use of void pointers for specific      */
/*            data structures.                               */
/*                                                           */
/*************************************************************/

/***************************************************************************/
/*                                                                         */
/* Permission is hereby granted, free of charge, to any person obtaining   */
/* a copy of this software and associated documentation files (the         */
/* "Software"), to deal in the Software without restriction, including     */
/* without limitation the rights to use, copy, modify, merge, publish,     */
/* distribute, and/or sell copies of the Software, and to permit persons   */
/* to whom the Software is furnished to do so.                             */
/*                                                                         */
/* THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS */
/* OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF              */
/* MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT   */
/* OF THIRD PARTY RIGHTS. IN NO EVENT SHALL THE AUTHORS BE LIABLE FOR ANY  */
/* CLAIM, OR ANY SPECIAL INDIRECT OR CONSEQUENTIAL DAMAGES, OR ANY DAMAGES */
/* WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN   */
/* ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF */
/* OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.          */
/*                                                                         */
/***************************************************************************/

#include "clips.h"

#include <math.h>

void UserFunctions(void);
void EnvUserFunctions(Environment *);

   void                           RTA(Environment *,UDFContext *,CLIPSValue *);
   void                           MUL(Environment *,UDFContext *,CLIPSValue *);
   void                           Positivep(Environment *,UDFContext *,CLIPSValue *);
   void                           Cube(Environment *,UDFContext *,CLIPSValue *);
   void                           TripleNumber(Environment *,UDFContext *,CLIPSValue *);
   void                           Reverse(Environment *,UDFContext *,CLIPSValue *);
   void                           MFL(Environment *,UDFContext *,CLIPSValue *);
   void                           CntMFChars(Environment *,UDFContext *,CLIPSValue *);
   void                           Sample4(Environment *,UDFContext *,CLIPSValue *);
   void                           Rest(Environment *,UDFContext *,CLIPSValue *);

/*********************************************************/
/* UserFunctions: Informs the expert system environment  */
/*   of any user defined functions. In the default case, */
/*   there are no user defined functions. To define      */
/*   functions, either this function must be replaced by */
/*   a function with the same name within this file, or  */
/*   this function can be deleted from this file and     */
/*   included in another file.                           */
/*********************************************************/
void UserFunctions()
  {
   // Use of UserFunctions is deprecated.
   // Use EnvUserFunctions instead.
  }
  
/***********************************************************/
/* EnvUserFunctions: Informs the expert system environment */
/*   of any user defined functions. In the default case,   */
/*   there are no user defined functions. To define        */
/*   functions, either this function must be replaced by   */
/*   a function with the same name within this file, or    */
/*   this function can be deleted from this file and       */
/*   included in another file.                             */
/***********************************************************/
void EnvUserFunctions(
  Environment *environment)
  {
#if MAC_XCD
#pragma unused(environment)
#endif
   EnvAddUDF(environment,"rta","d",2,2,"ld",RTA,"RTA",NULL);
   EnvAddUDF(environment,"mul","l",2,2,"ld",MUL,"MUL",NULL);
   EnvAddUDF(environment,"positivep","b",1,1,"ld",Positivep,"Positivep",NULL);
   EnvAddUDF(environment,"cube","bld",1,1,"ld",Cube,"Cube",NULL);
   EnvAddUDF(environment,"triple","ld",1,1,"ld",TripleNumber,"TripleNumber",NULL);
   EnvAddUDF(environment,"reverse","syn",1,1,"syn",Reverse,"Reverse",NULL);
   EnvAddUDF(environment,"mfl","l",1,1,"m",MFL,"MFL",NULL);
   EnvAddUDF(environment,"cmfc","l",1,1,"m",CntMFChars,"CntMFChars",NULL);
   EnvAddUDF(environment,"sample4","m",0,0,NULL,Sample4,"Sample4",NULL);
   EnvAddUDF(environment,"rest","m",1,1,"m",Rest,"Rest",NULL);
  }

/*******/
/* RTA */
/*******/
void RTA(
  Environment *theEnv,
  UDFContext *context,
  CLIPSValue *returnValue)
  {
   CLIPSValue base, height;

   if (! UDFFirstArgument(context,NUMBER_TYPES,&base))
     { return; }

   if (! UDFNextArgument(context,NUMBER_TYPES,&height))
     { return; }

   CVSetFloat(returnValue,0.5 * CVToFloat(&base) * CVToFloat(&height));
  }

/*******/
/* MUL */
/*******/
void MUL(
  Environment *theEnv,
  UDFContext *context,
  CLIPSValue *returnValue)
  {
   CLIPSValue theArg;
   CLIPSInteger firstNumber, secondNumber;
   
   /*=============================================================*/
   /* Get the first argument using the UDFFirstArgument function. */
   /* Return if the correct type has not been passed.             */
   /*=============================================================*/

   if (! UDFFirstArgument(context,NUMBER_TYPES,&theArg))
     { return; }

   /*========================================================*/
   /* Convert the first argument to an integer. If it's not  */
   /* an integer, then it must be a float (so round it to    */
   /* the nearest integer using the C library ceil function. */
   /*========================================================*/

   if (mCVIsType(&theArg,INTEGER_TYPE))
     { firstNumber = CVToInteger(&theArg); }
   else /* the type must be FLOAT */
     { firstNumber = (CLIPSInteger) ceil(CVToFloat(&theArg) - 0.5); }

   /*=============================================================*/
   /* Get the second argument using the UDFNextArgument function. */
   /* Return if the correct type has not been passed.             */
   /*=============================================================*/
   
   if (! UDFNextArgument(context,NUMBER_TYPES,&theArg))
     { return; }

   /*========================================================*/
   /* Convert the second argument to an integer. If it's not */
   /* an integer, then it must be a float (so round it to    */
   /* the nearest integer using the C library ceil function. */
   /*========================================================*/

   if (mCVIsType(&theArg,INTEGER_TYPE))
     { secondNumber = CVToInteger(&theArg); }
   else /* the type must be FLOAT */
     { secondNumber = (CLIPSInteger) ceil(CVToFloat(&theArg) - 0.5); }

   /*=========================================================*/
   /* Multiply the two values together and return the result. */
   /*=========================================================*/

   CVSetInteger(returnValue,firstNumber * secondNumber);
  }

/*************/
/* Positivep */
/*************/
void Positivep(
  Environment *theEnv,
  UDFContext *context,
  CLIPSValue *returnValue)
  {
   CLIPSValue theArg;

   /*==================================*/
   /* Get the first argument using the */
   /* UDFFirstArgument function.       */
   /*==================================*/

   if (! UDFFirstArgument(context,NUMBER_TYPES,&theArg))
     { return; }

   /*=====================================*/
   /* Determine if the value is positive. */
   /*=====================================*/

   if (CVIsType(&theArg,INTEGER_TYPE))
     {
      if (CVToInteger(&theArg) <= 0L)
        { CVSetBoolean(returnValue,false); }
      else
        { CVSetBoolean(returnValue,true); }
     }
   else /* the type must be FLOAT */
     {
      if (CVToFloat(&theArg) <= 0.0)
        { CVSetBoolean(returnValue,false); }
      else
        { CVSetBoolean(returnValue,true); }
     }
  }

/********/
/* Cube */
/********/
void Cube(
  Environment *theEnv,
  UDFContext *context,
  CLIPSValue *returnValue)
  {
   CLIPSValue theArg;
   CLIPSInteger integerValue;
   CLIPSFloat floatValue;

   /*==================================*/
   /* Get the first argument using the */
   /* UDFFirstArgument function.       */
   /*==================================*/

   if (! UDFFirstArgument(context,NUMBER_TYPES,&theArg))
     {
      CVSetBoolean(returnValue,false);
      return;
     }

   /*====================*/
   /* Cube the argument. */
   /*====================*/

   if (CVIsType(&theArg,INTEGER_TYPE))
     {
      integerValue = CVToInteger(&theArg);
      CVSetInteger(returnValue,integerValue * integerValue * integerValue);
     }
   else /* the type must be FLOAT */
     {
      floatValue = CVToFloat(&theArg);
      CVSetFloat(returnValue,floatValue * floatValue * floatValue);
     }
  }

/****************/
/* TripleNumber */
/****************/
void TripleNumber( 
  Environment *theEnv,
  UDFContext *context,
  CLIPSValue *returnValue)
  {
   CLIPSValue theArg;

   /*==================================*/
   /* Get the first argument using the */
   /* UDFFirstArgument function.       */
   /*==================================*/

   if (! UDFFirstArgument(context,NUMBER_TYPES,&theArg))
     { return; }

   /*======================*/
   /* Triple the argument. */
   /*======================*/

   if (CVIsType(&theArg,INTEGER_TYPE))
     { CVSetInteger(returnValue,3 * CVToInteger(&theArg)); }
   else /* the type must be FLOAT */
     { CVSetFloat(returnValue,3.0 * CVToFloat(&theArg)); }
  }

/***********/
/* Reverse */
/***********/
void Reverse(
  Environment *theEnv,
  UDFContext *context,
  CLIPSValue *returnValue)
  {
   CLIPSValue theArg;
   const char *theString;
   char *tempString;
   size_t length, i;

   /*=========================================================*/
   /* Get the first argument using the ArgTypeCheck function. */
   /*=========================================================*/

   if (! UDFFirstArgument(context,LEXEME_TYPES | INSTANCE_NAME_TYPE,&theArg))
     { return; }

   theString = mCVToString(&theArg);

   /*========================================================*/
   /* Allocate temporary space to store the reversed string. */
   /*========================================================*/

   length = strlen(theString);
   tempString = (char *) malloc(length + 1);

   /*=====================*/
   /* Reverse the string. */
   /*=====================*/

   for (i = 0; i < length; i++)
     { tempString[length - (i + 1)] = theString[i]; }
   tempString[length] = '\0';

   /*=====================================*/
   /* Set the return value before deallocating */
   /* the temporary reversed string.           */
   /*==========================================*/
 
   switch(CVType(&theArg))
     {
      case STRING_TYPE:
        CVSetString(returnValue,tempString);
        break;

      case SYMBOL_TYPE:
        CVSetSymbol(returnValue,tempString);
        break;
        
      case INSTANCE_NAME_TYPE:
        CVSetInstanceName(returnValue,tempString);
        break;
     }
     
   free(tempString);
  }

/*******/
/* MFL */
/*******/
void MFL(
  Environment *theEnv,
  UDFContext *context,
  CLIPSValue *returnValue)
  {
   CLIPSValue theArg;

   /*======================================================*/
   /* Check that the first argument is a multifield value. */
   /*======================================================*/

   if (! UDFFirstArgument(context,MULTIFIELD_TYPE,&theArg))
     {
      CVSetInteger(returnValue,-1);
      return;
     }
   
   /*============================================*/
   /* Return the length of the multifield value. */
   /*============================================*/
    
   CVSetInteger(returnValue,MFLength(&theArg));
  }

/**************/
/* CntMFChars */
/**************/
void CntMFChars(
  Environment *theEnv,
  UDFContext *context,
  CLIPSValue *returnValue)
  {
   CLIPSValue theArg, theValue;
   CLIPSInteger i, count = 0;
   CLIPSInteger mfLength;

   /*======================================================*/
   /* Check that the first argument is a multifield value. */
   /*======================================================*/

   if (! UDFFirstArgument(context,MULTIFIELD_TYPE,&theArg))
     { return; }
     
   /*=====================================*/
   /* Count the characters in each field. */
   /*=====================================*/

   mfLength = MFLength(&theArg);
   for (i = 1; i <= mfLength; i++)
     {
      MFNthValue(&theArg,i,&theValue);
      if (CVIsType(&theValue,LEXEME_TYPES))
        { count += strlen(CVToString(&theValue)); }
     }
     
   /*=============================*/
   /* Return the character count. */
   /*=============================*/
    
   CVSetInteger(returnValue,count);
  }

/***********/
/* Sample4 */
/***********/
void Sample4(
  Environment *theEnv,
  UDFContext *context,
  CLIPSValue *returnValue)
  {
   CLIPSValue mfValue, theValue;

   /*======================================*/
   /* Initialize the CLIPSValue variables. */
   /*======================================*/
   
   UDFCVInit(context,&mfValue);
   UDFCVInit(context,&theValue);

   /*=======================================*/
   /* Create a multifield value of length 2 */
   /*=======================================*/

   CVCreateMultifield(&mfValue,2);

   /*============================================*/
   /* The first field in the multi-field value   */
   /* will be a SYMBOL. Its value will be        */
   /* "altitude".                                */
   /*============================================*/

   CVSetSymbol(&theValue,"altitude");
   MFSetNthValue(&mfValue,1,&theValue);
   
   /*===========================================*/
   /* The second field in the multi-field value */
   /* will be a FLOAT. Its value will be 900.   */
   /*===========================================*/

   CVSetFloat(&theValue,900.0);
   MFSetNthValue(&mfValue,2,&theValue);

   /*=====================================================*/
   /* Assign the type and value to the return CLIPSValue. */
   /*=====================================================*/

   CVSetValue(returnValue,&mfValue);
  }

/********/
/* Rest */
/********/
void Rest(
  Environment *theEnv,
  UDFContext *context,
  CLIPSValue *returnValue)
  {
   CLIPSInteger mfLength;
   
   /*======================================================*/
   /* Check that the first argument is a multifield value. */
   /*======================================================*/

   if (! UDFFirstArgument(context,MULTIFIELD_TYPE,returnValue))
     { return; }

   /*===========================================*/
   /* Set the range to exclude the first value. */
   /*===========================================*/
   
   mfLength = MFLength(returnValue);
   if (mfLength != 0)
     { MFSetRange(returnValue,2,mfLength); }
  }

