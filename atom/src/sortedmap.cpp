/*-----------------------------------------------------------------------------
| Copyright (c) 2013, Nucleic Development Team.
|
| Distributed under the terms of the Modified BSD License.
|
| The full license is in the file COPYING.txt, distributed with this software.
|----------------------------------------------------------------------------*/
#include <algorithm>
#include <vector>
#include <iostream>
#include <sstream>
#include "pythonhelpers.h"


using namespace PythonHelpers;


class MapItem
{

public:

    MapItem() {}

    MapItem( PyObject* key, PyObject* value ) :
        m_key( newref( key ) ), m_value( newref( value ) ) {}

    MapItem( PyObjectPtr& key, PyObjectPtr& value ) :
        m_key( key ), m_value( value ) {}

    MapItem( PyObjectPtr& key, PyObject* value ) :
        m_key( key ), m_value( newref( value ) ) {}

    MapItem( PyObject* key, PyObjectPtr& value ) :
        m_key( newref( key ) ), m_value( value ) {}

    ~MapItem() {}

    bool key_lessthan( PyObject* key )
    {
        if( m_key == key )
            return false;
        return m_key.richcompare( key, Py_LT );
    }

    bool key_equals( PyObject* key )
    {
        return m_key == key || m_key.richcompare( key, Py_EQ );
    }

    void update( PyObject* value )
    {
        m_value = newref( value );
    }

    PyObject* key()
    {
        return m_key.get();
    }

    PyObject* value()
    {
        return m_value.get();
    }

    static struct _CompLess
    {
        bool operator()( MapItem& item, PyObject* key )
        {
            return item.key_lessthan( key );
        }
    } CompLess;

private:

    PyObjectPtr m_key;
    PyObjectPtr m_value;
};


typedef std::vector<MapItem> sortedmap_t;


struct SortedMap
{
    PyObject_HEAD
    sortedmap_t* sortedmap;

    PyObject* getitem( PyObject* key, PyObject* default_value = 0 )
    {
        sortedmap_t::iterator it = std::lower_bound(
            sortedmap->begin(), sortedmap->end(), key, MapItem::CompLess
        );
        if( it == sortedmap->end() )
        {
            if( default_value )
                return newref( default_value );
            return lookup_fail( key );
        }
        if( it->key_equals( key ) )
            return newref( it->value() );
        if( default_value )
            return newref( default_value );
        return lookup_fail( key );
    }

    int setitem( PyObject* key, PyObject* value )
    {
        sortedmap_t::iterator it = std::lower_bound(
            sortedmap->begin(), sortedmap->end(), key, MapItem::CompLess
        );
        if( it == sortedmap->end() )
            sortedmap->push_back( MapItem( key, value ) );
        else if( it->key_equals( key ) )
            it->update( value );
        else
            sortedmap->insert( it, MapItem( key, value ) );
        return 0;
    }

    int delitem( PyObject* key )
    {
        sortedmap_t::iterator it = std::lower_bound(
            sortedmap->begin(), sortedmap->end(), key, MapItem::CompLess
        );
        if( it == sortedmap->end() )
        {
            lookup_fail( key );
            return -1;
        }
        if( it->key_equals( key ) )
        {
            sortedmap->erase( it );
            return 0;
        }
        lookup_fail( key );
        return -1;
    }

    bool contains( PyObject* key )
    {
        sortedmap_t::iterator it = std::lower_bound(
            sortedmap->begin(), sortedmap->end(), key, MapItem::CompLess
        );
        if( it == sortedmap->end() )
            return false;
        return it->key_equals( key );
    }

    PyObject* pop( PyObject* key, PyObject* default_value=0 )
    {
        sortedmap_t::iterator it = std::lower_bound(
            sortedmap->begin(), sortedmap->end(), key, MapItem::CompLess
        );
        if( it == sortedmap->end() )
        {
            if( default_value )
                return newref( default_value );
            return lookup_fail( key );
        }
        if( it->key_equals( key ) )
        {
            PyObject* res = newref( it->value() );
            sortedmap->erase( it );
            return res;
        }
        if( default_value )
            return newref( default_value );
        return lookup_fail( key );
    }

