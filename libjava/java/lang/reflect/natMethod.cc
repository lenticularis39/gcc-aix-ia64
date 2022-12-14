// natMethod.cc - Native code for Method class.

/* Copyright (C) 1998, 1999, 2000  Free Software Foundation

   This file is part of libgcj.

This software is copyrighted work licensed under the terms of the
Libgcj License.  Please consult the file "LIBGCJ_LICENSE" for
details.  */

#include <config.h>

#if HAVE_ALLOCA_H
#include <alloca.h>
#endif

#include <gcj/cni.h>
#include <jvm.h>
#include <jni.h>

#include <java/lang/reflect/Method.h>
#include <java/lang/reflect/Constructor.h>
#include <java/lang/reflect/InvocationTargetException.h>
#include <java/lang/reflect/Modifier.h>

#include <java/lang/Void.h>
#include <java/lang/Byte.h>
#include <java/lang/Boolean.h>
#include <java/lang/Character.h>
#include <java/lang/Short.h>
#include <java/lang/Integer.h>
#include <java/lang/Long.h>
#include <java/lang/Float.h>
#include <java/lang/Double.h>
#include <java/lang/IllegalArgumentException.h>
#include <java/lang/NullPointerException.h>
#include <java/lang/Class.h>
#include <gcj/method.h>
#include <gnu/gcj/RawData.h>

// FIXME: remove these
#define ObjectClass java::lang::Object::class$
#define ClassClass java::lang::Class::class$

#include <stdlib.h>

#include <ffi.h>

// FIXME: remove these.
#define BooleanClass java::lang::Boolean::class$
#define VoidClass java::lang::Void::class$
#define ByteClass java::lang::Byte::class$
#define ShortClass java::lang::Short::class$
#define CharacterClass java::lang::Character::class$
#define IntegerClass java::lang::Integer::class$
#define LongClass java::lang::Long::class$
#define FloatClass java::lang::Float::class$
#define DoubleClass java::lang::Double::class$

struct cpair
{
  jclass prim;
  jclass wrap;
};

// This is used to determine when a primitive widening conversion is
// allowed.
static cpair primitives[] =
{
#define BOOLEAN 0
  { JvPrimClass (boolean), &BooleanClass },
  { JvPrimClass (byte), &ByteClass },
#define SHORT 2
  { JvPrimClass (short), &ShortClass },
#define CHAR 3
  { JvPrimClass (char), &CharacterClass },
  { JvPrimClass (int), &IntegerClass },
  { JvPrimClass (long), &LongClass },
  { JvPrimClass (float), &FloatClass },
  { JvPrimClass (double), &DoubleClass },
  { NULL, NULL }
};

static inline jboolean
can_widen (jclass from, jclass to)
{
  int fromx = -1, tox = -1;

  for (int i = 0; primitives[i].prim; ++i)
    {
      if (primitives[i].wrap == from)
	fromx = i;
      if (primitives[i].prim == to)
	tox = i;
    }

  // Can't handle a miss.
  if (fromx == -1 || tox == -1)
    return false;
  // Boolean arguments may not be widened.
  if (fromx == BOOLEAN && tox != BOOLEAN)
    return false;
  // Special-case short->char conversions.
  if (fromx == SHORT && tox == CHAR)
    return false;

  return fromx <= tox;
}

static inline ffi_type *
get_ffi_type (jclass klass)
{
  // A special case.
  if (klass == NULL)
    return &ffi_type_pointer;

  ffi_type *r;
  if (klass == JvPrimClass (byte))
    r = &ffi_type_sint8;
  else if (klass == JvPrimClass (short))
    r = &ffi_type_sint16;
  else if (klass == JvPrimClass (int))
    r = &ffi_type_sint32;
  else if (klass == JvPrimClass (long))
    r = &ffi_type_sint64;
  else if (klass == JvPrimClass (float))
    r = &ffi_type_float;
  else if (klass == JvPrimClass (double))
    r = &ffi_type_double;
  else if (klass == JvPrimClass (boolean))
    {
      // On some platforms a bool is a byte, on others an int.
      if (sizeof (jboolean) == sizeof (jbyte))
	r = &ffi_type_sint8;
      else
	{
	  JvAssert (sizeof (jboolean) == sizeof (jint));
	  r = &ffi_type_sint32;
	}
    }
  else if (klass == JvPrimClass (char))
    r = &ffi_type_uint16;
  else
    {
      JvAssert (! klass->isPrimitive());
      r = &ffi_type_pointer;
    }

  return r;
}

