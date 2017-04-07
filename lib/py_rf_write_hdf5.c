/* The Python C extension for the rf_write_hdf5 C library
 *
 * $Id$
 *
 * This file exports the following methods to python
 * init
 * rf_write
 * free
 */

#define NPY_NO_DEPRECATED_API NPY_1_7_API_VERSION

#include <Python.h>
#include <numpy/arrayobject.h>

#include "digital_rf.h"


// declarations
void init_py_rf_write_hdf5(void);
hid_t get_hdf5_data_type(char byteorder, char dtype_char, int bytecount);

static PyObject * _py_rf_write_hdf5_init(PyObject * self, PyObject * args)
/* _py_rf_write_hdf5_init returns a pointer as a PyCObject to the Digital_rf_write_object struct created
 *
 * Inputs: python list with
 * 	1. directory - python string where Hdf5 files will be written
 * 	2. byteorder - python string of 'big' or 'little' describing the data format
 * 	3. dtype_char - python string with one character representing data type (i,u,b,B,f,d)
 * 	4. bytecount - python int giving the number of bytes in the data
 * 	5. subdir_cadence_secs - python int giving the number of seconds of data per subdirectory
 * 	6. file_cadence_millisecs - python int giving milliseconds of data per file
 * 	7. start_global_index - python int giving start time in samples since 1970 (unix_timestamp * sample rate)
 * 	8. sample_rate_numerator - python long giving the sample rate numerator
 * 	9. sample_rate_denominator - python long giving the sample rate denominator
 * 	10. uuid_str - python string representing the uuid
 * 	11. compression_level - python int representing the compression level (0-9)
 * 	12. checksum - 1 if checksums set, 0 if no checksum
 * 	13. is_complex - 1 if complex (I and Q) samples, 0 if single valued
 * 	14. num_subchannels - number of subchannels in data.  Must be at least 1 (int)
 * 	15. is_continuous - 1 is continuous data (allows faster read if no compression or checksum), 0 id with gaps
 * 	16. marching_periods - 1 for marching periods, 0 for none
 *
 *  Returns PyObject representing pointer to malloced struct if success, NULL pointer if not
 */
{
	// input arguments
	char * directory = NULL;
	char * byteorder = NULL;
	char * dtype_char = NULL;
	int bytecount = 0;
	unsigned long long subdir_cadence_secs = 0;
	unsigned long long file_cadence_millisecs = 0;
	unsigned long long start_global_index = 0;
	unsigned long long sample_rate_numerator = 0;
	unsigned long long sample_rate_denominator = 0;
	char * uuid_str = NULL;
	int compression_level = 0;
	int checksum = 0;
	int is_complex = 0;
	int num_subchannels = 0;
	int is_continuous = 0;
	int marching_periods = 0;

	// local variables
	PyObject *retObj;
	hid_t hdf5_dtype;
	Digital_rf_write_object * hdf5_write_data_object;

	// parse input arguments
	if (!PyArg_ParseTuple(args, "sssiKKKKKsiiiiii",
			  &directory,
			  &byteorder,
			  &dtype_char,
			  &bytecount,
			  &subdir_cadence_secs,
			  &file_cadence_millisecs,
			  &start_global_index,
			  &sample_rate_numerator,
			  &sample_rate_denominator,
			  &uuid_str,
			  &compression_level,
			  &checksum,
			  &is_complex,
			  &num_subchannels,
			  &is_continuous,
			  &marching_periods))
	{
		return NULL;
	}

	// find out what Hdf5 data type to use
	hdf5_dtype = get_hdf5_data_type(byteorder[0], dtype_char[0], bytecount);
	if (hdf5_dtype == -1)
	{
		fprintf(stderr, "Failed to find datatype for %c, %c, %i\n", byteorder[0], dtype_char[0], bytecount);
		PyErr_SetString(PyExc_RuntimeError, "Failed to find datatype for %c, %c, %i\n");
		return(NULL);
	}

	// create needed object (for now, we always get matching periods)
	hdf5_write_data_object = digital_rf_create_write_hdf5(directory, hdf5_dtype, subdir_cadence_secs, file_cadence_millisecs,
			                                              start_global_index, sample_rate_numerator, sample_rate_denominator,
														  uuid_str, compression_level, checksum, is_complex, num_subchannels,
			                                              is_continuous, marching_periods);

	if (!hdf5_write_data_object)
	{
		PyErr_SetString(PyExc_RuntimeError, "Failed to create Digital_rf_write_object\n");
		return(NULL);
	}

	// create python wrapper around a pointer to return
	retObj = PyCObject_FromVoidPtr(hdf5_write_data_object, NULL);

    //return pointer;
    return(retObj);

}


