/*
 * ieee1284 - Python bindings for libieee1284
 * Copyright (C) 2004  Tim Waugh <twaugh@redhat.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 */

#include <Python.h>
#include <structmember.h>
#include "ieee1284.h"

static PyObject *pyieee1284_error;

typedef struct {
	PyObject_HEAD
	struct parport *port;
} ParportObject;

static int
Parport_init (ParportObject *self, PyObject *args, PyObject *kwds)
{
	PyErr_SetString (PyExc_TypeError,
			 "You cannot create this; use find_ports instead.");
	return -1;
}

static void
Parport_dealloc (ParportObject *self)
{
	if (self->port)
		ieee1284_unref (self->port);

	self->ob_type->tp_free ((PyObject *) self);
}

static PyObject *
Parport_getname (ParportObject *self, void *closure)
{
	return PyString_FromString (self->port->name);
}

static PyObject *
Parport_getbase_addr (ParportObject *self, void *closure)
{
	return PyInt_FromLong (self->port->base_addr);
}

static PyObject *
Parport_gethibase_addr (ParportObject *self, void *closure)
{
	return PyInt_FromLong (self->port->hibase_addr);
}

static PyObject *
Parport_getfilename (ParportObject *self, void *closure)
{
	if (self->port->filename)
		return PyString_FromString (self->port->filename);

	Py_INCREF (Py_None);
	return Py_None;
}

static PyGetSetDef Parport_getseters[] = {
	{ "name",
	  (getter) Parport_getname, NULL,
	  NULL },
	{ "base_addr",
	  (getter) Parport_getbase_addr, NULL,
	  NULL },
	{ "hibase_addr",
	  (getter) Parport_gethibase_addr, NULL,
	  NULL },
	{ "filename",
	  (getter) Parport_getfilename, NULL,
	  NULL },
	{ NULL }
};

/***/

static void
handle_error (int err)
{
	switch (err) {
	case E1284_OK:
		return;
	case E1284_NOMEM:
		PyErr_NoMemory ();
		return;
	case E1284_SYS:
		PyErr_SetFromErrno (PyExc_OSError);
		return;
	case E1284_NOTIMPL:
		PyErr_SetString (pyieee1284_error,
				 "Not implemented in libieee1284");
		return;
	case E1284_NOTAVAIL:
		PyErr_SetString (pyieee1284_error,
				 "Not available on this system");
		return;
	case E1284_TIMEDOUT:
		PyErr_SetString (pyieee1284_error,
				 "Operation timed out");
		return;
	case E1284_REJECTED:
		PyErr_SetString (pyieee1284_error,
				 "IEEE 1284 negotiation rejected");
		return;
	case E1284_NEGFAILED:
		PyErr_SetString (pyieee1284_error,
				 "Negotiation went wrong");
		return;
	case E1284_INIT:
		PyErr_SetString (pyieee1284_error,
				 "Error initialising port");
		return;
	case E1284_NOID:
		PyErr_SetString (pyieee1284_error,
				 "No IEEE 1284 ID available");
		return;
	case E1284_INVALIDPORT:
		PyErr_SetString (pyieee1284_error,
				 "Port is invalid (perhaps not opened?)");
		return;
	}

	PyErr_SetString (pyieee1284_error, "Unknown error");
}

static PyObject *
Parport_get_deviceid (ParportObject *self, PyObject *args)
{
	int daisy = -1;
	int flags = 0;
	char buffer[2000];
	ssize_t r;
	if (!PyArg_ParseTuple (args, "|ii", &daisy, &flags))
		return NULL;

	r = ieee1284_get_deviceid (self->port, daisy, flags, buffer,
				   sizeof (buffer));
	if (r < 0) {
		handle_error (r);
		return NULL;
	}

	return PyString_FromStringAndSize (buffer, r);
}

static PyObject *
Parport_open (ParportObject *self, PyObject *args)
{
	int flags = 0;
	int capabilities = 0;
	int r;
	if (!PyArg_ParseTuple (args, "|i", &flags))
		return NULL;

	r = ieee1284_open (self->port, flags, &capabilities);
	if (r < 0) {
		handle_error (r);
		return NULL;
	}

	return PyInt_FromLong (capabilities);
}