    PyObject* keys()
    {
        PyObject* pylist = PyList_New( sortedmap->size() );
        if( !pylist )
            return 0;
        Py_ssize_t listidx = 0;
        sortedmap_t::iterator it;
        sortedmap_t::iterator end_it = sortedmap->end();
        for( it = sortedmap->begin(); it != end_it; ++it )
        {
            PyList_SET_ITEM( pylist, listidx, newref( it->key() ) );
            ++listidx;
        }
        return pylist;
    }

    PyObject* values()
    {
        PyObject* pylist = PyList_New( sortedmap->size() );
        if( !pylist )
            return 0;
        Py_ssize_t listidx = 0;
        sortedmap_t::iterator it;
        sortedmap_t::iterator end_it = sortedmap->end();
        for( it = sortedmap->begin(); it != end_it; ++it )
        {
            PyList_SET_ITEM( pylist, listidx, newref( it->value() ) );
            ++listidx;
        }
        return pylist;
    }

    PyObject* items()
    {
        PyObject* pylist = PyList_New( sortedmap->size() );
        if( !pylist )
            return 0;
        Py_ssize_t listidx = 0;
        sortedmap_t::iterator it;
        sortedmap_t::iterator end_it = sortedmap->end();
        for( it = sortedmap->begin(); it != end_it; ++it )
        {
            PyObject* pytuple = PyTuple_New( 2 );
            if( !pytuple )
                return 0;
            PyTuple_SET_ITEM( pytuple, 0, newref( it->key() ) );
            PyTuple_SET_ITEM( pytuple, 1, newref( it->value() ) );
            PyList_SET_ITEM( pylist, listidx, pytuple );
            ++listidx;
        }
        return pylist;
    }

    static PyObject* lookup_fail( PyObject* key )
    {
        PyObjectPtr pystr( PyObject_Str( key ) );
        if( !pystr )
            return 0;
        PyObjectPtr pytuple( PyTuple_Pack(1, key ) );
        if (!pytuple)
            return 0;
        PyErr_SetObject(PyExc_KeyError, pytuple.get());
        return 0;
    }
};


static PyObject*
SortedMap_new( PyTypeObject* type, PyObject* args, PyObject* kwargs )
{
    PyObjectPtr mapptr( PyType_GenericNew( type, args, kwargs ) );
    if( !mapptr )
        return 0;
    SortedMap* self = reinterpret_cast<SortedMap*>( mapptr.get() );
    self->sortedmap = new sortedmap_t();
    return mapptr.release();
}


static void
SortedMap_clear( SortedMap* self )
{
    // Clearing the vector may cause arbitrary side effects on item
    // decref, including calls into methods which mutate the vector.
    // To avoid segfaults, first make the vector empty, then let the
    // destructors run for the old items.
    sortedmap_t empty;
    self->sortedmap->swap( empty );
}


static int
SortedMap_traverse( SortedMap* self, visitproc visit, void* arg )
{
    sortedmap_t::iterator it;
    sortedmap_t::iterator end_it = self->sortedmap->end();
    for( it = self->sortedmap->begin(); it != end_it; ++it )
    {
        Py_VISIT( it->key() );
        Py_VISIT( it->value() );
    }
    return 0;
}


static void
SortedMap_dealloc( SortedMap* self )
{
    SortedMap_clear( self );
    delete self->sortedmap;
    self->sortedmap = 0;
    self->ob_type->tp_free( reinterpret_cast<PyObject*>( self ) );
}


static Py_ssize_t
SortedMap_length( SortedMap* self )
{
    return static_cast<Py_ssize_t>( self->sortedmap->size() );
}


static PyObject*
SortedMap_subscript( SortedMap* self, PyObject* key )
{
    return self->getitem( key );
}


static int
SortedMap_ass_subscript( SortedMap* self, PyObject* key, PyObject* value )
{
    if( !value )
        return self->delitem( key );
    return self->setitem( key, value );
}


static PyMappingMethods
SortedMap_as_mapping = {
    ( lenfunc )SortedMap_length,              /*mp_length*/
    ( binaryfunc )SortedMap_subscript,        /*mp_subscript*/
    ( objobjargproc )SortedMap_ass_subscript, /*mp_ass_subscript*/
};