static PyObject * _py_rf_write_hdf5_rf_write(PyObject * self, PyObject * args)
/* _py_rf_write_hdf5_rf_write writes a block of continous data to an Hdf5 channel
 *
 * Inputs: python list with
 * 	1. PyCObject containing pointer to data structure
 * 	2. numpy array of data to write
 * 	3. next_sample - long long containing where sample id to be written (globally)
 *
 * 	Returns 1 if success, 0 if not
 *
 */
{
	// input arguments
	PyObject * pyCObject;
	PyArrayObject * pyNumArr;
	uint64_t next_sample;

	// local variables
	Digital_rf_write_object * hdf5_write_data_object;
	void * data; /* will point to numpy array's data block */
	uint64_t vector_length; /* will be set to length of data */
	int result;
	PyObject *retObj;

	// parse input arguments
	if (!PyArg_ParseTuple(args, "OOK",
			  &pyCObject,
			  &pyNumArr,
			  &next_sample))
	{
		return(NULL);
	}

	/* get C pointer to Digital_rf_write_object */
	hdf5_write_data_object = (Digital_rf_write_object *)PyCObject_AsVoidPtr(pyCObject);

	/* get C pointer to numpy array data */
	data = PyArray_DATA(pyNumArr);
	vector_length = (uint64_t)(PyArray_DIMS(pyNumArr)[0]);

	result = digital_rf_write_hdf5(hdf5_write_data_object, next_sample, data, vector_length);
	if (result)
	{
		PyErr_SetString(PyExc_RuntimeError, "Failed to write data\n");
		return(NULL);
	}

	/* success */
	retObj = Py_BuildValue("i", 1);
	return(retObj);

}


static PyObject * _py_rf_write_hdf5_rf_block_write(PyObject * self, PyObject * args)
/* _py_rf_write_hdf5_rf_block_write writes a block of data with gaps to an Hdf5 channel
 *
 * Inputs: python list with
 * 	1. PyCObject containing pointer to data structure
 * 	2. numpy array of data to write - may not be continuous
 * 	3. numpy array of global sample count - must be of type numpy.uint64
 * 	4. numpy array of block sample count - gives the position in each data arr of the
 * 		global sample given in the global sample count array above.  Len of this
 * 		array must be the same as the one before, and it must also be of type numpy.uint64
 *
 * 	 Returns 1 if success, 0 if not
 */
{
	// input arguments
	PyObject * pyCObject;
	PyArrayObject * pyNumArr;
	PyArrayObject * pyGlobalArr;
	PyArrayObject * pyBlockArr;

	// local variables
	Digital_rf_write_object * hdf5_write_data_object;
	void * data; /* will point to numpy array's data block */
	void * global_arr; /* will point to numpy pyGlobalArr's data block */
	void * block_arr; /* will point to numpy pyGlobalArr's data block */
	uint64_t vector_length;
	uint64_t index_length;
	int result;
	PyObject *retObj;

	// parse input arguments
	if (!PyArg_ParseTuple(args, "OOOO",
			  &pyCObject,
			  &pyNumArr,
			  &pyGlobalArr,
			  &pyBlockArr))
	{
		return(NULL);
	}

	/* get C pointer to Digital_rf_write_object */
	hdf5_write_data_object = (Digital_rf_write_object *)PyCObject_AsVoidPtr(pyCObject);

	/* get C pointers to numpy arrays */
	data = PyArray_DATA(pyNumArr);
	global_arr = PyArray_DATA(pyGlobalArr);
	block_arr = PyArray_DATA(pyBlockArr);

	/* get lengths */
	vector_length = (uint64_t)(PyArray_DIMS(pyNumArr)[0]);
	index_length = (uint64_t)(PyArray_DIMS(pyGlobalArr)[0]);
	/* verify consistant shape */
	if (index_length != PyArray_DIMS(pyBlockArr)[0])
	{
		PyErr_SetString(PyExc_RuntimeError, "Differing lengths of global and block arrays\n");
		return(NULL);
	}

	result = digital_rf_write_blocks_hdf5(hdf5_write_data_object, global_arr, block_arr, index_length, data, vector_length);
	if (result)
	{
		PyErr_SetString(PyExc_RuntimeError, "Failed to write data\n");
		return(NULL);
	}

	/* success */
	retObj = Py_BuildValue("i", 1);
	return(retObj);

}