jobject
java::lang::reflect::Method::invoke (jobject obj, jobjectArray args)
{
  if (parameter_types == NULL)
    getType ();

  jmethodID meth = _Jv_FromReflectedMethod (this);
  if (! java::lang::reflect::Modifier::isStatic(meth->accflags))
    {
      jclass k = obj ? obj->getClass() : NULL;
      if (! obj)
	JvThrow (new java::lang::NullPointerException);
      if (! declaringClass->isAssignableFrom(k))
	JvThrow (new java::lang::IllegalArgumentException);
      // FIXME: access checks.

      // Find the possibly overloaded method based on the runtime type
      // of the object.
      meth = _Jv_LookupDeclaredMethod (k, meth->name, meth->signature);
    }

  return _Jv_CallAnyMethodA (obj, return_type, meth, false,
			     parameter_types, args);
}

jint
java::lang::reflect::Method::getModifiers ()
{
  // Ignore all unknown flags.
  return _Jv_FromReflectedMethod (this)->accflags & Modifier::ALL_FLAGS;
}

jstring
java::lang::reflect::Method::getName ()
{
  if (name == NULL)
    name = _Jv_NewStringUtf8Const (_Jv_FromReflectedMethod (this)->name);
  return name;
}

/* Internal method to set return_type and parameter_types fields. */

void
java::lang::reflect::Method::getType ()
{
  _Jv_GetTypesFromSignature (_Jv_FromReflectedMethod (this),
			     declaringClass,
			     &parameter_types,
			     &return_type);

  // FIXME: for now we have no way to get exception information.
  exception_types = (JArray<jclass> *) JvNewObjectArray (0, &ClassClass,
							 NULL);
}

void
_Jv_GetTypesFromSignature (jmethodID method,
			   jclass declaringClass,
			   JArray<jclass> **arg_types_out,
			   jclass *return_type_out)
{

  _Jv_Utf8Const* sig = method->signature;
  java::lang::ClassLoader *loader = declaringClass->getClassLoader();
  char *ptr = sig->data;
  int numArgs = 0;
  /* First just count the number of parameters. */
  for (; ; ptr++)
    {
      switch (*ptr)
	{
	case 0:
	case ')':
	case 'V':
	  break;
	case '[':
	case '(':
	  continue;
	case 'B':
	case 'C':
	case 'D':
	case 'F':
	case 'S':
	case 'I':
	case 'J':
	case 'Z':
	  numArgs++;
	  continue;
	case 'L':
	  numArgs++;
	  do 
	    ptr++;
	  while (*ptr != ';' && ptr[1] != '\0');
	  continue;
	}
      break;
    }

  JArray<jclass> *args = (JArray<jclass> *)
    JvNewObjectArray (numArgs, &ClassClass, NULL);
  jclass* argPtr = elements (args);
  for (ptr = sig->data; *ptr != '\0'; ptr++)
    {
      int num_arrays = 0;
      jclass type;
      for (; *ptr == '[';  ptr++)
	num_arrays++;
      switch (*ptr)
	{
	default:
	  return;
	case ')':
	  argPtr = return_type_out;
	  continue;
	case '(':
	  continue;
	case 'V':
	case 'B':
	case 'C':
	case 'D':
	case 'F':
	case 'S':
	case 'I':
	case 'J':
	case 'Z':
	  type = _Jv_FindClassFromSignature(ptr, loader);
	  break;
	case 'L':
	  type = _Jv_FindClassFromSignature(ptr, loader);
	  do 
	    ptr++;
	  while (*ptr != ';' && ptr[1] != '\0');
	  break;
	}

      // FIXME: 2'nd argument should be "current loader"
      while (--num_arrays >= 0)
	type = _Jv_GetArrayClass (type, 0);
      // ARGPTR can be NULL if we are processing the return value of a
      // call from Constructor.
      if (argPtr)
	*argPtr++ = type;
    }
  *arg_types_out = args;
}