static int
SortedMap_contains( SortedMap* self, PyObject* key )
{
    return self->contains( key );
}


static PySequenceMethods
SortedMap_as_sequence = {
    0,                          /* sq_length */
    0,                          /* sq_concat */
    0,                          /* sq_repeat */
    0,                          /* sq_item */
    0,                          /* sq_slice */
    0,                          /* sq_ass_item */
    0,                          /* sq_ass_slice */
    ( objobjproc )SortedMap_contains, /* sq_contains */
    0,                          /* sq_inplace_concat */
    0,                          /* sq_inplace_repeat */
};


static PyObject*
SortedMap_get( SortedMap* self, PyObject* args )
{
    Py_ssize_t nargs = PyTuple_GET_SIZE( args );
    if( nargs == 1 )
        return self->getitem( PyTuple_GET_ITEM( args, 0 ), Py_None );
    if( nargs == 2 )
        return self->getitem( PyTuple_GET_ITEM( args, 0 ), PyTuple_GET_ITEM( args, 1 ) );
    std::ostringstream ostr;
    if( nargs > 2 )
        ostr << "get() expected at most 2 arguments, got " << nargs;
    else
        ostr << "get() expected at least 1 argument, got " << nargs;
    return py_type_fail( ostr.str().c_str() );
}


static PyObject*
SortedMap_pop( SortedMap* self, PyObject* args )
{
    Py_ssize_t nargs = PyTuple_GET_SIZE( args );
    if( nargs == 1 )
        return self->pop( PyTuple_GET_ITEM( args, 0 ) );
    if( nargs == 2 )
        return self->getitem( PyTuple_GET_ITEM( args, 0 ), PyTuple_GET_ITEM( args, 1 ) );
    std::ostringstream ostr;
    if( nargs > 2 )
        ostr << "pop() expected at most 2 arguments, got " << nargs;
    else
        ostr << "pop() expected at least 1 argument, got " << nargs;
    return py_type_fail( ostr.str().c_str() );
}


static PyObject*
SortedMap_clearmethod( SortedMap* self )
{
    // Clearing the vector may cause arbitrary side effects on item
    // decref, including calls into methods which mutate the vector.
    // To avoid segfaults, first make the vector empty, then let the
    // destructors run for the old items.
    sortedmap_t empty;
    self->sortedmap->swap( empty );
    Py_RETURN_NONE;
}


static PyObject*
SortedMap_keys( SortedMap* self )
{
    return self->keys();
}


static PyObject*
SortedMap_values( SortedMap* self )
{
    return self->values();
}


static PyObject*
SortedMap_items( SortedMap* self )
{
    return self->items();
}


static PyObject*
SortedMap_repr( SortedMap* self )
{
    std::ostringstream ostr;
    ostr << "sortedmap({";
    sortedmap_t::iterator it;
    sortedmap_t::iterator end_it = self->sortedmap->end();
    for( it = self->sortedmap->begin(); it != end_it; ++it )
    {
        PyObjectPtr keystr( PyObject_Str( it->key() ) );
        if( !keystr )
            return 0;
        PyObjectPtr valstr( PyObject_Str( it->value() ) );
        if( !valstr )
            return 0;
        ostr << PyString_AsString( keystr.get() ) << ": ";
        ostr << PyString_AsString( valstr.get() ) << ", ";
    }
    if( self->sortedmap->size() > 0 )
        ostr.seekp( -2, std::ios_base::cur );
    ostr << "})";
    return PyString_FromString( ostr.str().c_str() );
}


static PyObject*
SortedMap_contains_bool( SortedMap* self, PyObject* key )
{
    if( self->contains( key ) )
        Py_RETURN_TRUE;
    Py_RETURN_FALSE;
}


static PyObject*
SortedMap_sizeof( SortedMap* self, PyObject* args )
{
    Py_ssize_t size = self->ob_type->tp_basicsize;
    size += sizeof( sortedmap_t );
    size += sizeof( MapItem ) * self->sortedmap->capacity();
    return PyInt_FromSsize_t( size );
}