static PyObject *
Parport_close (ParportObject *self)
{
	int r = ieee1284_close (self->port);
	if (r < 0) {
		handle_error (r);
		return NULL;
	}

	Py_INCREF (Py_None);
	return Py_None;
}

static PyObject *
Parport_claim (ParportObject *self)
{
	int r = ieee1284_claim (self->port);
	if (r < 0) {
		handle_error (r);
		return NULL;
	}

	Py_INCREF (Py_None);
	return Py_None;
}

static PyObject *
Parport_release (ParportObject *self)
{
	ieee1284_release (self->port);
	Py_INCREF (Py_None);
	return Py_None;
}

static PyObject *
Parport_read_data (ParportObject *self)
{
	unsigned char b[2];
	int r = ieee1284_read_data (self->port);
	if (r < 0) {
		handle_error (r);
		return NULL;
	}

	return PyInt_FromLong (r);
}

static PyObject *
Parport_write_data (ParportObject *self, PyObject *args)
{
	long byte;
	if (!PyArg_ParseTuple (args, "i", &byte))
		return NULL;

	ieee1284_write_data (self->port, (unsigned char) byte);
	Py_INCREF (Py_None);
	return Py_None;
}

static PyObject *
Parport_data_dir (ParportObject *self, PyObject *args)
{
	int r;
	long reverse;
	if (!PyArg_ParseTuple (args, "i", &reverse))
		return NULL;

	r = ieee1284_data_dir (self->port, reverse);
	if (r < 0) {
		handle_error (r);
		return NULL;
	}

	Py_INCREF (Py_None);
	return Py_None;
}

static PyObject *
Parport_read_status (ParportObject *self)
{
	unsigned char b[2];
	int r = ieee1284_read_status (self->port);
	if (r < 0) {
		handle_error (r);
		return NULL;
	}

	return PyInt_FromLong (r);
}

static PyObject *
Parport_wait_status (ParportObject *self, PyObject *args)
{
	int mask, val;
	float f;
	struct timeval timeout;
	int r;
	if (!PyArg_ParseTuple (args, "iif", &mask, &val, &f))
		return NULL;

	timeout.tv_sec = (int) f;
	timeout.tv_usec = (int) ((f - timeout.tv_sec) * 1000000);
	r = ieee1284_wait_status (self->port, mask, val, &timeout);
	if (r < 0) {
		handle_error (r);
		return NULL;
	}

	Py_INCREF (Py_None);
	return Py_None;
}

static PyObject *
Parport_read_control (ParportObject *self)
{
	unsigned char b[2];
	int r = ieee1284_read_control (self->port);
	if (r < 0) {
		handle_error (r);
		return NULL;
	}

	return PyInt_FromLong (r);
}

static PyObject *
Parport_write_control (ParportObject *self, PyObject *args)
{
	long byte;
	if (!PyArg_ParseTuple (args, "i", &byte))
		return NULL;

	ieee1284_write_control (self->port, (unsigned char) byte);
	Py_INCREF (Py_None);
	return Py_None;
}

static PyObject *
Parport_frob_control (ParportObject *self, PyObject *args)
{
	long mask, val;
	if (!PyArg_ParseTuple (args, "ii", &mask, &val))
		return NULL;

	ieee1284_frob_control (self->port,
			       (unsigned char) mask,
			       (unsigned char) val);
	Py_INCREF (Py_None);
	return Py_None;
}

static PyObject *
Parport_negotiate (ParportObject *self, PyObject *args)
{
	long mode;
	int r;
	if (!PyArg_ParseTuple (args, "i", &mode))
		return NULL;

	r = ieee1284_negotiate (self->port, mode);
	if (r < 0) {
		handle_error (r);
		return NULL;
	}

	Py_INCREF (Py_None);
	return Py_None;
}

static PyObject *
Parport_terminate (ParportObject *self)
{
	ieee1284_terminate (self->port);
	Py_INCREF (Py_None);
	return Py_None;
}

static PyObject *
Parport_ecp_fwd_to_rev (ParportObject *self)
{
	int r = ieee1284_ecp_fwd_to_rev (self->port);
	if (r < 0) {
		handle_error (r);
		return NULL;
	}

	Py_INCREF (Py_None);
	return Py_None;
}