static PyObject * _py_rf_write_hdf5_get_last_file_written(PyObject * self, PyObject * args)
/* _py_rf_write_hdf5_get_last_file_written returns last file written as string
 *
 * Inputs: python list with
 * 	1. PyCObject containing pointer to data structure
 *
 *  Returns python string of full path to last file written
 */
{
	// input arguments
	PyObject * pyCObject;

	// local variables
	Digital_rf_write_object * hdf5_write_data_object;
	PyObject *retObj;
	char * last_file_written;

	// parse input arguments
	if (!PyArg_ParseTuple(args, "O",
			  &pyCObject))
	{
		return(NULL);
	}

	/* get C pointer to Digital_rf_write_object */
	hdf5_write_data_object = (Digital_rf_write_object *)PyCObject_AsVoidPtr(pyCObject);

	last_file_written = digital_rf_get_last_file_written(hdf5_write_data_object);

	/* success */
	retObj = Py_BuildValue("s", last_file_written);
	free(last_file_written);
	return(retObj);

}


static PyObject * _py_rf_write_hdf5_get_last_dir_written(PyObject * self, PyObject * args)
/* _py_rf_write_hdf5_get_last_dir_written returns last directory (full path to dir) written as string
 *
 * Inputs: python list with
 * 	1. PyCObject containing pointer to data structure
 *
 *  Returns python string of full path to last directory written
 */
{
	// input arguments
	PyObject * pyCObject;

	// local variables
	Digital_rf_write_object * hdf5_write_data_object;
	PyObject *retObj;
	char * last_dir_written;

	// parse input arguments
	if (!PyArg_ParseTuple(args, "O",
			  &pyCObject))
	{
		return(NULL);
	}

	/* get C pointer to Digital_rf_write_object */
	hdf5_write_data_object = (Digital_rf_write_object *)PyCObject_AsVoidPtr(pyCObject);

	last_dir_written = digital_rf_get_last_dir_written(hdf5_write_data_object);

	/* success */
	retObj = Py_BuildValue("s", last_dir_written);
	free(last_dir_written);
	return(retObj);

}


static PyObject * _py_rf_write_hdf5_get_last_utc_timestamp(PyObject * self, PyObject * args)
/* _py_rf_write_hdf5_get_last_utc_timestamp returns utc timestamp at time of last write as uint_64
 *
 * Inputs: python list with
 * 	1. PyCObject containing pointer to data structure
 *
 *  Returns python int of utc timestamp at time of last write
 */
{
	// input arguments
	PyObject * pyCObject;

	// local variables
	Digital_rf_write_object * hdf5_write_data_object;
	PyObject *retObj;
	uint64_t last_timestamp;

	// parse input arguments
	if (!PyArg_ParseTuple(args, "O",
			  &pyCObject))
	{
		return(NULL);
	}

	/* get C pointer to Digital_rf_write_object */
	hdf5_write_data_object = (Digital_rf_write_object *)PyCObject_AsVoidPtr(pyCObject);

	last_timestamp = digital_rf_get_last_write_time(hdf5_write_data_object);

	/* success */
	retObj = Py_BuildValue("i", last_timestamp);
	return(retObj);

}



static PyObject * _py_rf_write_hdf5_free(PyObject * self, PyObject * args)
/* _py_rf_write_hdf5_free frees all C references
 *
 * Inputs: python list with
 * 	1. PyCObject containing pointer to data structure
 *
 *  Returns 1 if success, 0 if not
 */
{
	// input arguments
	PyObject * pyCObject;

	// local variables
	Digital_rf_write_object * hdf5_write_data_object;
	PyObject *retObj;

	// parse input arguments
	if (!PyArg_ParseTuple(args, "O",
			  &pyCObject))
	{
		return(NULL);
	}

	/* get C pointer to Digital_rf_write_object */
	hdf5_write_data_object = (Digital_rf_write_object *)PyCObject_AsVoidPtr(pyCObject);

	digital_rf_close_write_hdf5(hdf5_write_data_object);


	/* success */
	retObj = Py_BuildValue("i", 1);
	return(retObj);

}