static PyMethodDef
SortedMap_methods[] = {
    { "get", ( PyCFunction )SortedMap_get, METH_VARARGS,
      "" },
    { "pop", ( PyCFunction )SortedMap_pop, METH_VARARGS,
      "" },
    { "clear", ( PyCFunction)SortedMap_clearmethod, METH_NOARGS,
      "" },
    { "keys", ( PyCFunction )SortedMap_keys, METH_NOARGS,
      "" },
    { "values", ( PyCFunction )SortedMap_values, METH_NOARGS,
      "" },
    { "items", ( PyCFunction )SortedMap_items, METH_NOARGS,
      "" },
    { "__contains__", ( PyCFunction )SortedMap_contains_bool, METH_O | METH_COEXIST,
      "" },
    { "__getitem__", ( PyCFunction )SortedMap_subscript, METH_O | METH_COEXIST,
      "" },
    { "__sizeof__", ( PyCFunction )SortedMap_sizeof, METH_NOARGS,
      "__sizeof__() -> size of object in memory, in bytes" },
    { 0 } // sentinel
};


PyTypeObject SortedMap_Type = {
    PyObject_HEAD_INIT( 0 )
    0,                                      /* ob_size */
    "sortedmap.sortedmap",                  /* tp_name */
    sizeof( SortedMap ),                    /* tp_basicsize */
    0,                                      /* tp_itemsize */
    (destructor)SortedMap_dealloc,          /* tp_dealloc */
    (printfunc)0,                           /* tp_print */
    (getattrfunc)0,                         /* tp_getattr */
    (setattrfunc)0,                         /* tp_setattr */
    (cmpfunc)0,                             /* tp_compare */
    (reprfunc)SortedMap_repr,               /* tp_repr */
    (PyNumberMethods*)0,                    /* tp_as_number */
    (PySequenceMethods*)&SortedMap_as_sequence, /* tp_as_sequence */
    (PyMappingMethods*)&SortedMap_as_mapping,   /* tp_as_mapping */
    (hashfunc)0,                            /* tp_hash */
    (ternaryfunc)0,                         /* tp_call */
    (reprfunc)0,                            /* tp_str */
    (getattrofunc)0,                        /* tp_getattro */
    (setattrofunc)0,                        /* tp_setattro */
    (PyBufferProcs*)0,                      /* tp_as_buffer */
    Py_TPFLAGS_DEFAULT | Py_TPFLAGS_HAVE_GC, /* tp_flags */
    0,                                      /* Documentation string */
    (traverseproc)SortedMap_traverse,       /* tp_traverse */
    (inquiry)SortedMap_clear,               /* tp_clear */
    (richcmpfunc)0,                         /* tp_richcompare */
    0,                                      /* tp_weaklistoffset */
    (getiterfunc)0,                         /* tp_iter */
    (iternextfunc)0,                        /* tp_iternext */
    (struct PyMethodDef*)SortedMap_methods, /* tp_methods */
    (struct PyMemberDef*)0,                 /* tp_members */
    0,                                      /* tp_getset */
    0,                                      /* tp_base */
    0,                                      /* tp_dict */
    (descrgetfunc)0,                        /* tp_descr_get */
    (descrsetfunc)0,                        /* tp_descr_set */
    0,                                      /* tp_dictoffset */
    (initproc)0,                            /* tp_init */
    (allocfunc)PyType_GenericAlloc,         /* tp_alloc */
    (newfunc)SortedMap_new,                 /* tp_new */
    (freefunc)0,                            /* tp_free */
    (inquiry)0,                             /* tp_is_gc */
    0,                                      /* tp_bases */
    0,                                      /* tp_mro */
    0,                                      /* tp_cache */
    0,                                      /* tp_subclasses */
    0,                                      /* tp_weaklist */
    (destructor)0                           /* tp_del */
};


static PyMethodDef
sortedmap_methods[] = {
    { 0 } // Sentinel
};


PyMODINIT_FUNC
initsortedmap( void )
{
    PyObject* mod = Py_InitModule( "sortedmap", sortedmap_methods );
    if( !mod )
        return;
    if( PyType_Ready( &SortedMap_Type ) )
        return;
    Py_INCREF( ( PyObject* )( &SortedMap_Type ) );
    PyModule_AddObject( mod, "sortedmap", ( PyObject* )( &SortedMap_Type ) );
}