static PyObject *
Parport_ecp_rev_to_fwd (ParportObject *self)
{
	int r = ieee1284_ecp_rev_to_fwd (self->port);
	if (r < 0) {
		handle_error (r);
		return NULL;
	}

	Py_INCREF (Py_None);
	return Py_None;
}

static PyObject *
Parport_set_timeout (ParportObject *self, PyObject *args)
{
	float f;
	struct timeval timeout, *oldto;
	if (!PyArg_ParseTuple (args, "f", &f))
		return NULL;

	timeout.tv_sec = (int) f;
	timeout.tv_usec = (int) ((f - timeout.tv_sec) * 1000000);
	oldto = ieee1284_set_timeout (self->port, &timeout);
	f = oldto->tv_sec + oldto->tv_usec * 1000000;
	return PyFloat_FromDouble (f);
}

#define READ_FUNCTION(x)					\
static PyObject *						\
Parport_##x (ParportObject *self, PyObject *args)		\
{								\
	int flags = 0;						\
	size_t len;						\
	ssize_t got;						\
	char *buffer;						\
	PyObject *ret;						\
								\
	if (!PyArg_ParseTuple (args, "i|i", &len, &flags))	\
		return NULL;					\
								\
	buffer = malloc (len);					\
	got = ieee1284_##x (self->port, flags, buffer, len);	\
	if (got < 0) {						\
		handle_error (got);				\
		free (buffer);					\
		return NULL;					\
	}							\
								\
	ret = PyString_FromStringAndSize (buffer, got);		\
	free (buffer);						\
	return ret;						\
}

#define READ_METHOD(x)						\
	{ #x, (PyCFunction) Parport_##x, METH_VARARGS,		\
	  #x "(length[,flags]) -> string\n"			\
	  "Reads data using specified transfer method." },

#define WRITE_FUNCTION(x)						\
static PyObject *							\
Parport_##x (ParportObject *self, PyObject *args)			\
{									\
	int flags = 0;							\
	int len;							\
	char *buffer;							\
	ssize_t wrote;							\
	PyObject *ret;							\
									\
	if (!PyArg_ParseTuple (args, "s#|i", &buffer, &len, &flags))	\
		return NULL;						\
									\
	wrote = ieee1284_##x (self->port, flags, buffer, len);		\
	if (write < 0) {						\
		handle_error (wrote);					\
		return NULL;						\
	}								\
									\
	return PyInt_FromLong (wrote);					\
}

#define WRITE_METHOD(x)						\
	{ #x, (PyCFunction) Parport_##x, METH_VARARGS,		\
	  #x "(string[,flags]) -> int\n"			\
	  "Writes data using specified transfer method.\n"	\
	  "Returns the number of bytes written." },

READ_FUNCTION(nibble_read)
READ_FUNCTION(byte_read)
READ_FUNCTION(epp_read_data)
READ_FUNCTION(epp_read_addr)
READ_FUNCTION(ecp_read_data)
READ_FUNCTION(ecp_read_addr)
WRITE_FUNCTION(compat_write)
WRITE_FUNCTION(epp_write_data)
WRITE_FUNCTION(epp_write_addr)
WRITE_FUNCTION(ecp_write_data)
WRITE_FUNCTION(ecp_write_addr)