// This is a very rough analog of the JNI CallNonvirtual<type>MethodA
// functions.  It handles both Methods and Constructors, and it can
// handle any return type.  In the Constructor case, the `obj'
// argument is unused and should be NULL; also, the `return_type' is
// the class that the constructor will construct.  RESULT is a pointer
// to a `jvalue' (see jni.h); for a void method this should be NULL.
// This function returns an exception (if one was thrown), or NULL if
// the call went ok.
jthrowable
_Jv_CallAnyMethodA (jobject obj,
		    jclass return_type,
		    jmethodID meth,
		    jboolean is_constructor,
		    JArray<jclass> *parameter_types,
		    jvalue *args,
		    jvalue *result)
{
  JvAssert (! is_constructor || ! obj);
  JvAssert (! is_constructor || return_type);

  // See whether call needs an object as the first argument.  A
  // constructor does need a `this' argument, but it is one we create.
  jboolean needs_this = false;
  if (is_constructor
      || ! java::lang::reflect::Modifier::isStatic(meth->accflags))
    needs_this = true;

  int param_count = parameter_types->length;
  if (needs_this)
    ++param_count;

  ffi_type *rtype;
  // A constructor itself always returns void.
  if (is_constructor || return_type == JvPrimClass (void))
    rtype = &ffi_type_void;
  else
    rtype = get_ffi_type (return_type);
  ffi_type **argtypes = (ffi_type **) alloca (param_count
					      * sizeof (ffi_type *));

  jclass *paramelts = elements (parameter_types);

  // FIXME: at some point the compiler is going to add extra arguments
  // to some functions.  In particular we are going to do this for
  // handling access checks in reflection.  We must add these hidden
  // arguments here.

  // Special case for the `this' argument of a constructor.  Note that
  // the JDK 1.2 docs specify that the new object must be allocated
  // before argument conversions are done.
  if (is_constructor)
    {
      // FIXME: must special-case String, arrays, maybe others here.
      obj = JvAllocObject (return_type);
    }

  int i = 0;
  int size = 0;
  if (needs_this)
    {
      // The `NULL' type is `Object'.
      argtypes[i++] = get_ffi_type (NULL);
      size += sizeof (jobject);
    }

  for (int arg = 0; i < param_count; ++i, ++arg)
    {
      argtypes[i] = get_ffi_type (paramelts[arg]);
      if (paramelts[arg]->isPrimitive())
	size += paramelts[arg]->size();
      else
	size += sizeof (jobject);
    }

  ffi_cif cif;
  if (ffi_prep_cif (&cif, FFI_DEFAULT_ABI, param_count,
		    rtype, argtypes) != FFI_OK)
    {
      // FIXME: throw some kind of VirtualMachineError here.
    }

  char *p = (char *) alloca (size);
  void **values = (void **) alloca (param_count * sizeof (void *));

  i = 0;
  if (needs_this)
    {
      values[i] = p;
      memcpy (p, &obj, sizeof (jobject));
      p += sizeof (jobject);
      ++i;
    }

  for (int arg = 0; i < param_count; ++i, ++arg)
    {
      int tsize;
      if (paramelts[arg]->isPrimitive())
	tsize = paramelts[arg]->size();
      else
	tsize = sizeof (jobject);

      // Copy appropriate bits from the jvalue into the ffi array.
      // FIXME: we could do this copying all in one loop, above, by
      // over-allocating a bit.
      values[i] = p;
      memcpy (p, &args[arg], tsize);
      p += tsize;
    }

  // FIXME: initialize class here.

  using namespace java::lang;
  using namespace java::lang::reflect;

  Throwable *ex = NULL;

  try
    {
      ffi_call (&cif, (void (*)()) meth->ncode, result, values);
    }
  catch (Throwable *ex2)
    {
      // FIXME: this is wrong for JNI.  But if we just return the
      // exception, then the non-JNI cases won't be able to
      // distinguish it from exceptions we might generate ourselves.
      // Sigh.
      ex = new InvocationTargetException (ex2);
    }

  if (is_constructor)
    result->l = obj;

  return ex;
}