/********** helper methods ******************************/
hid_t get_hdf5_data_type(char byteorder, char dtype_char, int bytecount)
/* get_hdf5_data_type returns an Hdf5 datatype that corresponds to the arguments
 *
 * Inputs:
 * 	char byteorder - char representing byteorder according to numpy.dtype
 * 		(< little-endian, > big-endian, | not applicable)
 * 	char dtype_char - i int, u - unsigned int, f - float, d - double
 * 	int bytecount - bytecount - ignored if f or d
 *
 * Returns hid_t HDF% datatype if recognized, -1 if not
 *
 */
{
	if (byteorder == '<')
	{
		if (dtype_char == 'f')
			return(H5T_IEEE_F32LE);
		else if (dtype_char == 'd')
			return(H5T_IEEE_F64LE);
		else if (dtype_char == 'i' && bytecount == 2)
			return(H5T_STD_I16LE);
		else if (dtype_char == 'h' && bytecount == 2)
			return(H5T_STD_I16LE);
		else if (dtype_char == 'i' && bytecount == 4)
			return(H5T_STD_I32LE);
		else if (dtype_char == 'i' && bytecount == 8)
			return(H5T_STD_I64LE);
		else if (dtype_char == 'l' && bytecount == 8)
			return(H5T_STD_I64LE);
		else if (dtype_char == 'u' && bytecount == 2)
			return(H5T_STD_U16LE);
		else if (dtype_char == 'u' && bytecount == 4)
			return(H5T_STD_U32LE);
		else if (dtype_char == 'u' && bytecount == 8)
			return(H5T_STD_U64LE);
	}
	else if (byteorder == '>')
	{
		if (dtype_char == 'f')
			return(H5T_IEEE_F32BE);
		else if (dtype_char == 'd')
			return(H5T_IEEE_F64BE);
		else if (dtype_char == 'i' && bytecount == 2)
			return(H5T_STD_I16BE);
		else if (dtype_char == 'h' && bytecount == 2)
			return(H5T_STD_I16BE);
		else if (dtype_char == 'i' && bytecount == 4)
			return(H5T_STD_I32BE);
		else if (dtype_char == 'i' && bytecount == 8)
			return(H5T_STD_I64BE);
		else if (dtype_char == 'l' && bytecount == 8)
			return(H5T_STD_I64BE);
		else if (dtype_char == 'u' && bytecount == 2)
			return(H5T_STD_U16BE);
		else if (dtype_char == 'u' && bytecount == 4)
			return(H5T_STD_U32BE);
		else if (dtype_char == 'u' && bytecount == 8)
			return(H5T_STD_U64BE);
	}
	else if (dtype_char == 'b')
		return(H5T_NATIVE_CHAR);
	else if (dtype_char == 'B')
			return(H5T_NATIVE_UCHAR);
	// error if we got here
	return(-1);
}


static PyObject * _py_rf_write_hdf5_get_unix_time(PyObject * self, PyObject * args)
/* _py_rf_write_hdf5_get_unix_time returns a tuple of (year,month,day,hour,minute,second,picosecond)
 * given an input unix_sample_index and sample_rate
 *
 * Inputs: python list with
 * 	1. unix_sample_index - python int representing number of samples at given sample rate since UT midnight 1970-01-01
 * 	2. sample_rate_numerator - python int sample rate numerator in Hz
 * 	3. sample_rate_denominator - python int sample rate denominator in Hz
 *
 *  Returns tuple with (year,month,day,hour,minute,second,picosecond) if success, NULL pointer if not
 */
{
	// input arguments
	unsigned long long unix_sample_index = 0;
	uint64_t sample_rate_numerator = 0;
	uint64_t sample_rate_denominator = 0;

	// local variables
	PyObject *retObj;
	int year, month, day;
	int hour, minute, second;
	uint64_t picosecond;
	int result;
	long double sample_rate;

	// parse input arguments
	if (!PyArg_ParseTuple(args, "KKK",
			  &unix_sample_index,
			  &sample_rate_numerator,
			  &sample_rate_denominator))
	{
		return NULL;
	}

	sample_rate =  (long double)sample_rate_numerator / (long double)sample_rate_denominator;

	// call underlying method
	result = digital_rf_get_unix_time(unix_sample_index, sample_rate, &year, &month, &day,
                                      &hour, &minute, &second, &picosecond);
	if (result != 0)
		return(NULL);

	// create needed object
	retObj = Py_BuildValue("iiiiiiK",  year, month, day,
                           hour, minute, second, picosecond);

    //return tuple;
    return(retObj);

}



/********** Initialization code for module ******************************/

static PyMethodDef _py_rf_write_hdf5Methods[] =
{
	  {"init",           	           _py_rf_write_hdf5_init,          		METH_VARARGS},
	  {"rf_write",           	       _py_rf_write_hdf5_rf_write,          	METH_VARARGS},
	  {"rf_block_write",           	   _py_rf_write_hdf5_rf_block_write,    	METH_VARARGS},
	  {"free",           	           _py_rf_write_hdf5_free,              	METH_VARARGS},
	  {"get_last_file_written",        _py_rf_write_hdf5_get_last_file_written, METH_VARARGS},
	  {"get_last_dir_written",         _py_rf_write_hdf5_get_last_dir_written,  METH_VARARGS},
	  {"get_last_utc_timestamp",       _py_rf_write_hdf5_get_last_utc_timestamp,METH_VARARGS},
	  {"get_unix_time",           	   _py_rf_write_hdf5_get_unix_time,     	METH_VARARGS},
      {NULL,      NULL}        /* Sentinel */
};


void init_py_rf_write_hdf5()
{
    PyImport_AddModule("_py_rf_write_hdf5");
    Py_InitModule("_py_rf_write_hdf5", _py_rf_write_hdf5Methods);
	// needed to initialize numpy C api and not have segfaults
	import_array();
}