PyMethodDef Parport_methods[] = {
	{ "get_deviceid", (PyCFunction) Parport_get_deviceid, METH_VARARGS,
	  "get_deviceid(daisy, flags) -> string\n"
	  "Returns an IEEE 1284 Device ID of the device." },
	{ "open", (PyCFunction) Parport_open, METH_VARARGS,
	  "open(flags) -> int\n"
	  "Opens a port and returns a capabilities bitmask." },
	{ "close", (PyCFunction) Parport_close, METH_NOARGS,
	  "close() -> None\n"
	  "Closes a port." },
	{ "claim", (PyCFunction) Parport_claim, METH_NOARGS,
	  "claim() -> None\n"
	  "Claims a port." },
	{ "release", (PyCFunction) Parport_release, METH_NOARGS,
	  "release() -> None\n"
	  "Releases a port." },
	{ "read_data", (PyCFunction) Parport_read_data, METH_NOARGS,
	  "read_data() -> int\n"
	  "Reads the byte on the data lines." },
	{ "write_data", (PyCFunction) Parport_write_data, METH_VARARGS,
	  "write_data(byte) -> None\n"
	  "Writes the byte to the data lines." },
	{ "data_dir", (PyCFunction) Parport_data_dir, METH_VARARGS,
	  "data_dir(reverse) -> None\n"
	  "Sets the direction of the data lines." },
	{ "read_status", (PyCFunction) Parport_read_status, METH_NOARGS,
	  "read_status() -> int\n"
	  "Reads the values on the status lines." },
	{ "wait_status", (PyCFunction) Parport_wait_status, METH_VARARGS,
	  "wait_status(mask,val,timeout) -> None\n"
	  "Waits for timeout (in seconds), until the status lines are as\n"
	  "specified." },
	{ "read_control", (PyCFunction) Parport_read_control, METH_NOARGS,
	  "read_control() -> int\n"
	  "Reads the values on the control lines." },
	{ "write_control", (PyCFunction) Parport_write_control, METH_VARARGS,
	  "write_control(byte) -> None\n"
	  "Writes the values to the data lines." },
	{ "frob_control", (PyCFunction) Parport_frob_control, METH_VARARGS,
	  "frob_control(mask,val) -> None\n"
	  "Frobnicates the values on the data lines." },
	{ "negotiate", (PyCFunction) Parport_negotiate, METH_VARARGS,
	  "negotiate(mode) -> None\n"
	  "Negotiates to the desired IEEE 1284 transer mode." },
	{ "terminate", (PyCFunction) Parport_terminate, METH_NOARGS,
	  "terminate() -> None\n"
	  "Terminates the current IEEE 1284 transer mode." },
	{ "ecp_fwd_to_rev", (PyCFunction) Parport_ecp_fwd_to_rev, METH_NOARGS,
	  "ecp_fwd_to_rev() -> None\n"
	  "Switches to ECP reverse mode." },
	{ "ecp_rev_to_fwd", (PyCFunction) Parport_ecp_rev_to_fwd, METH_NOARGS,
	  "ecp_rev_to_fwd() -> None\n"
	  "Switches to ECP forward mode." },
	{ "set_timeout", (PyCFunction) Parport_set_timeout, METH_VARARGS,
	  "set_timeout(float) -> float\n"
	  "Sets transfer timeout, in seconds.  Returns old timeout value." },
READ_METHOD(nibble_read)
READ_METHOD(byte_read)
READ_METHOD(epp_read_data)
READ_METHOD(epp_read_addr)
READ_METHOD(ecp_read_data)
READ_METHOD(ecp_read_addr)
WRITE_METHOD(compat_write)
WRITE_METHOD(epp_write_data)
WRITE_METHOD(epp_write_addr)
WRITE_METHOD(ecp_write_data)
WRITE_METHOD(ecp_write_addr)
	{ NULL }
};

/***/

static PyTypeObject ParportType = {
	PyObject_HEAD_INIT(NULL)
	0,					/* ob_size */
	"ieee1284.Parport",			/* tp_name */
	sizeof (ParportObject),			/* tp_basicsize */
	0,					/* tp_itemsize */
	(destructor)Parport_dealloc,		/* tp_dealloc */
	0,					/* tp_print */
	0,					/* tp_getattr */
	0,					/* tp_setattr */
	0,					/* tp_compare */
	0,					/* tp_repr */
	0,					/* tp_as_number */
	0,					/* tp_as_sequence */
	0,					/* tp_as_mapping */
	0,					/* tp_hash */
	0,					/* tp_call */
	0,					/* tp_str */
	0,					/* tp_getattro */
	0,					/* tp_setattro */
	0,					/* tp_as_buffer */
	Py_TPFLAGS_DEFAULT,			/* tp_flags */
	"parallel port object",			/* tp_doc */
};

static PyObject *
parport_object (struct parport *port)
{
	ParportObject *ret;
	ret = (ParportObject *) ParportType.tp_new (&ParportType,
						    Py_None,
						    Py_None);
	ret->port = port;
	ieee1284_ref (port);
	return (PyObject *) ret;
}