// This is another version of _Jv_CallAnyMethodA, but this one does
// more checking and is used by the reflection (and not JNI) code.
jobject
_Jv_CallAnyMethodA (jobject obj,
		    jclass return_type,
		    jmethodID meth,
		    jboolean is_constructor,
		    JArray<jclass> *parameter_types,
		    jobjectArray args)
{
  // FIXME: access checks.

  if (parameter_types->length == 0 && args == NULL)
    {
      // The JDK accepts this, so we do too.
    }
  else if (parameter_types->length != args->length)
    JvThrow (new java::lang::IllegalArgumentException);

  int param_count = parameter_types->length;

  jclass *paramelts = elements (parameter_types);
  jobject *argelts = args == NULL ? NULL : elements (args);
  jvalue argvals[param_count];

#define COPY(Where, What, Type) \
  do { \
    Type val = (What); \
    memcpy ((Where), &val, sizeof (Type)); \
  } while (0)

  for (int i = 0; i < param_count; ++i)
    {
      jclass k = argelts[i] ? argelts[i]->getClass() : NULL;
      if (paramelts[i]->isPrimitive())
	{
	  if (! argelts[i]
	      || ! k
	      || ! can_widen (k, paramelts[i]))
	    JvThrow (new java::lang::IllegalArgumentException);
	    
	  if (paramelts[i] == JvPrimClass (boolean))
	    COPY (&argvals[i],
		  ((java::lang::Boolean *) argelts[i])->booleanValue(),
		  jboolean);
	  else if (paramelts[i] == JvPrimClass (char))
	    COPY (&argvals[i],
		  ((java::lang::Character *) argelts[i])->charValue(),
		  jchar);
          else
	    {
	      java::lang::Number *num = (java::lang::Number *) argelts[i];
	      if (paramelts[i] == JvPrimClass (byte))
		COPY (&argvals[i], num->byteValue(), jbyte);
	      else if (paramelts[i] == JvPrimClass (short))
		COPY (&argvals[i], num->shortValue(), jshort);
	      else if (paramelts[i] == JvPrimClass (int))
		COPY (&argvals[i], num->intValue(), jint);
	      else if (paramelts[i] == JvPrimClass (long))
		COPY (&argvals[i], num->longValue(), jlong);
	      else if (paramelts[i] == JvPrimClass (float))
		COPY (&argvals[i], num->floatValue(), jfloat);
	      else if (paramelts[i] == JvPrimClass (double))
		COPY (&argvals[i], num->doubleValue(), jdouble);
	    }
	}
      else
	{
	  if (argelts[i] && ! paramelts[i]->isAssignableFrom (k))
	    JvThrow (new java::lang::IllegalArgumentException);
	  COPY (&argvals[i], argelts[i], jobject);
	}
    }

  jvalue ret_value;
  java::lang::Throwable *ex = _Jv_CallAnyMethodA (obj,
						  return_type,
						  meth,
						  is_constructor,
						  parameter_types,
						  argvals,
						  &ret_value);

  if (ex)
    JvThrow (ex);

  jobject r;
#define VAL(Wrapper, Field)  (new Wrapper (ret_value.Field))
  if (is_constructor)
    r = ret_value.l;
  else  if (return_type == JvPrimClass (byte))
    r = VAL (java::lang::Byte, b);
  else if (return_type == JvPrimClass (short))
    r = VAL (java::lang::Short, s);
  else if (return_type == JvPrimClass (int))
    r = VAL (java::lang::Integer, i);
  else if (return_type == JvPrimClass (long))
    r = VAL (java::lang::Long, j);
  else if (return_type == JvPrimClass (float))
    r = VAL (java::lang::Float, f);
  else if (return_type == JvPrimClass (double))
    r = VAL (java::lang::Double, d);
  else if (return_type == JvPrimClass (boolean))
    r = VAL (java::lang::Boolean, z);
  else if (return_type == JvPrimClass (char))
    r = VAL (java::lang::Character, c);
  else if (return_type == JvPrimClass (void))
    r = NULL;
  else
    {
      JvAssert (return_type == NULL || ! return_type->isPrimitive());
      r = ret_value.l;
    }

  return r;
}