static PyObject *
pyieee1284_find_ports (PyObject *self, PyObject *args)
{
	struct parport_list pl;
	int flags = 0;
	int err;
	int i;
	PyObject *ret;
	if (!PyArg_ParseTuple (args, "|i", &flags))
	    return NULL;

	err = ieee1284_find_ports (&pl, flags);

	if (err) {
		handle_error (err);
		return NULL;
	}


	ret = PyDict_New ();
	for (i = 0; i <  pl.portc; i++) {
		struct parport *port = pl.portv[i];
		char *name = strdup (port->name);
		PyObject *portobj = parport_object (port);
		PyDict_SetItemString (ret, name, portobj);
		free (name);
		Py_DECREF (portobj);
	}
	ieee1284_free_ports (&pl);
	return ret;
}

static PyMethodDef Ieee1284Methods[] = {
	{"find_ports", pyieee1284_find_ports, METH_VARARGS,
	 "find_ports(flags) -> dict\n"
	 "Returns a dict, indexed by name, of parallel ports."},
	{NULL, NULL, 0, NULL}
};

#ifndef PyMODINIT_FUNC
#define PyMODINIT_FUNC void
#endif
PyMODINIT_FUNC
initieee1284 (void)
{
	PyObject *m = Py_InitModule ("ieee1284", Ieee1284Methods);
	PyObject *d = PyModule_GetDict (m);
	PyObject *c;

	ParportType.tp_new = PyType_GenericNew;
	ParportType.tp_init = (initproc) Parport_init;
	ParportType.tp_getset = Parport_getseters;
	ParportType.tp_methods = Parport_methods;
	if (PyType_Ready (&ParportType) < 0)
		return;

	Py_INCREF (&ParportType);
	PyModule_AddObject (m, "Parport", (PyObject *) &ParportType);

	pyieee1284_error = PyErr_NewException("ieee1284.error", NULL, NULL);
	Py_INCREF (pyieee1284_error);
	PyModule_AddObject (m, "error", pyieee1284_error);

#define CONSTANT(x)					\
        do {						\
		c = PyInt_FromLong (x);			\
		PyDict_SetItemString (d, #x, c);	\
		Py_DECREF (c);				\
	} while (0)

	CONSTANT (F1284_FRESH);
	CONSTANT (F1284_EXCL);
	CONSTANT (CAP1284_RAW);
	CONSTANT (CAP1284_NIBBLE);
	CONSTANT (CAP1284_BYTE);
	CONSTANT (CAP1284_COMPAT);
	CONSTANT (CAP1284_BECP);
	CONSTANT (CAP1284_ECP);
	CONSTANT (CAP1284_ECPRLE);
	CONSTANT (CAP1284_ECPSWE);
	CONSTANT (CAP1284_EPP);
	CONSTANT (CAP1284_EPPSL);
	CONSTANT (CAP1284_EPPSWE);
	CONSTANT (CAP1284_IRQ);
	CONSTANT (CAP1284_DMA);
	CONSTANT (S1284_NFAULT);
	CONSTANT (S1284_SELECT);
	CONSTANT (S1284_PERROR);
	CONSTANT (S1284_NACK);
	CONSTANT (S1284_BUSY);
	CONSTANT (S1284_INVERTED);
	CONSTANT (C1284_NSTROBE);
	CONSTANT (C1284_NAUTOFD);
	CONSTANT (C1284_NINIT);
	CONSTANT (C1284_NSELECTIN);
	CONSTANT (C1284_INVERTED);
	CONSTANT (M1284_NIBBLE);
	CONSTANT (M1284_BYTE);
	CONSTANT (M1284_COMPAT);
	CONSTANT (M1284_BECP);
	CONSTANT (M1284_ECP);
	CONSTANT (M1284_ECPRLE);
	CONSTANT (M1284_ECPSWE);
	CONSTANT (M1284_EPP);
	CONSTANT (M1284_EPPSL);
	CONSTANT (M1284_EPPSWE);
	CONSTANT (M1284_FLAG_DEVICEID);
	CONSTANT (M1284_FLAG_EXT_LINK);
	CONSTANT (F1284_NONBLOCK);
	CONSTANT (F1284_SWE);
	CONSTANT (F1284_RLE);
	CONSTANT (F1284_FASTEPP);
}
